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
// The 256 values once (the same formula, so the same floats): every Stroke line converts its colour, the HUD's and the
// screens' by the thousand a frame, and three powf a line were a twentieth of a Cortex-A53's frame.
struct SrgbTable {
    float v[256];
    SrgbTable() {
        for (u32 i = 0; i < 256; ++i) v[i] = srgbChannel(i);
    }
};
// On first use (a namespace-scope table could be read by another file's static initialiser before its own ran).
const float* srgbTable() {
    static const SrgbTable t;
    return t.v;
}
}  // namespace

Vec3 srgbToLinear(u32 c) {
    const float* t = srgbTable();
    return {t[c & 0xFFu], t[(c >> 8) & 0xFFu], t[(c >> 16) & 0xFFu]};
}

u32 packLinear(Vec3 c) {
    const auto q = [](float v) { return static_cast<u32>(clampf(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    return q(c.x) | (q(c.y) << 8) | (q(c.z) << 16) | (255u << 24);
}

void writeGlow(GlowGpu& out, Vec2 p, float radius, Vec3 colour) {
    GlowGpu g;
    g.x = p.x;
    g.y = p.y;
    g.radius = radius;
    g.pad = 0.0f;
    floatToHalf4(colour.x, colour.y, colour.z, 0.0f, g.rgb);
    std::memcpy(&out, &g, sizeof g);
}

void writeTriVertex(TriVertexGpu& out, Vec2 p, Vec3 colour) {
    TriVertexGpu v;
    v.x = p.x;
    v.y = p.y;
    floatToHalf4(colour.x, colour.y, colour.z, 0.0f, v.rgb);
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
