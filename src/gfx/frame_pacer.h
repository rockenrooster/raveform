#pragma once
// Presentation and pacing. Android: Swappy (Android Frame Pacing) on top of FIFO, with a plain FIFO
// fallback (--no-swappy, or Swappy init failure: PLAN 9 risk 11). Desktop: plain FIFO.
//
// Display-side measurement for benchmarks (when the device has VK_GOOGLE_display_timing):
// - plain FIFO: every present carries a present ID and pollDisplayTimings() returns the time each frame
//   actually reached the panel (the present fence), so intervals can be measured on the display;
// - Swappy: Swappy owns the extension, so its own frame statistics are used instead (histograms in
//   refresh periods, see SwappyStats).

#include "core/types.h"
#include "gfx/vk.h"

namespace rf {

// One frame as the panel showed it (VK_GOOGLE_display_timing, CLOCK_MONOTONIC nanoseconds).
struct DisplayTiming {
    u32 presentId = 0;
    u64 actualNs = 0;    // 0: the timestamp was invalid (frame dropped or not reported)
    u64 earliestNs = 0;
    u64 marginNs = 0;
};

// Swappy's frame statistics (display side, in refresh periods; bucket 5 means 5 or more).
struct PacerStats {
    bool valid = false;
    u64 totalFrames = 0;
    u64 offsetFromPrevious[6] = {};  // refresh periods between consecutive frames on the panel
    u64 lateFrames[6] = {};          // refresh periods after the time Swappy asked for
    u64 idleFrames[6] = {};          // refresh periods a finished frame waited in the compositor queue
    u64 latencyFrames[6] = {};       // refresh periods from frame start to the panel
};

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

    // After a successful acquire (Swappy statistics need the frame start).
    void onFrameStart(VkQueue queue, u32 imageIndex);
    VkResult present(VkQueue queue, const VkPresentInfoKHR& info);

    // Display timing (plain FIFO only). Returns up to `max` frames that reached the panel since the
    // last call, oldest first. Benchmarks only: the query may talk to the compositor.
    bool displayTimingActive() const { return displayTiming_ && !swappyActive_ && swapchain_; }
    u32 pollDisplayTimings(DisplayTiming* out, u32 max);
    // Present ID of the last presented frame (0 when display timing is not active).
    u32 lastPresentId() const { return displayTimingActive() ? nextPresentId_ : 0; }
    // Swappy statistics (Swappy only).
    void enableStats(bool enabled);
    void clearStats();
    PacerStats stats() const;

    // Live panel refresh from the platform (SDL display mode or the Android Display), refreshed by
    // the app on start-up, display events and after the refresh-rate vote settles. With Swappy, the
    // swap interval follows it when the panel runs slower than requested (a system or user setting
    // overruled the vote): Swappy then never aims at presents the panel cannot show.
    void setDisplayRefreshHz(float hz);

    bool swappyActive() const { return swappyActive_; }
    const char* modeName() const { return swappyActive_ ? "swappy" : "fifo"; }
    // The panel's current refresh period (live display mode; Swappy's value at swapchain creation as
    // a fallback).
    u64 refreshPeriodNs() const;
    // The frame interval the pacer achieves: the requested swap interval, but never shorter than the
    // panel's refresh period (a vote for a faster mode can be overruled by the system).
    u64 targetIntervalNs() const;
    // The frame interval asked for (target rate or panel maximum) and the swap interval Swappy uses
    // now (0 for plain FIFO).
    u64 requestedIntervalNs() const { return swappyActive_ ? requestedNs_ : 0; }
    u64 swapIntervalNs() const { return swappyActive_ ? intervalNs_ : 0; }
    u32 supportedPeriodCount() const { return supportedCount_; }
    u64 supportedPeriodNs(u32 i) const { return supportedPeriods_[i]; }
    // operator new calls made inside Swappy calls since start (Swappy allocates there; we do not).
    u64 pacerAllocations() const { return pacerAllocs_; }
    // Refresh-rate vote on the surface (Android API 30+): the rate asked for, the result code
    // (0 = accepted by the window; -1 = the API is missing) and the API used.
    float votedHz() const { return votedHz_; }
    int voteResult() const { return voteResult_; }
    const char* voteApi() const { return voteApi_; }

private:
    void vote(void* nativeWindow, float hz);

    bool wantSwappy_ = false;
    bool swappyActive_ = false;
    bool displayTiming_ = false;  // VK_GOOGLE_display_timing enabled on the device
    bool wantStats_ = false;
    bool statsActive_ = false;
    float targetHz_ = 0.0f;
    float displayHz_ = 0.0f;
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    u64 swappyRefreshNs_ = 0;
    u64 requestedNs_ = 0;  // what the swapchain asked for (the vote)
    u64 intervalNs_ = 0;   // what Swappy uses: requestedNs_, or the panel period when that is longer
    u64 pacerAllocs_ = 0;
    u32 nextPresentId_ = 0;
    u32 supportedCount_ = 0;
    u64 supportedPeriods_[8] = {};
    float votedHz_ = 0.0f;
    int voteResult_ = -1;
    const char* voteApi_ = "none";
    [[maybe_unused]] char extensionNames_[4][VK_MAX_EXTENSION_NAME_SIZE] = {};  // Swappy's (Android)
};

}  // namespace rf
