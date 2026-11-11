#include "gfx/frame_pacer.h"

#include <cstdio>
#include <cstring>

#if defined(__ANDROID__)
#include <SDL3/SDL_system.h>
#include <android/native_window.h>
#include <jni.h>
#include <swappy/swappyVk.h>

#include "platform/android/android_glue.h"
#endif

namespace rf {

void FramePacer::configure(bool useSwappy, float targetHz) {
#if defined(__ANDROID__)
    wantSwappy_ = useSwappy;
#else
    (void)useSwappy;
    wantSwappy_ = false;
#endif
    targetHz_ = targetHz;
}

u32 FramePacer::deviceExtensions(VkPhysicalDevice physical, VkExtensionProperties* available, u32 availableCount,
                                 const char** out, u32 maxOut) {
#if defined(__ANDROID__)
    if (!wantSwappy_) return 0;
    u32 required = 0;
    SwappyVk_determineDeviceExtensions(physical, availableCount, available, &required, nullptr);
    if (required > countOf(extensionNames_) || required > maxOut) {
        RF_WARN("swappy: wants %u device extensions, ignoring", required);
        return 0;
    }
    char* names[4] = {extensionNames_[0], extensionNames_[1], extensionNames_[2], extensionNames_[3]};
    SwappyVk_determineDeviceExtensions(physical, availableCount, available, &required, names);
    for (u32 i = 0; i < required; ++i) out[i] = extensionNames_[i];
    return required;
#else
    (void)physical;
    (void)available;
    (void)availableCount;
    (void)out;
    (void)maxOut;
    return 0;
#endif
}

void FramePacer::onDeviceCreated(VkPhysicalDevice physical, VkDevice device, VkQueue queue, u32 queueFamily) {
    physical_ = physical;
    device_ = device;
#if defined(__ANDROID__)
    if (wantSwappy_) {
        SwappyVk_setAutoSwapInterval(false);
        SwappyVk_setAutoPipelineMode(false);
        SwappyVk_setQueueFamilyIndex(device, queue, queueFamily);
        const u32 v = Swappy_version();  // packed (major << 16) | (minor << 8) | bugfix
        RF_LOG("swappy: library version %u.%u.%u", v >> 16, (v >> 8) & 0xFFu, v & 0xFFu);
    }
#else
    (void)queue;
    (void)queueFamily;
#endif
}

void FramePacer::onSwapchainCreated(VkSwapchainKHR swapchain, void* nativeWindow) {
    swappyActive_ = false;
    intervalNs_ = 0;
#if defined(__ANDROID__)
    if (!wantSwappy_) return;
    JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    jobject activity = android::activityRef();
    u64 refreshNs = 0;
    if (!env || !activity ||
        !SwappyVk_initAndGetRefreshCycleDuration(env, activity, physical_, device_, swapchain, &refreshNs)) {
        RF_WARN("swappy: init failed, presenting with plain FIFO");
        return;
    }
    swappyActive_ = true;
    swappyRefreshNs_ = refreshNs;
    SwappyVk_setWindow(device_, swapchain, static_cast<ANativeWindow*>(nativeWindow));

    const int n = SwappyVk_getSupportedRefreshPeriodsNS(nullptr, 0, swapchain);
    supportedCount_ = n > 0 ? static_cast<u32>(n < 8 ? n : 8) : 0;
    if (supportedCount_) SwappyVk_getSupportedRefreshPeriodsNS(supportedPeriods_, static_cast<int>(supportedCount_), swapchain);

    // Target: the requested rate, else the fastest period the panel supports.
    u64 wanted = targetHz_ > 0.0f ? static_cast<u64>(1e9 / targetHz_) : 0;
    u64 best = 0;
    for (u32 i = 0; i < supportedCount_; ++i) {
        const u64 p = supportedPeriods_[i];
        if (wanted == 0) {
            if (best == 0 || p < best) best = p;
        } else {
            const u64 d = p > wanted ? p - wanted : wanted - p;
            const u64 bd = best > wanted ? best - wanted : wanted - best;
            if (best == 0 || d < bd) best = p;
        }
    }
    if (best == 0) best = wanted ? wanted : refreshNs;
    intervalNs_ = best;
    SwappyVk_setSwapIntervalNS(device_, swapchain, intervalNs_);
    char list[128] = {};
    size_t used = 0;
    for (u32 i = 0; i < supportedCount_ && used + 16 < sizeof list; ++i)
        used += static_cast<size_t>(std::snprintf(list + used, sizeof list - used, "%s%.2f", i ? "/" : "",
                                                  1e9 / static_cast<double>(supportedPeriods_[i])));
    RF_LOG("swappy: refresh %.3f ms, supported Hz [%s], swap interval %.3f ms", refreshNs * 1e-6, list,
           intervalNs_ * 1e-6);
#else
    (void)swapchain;
    (void)nativeWindow;
#endif
}

void FramePacer::onSwapchainDestroying(VkSwapchainKHR swapchain) {
#if defined(__ANDROID__)
    if (swappyActive_) SwappyVk_destroySwapchain(device_, swapchain);
#else
    (void)swapchain;
#endif
    swappyActive_ = false;
}

void FramePacer::onDeviceDestroying(VkDevice device) {
#if defined(__ANDROID__)
    if (wantSwappy_) SwappyVk_destroyDevice(device);
#else
    (void)device;
#endif
    device_ = VK_NULL_HANDLE;
}

VkResult FramePacer::present(VkQueue queue, const VkPresentInfoKHR& info) {
#if defined(__ANDROID__)
    if (swappyActive_) return SwappyVk_queuePresent(queue, &info);
#endif
    return vkQueuePresentKHR(queue, &info);
}

u64 FramePacer::refreshPeriodNs() const {
    if (swappyActive_ && swappyRefreshNs_) return swappyRefreshNs_;
    return displayHz_ > 1.0f ? static_cast<u64>(1e9 / displayHz_) : 16666667ull;
}

u64 FramePacer::targetIntervalNs() const {
    if (swappyActive_ && intervalNs_) return intervalNs_;
    return refreshPeriodNs();
}

}  // namespace rf
