#include "audio/tempo_map.h"

#include <cmath>

namespace rf::audio {

void TempoMap::reset(double bpm, double sampleRate, double frame, double tick) {
    head_ = 0;
    count_ = 1;
    seg_[0] = {frame, tick, ticksPerFrameFor(bpm, sampleRate)};
}

void TempoMap::changeAt(double tick, double ticksPerFrame) {
    const TempoSegment& l = last();
    TempoSegment s;
    s.tick = tick;
    s.frame = l.frame + (tick - l.tick) / l.ticksPerFrame;
    s.ticksPerFrame = ticksPerFrame;
    appendSegment(s);
}

void TempoMap::appendSegment(const TempoSegment& s) {
    if (count_ && s.tick <= last().tick) {
        seg_[(head_ + count_ - 1) % kCapacity] = s;  // the same start: the newer tempo wins
        return;
    }
    if (count_ == kCapacity) {
        head_ = (head_ + 1) % kCapacity;
        --count_;
    }
    seg_[(head_ + count_) % kCapacity] = s;
    ++count_;
}

const TempoSegment& TempoMap::forFrame(double frame) const {
    // Newest first: lookups are almost always near the present.
    for (u32 i = count_; i-- > 1;)
        if (at(i).frame <= frame) return at(i);
    return at(0);
}

const TempoSegment& TempoMap::forTick(double tick) const {
    for (u32 i = count_; i-- > 1;)
        if (at(i).tick <= tick) return at(i);
    return at(0);
}

double TempoMap::tickAt(double frame) const {
    const TempoSegment& s = forFrame(frame);
    return s.tick + (frame - s.frame) * s.ticksPerFrame;
}

double TempoMap::frameAt(double tick) const {
    const TempoSegment& s = forTick(tick);
    return s.frame + (tick - s.tick) / s.ticksPerFrame;
}

i64 TempoMap::gridFrame(double tick) const { return static_cast<i64>(std::llround(frameAt(tick))); }

double TempoMap::ticksPerFrameAt(double frame) const { return forFrame(frame).ticksPerFrame; }

}  // namespace rf::audio
