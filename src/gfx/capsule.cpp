#include "gfx/capsule.h"

#include <cmath>
#include <cstring>

#include "core/half.h"

namespace rf {

namespace {
float srgbChannel(u32 v) {
    const float c = static_cast<float>(v & 0xFFu) / 255.0f;
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
}  // namespace

Vec3 srgbToLinear(u32 c) { return {srgbChannel(c), srgbChannel(c >> 8), srgbChannel(c >> 16)}; }

u32 packLinear(Vec3 c) {
    const auto q = [](float v) { return static_cast<u32>(clampf(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    return q(c.x) | (q(c.y) << 8) | (q(c.z) << 16) | (255u << 24);
}

void writeCapsule(CapsuleGpu& out, Vec2 a, Vec2 b, float halfWidth, Vec3 c0, Vec3 c1, u32 flags) {
    CapsuleGpu g;
    g.p0x = a.x;
    g.p0y = a.y;
    g.p1x = b.x;
    g.p1y = b.y;
    g.c0[0] = floatToHalf(c0.x);
    g.c0[1] = floatToHalf(c0.y);
    g.c0[2] = floatToHalf(c0.z);
    g.c0[3] = floatToHalf(halfWidth);
    g.c1[0] = floatToHalf(c1.x);
    g.c1[1] = floatToHalf(c1.y);
    g.c1[2] = floatToHalf(c1.z);
    g.c1[3] = floatToHalf(static_cast<float>(flags));
    std::memcpy(&out, &g, sizeof g);  // one sequential write into write-combined memory
}

void writeGlow(GlowGpu& out, Vec2 p, float radius, Vec3 colour) {
    GlowGpu g;
    g.x = p.x;
    g.y = p.y;
    g.radius = radius;
    g.pad = 0.0f;
    g.rgb[0] = floatToHalf(colour.x);
    g.rgb[1] = floatToHalf(colour.y);
    g.rgb[2] = floatToHalf(colour.z);
    g.rgb[3] = 0;
    std::memcpy(&out, &g, sizeof g);
}

void writeTriVertex(TriVertexGpu& out, Vec2 p, Vec3 colour) {
    TriVertexGpu v;
    v.x = p.x;
    v.y = p.y;
    v.rgb[0] = floatToHalf(colour.x);
    v.rgb[1] = floatToHalf(colour.y);
    v.rgb[2] = floatToHalf(colour.z);
    v.rgb[3] = 0;
    std::memcpy(&out, &v, sizeof v);
}

void CapsuleBatch::line(Vec2 a, Vec2 b, const Stroke& s) {
    const Vec3 c = srgbToLinear(s.color) * s.intensity;
    line(a, b, s.radius, c, c, 0);
}

void CapsuleBatch::polygon(Vec2 c, float radius, u32 sides, float rotation, const Stroke& s) {
    if (sides < 2) return;
    const Vec3 col = srgbToLinear(s.color) * s.intensity;
    // Exterior turn per corner is 360/sides degrees: flat joints below 20 degrees (18+ sides).
    const u32 flags = sides >= 18 ? (kFlatStart | kFlatEnd) : 0u;
    Vec2 prev = c + polar(radius, rotation);
    for (u32 i = 1; i <= sides; ++i) {
        const Vec2 next = c + polar(radius, rotation + kTau * static_cast<float>(i) / static_cast<float>(sides));
        line(prev, next, s.radius, col, col, flags);
        prev = next;
    }
}

void TriBatch::tri(Vec2 a, Vec3 ca, Vec2 b, Vec3 cb, Vec2 c, Vec3 cc) {
    TriVertexGpu* v0 = push();
    TriVertexGpu* v1 = push();
    TriVertexGpu* v2 = push();
    if (!v0 || !v1 || !v2) return;  // the partial triangle is dropped with the batch's count below
    writeTriVertex(*v0, a, ca);
    writeTriVertex(*v1, b, cb);
    writeTriVertex(*v2, c, cc);
}

}  // namespace rf
