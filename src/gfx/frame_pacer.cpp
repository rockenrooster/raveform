#include "gfx/frame_pacer.h"

#include <cstdio>
#include <cstring>

#include "core/alloc_counter.h"

#if defined(__ANDROID__)
#include <SDL3/SDL_system.h>
#include <android/native_window.h>
#include <dlfcn.h>
#include <jni.h>
#include <swappy/swappyVk.h>

#include "platform/android/android_glue.h"
#endif

namespace rf {

#if defined(__ANDROID__)
namespace {

// Refresh-rate vote on the surface (TECH_SPEC 3.1). API 30/31 functions of libnativewindow.so, looked
// up at run time because minSdk is 29. libmain.so does not link libnativewindow.so, so the library is
// opened explicitly (dlsym(RTLD_DEFAULT) does not search it and found nothing on the test phones).
using SetFrameRateWithStrategy = int32_t (*)(ANativeWindow*, float, int8_t, int8_t);
using SetFrameRate = int32_t (*)(ANativeWindow*, float, int8_t);

struct FrameRateApi {
    SetFrameRateWithStrategy withStrategy = nullptr;
    SetFrameRate plain = nullptr;
};

const FrameRateApi& frameRateApi() {
    static const FrameRateApi api = [] {
        FrameRateApi a;
        if (void* lib = dlopen("libnativewindow.so", RTLD_NOW | RTLD_LOCAL)) {  // never closed
            a.withStrategy = reinterpret_cast<SetFrameRateWithStrategy>(
                dlsym(lib, "ANativeWindow_setFrameRateWithChangeStrategy"));
            a.plain = reinterpret_cast<SetFrameRate>(dlsym(lib, "ANativeWindow_setFrameRate"));
        }
        return a;
    }();
    return api;
}

}  // namespace
#endif

void FramePacer::vote(void* nativeWindow, float hz) {
    votedHz_ = hz;
    voteResult_ = -1;
    voteApi_ = "none";
#if defined(__ANDROID__)
    constexpr int8_t kCompatDefault = 0;  // ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_DEFAULT (games)
    constexpr int8_t kChangeAlways = 1;   // ANATIVEWINDOW_CHANGE_FRAME_RATE_ALWAYS
    const FrameRateApi& api = frameRateApi();
    auto* w = static_cast<ANativeWindow*>(nativeWindow);
    if (w && api.withStrategy) {
        voteResult_ = api.withStrategy(w, hz, kCompatDefault, kChangeAlways);
        voteApi_ = "setFrameRateWithChangeStrategy";
    } else if (w && api.plain) {
        voteResult_ = api.plain(w, hz, kCompatDefault);
        voteApi_ = "setFrameRate";
    }
    RF_LOG("display: voted %.2f Hz via ANativeWindow_%s -> %d", hz, voteApi_, voteResult_);
#else
    (void)nativeWindow;
#endif
}

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
    if (!wantSwappy_) {
        // Plain FIFO: display timing only measures (present IDs with "as soon as possible" times).
        for (u32 i = 0; i < availableCount && maxOut > 0; ++i) {
            if (std::strcmp(available[i].extensionName, VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME) == 0) {
                out[0] = VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME;
                displayTiming_ = true;
                return 1;
            }
        }
        return 0;
    }
    u32 required = 0;
    SwappyVk_determineDeviceExtensions(physical, availableCount, available, &required, nullptr);
    if (required > countOf(extensionNames_) || required > maxOut) {
        RF_WARN("swappy: wants %u device extensions, ignoring", required);
        return 0;
    }
    char* names[4] = {extensionNames_[0], extensionNames_[1], extensionNames_[2], extensionNames_[3]};
    SwappyVk_determineDeviceExtensions(physical, availableCount, available, &required, names);
    for (u32 i = 0; i < required; ++i) {
        out[i] = extensionNames_[i];
        if (std::strcmp(extensionNames_[i], VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME) == 0) displayTiming_ = true;
    }
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
    statsActive_ = false;
    intervalNs_ = 0;
    swapchain_ = swapchain;
#if defined(__ANDROID__)
    if (!wantSwappy_) {
        // Plain FIFO: only an explicit --fps is voted for (the supported list comes from Swappy).
        if (targetHz_ > 0.0f) vote(nativeWindow, targetHz_);
        return;
    }
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
    requestedNs_ = best;
    intervalNs_ = best;
    SwappyVk_setSwapIntervalNS(device_, swapchain, intervalNs_);
    vote(nativeWindow, static_cast<float>(1e9 / static_cast<double>(intervalNs_)));
    char list[128] = {};
    size_t used = 0;
    for (u32 i = 0; i < supportedCount_ && used + 16 < sizeof list; ++i)
        used += static_cast<size_t>(std::snprintf(list + used, sizeof list - used, "%s%.2f", i ? "/" : "",
                                                  1e9 / static_cast<double>(supportedPeriods_[i])));
    RF_LOG("swappy: refresh %.3f ms, supported Hz [%s], swap interval %.3f ms", refreshNs * 1e-6, list,
           intervalNs_ * 1e-6);
    if (displayHz_ > 1.0f) setDisplayRefreshHz(displayHz_);  // the panel may already be known to run slower
    if (wantStats_) enableStats(true);
#else
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
    statsActive_ = false;
    swapchain_ = VK_NULL_HANDLE;
}

