#pragma once
// The game's musical clock (TECH_SPEC 4): turns heard time into sim time and into the musical position of
// every sim tick. While the band plays, the sim advances by the progress of the band's heard clock (so
// the 60 Hz tick runs on heard time, and a beat crossed by a tick is a beat the player hears), and each
// tick's musical position follows the band's heard tick, one tick ahead because the picture is drawn
// interpolated one tick behind the newest state. Without the band (pictures, --band off, no device) it is
// the simulated clock: real time, or exactly 1/60 s per frame with --fixed-step, at the Track's tempo.
// The musical position never runs backwards.

#include "core/types.h"

namespace rf {

struct BeatView;

class MusicClock {
public:
    void reset(float bpm, double music = 0.0);
    // Once per frame. Returns the sim time to add (seconds).
    double beginFrame(const BeatView* band, float realDt, bool fixedStep);
    // The musical position at sim time `t` (inside this frame's span).
    double musicAt(double t) const;
    double simTime() const { return simTime_; }
    double frameStart() const { return frameStart_; }
    float bpm() const { return bpm_; }
    float ticksPerSecond() const { return bpm_ * 96.0f / 60.0f; }
    bool fromBand() const { return fromBand_; }
    // What the player hears now (the band's heard tick, or the simulated clock's position).
    double heardNow() const { return heardNow_; }
    float latencyTicks() const { return latencyTicks_; }
    u32 snaps() const { return snaps_; }
    // M3 stage B: the run's music is the band's heard tick minus offset (a whole number of bars, so the
    // run's bar lines stay on the band's), set when a paused run resumes. resume() also restarts the clock
    // at music (the position the run paused at).
    void resume(double music, double offset);
    double offset() const { return offset_; }
    // The simulated clock's tempo (the director's: the Build's last bar, the Track change, the loop).
    void setBpm(float bpm) { if (!fromBand_ && bpm > 1.0f) bpm_ = bpm; }

private:
    double simTime_ = 0.0, frameStart_ = 0.0;
    double musicStart_ = 0.0, musicEnd_ = 0.0, rate_ = 192.0;
    double heardNow_ = 0.0;
    double lastSongS_ = 0.0;
    double offset_ = 0.0;
    float bpm_ = 120.0f;
    float latencyTicks_ = 0.0f;
    bool anchored_ = false, fromBand_ = false;
    u32 snaps_ = 0;
};

}  // namespace rf
