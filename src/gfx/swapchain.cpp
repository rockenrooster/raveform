#include "gfx/swapchain.h"

#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#include <utility>

#include "gfx/frame_pacer.h"
#include "gfx/gpu_context.h"

#if defined(__ANDROID__)
#include <android/native_window.h>

#include "platform/android/android_glue.h"
#endif

namespace rf {

int rotationDegrees(VkSurfaceTransformFlagBitsKHR transform) {
    switch (transform) {
        case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR: return 90;
        case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR: return 180;
        case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR: return 270;
        default: return 0;
    }
}

const char* transformName(VkSurfaceTransformFlagBitsKHR transform) {
    switch (transform) {
        case VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR: return "identity";
        case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR: return "rotate90";
        case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR: return "rotate180";
        case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR: return "rotate270";
        case VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR: return "inherit";
        default: return "mirror";
    }
}

namespace {

const char* formatName(VkFormat f) {
    switch (f) {
        case VK_FORMAT_B8G8R8A8_SRGB: return "B8G8R8A8_SRGB";
        case VK_FORMAT_R8G8B8A8_SRGB: return "R8G8B8A8_SRGB";
        case VK_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
        case VK_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return "A2B10G10R10_UNORM";
        default: return "other";
    }
}

u32 clampU32(u32 v, u32 lo, u32 hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace

bool Swapchain::createSurface(GpuContext& gpu, SDL_Window* window) {
#if defined(__ANDROID__)
    auto* win = static_cast<ANativeWindow*>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr));
    if (!win) return false;
    ANativeWindow_acquire(win);
    android::setSurfaceInUse(true);  // surfaceDestroyed() on the UI thread now waits for us

    VkAndroidSurfaceCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    info.window = win;
    const VkResult r = vkCreateAndroidSurfaceKHR(gpu.instance, &info, nullptr, &surface);
    if (r != VK_SUCCESS) {
        RF_ERR("vkCreateAndroidSurfaceKHR: %s", vkResultName(r));
        surface = VK_NULL_HANDLE;
        ANativeWindow_release(win);
        android::setSurfaceInUse(false);
        return false;
    }
    nativeWindow = win;
    RF_LOG("surface: ANativeWindow %dx%d", ANativeWindow_getWidth(win), ANativeWindow_getHeight(win));
    return true;
#else
    if (!SDL_Vulkan_CreateSurface(window, gpu.instance, nullptr, &surface)) {
        RF_ERR("SDL_Vulkan_CreateSurface: %s", SDL_GetError());
        surface = VK_NULL_HANDLE;
        return false;
    }
    return true;
#endif
}

void Swapchain::destroySurface(GpuContext& gpu) {
    if (surface) {
        vkDestroySurfaceKHR(gpu.instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
#if defined(__ANDROID__)
    if (nativeWindow) {
        ANativeWindow_release(static_cast<ANativeWindow*>(nativeWindow));
        nativeWindow = nullptr;
    }
    android::setSurfaceInUse(false);
#endif
}

bool Swapchain::create(GpuContext& gpu, FramePacer& pacer, SDL_Window* window) {
    VkBool32 supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(gpu.physical, gpu.queueFamily, surface, &supported);
    if (!supported) {
        RF_ERR("swapchain: queue family %u cannot present to this surface", gpu.queueFamily);
        return false;
    }

    VkSurfaceCapabilitiesKHR caps;
    VkResult r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu.physical, surface, &caps);
    if (r != VK_SUCCESS) {
        RF_ERR("vkGetPhysicalDeviceSurfaceCapabilitiesKHR: %s", vkResultName(r));
        return false;
    }

    // currentExtent is in the current (rotated) orientation, i.e. the logical landscape size.
    VkExtent2D current = caps.currentExtent;
    if (current.width == 0xFFFFFFFFu) {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        current.width = clampU32(static_cast<u32>(w), caps.minImageExtent.width, caps.maxImageExtent.width);
        current.height = clampU32(static_cast<u32>(h), caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (current.width == 0 || current.height == 0) return false;  // minimized

    transform = caps.currentTransform;
    rotationDeg = rotationDegrees(transform);
    if (rotationDeg == 0 && transform != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR &&
        (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)) {
        // Mirrored or inherited transforms: let the compositor handle them.
        transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    logical = current;
    extent = current;
    if (rotationDeg == 90 || rotationDeg == 270) std::swap(extent.width, extent.height);

    VkSurfaceFormatKHR formats[64];
    u32 formatCount = countOf(formats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.physical, surface, &formatCount, formats);
    if (formatCount == 0) {
        RF_ERR("swapchain: surface reports no formats");
        return false;
    }
    int chosen = -1;
    const VkFormat preferred[] = {VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM,
                                  VK_FORMAT_R8G8B8A8_UNORM};
    const auto pick = [&](VkColorSpaceKHR space) {
        for (u32 p = 0; p < countOf(preferred); ++p)
            for (u32 i = 0; i < formatCount; ++i)
                if (formats[i].format == preferred[p] && formats[i].colorSpace == space) return static_cast<int>(i);
        return -1;
    };
    // TECH_SPEC 5.2: Display-P3 (its curve is sRGB's) for the VIVID look, where offered; sRGB otherwise.
    const int p3 = gpu.colorSpaces() ? pick(VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT) : -1;
    p3Offered = p3 >= 0;
    if (wantP3 && p3 >= 0) chosen = p3;
    if (chosen < 0) chosen = pick(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    if (chosen < 0) chosen = 0;
    format = formats[chosen].format;
    colorSpace = formats[chosen].colorSpace;
    srgbFormat = format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB;

    VkPresentModeKHR modes[8];
    u32 modeCount = countOf(modes);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.physical, surface, &modeCount, modes);  // FIFO is always there

    u32 wantImages = caps.minImageCount > 3 ? caps.minImageCount : 3;
    if (caps.maxImageCount && wantImages > caps.maxImageCount) wantImages = caps.maxImageCount;
    if (wantImages > kMaxImages) wantImages = kMaxImages;
    canCapture = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;

    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    const VkCompositeAlphaFlagBitsKHR alphaOrder[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR};
    for (VkCompositeAlphaFlagBitsKHR a : alphaOrder)
        if (caps.supportedCompositeAlpha & a) {
            alpha = a;
            break;
        }

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface;
    info.minImageCount = wantImages;
    info.imageFormat = format;
    info.imageColorSpace = colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (canCapture ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0u);
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = transform;
    info.compositeAlpha = alpha;
    info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    info.clipped = VK_TRUE;
    r = vkCreateSwapchainKHR(gpu.device, &info, nullptr, &handle);
    if (r != VK_SUCCESS) {
        RF_ERR("vkCreateSwapchainKHR: %s", vkResultName(r));
        handle = VK_NULL_HANDLE;
        return false;
    }

    imageCount = kMaxImages;
    r = vkGetSwapchainImagesKHR(gpu.device, handle, &imageCount, images);
    if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
        RF_ERR("vkGetSwapchainImagesKHR: %s", vkResultName(r));
        destroy(gpu, pacer);
        return false;
    }
    for (u32 i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo view{};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = images[i];
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = format;
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.levelCount = 1;
        view.subresourceRange.layerCount = 1;
        r = vkCreateImageView(gpu.device, &view, nullptr, &views[i]);
        if (r != VK_SUCCESS) {
            RF_ERR("vkCreateImageView: %s", vkResultName(r));
            destroy(gpu, pacer);
            return false;
        }
    }

    pacer.onSwapchainCreated(handle, nativeWindow);
    RF_LOG("swapchain: image %ux%u, logical %ux%u, %s, %s (Display-P3 %s), %u images, transform %s (%d deg), present %s, capture %s",
           extent.width, extent.height, logical.width, logical.height, formatName(format),
           colorSpace == VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT ? "Display-P3" : (colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ? "sRGB" : "other colour space"),
           p3Offered ? "offered" : "not offered", imageCount, transformName(caps.currentTransform), rotationDeg, pacer.modeName(),
           canCapture ? "yes" : "no");
    return true;
}

void Swapchain::destroy(GpuContext& gpu, FramePacer& pacer) {
    for (u32 i = 0; i < kMaxImages; ++i) {
        if (views[i]) vkDestroyImageView(gpu.device, views[i], nullptr);
        views[i] = VK_NULL_HANDLE;
        images[i] = VK_NULL_HANDLE;
    }
    if (handle) {
        pacer.onSwapchainDestroying(handle);
        vkDestroySwapchainKHR(gpu.device, handle, nullptr);
        handle = VK_NULL_HANDLE;
    }
    imageCount = 0;
}

}  // namespace rf
