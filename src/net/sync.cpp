#include "net/sync.h"

#include <cmath>

namespace rf::net {

namespace {
constexpr double kTicksPerBar = 384.0;
constexpr double kTicksPerBeat = 96.0;
}  // namespace

// ---------------------------------------------------------------------------------------------- ClockSync

void ClockSync::reset() {
    count_ = head_ = 0;
    best_ = Sample{};
    average_ = 0.0;
    window_ = 0.0;
    estimates_ = 0;
    steered_ = 0.0;
    steeredValid_ = false;
    lastLocal_ = 0;
    dilation_ = 0.0f;
}

void ClockSync::addSample(i64 t0, i64 t1, i64 t2, i64 t3) {
    Sample s;
    s.local = t3;
    s.rtt = (t3 - t0) - (t2 - t1);
    if (s.rtt < 0) s.rtt = 0;
    if (s.rtt > kMaxRttUs) return;  // (a round trip of seconds, a device that stopped: it says nothing of the clocks)
    s.offset = ((t1 - t0) + (t2 - t3)) / 2;
    ring_[head_] = s;
    head_ = (head_ + 1) % kSamples;
    if (count_ < kSamples) ++count_;
    choose();
    ++estimates_;
    const double a = 1.0 / static_cast<double>(estimates_) > 1.0 / kAverageSamples ? 1.0 / static_cast<double>(estimates_) : 1.0 / kAverageSamples;
    average_ += (window_ - average_) * a;
    if (lastLocal_ == 0) lastLocal_ = t3;
    if (!steeredValid_) snap();
}

void ClockSync::choose() {
    // TECH_SPEC 8.2: the minimum-RTT sample of the last 16 leads. A sample's offset is wrong by at most half its
    // round trip, so the window's estimate weighs every sample by 1 / RTT^2 against it: the minimum-RTT one
    // dominates, those nearly as quick steady it.
    u32 best = 0;
    for (u32 k = 1; k < count_; ++k)
        if (ring_[k].rtt < ring_[best].rtt) best = k;
    best_ = ring_[best];
    double sw = 0.0, so = 0.0;
    for (u32 k = 0; k < count_; ++k) {
        const double r = static_cast<double>(ring_[k].rtt) * 1e-3 + 1.0;  // ms, +1 so a zero RTT stays finite
        const double w = 1.0 / (r * r);
        sw += w;
        so += w * static_cast<double>(ring_[k].offset);
    }
    window_ = sw > 0.0 ? so / sw : static_cast<double>(best_.offset);
}

void ClockSync::snap() {
    steered_ = average_;
    steeredValid_ = valid();
}

void ClockSync::forgetSamples() {
    count_ = head_ = 0;
    best_ = Sample{};
    estimates_ = 0;  // (the next sample sets the average outright: addSample's weight is 1 / estimates)
}

i64 ClockSync::hostNowUs(i64 localUs) {
    if (!steeredValid_) {
        lastLocal_ = localUs;
        return localUs + static_cast<i64>(average_);
    }
    const i64 dt = localUs - lastLocal_;
    lastLocal_ = localUs;
    if (dt > 0) {
        // Toward the average, by at most 1% of the time that passed (host time never jumps).
        const double err = average_ - steered_;
        const double maxStep = kMaxDilation * static_cast<double>(dt);
        const double step = err > maxStep ? maxStep : (err < -maxStep ? -maxStep : err);
        steered_ += step;
        dilation_ = static_cast<float>(step / static_cast<double>(dt));
    }
    return localUs + static_cast<i64>(steered_);
}

// ---------------------------------------------------------------------------------------------- SongModel

void SongModel::set(const SongAnchor& a) {
    // Anchors arrive in order on a sequenced channel; an older one never replaces a newer one.
    if (valid_ && a.hostUs < a_.hostUs) return;
    a_ = a;
    valid_ = true;
}

double SongModel::songAt(i64 hostUs) const {
    if (!valid_) return 0.0;
    const double s = static_cast<double>(hostUs - a_.hostUs) * 1e-6;
    const double reach = a_.song + s * a_.tps;
    if (s <= 0.0 || reach <= a_.nextBar || a_.tps <= 0.0) return reach;
    // Past the next bar line: the tempo after it.
    const double toBar = (a_.nextBar - a_.song) / a_.tps;
    return a_.nextBar + (s - toBar) * a_.tpsNext;
}

double SongModel::tpsAt(i64 hostUs) const {
    if (!valid_) return 192.0;
    return songAt(hostUs) < a_.nextBar ? a_.tps : a_.tpsNext;
}

// ---------------------------------------------------------------------------------------------- SongClock

void SongClock::reset(i64 hostUs, double song, float bpm) {
    hostUs_ = hostUs;
    song_ = song;
    bpm_ = bpm > 1.0f ? bpm : 120.0f;
}

void SongClock::advanceTo(i64 hostUs, TempoFn fn, void* user) {
    if (hostUs <= hostUs_) return;
    double left = static_cast<double>(hostUs - hostUs_) * 1e-6;
    hostUs_ = hostUs;
    for (u32 guard = 0; guard < 64 && left > 0.0; ++guard) {
        const double tps = static_cast<double>(bpm_) * kTicksPerBeat / 60.0;
        const double bar = (std::floor(song_ / kTicksPerBar + 1e-9) + 1.0) * kTicksPerBar;
        const double toBar = (bar - song_) / tps;
        if (toBar > left) {
            song_ += left * tps;
            return;
        }
        // A bar line: the tempo of the bar it begins.
        song_ = bar;
        left -= toBar;
        if (fn) {
            const float b = fn(user, bar);
            if (b > 1.0f) bpm_ = b;
        }
    }
}

SongAnchor SongClock::anchor(TempoFn fn, void* user) const {
    SongAnchor a;
    a.hostUs = hostUs_;
    a.song = song_;
    a.tps = static_cast<double>(bpm_) * kTicksPerBeat / 60.0;
    a.nextBar = (std::floor(song_ / kTicksPerBar + 1e-9) + 1.0) * kTicksPerBar;
    const float next = fn ? fn(user, a.nextBar) : bpm_;
    a.tpsNext = static_cast<double>(next > 1.0f ? next : bpm_) * kTicksPerBeat / 60.0;
    return a;
}

}  // namespace rf::net
