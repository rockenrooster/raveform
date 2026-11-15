#pragma once
// Window surface and swapchain with Android pre-rotation (TECH_SPEC 5.2): preTransform =
// currentTransform, the image extent in the panel's native orientation, and the rotation applied in
// clip space by the shaders. Everything upstream works in the logical landscape size.

#include "core/types.h"
#include "gfx/vk.h"

struct SDL_Window;

namespace rf {

class GpuContext;
class FramePacer;

// Clockwise degrees the content must be rotated by for a surface transform (0, 90, 180, 270).
int rotationDegrees(VkSurfaceTransformFlagBitsKHR transform);
const char* transformName(VkSurfaceTransformFlagBitsKHR transform);

class Swapchain {
public:
    static constexpr u32 kMaxImages = 8;

    // Android: false while the activity has no native window (backgrounded). The surface holds its
    // own ANativeWindow reference until destroySurface().
    bool createSurface(GpuContext& gpu, SDL_Window* window);
    void destroySurface(GpuContext& gpu);
    bool hasSurface() const { return surface != VK_NULL_HANDLE; }

    // False (with no swapchain) when the surface is zero-sized (minimized) or on error.
    bool create(GpuContext& gpu, FramePacer& pacer, SDL_Window* window);
    void destroy(GpuContext& gpu, FramePacer& pacer);
    bool valid() const { return handle != VK_NULL_HANDLE; }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D extent{};   // physical image size (panel orientation)
    VkExtent2D logical{};  // landscape size the game draws in
    VkSurfaceTransformFlagBitsKHR transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    int rotationDeg = 0;
    bool srgbFormat = true;  // the format encodes linear -> sRGB on write
    bool canCapture = false; // images allow TRANSFER_SRC (screenshots)
    // TECH_SPEC 5.2 (M6): the VIVID look presents in Display-P3 (the same values, mapped by the display to its wider gamut)
    // where the surface offers an 8-bit format in that colour space. wantP3 is the setting; p3Offered what the surface lists.
    bool wantP3 = false;
    bool p3Offered = false;
    u32 imageCount = 0;
    VkImage images[kMaxImages] = {};
    VkImageView views[kMaxImages] = {};
    void* nativeWindow = nullptr;  // Android ANativeWindow*
};

}  // namespace rf
