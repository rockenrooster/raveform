#pragma once
// The simulation (TECH_SPEC 7): shared constants and small helpers. The simulation never reads the wall
// clock, iterates in a stable order, draws random numbers from its own seeded streams and allocates
// nothing after init, so the same binary, seed and inputs give the same state (TECH_SPEC 7.3).

#include <cmath>

#include "core/math.h"
#include "core/types.h"

namespace rf::sim {

// ---- Time (TECH_SPEC 4): a fixed 60 Hz tick; musical time in PPQ ticks (96 per quarter note).
constexpr float kDt = 1.0f / 60.0f;
constexpr double kTicksPerBeat = 96.0;
constexpr double kTicksPerStep = 24.0;  // a sixteenth
constexpr double kTicksPerBar = 384.0;  // 4/4

// ---- Arena (GAME_DESIGN 9): 9,600 x 5,400 wu bounded by walls, centred on the origin, y down.
constexpr float kArenaHalfW = 4800.0f;
constexpr float kArenaHalfH = 2700.0f;

// ---- Pools (TECH_SPEC 7.1): fixed capacities, gameplay rules identical on every device.
constexpr u32 kMaxPlayers = 4;
constexpr u32 kMaxEnemies = 3072;
constexpr u32 kMaxEnemyBullets = 2048;
constexpr u32 kMaxShots = 1024;  // per player
constexpr u32 kMaxShards = 2048;

// ---- Spatial grid (TECH_SPEC 7.2): uniform 64 wu cells over the arena (150 x 85).
constexpr float kCellWu = 64.0f;
constexpr u32 kGridCols = 150;
constexpr u32 kGridRows = 85;

// ---- The player (GAME_DESIGN 5).
constexpr float kPlayerSpeed = 520.0f;
constexpr float kPlayerVisualRadius = 14.0f;
constexpr float kPlayerHitRadius = 5.0f;
constexpr float kShotSpeed = 1400.0f;
constexpr u8 kStartCells = 3, kMaxCells = 6;
constexpr u8 kStartNovas = 2, kNovaCapacity = 3;
constexpr float kInvulnerableS = 1.5f;
constexpr float kPanicRadiusWu = 220.0f;
constexpr float kDashWu = 220.0f, kDashS = 0.12f;
constexpr double kDashRechargeBeats = 4.0;
constexpr float kPerfectDashS = 0.060f;  // R7: within +-60 ms of a beat
constexpr float kFireContinueS = 0.400f;  // GAME_DESIGN 2: the right thumb leaves the stick for a button
constexpr float kShardMagnetWu = 90.0f;
constexpr double kShardLifeBars = 4.0;
constexpr double kWarpBeats = 2.0;        // R4: every spawn warps in for exactly two beats

inline Vec2 dirOf(float angle) { return {std::cos(angle), std::sin(angle)}; }
inline Vec2 perpOf(Vec2 v) { return {-v.y, v.x}; }
inline float angleOf(Vec2 v) { return std::atan2(v.y, v.x); }
inline float lengthSq(Vec2 v) { return v.x * v.x + v.y * v.y; }
inline Vec2 normalizeOr(Vec2 v, Vec2 fallback) {
    const float l = length(v);
    return l > 1e-6f ? v * (1.0f / l) : fallback;
}
// Wraps an angle difference into [-pi, pi].
inline float wrapAngle(float a) {
    while (a > kPi) a -= kTau;
    while (a < -kPi) a += kTau;
    return a;
}
// Turns `from` toward `to` by at most `maxStep` radians.
inline float turnToward(float from, float to, float maxStep) {
    const float d = wrapAngle(to - from);
    return from + (d > maxStep ? maxStep : (d < -maxStep ? -maxStep : d));
}

// The first grid crossing of period `period` (ticks) in (t0, t1]: true and its tick when there is one.
// Crossings in a tick are visited with forCrossings().
inline bool crossed(double t0, double t1, double period) {
    return std::floor(t1 / period) > std::floor(t0 / period);
}
template <typename F>
inline void forCrossings(double t0, double t1, double period, F&& f) {
    const double k0 = std::floor(t0 / period) + 1.0, k1 = std::floor(t1 / period);
    for (double k = k0; k <= k1; k += 1.0) f(k * period, static_cast<i64>(k));
}

// Deterministic per-entity random numbers: a 32-bit xorshift state (never zero).
inline u32 xorshift(u32& s) {
    u32 x = s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s = x;
    return x;
}
inline float rand01(u32& s) { return static_cast<float>(xorshift(s) >> 8) * (1.0f / 16777216.0f); }
inline float randRange(u32& s, float lo, float hi) { return lo + (hi - lo) * rand01(s); }
inline u32 hash32(u32 a, u32 b) {
    u32 h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h ? h : 0x1234567u;
}

}  // namespace rf::sim
