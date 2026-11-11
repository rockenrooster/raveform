#pragma once
// Minimal maths for M0. Grows with the renderer and sim (M1, M3).

#include <cmath>

namespace rf {

constexpr float kPi = 3.14159265358979f;
constexpr float kTau = 6.28318530717959f;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }
inline Vec2 polar(float radius, float angle) { return {radius * std::cos(angle), radius * std::sin(angle)}; }

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace rf
