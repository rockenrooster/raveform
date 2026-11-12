#include "game/music_clock.h"

#include <cmath>

#include "audio/band.h"
#include "core/log.h"
#include "sim/sim_types.h"

namespace rf {

void MusicClock::reset(float bpm, double music) {
    bpm_ = bpm > 1.0f ? bpm : 120.0f;
    simTime_ = frameStart_ = 0.0;
    musicStart_ = musicEnd_ = heardNow_ = music;
    rate_ = ticksPerSecond();
    anchored_ = fromBand_ = false;
    latencyTicks_ = 0.0f;
}

double MusicClock::beginFrame(const BeatView* band, float realDt, bool fixedStep) {
    frameStart_ = simTime_;
    musicStart_ = musicEnd_;
    double dS = 0.0;
    const bool live = !fixedStep && band && band->running;
    if (!live) {
        // The simulated clock.
        anchored_ = fromBand_ = false;
        dS = fixedStep ? static_cast<double>(sim::kDt) : (realDt < 0.0f ? 0.0 : (realDt > 0.1f ? 0.1 : realDt));
        rate_ = ticksPerSecond();
        latencyTicks_ = 0.0f;
    } else {
        // Heard time drives the sim (at most 0.1 s a frame, never backwards).
        if (!anchored_) {
            anchored_ = true;
            dS = realDt < 0.0f ? 0.0 : (realDt > 0.1f ? 0.1 : realDt);
        } else {
            dS = band->songSeconds - lastSongS_;
            dS = dS < 0.0 ? 0.0 : (dS > 0.1 ? 0.1 : dS);
        }
        lastSongS_ = band->songSeconds;
        fromBand_ = true;
        bpm_ = static_cast<float>(band->bpm);
        const double nominal = ticksPerSecond();
        latencyTicks_ = static_cast<float>(band->latencyMs * 1e-3 * nominal);
        // The end of this frame's sim time is drawn one tick behind (interpolation): its music one tick
        // ahead of what is heard at the frame's display time.
        const double target = band->heardTick - offset_ + nominal * static_cast<double>(sim::kDt);
        const double err = target - (musicStart_ + nominal * dS);
        if (err > sim::kTicksPerBar) {
            // Far behind (the band's song started late, a resumed stream): jump ahead.
            if (snaps_ < 8) RF_LOG("play: musical clock jumps %.1f ticks ahead to the band's", err);
            ++snaps_;
            musicStart_ += err;
            rate_ = nominal;
        } else if (err < -sim::kTicksPerBar) {
            // Far ahead (the heard clock jumped back): hold until it catches up.
            if (snaps_ < 8) RF_LOG("play: musical clock holds (%.1f ticks ahead of the band)", -err);
            ++snaps_;
            rate_ = 0.0;
        } else if (dS > 1e-6) {
            double r = (target - musicStart_) / dS;
            r = r < 0.5 * nominal ? 0.5 * nominal : (r > 1.5 * nominal ? 1.5 * nominal : r);
            rate_ = r;
        } else {
            rate_ = nominal;
        }
    }
    musicEnd_ = musicStart_ + rate_ * dS;
    simTime_ += dS;
    heardNow_ = live ? band->heardTick - offset_ : musicEnd_;
    return dS;
}

void MusicClock::resume(double music, double offset) {
    offset_ = offset;
    musicStart_ = musicEnd_ = heardNow_ = music;
    anchored_ = false;
}

double MusicClock::musicAt(double t) const {
    double m = musicStart_ + rate_ * (t - frameStart_);
    if (m < musicStart_) m = musicStart_;
    if (m > musicEnd_) m = musicEnd_;
    return m;
}

}  // namespace rf