void FramePacer::onDeviceDestroying(VkDevice device) {
#if defined(__ANDROID__)
    if (wantSwappy_) SwappyVk_destroyDevice(device);
#else
    (void)device;
#endif
    device_ = VK_NULL_HANDLE;
}

void FramePacer::onFrameStart(VkQueue queue, u32 imageIndex) {
#if defined(__ANDROID__)
    if (statsActive_) {
        const u64 before = heapAllocCountThisThread();
        SwappyVk_recordFrameStart(queue, swapchain_, imageIndex);
        pacerAllocs_ += heapAllocCountThisThread() - before;
    }
#else
    (void)queue;
    (void)imageIndex;
#endif
}

VkResult FramePacer::present(VkQueue queue, const VkPresentInfoKHR& info) {
    // Allocations inside the present call belong to the pacing library, not to our frame code.
    const u64 allocsBefore = heapAllocCountThisThread();
    VkResult r;
#if defined(__ANDROID__)
    if (swappyActive_) {
        r = SwappyVk_queuePresent(queue, &info);
    } else if (displayTimingActive()) {
        VkPresentTimeGOOGLE time{++nextPresentId_, 0};  // desired time 0: as soon as possible
        VkPresentTimesInfoGOOGLE times{};
        times.sType = VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE;
        times.pNext = info.pNext;
        times.swapchainCount = 1;
        times.pTimes = &time;
        VkPresentInfoKHR withTimes = info;
        withTimes.pNext = &times;
        r = vkQueuePresentKHR(queue, &withTimes);
    } else {
        r = vkQueuePresentKHR(queue, &info);
    }
#else
    r = vkQueuePresentKHR(queue, &info);
#endif
    pacerAllocs_ += heapAllocCountThisThread() - allocsBefore;
    return r;
}

u32 FramePacer::pollDisplayTimings(DisplayTiming* out, u32 max) {
    u32 n = 0;
#if defined(__ANDROID__)
    if (!displayTimingActive() || !vkGetPastPresentationTimingGOOGLE) return 0;
    VkPastPresentationTimingGOOGLE buf[16];
    while (n < max) {
        u32 count = max - n < countOf(buf) ? max - n : countOf(buf);
        const VkResult r = vkGetPastPresentationTimingGOOGLE(device_, swapchain_, &count, buf);
        if (r != VK_SUCCESS && r != VK_INCOMPLETE) break;
        for (u32 i = 0; i < count; ++i) {
            DisplayTiming& t = out[n++];
            t.presentId = buf[i].presentID;
            t.actualNs = buf[i].actualPresentTime;
            t.earliestNs = buf[i].earliestPresentTime;
            t.marginNs = buf[i].presentMargin;
        }
        if (r != VK_INCOMPLETE || count == 0) break;
    }
#else
    (void)out;
    (void)max;
#endif
    return n;
}

void FramePacer::enableStats(bool enabled) {
    wantStats_ = enabled;
#if defined(__ANDROID__)
    if (swappyActive_ && swapchain_ && displayTiming_) {
        SwappyVk_enableStats(swapchain_, enabled);
        statsActive_ = enabled;
    }
#endif
}

void FramePacer::clearStats() {
#if defined(__ANDROID__)
    if (statsActive_) SwappyVk_clearStats(swapchain_);
#endif
}

PacerStats FramePacer::stats() const {
    PacerStats s;
#if defined(__ANDROID__)
    if (!statsActive_) return s;
    SwappyStats raw{};
    SwappyVk_getStats(swapchain_, &raw);
    s.valid = true;
    s.totalFrames = raw.totalFrames;
    for (u32 i = 0; i < 6 && i < MAX_FRAME_BUCKETS; ++i) {
        s.offsetFromPrevious[i] = raw.offsetFromPreviousFrame[i];
        s.lateFrames[i] = raw.lateFrames[i];
        s.idleFrames[i] = raw.idleFrames[i];
        s.latencyFrames[i] = raw.latencyFrames[i];
    }
#endif
    return s;
}

void FramePacer::setDisplayRefreshHz(float hz) {
    displayHz_ = hz;
#if defined(__ANDROID__)
    if (!swappyActive_ || !swapchain_ || !requestedNs_ || hz <= 1.0f) return;
    const u64 panelNs = static_cast<u64>(1e9 / static_cast<double>(hz));
    // A different panel mode, not rounding (60.00 vs 59.94 Hz): only then change the interval.
    const u64 wanted = panelNs > requestedNs_ + requestedNs_ / 5 ? panelNs : requestedNs_;
    if (wanted == intervalNs_) return;
    intervalNs_ = wanted;
    SwappyVk_setSwapIntervalNS(device_, swapchain_, intervalNs_);
    RF_LOG("swappy: panel at %.2f Hz, swap interval now %.3f ms (asked for %.3f ms)", hz, intervalNs_ * 1e-6,
           requestedNs_ * 1e-6);
#endif
}

u64 FramePacer::refreshPeriodNs() const {
    if (displayHz_ > 1.0f) return static_cast<u64>(1e9 / displayHz_);
    if (swappyActive_ && swappyRefreshNs_) return swappyRefreshNs_;
    return 16666667ull;
}

u64 FramePacer::targetIntervalNs() const {
    const u64 refresh = refreshPeriodNs();
    if (swappyActive_ && intervalNs_ > refresh) return intervalNs_;
    return refresh;
}

}  // namespace rf
