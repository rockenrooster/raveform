#pragma once
// Deterministic random numbers for scripted scenes (design/scenes.md: one generator seeded with 1).
// PCG32 (O'Neill), no allocation, identical on every platform.

#include "core/types.h"

namespace rf {

class Rng {
public:
    explicit Rng(u64 seed = 1) { reseed(seed); }
    void reseed(u64 seed) {
        state_ = 0;
        inc_ = (seed << 1u) | 1u;
        next();
        state_ += seed;
        next();
    }
    u32 next() {
        const u64 old = state_;
        state_ = old * 6364136223846793005ull + inc_;
        const u32 x = static_cast<u32>(((old >> 18u) ^ old) >> 27u);
        const u32 r = static_cast<u32>(old >> 59u);
        return (x >> r) | (x << ((32u - r) & 31u));
    }
    float uniform() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }  // [0, 1)
    float range(float lo, float hi) { return lo + (hi - lo) * uniform(); }
    u32 below(u32 n) { return n ? next() % n : 0u; }
    float sign() { return (next() & 1u) ? 1.0f : -1.0f; }

private:
    u64 state_ = 0;
    u64 inc_ = 1;
};

}  // namespace rf
