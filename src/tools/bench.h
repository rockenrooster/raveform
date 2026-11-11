#pragma once
// Benchmark scenarios (TECH_SPEC 9). bench_idle: discard a warm-up window, then record every
// present-to-present interval and CPU frame time for N seconds; the app prints one RFBENCH line.

#include "core/stats.h"
#include "core/types.h"

namespace rf {

class Bench {
public:
    enum class Phase { Off, Warmup, Measure, Done, Aborted };
    enum class Event { None, MeasureStarted, Finished };

    bool begin(const char* scenario, float warmupSec, float measureSec, u64 nowNs);
    // Once per presented frame; intervalMs < 0 when the frame has no valid interval (first frame).
    Event onFrame(u64 nowNs, float intervalMs, float cpuMs);
    void abort(const char* reason);
    // The window lost focus during the run (shared devices: another app, a dialog, the shade).
    // The run continues but is reported as invalid.
    void noteInterruption() {
        if (active()) ++interruptions_;
    }
    u32 interruptions() const { return interruptions_; }

    bool active() const { return phase_ == Phase::Warmup || phase_ == Phase::Measure; }
    bool finished() const { return phase_ == Phase::Done || phase_ == Phase::Aborted; }
    Phase phase() const { return phase_; }
    const char* scenario() const { return scenario_; }
    const char* abortReason() const { return abortReason_; }
    float warmupSeconds() const { return warmupSec_; }
    float measuredSeconds() const { return measuredSec_; }

    // Valid after Finished. finish*() sort the recorded samples, so call countAbove() first.
    u32 intervalCount() const { return intervals_.count(); }
    u32 intervalsAbove(float ms) const { return intervals_.countAbove(ms); }
    bool overflowed() const { return intervals_.overflowed() || cpu_.overflowed(); }
    Summary finishIntervals() { return intervals_.finish(); }
    Summary finishCpu() { return cpu_.finish(); }

private:
    Phase phase_ = Phase::Off;
    char scenario_[32] = {};
    const char* abortReason_ = "";
    float warmupSec_ = 0.0f;
    float measureSec_ = 0.0f;
    float measuredSec_ = 0.0f;
    u32 interruptions_ = 0;
    u64 phaseStartNs_ = 0;
    SampleRecorder intervals_;
    SampleRecorder cpu_;
};

}  // namespace rf
