#include "tools/bench.h"

#include <cstdio>

#include "core/log.h"

namespace rf {

bool Bench::begin(const char* scenario, float warmupSec, float measureSec, u64 nowNs) {
    std::snprintf(scenario_, sizeof scenario_, "%s", scenario);
    warmupSec_ = warmupSec;
    measureSec_ = measureSec;
    measuredSec_ = 0.0f;
    // Room for the whole run at up to 240 Hz plus slack; allocated once, before measuring.
    const u32 capacity = static_cast<u32>((measureSec + 5.0f) * 240.0f) + 1024;
    if (!intervals_.init(capacity) || !cpu_.init(capacity)) {
        RF_ERR("bench: cannot allocate %u samples", capacity);
        return false;
    }
    phase_ = warmupSec > 0.0f ? Phase::Warmup : Phase::Measure;
    phaseStartNs_ = nowNs;
    RF_LOG("bench: %s, warm-up %.1f s, measuring %.1f s", scenario_, warmupSec, measureSec);
    return true;
}

Bench::Event Bench::onFrame(u64 nowNs, float intervalMs, float cpuMs) {
    const double elapsed = static_cast<double>(nowNs - phaseStartNs_) * 1e-9;
    if (phase_ == Phase::Warmup) {
        if (elapsed >= warmupSec_) {
            phase_ = Phase::Measure;
            phaseStartNs_ = nowNs;
            return Event::MeasureStarted;
        }
        return Event::None;
    }
    if (phase_ != Phase::Measure) return Event::None;
    if (intervalMs >= 0.0f) {
        intervals_.add(intervalMs);
        cpu_.add(cpuMs);
    }
    if (elapsed >= measureSec_) {
        measuredSec_ = static_cast<float>(elapsed);
        phase_ = Phase::Done;
        return Event::Finished;
    }
    return Event::None;
}

void Bench::abort(const char* reason) {
    if (!active()) return;
    abortReason_ = reason;
    phase_ = Phase::Aborted;
    RF_WARN("bench: aborted (%s)", reason);
}

}  // namespace rf
