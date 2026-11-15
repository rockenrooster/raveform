#pragma once
// Time and song sync (M4; TECH_SPEC 8.2).
//
// ClockSync: an NTP-style exchange (the client's send time t0, the host's receive and send times t1, t2, the
// client's receive time t3) at 4 Hz; the offset of the sample with the smallest round trip among the last
// 16 leads the estimate of host time - local time (the window's samples weighed by 1 / RTT^2, so the quickest
// dominates). Jitter makes any one estimate wrong by up to half its round trip, so the estimates are averaged (a running mean of the first ones, then over about 20 s; two clocks
// drift apart by tens of microseconds a second, so the average lags by about a millisecond). The clock the
// client runs on follows the average by dilating its own time by at most 1% (never a jump once a run is
// going; snap() before one starts).
//
// SongModel: the host publishes its song (the band's heard musical position) anchored to host time: "song
// tick T is heard at host time H", the tempo now and the tempo after the next bar line (tempo changes only on
// bar lines). Every device computes the song tick for any host time from the newest anchor.
//
// SongClock: the host's song when no band plays it (headless peers, bots): ticks advance with time at the
// tempo the director gives each bar.

#include "core/types.h"

namespace rf::net {

class ClockSync {
public:
    static constexpr u32 kSamples = 16;
    static constexpr double kMaxDilation = 0.01;  // TECH_SPEC 8.2: +-1%
    static constexpr double kAverageSamples = 80.0;  // the average's reach, in samples (20 s at 4 Hz)
    static constexpr i64 kMaxRttUs = 1000000;        // a sample whose round trip took longer is dropped

    void reset();
    void addSample(i64 t0, i64 t1, i64 t2, i64 t3);
    bool valid() const { return count_ > 0; }
    u32 samples() const { return count_; }
    i64 estimateUs() const { return static_cast<i64>(average_); }  // host - local: the averaged minimum-RTT offsets
    i64 bestUs() const { return best_.offset; }                     // the minimum-RTT sample's own
    i64 rttUs() const { return best_.rtt; }
    // Host time now, by the steered offset (advance once per frame with the local time).
    i64 hostNowUs(i64 localUs);
    i64 hostAt(i64 localUs) const { return localUs + static_cast<i64>(steered_); }  // without advancing
    void snap();  // the steered offset jumps to the estimate (before a run starts)
    // Pause-on-leave: the samples from before this device was away are forgotten (the steered clock runs on); the estimate
    // starts again from the next ones, and snap() may take it once they have settled (the run is paused for everyone then).
    void forgetSamples();
    double steeredUs() const { return steered_; }
    float dilation() const { return dilation_; }  // the last frame's (rate - 1)

private:
    struct Sample {
        i64 offset = 0, rtt = 0, local = 0;  // local: when its answer arrived
    };
    void choose();
    Sample ring_[kSamples];
    u32 count_ = 0, head_ = 0;
    Sample best_;
    double average_ = 0.0, window_ = 0.0;
    u32 estimates_ = 0;
    double steered_ = 0.0;
    bool steeredValid_ = false;
    i64 lastLocal_ = 0;
    float dilation_ = 0.0f;
};

struct SongAnchor {
    i64 hostUs = 0;
    double song = 0.0;     // song ticks at hostUs
    double tps = 192.0;    // ticks per second until nextBar
    double nextBar = 0.0;  // the next bar line (song ticks)
    double tpsNext = 192.0;
};

class SongModel {
public:
    void reset() { valid_ = false; }
    void set(const SongAnchor& a);
    bool valid() const { return valid_; }
    const SongAnchor& anchor() const { return a_; }
    double songAt(i64 hostUs) const;
    double tpsAt(i64 hostUs) const;

private:
    SongAnchor a_;
    bool valid_ = false;
};

class SongClock {
public:
    // Tempo (BPM) of the bar that begins at song tick `barTick`.
    using TempoFn = float (*)(void* user, double barTick);
    void reset(i64 hostUs, double song, float bpm);
    // Moves to host time `hostUs` (not backwards), changing tempo on each bar line it passes.
    void advanceTo(i64 hostUs, TempoFn fn, void* user);
    double song() const { return song_; }
    i64 hostUs() const { return hostUs_; }
    float bpm() const { return bpm_; }
    SongAnchor anchor(TempoFn fn, void* user) const;  // as published (the next bar's tempo asked now)

private:
    i64 hostUs_ = 0;
    double song_ = 0.0;
    float bpm_ = 120.0f;
};

}  // namespace rf::net
