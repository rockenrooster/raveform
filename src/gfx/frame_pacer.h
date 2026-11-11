#pragma once
// Presentation and pacing. Android: Swappy (Android Frame Pacing) on top of FIFO, with a plain FIFO
// fallback (--no-swappy, or Swappy init failure: PLAN 9 risk 11). Desktop: plain FIFO.

#include "core/types.h"
#include "gfx/vk.h"

namespace rf {

class FramePacer {
public:
    // targetHz 0 = the panel's highest supported refresh rate.
    void configure(bool useSwappy, float targetHz);

    // Before vkCreateDevice: writes extension names the pacer needs (subset of `available`) to `out`.
    u32 deviceExtensions(VkPhysicalDevice physical, VkExtensionProperties* available, u32 availableCount,
                         const char** out, u32 maxOut);
    void onDeviceCreated(VkPhysicalDevice physical, VkDevice device, VkQueue queue, u32 queueFamily);
    void onSwapchainCreated(VkSwapchainKHR swapchain, void* nativeWindow);
    void onSwapchainDestroying(VkSwapchainKHR swapchain);
    void onDeviceDestroying(VkDevice device);

    VkResult present(VkQueue queue, const VkPresentInfoKHR& info);

    // Display refresh from the platform (SDL display mode or the Android Display), used when Swappy
    // does not report one.
    void setDisplayRefreshHz(float hz) { displayHz_ = hz; }

    bool swappyActive() const { return swappyActive_; }
    const char* modeName() const { return swappyActive_ ? "swappy" : "fifo"; }
    // Refresh period reported by Swappy at swapchain creation, else from the display mode.
    u64 refreshPeriodNs() const;
    // The frame interval the pacer aims for (the refresh period for plain FIFO).
    u64 targetIntervalNs() const;
    u32 supportedPeriodCount() const { return supportedCount_; }
    u64 supportedPeriodNs(u32 i) const { return supportedPeriods_[i]; }
    // operator new calls made inside present() since start (Swappy allocates there; we do not).
    u64 presentAllocations() const { return presentAllocs_; }

private:
    bool wantSwappy_ = false;
    bool swappyActive_ = false;
    float targetHz_ = 0.0f;
    float displayHz_ = 0.0f;
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    u64 swappyRefreshNs_ = 0;
    u64 intervalNs_ = 0;
    u64 presentAllocs_ = 0;
    u32 supportedCount_ = 0;
    u64 supportedPeriods_[8] = {};
    [[maybe_unused]] char extensionNames_[4][VK_MAX_EXTENSION_NAME_SIZE] = {};  // Swappy's (Android)
};

}  // namespace rf
