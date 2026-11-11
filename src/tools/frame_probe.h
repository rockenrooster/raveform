#pragma once
// Per-frame attribution for benchmarks (TECH_SPEC 9). Every present-to-present interval is split into
// the main thread's phases (they add up to the interval), and paired with what the scheduler did to
// the thread meanwhile: CPU time, run-queue wait (runnable but not running = preempted or waiting for
// a core), context switches, page faults and the CPU it ran on. Where the device reports it, the time
// each frame reached the panel is attached as well. Late frames are classified, the worst ones kept
// with their full breakdown, and a per-second timeline lets scripts line the run up with the device's
// CPU load. Fixed storage, no allocation after init(); a few cheap syscalls per frame (Linux/Android).

#include "core/stats.h"
#include "core/types.h"

namespace rf {

class JsonWriter;
struct DisplayTiming;

enum class Phase : u8 {
    Post,     // previous present returned -> next loop start: bookkeeping, overlay text, benchmark
    Events,   // SDL event pump
    Audio,    // AudioDevice::update()
    Begin,    // beginFrame minus its two waits: surface checks, fence reset, frame start
    Fence,    // vkWaitForFences: the frame slot (GPU work two frames back)
    Acquire,  // vkAcquireNextImageKHR
    Scene,    // scene and overlay into the capsule batch
    Record,   // command buffer recording
    Submit,   // vkQueueSubmit
    Present,  // the present call (Swappy waits here)
    Count
};

class FrameProbe {
public:
    static constexpr u32 kPhases = static_cast<u32>(Phase::Count);
    static constexpr u32 kWorst = 16;     // frames kept with their full breakdown
    static constexpr u32 kSeconds = 600;  // timeline buckets
    static constexpr u32 kCpus = 16;
    static constexpr u32 kPending = 32;   // frames waiting for their display timing

    static const char* phaseName(u32 i);
    // CPU the calling thread runs on (-1 when unknown).
    static int currentCpu();

    // Once, before the benchmark: allocates the recorders and opens the scheduler statistics of the
    // calling (main) thread.
    bool init(u32 capacity);
    // Start of the measured window. `displayIds`: frames carry display-timing IDs.
    void begin(u64 nowNs, bool displayIds);
    // Every presented frame while a benchmark runs; outside the window only the scheduler baseline is
    // kept. phaseNs sums to intervalMs. presentId 0 = no display timing for this frame.
    void onFrame(u64 presentNs, float intervalMs, const u64 phaseNs[kPhases], int cpuStart, float targetMs,
                 u32 presentId, bool measuring);
    // A frame that reached the panel (VK_GOOGLE_display_timing), any order of arrival per poll.
    void onDisplayTiming(const DisplayTiming& t, float refreshMs);
    // End of the window: flushes frames still waiting for display timing.
    void finish(float targetMs, float refreshMs);

    // JSON members (inside an open object). `full` adds the worst-frame table and the timeline.
    void write(JsonWriter& j, bool full, float targetMs, float refreshMs);

    u32 displayMissed() const { return displayMissed_; }
    u32 displayCount() const { return display_.count(); }

private:
    struct Record {
        u32 frame = 0;           // index in the measured window
        u32 presentId = 0;
        u32 age = 0;             // frames since it was recorded (pending only)
        float t = 0.0f;          // seconds since the window started
        float intervalMs = 0.0f; // present call to present call
        float displayMs = -1.0f; // on the panel (-1: unknown)
        float marginMs = 0.0f;   // display timing present margin
        float phaseMs[kPhases] = {};
        float threadCpuMs = -1.0f;
        float runWaitMs = -1.0f;
        i32 nvcsw = -1;
        i32 nivcsw = -1;
        i32 minflt = -1;
        i32 majflt = -1;
        i32 cpuStart = -1;
        i32 cpuEnd = -1;
        bool used = false;
    };
    struct ThreadSample {
        u64 cpuNs = 0;
        u64 runWaitNs = 0;
        i64 nvcsw = 0;
        i64 nivcsw = 0;
        i64 minflt = 0;
        i64 majflt = 0;
        bool ok = false;
        bool runWaitOk = false;
    };
    struct Second {
        u16 frames = 0;
        u16 missed = 0;
        u16 displayMissed = 0;
        u16 nivcsw = 0;
        float maxMs = 0.0f;
        float runWaitMs = 0.0f;
        float threadCpuMs = 0.0f;
    };

    bool sampleThread(ThreadSample& s) const;
    void finalize(const Record& r, float targetMs, float refreshMs);

    [[maybe_unused]] int schedFd_ = -1;  // Linux/Android only
    bool displayIds_ = false;
    u64 windowStartNs_ = 0;
    double bootStartSec_ = -1.0;
    [[maybe_unused]] u64 processCpuStartNs_ = 0;
    u64 processCpuNs_ = 0;
    float pendingTargetMs_ = 16.667f;  // CPU-side target for frames finalized by a display timing
    u32 frames_ = 0;
    u32 firstPresentId_ = 0;
    ThreadSample prev_;

    // Aggregates over the window.
    double phaseSumMs_[kPhases] = {};
    float phaseMaxMs_[kPhases] = {};
    double threadCpuMs_ = 0.0;
    double runWaitMs_ = 0.0;
    float runWaitMaxMs_ = 0.0f;
    u32 runWaitOver1_ = 0;
    u32 runWaitOver4_ = 0;
    i64 nvcsw_ = 0;
    i64 nivcsw_ = 0;
    i64 minflt_ = 0;
    i64 majflt_ = 0;
    u32 cpuHist_[kCpus] = {};
    u32 migrations_ = 0;
    SampleRecorder work_;     // interval minus the fence, acquire and present waits
    SampleRecorder display_;  // present-to-present on the panel
    Summary workSummary_;     // computed by finish()
    Summary displaySummary_;

    // Late frames (interval > 1.5 x target on the CPU side, or > 1.5 refresh periods on the panel).
    u32 missed_ = 0;
    u32 missedWork_ = 0;       // the main thread's own work (not the waits) took >= half the target
    u32 missedWaits_ = 0;      // fence, acquire or present waits dominated
    u32 missedRunWait_ = 0;    // the thread waited >= 2 ms for a CPU in that interval
    u32 missedFaults_ = 0;     // major page faults in that interval
    u32 dominant_[kPhases] = {};
    u32 displayMissed_ = 0;
    u32 displayMissedCpuOk_ = 0;  // missed on the panel although the CPU-side interval was fine
    u32 displayDropped_ = 0;      // timestamps reported invalid (frame never shown)
    u32 displayHist_[5] = {};     // display interval in refresh periods: 1, 2, 3, 4, 5+ (index 0: < 0.5)
    u32 lastDisplayId_ = 0;
    u64 lastDisplayNs_ = 0;

    Record pending_[kPending];
    Record worst_[kWorst];
    Second seconds_[kSeconds];
};

}  // namespace rf
