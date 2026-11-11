#pragma once
// Scene primitives (design/look.md §9, TECH_SPEC 5.3) as written into persistently mapped GPU memory.
// Positions are content pixels (y down): the renderer maps content to the logical screen with a scale
// and an offset (identity for the game; scale height/1080 and centring for 2400x1080 scene files).
// Colours are linear light already multiplied by their HDR intensity (binary16).
// Layouts are shared with shaders/capsule.vert, glow.vert and tri.vert.

#include "core/math.h"
#include "core/types.h"

namespace rf {

// Capsule end flags (look.md §4 joints): a flat end has no cap and a one-pixel ramp instead.
enum CapsuleFlags : u32 {
    kFlatStart = 1u,
    kFlatEnd = 2u,
    kDisc = 4u,  // a disc: its radius is not scaled by the look's line width
};

struct CapsuleGpu {
    float p0x, p0y, p1x, p1y;  // content pixels
    u16 c0[4];                 // binary16: colour at p0 (rgb), half width in content pixels
    u16 c1[4];                 // binary16: colour at p1 (rgb), flags
};
static_assert(sizeof(CapsuleGpu) == 32, "capsule instance must stay 32 bytes");

// Soft light: e^(-d^2 / 2 sigma^2), sigma = radius / radiusOverSigma, cut off at a square of
// cutoffRadii x radius (look.md §3).
struct GlowGpu {
    float x, y, radius;  // content pixels
    float pad;
    u16 rgb[4];          // binary16 colour, alpha unused
};
static_assert(sizeof(GlowGpu) == 24, "glow instance must stay 24 bytes");

// Flat additive triangle corner (glass).
struct TriVertexGpu {
    float x, y;
    u16 rgb[4];
};
static_assert(sizeof(TriVertexGpu) == 16, "triangle vertex must stay 16 bytes");

// One corner of one glyph segment (TECH_SPEC 5.3 "Glyph"): four per segment, built once at load from
// design/glyphs.json. Unit space with y down (the file is y up), +x forward. Shared with glyph.vert.
struct GlyphVertexGpu {
    float x0, y0, x1, y1;  // the segment, glyph units
    u16 info[4];           // binary16: glyph extent, layer (0 hull, 1 ornament, 2 core), joint flags, field (0 heading, 1 spin)
};
static_assert(sizeof(GlyphVertexGpu) == 24, "glyph vertex must stay 24 bytes");

// How an instance is coloured (glyph.vert).
enum GlyphMode : u32 {
    kGlyphEnemy = 0,     // look.md §2: hull field by the shape's kind, ornament accent+partner, core white
    kGlyphWhite = 1,     // every segment white (variant marks, the elite's and the player's white copies)
    kGlyphPlayer = 2,    // hull from the centre out over 1.3 radii: white, a, b; core white
    kGlyphRingA = 3,     // around the centre, repeating: a, white, a, b (player halo and Riff rings)
    kGlyphRingW = 4,     // around the centre, repeating: white, a, white, b (shock rings)
    kGlyphFlat = 5,      // flat colour a (Shards, warp-in rings, enemy bullets, the threat layer)
};

// One glyph on screen (TECH_SPEC 5.3): world position, two rotations (hull and core / ornament),
// scale, the hull's intensity and half width, and the two colours the fields run between.
struct GlyphInstanceGpu {
    float x, y;      // world units
    u16 rot[4];      // binary16: cos, sin of layers 0 and 2; cos, sin of layer 1
    u16 shape[4];    // binary16: world units per glyph unit, hull intensity, hull half width (ref px), mode
    u32 a;           // RGBA8, linear light: hue (enemy), player colour, flat colour
    u32 b;           // RGBA8, linear light: partner (enemy), deep or light (player)
};
static_assert(sizeof(GlyphInstanceGpu) == 32, "glyph instance must stay 32 bytes");

// Stateless spark (TECH_SPEC 5.3 "Particle"): written once when it is born; the vertex shader moves,
// stretches and fades it. Shared with particle.vert.
struct ParticleGpu {
    float x, y;            // birth position, world units
    float t0;              // birth time, seconds on the stage clock
    u16 motion[4];         // binary16: velocity x, y (wu/s), life (s), drag (1/s)
    u16 light[2];          // binary16: intensity at birth, half width (ref px)
    u32 colour;            // RGBA8, linear light: the spark's own colour
    u32 flags;             // reserved
};
static_assert(sizeof(ParticleGpu) == 32, "particle must stay 32 bytes");

constexpr u32 rgba(u8 r, u8 g, u8 b, u8 a = 255) {
    return u32(r) | (u32(g) << 8) | (u32(b) << 16) | (u32(a) << 24);
}

// Linear colour (0..1 per channel) as RGBA8 UNORM; the shaders read it back as linear light.
u32 packLinear(Vec3 c);

// Linear-light colour from an sRGB byte colour (M0 content and the dev overlay).
Vec3 srgbToLinear(u32 rgba8);

void writeCapsule(CapsuleGpu& out, Vec2 a, Vec2 b, float halfWidth, Vec3 c0, Vec3 c1, u32 flags);
void writeGlow(GlowGpu& out, Vec2 p, float radius, Vec3 colour);
void writeTriVertex(TriVertexGpu& out, Vec2 p, Vec3 colour);

// M0-style stroke: an sRGB colour times an HDR intensity, round caps.
struct Stroke {
    float radius = 1.0f;     // pixels (half the line width)
    u32 color = rgba(255, 255, 255);
    float intensity = 1.0f;  // HDR multiplier on the colour
    float skirt = 0.0f;      // unused since M1 (the bloom makes the glow); kept for the M0 call sites
};

template <typename T>
class Span {
public:
    void begin(T* memory, u32 capacity) {
        mem_ = memory;
        cap_ = capacity;
        count_ = 0;
        dropped_ = 0;
    }
    T* push() {
        if (count_ >= cap_) {
            ++dropped_;
            return nullptr;
        }
        return mem_ + count_++;
    }
    u32 count() const { return count_; }
    u32 dropped() const { return dropped_; }
    u32 capacity() const { return cap_; }

private:
    T* mem_ = nullptr;
    u32 cap_ = 0;
    u32 count_ = 0;
    u32 dropped_ = 0;
};

class CapsuleBatch : public Span<CapsuleGpu> {
public:
    void line(Vec2 a, Vec2 b, float halfWidth, Vec3 c0, Vec3 c1, u32 flags = 0) {
        if (CapsuleGpu* g = push()) writeCapsule(*g, a, b, halfWidth, c0, c1, flags);
    }
    void line(Vec2 a, Vec2 b, const Stroke& s);
    void dot(Vec2 p, const Stroke& s) { line(p, p, s); }
    // Closed polygon / circle approximation centred on `c`. Turns below the flat-joint angle get flat
    // joints, so rings do not turn into strings of beads (look.md §4).
    void polygon(Vec2 c, float radius, u32 sides, float rotation, const Stroke& s);
};

class GlowBatch : public Span<GlowGpu> {
public:
    void glow(Vec2 p, float radius, Vec3 colour) {
        if (GlowGpu* g = push()) writeGlow(*g, p, radius, colour);
    }
};

class TriBatch : public Span<TriVertexGpu> {
public:
    void tri(Vec2 a, Vec3 ca, Vec2 b, Vec3 cb, Vec2 c, Vec3 cc);
};

}  // namespace rf
