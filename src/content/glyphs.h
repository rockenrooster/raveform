#pragma once
// Glyph shapes (design/look.md §4) loaded from design/glyphs.json, embedded at build time, plus a few
// synthetic shapes the effects need (unit circles). Each glyph becomes a static mesh of capsule
// segments in unit space (y down, +x forward), sorted by layer so that the hull (layer 0) is a prefix:
// the low-detail LOD and the white copies draw only that prefix. Joint flags follow the look.md §4
// rule, so rings and curves do not light their joints twice.

#include "core/math.h"
#include "core/types.h"
#include "gfx/capsule.h"

namespace rf {

enum class GlyphKind : u8 { Enemy, Player, Pickup, Threat, Mark, Shape };

struct GlyphDef {
    char id[20] = {};
    char family[16] = {};
    u8 tier = 0;  // 1..5 for enemies
    GlyphKind kind = GlyphKind::Shape;
    bool facesHeading = false;
    float size = 1.0f;    // visual radius relative to the standard enemy (14 wu)
    float extent = 1.0f;  // farthest point from the centre, glyph units
    u32 firstSegment = 0;
    u32 segmentCount = 0;
    u32 hullCount = 0;    // layer-0 segments (a prefix)
    u32 layerCount[3] = {};
    u32 glassFirst = 0;   // triangles of the hull's first closed loop (elite glass), in `glass`
    u32 glassCount = 0;
};

struct GlyphSegment {
    Vec2 a, b;  // glyph units, y down
    u8 layer = 0;
};

class GlyphSet {
public:
    static constexpr u32 kMaxGlyphs = 160;  // play test 2: the halo's partial arcs (halo_k_of_n) join the file's 99

    GlyphSet() = default;
    GlyphSet(const GlyphSet&) = delete;
    GlyphSet& operator=(const GlyphSet&) = delete;
    ~GlyphSet();

    bool load(float jointFlatBelowDegrees);

    int find(const char* id) const;  // -1 when missing
    const GlyphDef& def(u32 i) const { return defs_[i]; }
    u32 count() const { return count_; }
    // Enemy glyph of a family at a tier (1..5), -1 when missing.
    int enemy(const char* family, u32 tier) const;

    const GlyphVertexGpu* vertices() const { return vertices_; }
    u32 vertexCount() const { return segmentCount_ * 4; }
    u32 segmentCount() const { return segmentCount_; }
    const GlyphSegment* segments() const { return segments_; }
    const Vec2* glass() const { return glass_; }  // three corners per triangle, glyph units

private:
    bool addGlyph(const GlyphDef& base, const GlyphSegment* segs, u32 n, float flatBelowDegrees);

    GlyphDef defs_[kMaxGlyphs];
    u32 count_ = 0;
    GlyphVertexGpu* vertices_ = nullptr;
    GlyphSegment* segments_ = nullptr;
    u32 segmentCount_ = 0;
    u32 segmentCapacity_ = 0;
    Vec2* glass_ = nullptr;
    u32 glassTris_ = 0;
    u32 glassCapacity_ = 0;
};

}  // namespace rf
