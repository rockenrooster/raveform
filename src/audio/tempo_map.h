#pragma once
// Tempo map (TECH_SPEC 4): segments of {startFrame, startTick, ticksPerFrame} that turn audio frames
// into musical ticks (96 per quarter) and back. Written by the audio thread (which changes tempo only on
// grid lines) and mirrored on the main thread from the Tempo events. A segment starts on an exact tick;
// its start frame is fractional, so grid positions stay exact across tempo changes. Fixed capacity:
// the oldest segments are forgotten once newer ones cover the present.

#include "core/types.h"

namespace rf::audio {

constexpr i64 kPpq = 96;                 // ticks per quarter note
constexpr i64 kTicksPerBar = 4 * kPpq;   // 4/4
constexpr i64 kTicksPerStep = kPpq / 4;  // a sixteenth

struct TempoSegment {
    double frame = 0.0;  // exact (fractional) frame where the segment starts
    double tick = 0.0;
    double ticksPerFrame = 0.0;
};

inline double ticksPerFrameFor(double bpm, double sampleRate) { return bpm * static_cast<double>(kPpq) / (60.0 * sampleRate); }

class TempoMap {
public:
    static constexpr u32 kCapacity = 64;

    void reset(double bpm, double sampleRate, double frame = 0.0, double tick = 0.0);
    // A new tempo from `tick` on (not before the newest segment's start).
    void changeAt(double tick, double ticksPerFrame);
    // Mirrors: the segment as published. A segment on the last one's tick but at a later frame follows it (M4: the
    // song held on that tick in between, a tempo of 0); otherwise one on or before the last's tick replaces it.
    void appendSegment(const TempoSegment& s);

    double tickAt(double frame) const;
    double frameAt(double tick) const;  // exact (fractional) frame of a tick
    i64 gridFrame(double tick) const;   // the frame a note on `tick` starts: round(frameAt(tick))
    double ticksPerFrameAt(double frame) const;
    double bpmAt(double frame, double sampleRate) const {
        return ticksPerFrameAt(frame) * 60.0 * sampleRate / static_cast<double>(kPpq);
    }
    const TempoSegment& last() const { return at(count_ - 1); }
    u32 count() const { return count_; }
    const TempoSegment& at(u32 i) const { return seg_[(head_ + i) % kCapacity]; }

private:
    const TempoSegment& forFrame(double frame) const;
    const TempoSegment& forTick(double tick) const;
    TempoSegment seg_[kCapacity];
    u32 head_ = 0;
    u32 count_ = 0;
};

}  // namespace rf::audio
