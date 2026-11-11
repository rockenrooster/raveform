#pragma once
// Capsule instances (TECH_SPEC 5.3): round-capped segments written straight into the persistently
// mapped per-frame streaming buffer. Layout is shared with shaders/capsule.vert.

#include "core/math.h"
#include "core/types.h"

namespace rf {

struct CapsuleInstance {
    float p0x, p0y, p1x, p1y;  // logical pixels
    u16 radius;                // binary16, pixels
    u16 intensity;             // binary16, HDR multiplier
    u32 color;                 // RGBA8 sRGB, R in the low byte
    u32 flags;                 // bits 0-7: glow skirt width in quarter pixels
    u32 reserved;
};
static_assert(sizeof(CapsuleInstance) == 32, "capsule instance must stay 32 bytes");

constexpr u32 rgba(u8 r, u8 g, u8 b, u8 a = 255) {
    return u32(r) | (u32(g) << 8) | (u32(b) << 16) | (u32(a) << 24);
}

struct Stroke {
    float radius = 1.0f;     // pixels (half the line width)
    u32 color = rgba(255, 255, 255);
    float intensity = 1.0f;  // HDR multiplier on the colour
    float skirt = 0.0f;      // soft glow width in pixels (0-63)
};

class CapsuleBatch {
public:
    void begin(CapsuleInstance* memory, u32 capacity) {
        mem_ = memory;
        cap_ = capacity;
        count_ = 0;
        dropped_ = 0;
    }
    void line(Vec2 a, Vec2 b, const Stroke& s);
    void dot(Vec2 p, const Stroke& s) { line(p, p, s); }
    // Closed polygon / circle approximation centred on `c`.
    void polygon(Vec2 c, float radius, u32 sides, float rotation, const Stroke& s);
    u32 count() const { return count_; }
    u32 dropped() const { return dropped_; }

private:
    CapsuleInstance* mem_ = nullptr;
    u32 cap_ = 0;
    u32 count_ = 0;
    u32 dropped_ = 0;
};

}  // namespace rf
