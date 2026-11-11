#include "tools/frame_probe.h"

#include <cstdio>
#include <ctime>

#include "core/json.h"
#include "core/log.h"
#include "gfx/frame_pacer.h"

#if defined(__linux__)
#include <fcntl.h>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>
#ifndef RUSAGE_THREAD
#define RUSAGE_THREAD 1
#endif
#endif

namespace rf {

namespace {

constexpr const char* kPhaseNames[FrameProbe::kPhases] = {"post",    "events", "audio",  "begin",  "fence",
                                                          "acquire", "scene",  "record", "submit", "present"};

bool isWait(u32 phase) {
    return phase == static_cast<u32>(Phase::Fence) || phase == static_cast<u32>(Phase::Acquire) ||
           phase == static_cast<u32>(Phase::Present);
}

#if defined(__linux__)
u64 clockNs(clockid_t id) {
    timespec ts{};
    clock_gettime(id, &ts);
    return static_cast<u64>(ts.tv_sec) * 1000000000ull + static_cast<u64>(ts.tv_nsec);
}

// /proc/<pid>/task/<tid>/schedstat: "<ns on cpu> <ns waiting on a run queue> <timeslices>".
bool parseRunWait(const char* s, u64& out) {
    while (*s >= '0' && *s <= '9') ++s;  // first field
    while (*s == ' ') ++s;
    if (*s < '0' || *s > '9') return false;
    u64 v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + static_cast<u64>(*s++ - '0');
    out = v;
    return true;
}
#endif

}  // namespace

const char* FrameProbe::phaseName(u32 i) { return i < kPhases ? kPhaseNames[i] : "?"; }

int FrameProbe::currentCpu() {
#if defined(__linux__)
    return sched_getcpu();
#else
    return -1;
#endif
}

bool FrameProbe::init(u32 capacity) {
    if (!work_.init(capacity) || !display_.init(capacity)) return false;
#if defined(__linux__)
    char path[64];
    std::snprintf(path, sizeof path, "/proc/self/task/%d/schedstat", static_cast<int>(gettid()));
    schedFd_ = open(path, O_RDONLY | O_CLOEXEC);
    if (schedFd_ < 0) RF_WARN("probe: %s is not readable; run-queue wait unavailable", path);
#endif
    return true;
}

void FrameProbe::begin(u64 nowNs, bool displayIds) {
    windowStartNs_ = nowNs;
    displayIds_ = displayIds;
#if defined(__linux__)
    bootStartSec_ = static_cast<double>(clockNs(CLOCK_BOOTTIME)) * 1e-9;
    processCpuStartNs_ = clockNs(CLOCK_PROCESS_CPUTIME_ID);
#endif
}

bool FrameProbe::sampleThread(ThreadSample& s) const {
#if defined(__linux__)
    s.cpuNs = clockNs(CLOCK_THREAD_CPUTIME_ID);
    rusage ru{};
    if (getrusage(RUSAGE_THREAD, &ru) == 0) {
        s.nvcsw = ru.ru_nvcsw;
        s.nivcsw = ru.ru_nivcsw;
        s.minflt = ru.ru_minflt;
        s.majflt = ru.ru_majflt;
    }
    if (schedFd_ >= 0) {
        char buf[96];
        const ssize_t n = pread(schedFd_, buf, sizeof buf - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            s.runWaitOk = parseRunWait(buf, s.runWaitNs);
        }
    }
    s.ok = true;
    return true;
#else
    (void)s;
    return false;
#endif
}

void FrameProbe::onFrame(u64 presentNs, float intervalMs, const u64 phaseNs[kPhases], int cpuStart, float targetMs,
                         u32 presentId, bool measuring) {
    ThreadSample now;
    sampleThread(now);
    pendingTargetMs_ = targetMs;
    if (!measuring) {
        prev_ = now;
        return;
    }

    Record r;
    r.used = true;
    r.frame = frames_++;
    r.presentId = displayIds_ ? presentId : 0;
    if (r.presentId && firstPresentId_ == 0) firstPresentId_ = r.presentId;
    r.t = static_cast<float>(static_cast<double>(presentNs - windowStartNs_) * 1e-9);
    r.intervalMs = intervalMs;
    float workMs = 0.0f;
    for (u32 i = 0; i < kPhases; ++i) {
        r.phaseMs[i] = static_cast<float>(static_cast<double>(phaseNs[i]) * 1e-6);
        phaseSumMs_[i] += r.phaseMs[i];
        if (r.phaseMs[i] > phaseMaxMs_[i]) phaseMaxMs_[i] = r.phaseMs[i];
        if (!isWait(i)) workMs += r.phaseMs[i];
    }
    work_.add(workMs);
    if (now.ok && prev_.ok) {
        r.threadCpuMs = static_cast<float>(static_cast<double>(now.cpuNs - prev_.cpuNs) * 1e-6);
        if (now.runWaitOk && prev_.runWaitOk) {
            r.runWaitMs = static_cast<float>(static_cast<double>(now.runWaitNs - prev_.runWaitNs) * 1e-6);
        }
        r.nvcsw = static_cast<i32>(now.nvcsw - prev_.nvcsw);
        r.nivcsw = static_cast<i32>(now.nivcsw - prev_.nivcsw);
        r.minflt = static_cast<i32>(now.minflt - prev_.minflt);
        r.majflt = static_cast<i32>(now.majflt - prev_.majflt);
        threadCpuMs_ += r.threadCpuMs;
        nvcsw_ += r.nvcsw;
        nivcsw_ += r.nivcsw;
        minflt_ += r.minflt;
        majflt_ += r.majflt;
        if (r.runWaitMs >= 0.0f) {
            runWaitMs_ += r.runWaitMs;
            if (r.runWaitMs > runWaitMaxMs_) runWaitMaxMs_ = r.runWaitMs;
            runWaitOver1_ += r.runWaitMs > 1.0f ? 1u : 0u;
            runWaitOver4_ += r.runWaitMs > 4.0f ? 1u : 0u;
        }
    }
    prev_ = now;
    r.cpuStart = cpuStart;
    r.cpuEnd = currentCpu();
    if (r.cpuEnd >= 0 && r.cpuEnd < static_cast<i32>(kCpus)) ++cpuHist_[r.cpuEnd];
    if (r.cpuStart >= 0 && r.cpuEnd >= 0 && r.cpuStart != r.cpuEnd) ++migrations_;

    const u32 sec = static_cast<u32>(r.t);
    if (sec < kSeconds) {
        Second& s = seconds_[sec];
        ++s.frames;
        s.missed += r.intervalMs > 1.5f * targetMs ? 1 : 0;
        if (r.intervalMs > s.maxMs) s.maxMs = r.intervalMs;
        if (r.runWaitMs > 0.0f) s.runWaitMs += r.runWaitMs;
        if (r.threadCpuMs > 0.0f) s.threadCpuMs += r.threadCpuMs;
        if (r.nivcsw > 0) s.nivcsw = static_cast<u16>(s.nivcsw + r.nivcsw);
    }

    if (r.presentId == 0) {
        finalize(r, targetMs, 0.0f);
        return;
    }
    // Wait for the panel's timestamp (reported about five frames later); give up after 24 frames.
    Record* slot = nullptr;
    for (Record& p : pending_) {
        if (!p.used) {
            if (!slot) slot = &p;
            continue;
        }
        if (++p.age >= 24) {
            finalize(p, targetMs, 0.0f);
            p.used = false;
            if (!slot) slot = &p;
        }
    }
    if (!slot) {  // full: finalize the oldest without display data
        slot = &pending_[0];
        for (Record& p : pending_)
            if (p.frame < slot->frame) slot = &p;
        finalize(*slot, targetMs, 0.0f);
    }
    *slot = r;
}

void FrameProbe::onDisplayTiming(const DisplayTiming& t, float refreshMs) {
    float displayMs = -1.0f;
    if (t.actualNs != 0 && lastDisplayNs_ != 0 && t.presentId == lastDisplayId_ + 1 && t.actualNs > lastDisplayNs_) {
        displayMs = static_cast<float>(static_cast<double>(t.actualNs - lastDisplayNs_) * 1e-6);
    }
    lastDisplayId_ = t.presentId;
    lastDisplayNs_ = t.actualNs;
    if (!displayIds_ || firstPresentId_ == 0 || t.presentId < firstPresentId_) return;  // outside the window

    if (t.actualNs == 0) ++displayDropped_;
    if (displayMs > 0.0f) {
        display_.add(displayMs);
        if (refreshMs > 0.0f) {
            const float periods = displayMs / refreshMs;
            const u32 b = periods < 0.5f ? 0u : (periods < 1.5f ? 1u : (periods < 2.5f ? 2u : (periods < 3.5f ? 3u : 4u)));
            ++displayHist_[b];
        }
    }
    for (Record& p : pending_) {
        if (!p.used || p.presentId != t.presentId) continue;
        p.displayMs = displayMs;
        p.marginMs = static_cast<float>(static_cast<double>(t.marginNs) * 1e-6);
        finalize(p, pendingTargetMs_, refreshMs);
        p.used = false;
        break;
    }
}

void FrameProbe::finalize(const Record& r, float targetMs, float refreshMs) {
    const bool cpuMissed = targetMs > 0.0f && r.intervalMs > 1.5f * targetMs;
    const bool displayMissed = refreshMs > 0.0f && r.displayMs > 1.5f * refreshMs;
    if (cpuMissed) {
        ++missed_;
        float workMs = 0.0f;
        u32 topWork = 0;
        u32 topWait = static_cast<u32>(Phase::Fence);
        for (u32 i = 0; i < kPhases; ++i) {
            if (isWait(i)) {
                if (r.phaseMs[i] > r.phaseMs[topWait]) topWait = i;
            } else {
                workMs += r.phaseMs[i];
                if (r.phaseMs[i] > r.phaseMs[topWork]) topWork = i;
            }
        }
        if (workMs >= 0.5f * targetMs) {
            ++missedWork_;
            ++dominant_[topWork];
        } else {
            ++missedWaits_;
            ++dominant_[topWait];
        }
        if (r.runWaitMs >= 2.0f) ++missedRunWait_;
        if (r.majflt > 0) ++missedFaults_;
    }
    if (displayMissed) {
        ++displayMissed_;
        if (!cpuMissed) ++displayMissedCpuOk_;
        const u32 sec = static_cast<u32>(r.t);
        if (sec < kSeconds) ++seconds_[sec].displayMissed;
    }
    // Keep the worst frames, ranked by the later of the CPU-side and the panel interval.
    const float key = r.displayMs > r.intervalMs ? r.displayMs : r.intervalMs;
    Record* slot = nullptr;
    float slotKey = 0.0f;
    for (Record& w : worst_) {
        if (!w.used) {
            slot = &w;
            break;
        }
        const float k = w.displayMs > w.intervalMs ? w.displayMs : w.intervalMs;
        if (!slot || k < slotKey) {
            slot = &w;
            slotKey = k;
        }
    }
    if (slot && (!slot->used || key > slotKey)) {
        *slot = r;
        slot->used = true;
        slot->age = 0;
    }
}

void FrameProbe::finish(float targetMs, float refreshMs) {
    pendingTargetMs_ = targetMs;
    for (Record& p : pending_) {
        if (!p.used) continue;
        finalize(p, targetMs, refreshMs);
        p.used = false;
    }
#if defined(__linux__)
    processCpuNs_ = clockNs(CLOCK_PROCESS_CPUTIME_ID) - processCpuStartNs_;
#endif
    workSummary_ = work_.finish();
    displaySummary_ = display_.finish();
}

void FrameProbe::write(JsonWriter& j, bool full, float targetMs, float refreshMs) {
    (void)targetMs;
    j.num("boottime_start_s", bootStartSec_, 3);
    j.num("process_cpu_ms", static_cast<double>(processCpuNs_) * 1e-6, 1);

    j.beginObject("phases_ms");  // [mean, max] per phase; the phases add up to each interval
    for (u32 i = 0; i < kPhases; ++i) {
        j.beginArray(kPhaseNames[i]);
        j.num(nullptr, frames_ ? phaseSumMs_[i] / frames_ : 0.0, 3);
        j.num(nullptr, phaseMaxMs_[i], 3);
        j.endArray();
    }
    j.endObject();
    j.beginObject("work_ms");  // the interval minus the fence, acquire and present waits
    j.num("p50", workSummary_.p50);
    j.num("p99", workSummary_.p99);
    j.num("max", workSummary_.max);
    j.endObject();

    j.beginObject("sched");
    j.boolean("available", prev_.ok);
    j.num("thread_cpu_ms", threadCpuMs_, 1);
    j.num("run_wait_ms", runWaitMs_, 1);
    j.num("run_wait_max_ms", runWaitMaxMs_);
    j.integer("frames_run_wait_over_1ms", runWaitOver1_);
    j.integer("frames_run_wait_over_4ms", runWaitOver4_);
    j.integer("nvcsw", nvcsw_);
    j.integer("nivcsw", nivcsw_);
    j.integer("minflt", minflt_);
    j.integer("majflt", majflt_);
    j.integer("migrations", migrations_);
    u32 lastCpu = 0;
    for (u32 c = 0; c < kCpus; ++c)
        if (cpuHist_[c]) lastCpu = c + 1;
    j.beginArray("frames_by_cpu");  // CPU at the end of each frame, index = CPU number
    for (u32 c = 0; c < lastCpu; ++c) j.integer(nullptr, cpuHist_[c]);
    j.endArray();
    j.endObject();

    j.beginObject("late");
    j.integer("missed", missed_);
    j.integer("own_work", missedWork_);
    j.integer("waits", missedWaits_);
    j.integer("run_wait_2ms", missedRunWait_);
    j.integer("major_faults", missedFaults_);
    j.beginObject("dominant_phase");
    for (u32 i = 0; i < kPhases; ++i)
        if (dominant_[i]) j.integer(kPhaseNames[i], dominant_[i]);
    j.endObject();
    j.integer("display_missed", displayMissed_);
    j.integer("display_missed_cpu_ok", displayMissedCpuOk_);
    j.endObject();

    if (displayIds_) {
        j.beginObject("display");
        j.str("source", "display_timing");
        j.integer("frames", display_.count());
        j.integer("window_frames", frames_);
        j.integer("dropped", displayDropped_);
        j.num("p50", displaySummary_.p50);
        j.num("p95", displaySummary_.p95);
        j.num("p99", displaySummary_.p99);
        j.num("p999", displaySummary_.p999);
        j.num("max", displaySummary_.max);
        j.integer("missed", displayMissed_);
        j.num("missed_pct", display_.count() ? 100.0 * displayMissed_ / display_.count() : 0.0, 2);
        j.beginArray("refresh_periods");  // intervals of <0.5, 1, 2, 3, 4+ refresh periods
        for (u32 b = 0; b < 5; ++b) j.integer(nullptr, displayHist_[b]);
        j.endArray();
        j.num("refresh_ms", refreshMs);
        j.endObject();
    }

    if (!full) return;
    j.beginArray("phase_order");
    for (u32 i = 0; i < kPhases; ++i) j.str(nullptr, kPhaseNames[i]);
    j.endArray();
    // Worst frames, slowest first.
    const Record* order[kWorst];
    u32 n = 0;
    for (const Record& w : worst_)
        if (w.used) order[n++] = &w;
    for (u32 a = 1; a < n; ++a) {
        const Record* x = order[a];
        const float kx = x->displayMs > x->intervalMs ? x->displayMs : x->intervalMs;
        u32 b = a;
        for (; b > 0; --b) {
            const Record* y = order[b - 1];
            const float ky = y->displayMs > y->intervalMs ? y->displayMs : y->intervalMs;
            if (ky >= kx) break;
            order[b] = y;
        }
        order[b] = x;
    }
    j.beginArray("worst");
    for (u32 i = 0; i < n; ++i) {
        const Record& r = *order[i];
        j.beginObject();
        j.integer("frame", r.frame);
        j.num("t", r.t, 3);
        j.num("iv", r.intervalMs);
        j.num("disp", r.displayMs);
        j.num("margin", r.marginMs);
        j.beginArray("ph");
        for (u32 p = 0; p < kPhases; ++p) j.num(nullptr, r.phaseMs[p]);
        j.endArray();
        j.num("tcpu", r.threadCpuMs);
        j.num("rq", r.runWaitMs);
        j.integer("nv", r.nvcsw);
        j.integer("niv", r.nivcsw);
        j.integer("minflt", r.minflt);
        j.integer("majflt", r.majflt);
        j.pair("cpu", r.cpuStart, r.cpuEnd);
        j.endObject();
    }
    j.endArray();
    // [second, frames, missed, missed on the panel, max interval ms, run-queue wait ms, thread CPU ms,
    //  involuntary context switches]
    j.beginArray("timeline");
    for (u32 s = 0; s < kSeconds; ++s) {
        const Second& x = seconds_[s];
        if (!x.frames) continue;
        j.beginArray();
        j.integer(nullptr, s);
        j.integer(nullptr, x.frames);
        j.integer(nullptr, x.missed);
        j.integer(nullptr, x.displayMissed);
        j.num(nullptr, x.maxMs, 2);
        j.num(nullptr, x.runWaitMs, 2);
        j.num(nullptr, x.threadCpuMs, 1);
        j.integer(nullptr, x.nivcsw);
        j.endArray();
    }
    j.endArray();
}

}  // namespace rf
