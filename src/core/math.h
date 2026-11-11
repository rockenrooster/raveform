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
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float length(Vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }

// Linear RGB (light), values above 1 are normal (HDR).
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 mix(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace rf
