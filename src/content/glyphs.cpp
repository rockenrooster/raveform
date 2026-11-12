#include "content/glyphs.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/half.h"
#include "core/json_doc.h"
#include "core/log.h"

namespace rf {

namespace {
const char kGlyphsJson[] =
#include "glyphs.json.inc"
    ;

constexpr u32 kMaxSegmentsPerGlyph = 256;

bool near(Vec2 a, Vec2 b) { return std::fabs(a.x - b.x) < 1e-3f && std::fabs(a.y - b.y) < 1e-3f; }

float cross(Vec2 o, Vec2 a, Vec2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }

// Ear clipping of a simple polygon; writes up to (n - 2) triangles. Returns the triangle count.
u32 triangulate(const Vec2* pts, u32 n, Vec2* out, u32 maxTris) {
    if (n < 3 || n > 64) return 0;
    u32 idx[64];
    for (u32 i = 0; i < n; ++i) idx[i] = i;
    float area = 0.0f;
    for (u32 i = 0; i < n; ++i) {
        const Vec2 a = pts[i], b = pts[(i + 1) % n];
        area += a.x * b.y - b.x * a.y;
    }
    const float sign = area >= 0.0f ? 1.0f : -1.0f;
    u32 tris = 0, m = n, guard = 0;
    while (m > 3 && tris < maxTris && guard++ < 4096) {
        bool clipped = false;
        for (u32 k = 0; k < m; ++k) {
            const Vec2 a = pts[idx[(k + m - 1) % m]], b = pts[idx[k]], c = pts[idx[(k + 1) % m]];
            if (cross(a, b, c) * sign <= 1e-7f) continue;  // reflex or degenerate corner
            bool inside = false;
            for (u32 q = 0; q < m && !inside; ++q) {
                const Vec2 p = pts[idx[q]];
                if (near(p, a) || near(p, b) || near(p, c)) continue;
                inside = cross(a, b, p) * sign > 0.0f && cross(b, c, p) * sign > 0.0f && cross(c, a, p) * sign > 0.0f;
            }
            if (inside) continue;
            out[tris * 3 + 0] = a;
            out[tris * 3 + 1] = b;
            out[tris * 3 + 2] = c;
            ++tris;
            for (u32 q = k; q + 1 < m; ++q) idx[q] = idx[q + 1];
            --m;
            clipped = true;
            break;
        }
        if (!clipped) break;  // not simple: keep what we have
    }
    if (m == 3 && tris < maxTris) {
        out[tris * 3 + 0] = pts[idx[0]];
        out[tris * 3 + 1] = pts[idx[1]];
        out[tris * 3 + 2] = pts[idx[2]];
        ++tris;
    }
    return tris;
}

// look.md §4 joints: where two segments of one layer meet running (nearly) straight through, both
// ends are cut flat. Ends that meet at sharper corners keep round caps. Greedy pairing, straightest first.
void jointFlags(const GlyphSegment* s, u32 n, float flatBelowDegrees, u32* flags) {
    const float limit = -std::cos(flatBelowDegrees * kPi / 180.0f);
    struct End {
        u32 seg;
        u32 bit;
        Vec2 at, away;
        u8 layer;
    };
    End ends[2 * kMaxSegmentsPerGlyph];
    u32 count = 0;
    for (u32 i = 0; i < n; ++i) {
        flags[i] = 0;
        const Vec2 d = s[i].b - s[i].a;
        const float len = length(d);
        if (len < 1e-4f) continue;
        const Vec2 u = d * (1.0f / len);
        ends[count++] = {i, kFlatStart, s[i].a, u, s[i].layer};
        ends[count++] = {i, kFlatEnd, s[i].b, u * -1.0f, s[i].layer};
    }
    bool used[2 * kMaxSegmentsPerGlyph] = {};
    for (;;) {
        // The straightest unused pair anywhere in the glyph (n is small).
        float best = limit;
        u32 bi = 0, bj = 0;
        bool found = false;
        for (u32 i = 0; i < count; ++i) {
            if (used[i]) continue;
            for (u32 j = i + 1; j < count; ++j) {
                if (used[j] || ends[i].seg == ends[j].seg || ends[i].layer != ends[j].layer || !near(ends[i].at, ends[j].at))
                    continue;
                const float straight = dot(ends[i].away, ends[j].away);
                if (straight <= best) {
                    best = straight;
                    bi = i;
                    bj = j;
                    found = true;
                }
            }
        }
        if (!found) break;
        used[bi] = used[bj] = true;
        flags[ends[bi].seg] |= ends[bi].bit;
        flags[ends[bj].seg] |= ends[bj].bit;
    }
}
}  // namespace

GlyphSet::~GlyphSet() {
    std::free(vertices_);
    std::free(segments_);
    std::free(glass_);
}

int GlyphSet::find(const char* id) const {
    for (u32 i = 0; i < count_; ++i)
        if (std::strcmp(defs_[i].id, id) == 0) return static_cast<int>(i);
    return -1;
}

int GlyphSet::enemy(const char* family, u32 tier) const {
    for (u32 i = 0; i < count_; ++i)
        if (defs_[i].kind == GlyphKind::Enemy && defs_[i].tier == tier && std::strcmp(defs_[i].family, family) == 0)
            return static_cast<int>(i);
    return -1;
}

bool GlyphSet::addGlyph(const GlyphDef& base, const GlyphSegment* segsIn, u32 n, float flatBelowDegrees) {
    if (count_ >= kMaxGlyphs || n == 0 || n > kMaxSegmentsPerGlyph) {
        RF_ERR("glyphs: cannot add '%s' (%u segments)", base.id, n);
        return false;
    }
    if (segmentCount_ + n > segmentCapacity_) {
        const u32 cap = (segmentCount_ + n) * 2;
        void* v = std::realloc(vertices_, sizeof(GlyphVertexGpu) * 4 * cap);
        if (!v) return false;
        vertices_ = static_cast<GlyphVertexGpu*>(v);
        void* s = std::realloc(segments_, sizeof(GlyphSegment) * cap);
        if (!s) return false;
        segments_ = static_cast<GlyphSegment*>(s);
        segmentCapacity_ = cap;
    }
    GlyphDef d = base;
    d.firstSegment = segmentCount_;
    d.segmentCount = n;
    // Sort by layer (stable), so the hull is a prefix.
    GlyphSegment* out = segments_ + segmentCount_;
    u32 k = 0;
    for (u8 layer = 0; layer < 3; ++layer) {
        for (u32 i = 0; i < n; ++i)
            if (segsIn[i].layer == layer) out[k++] = segsIn[i];
        d.layerCount[layer] = 0;
    }
    for (u32 i = 0; i < n; ++i) ++d.layerCount[out[i].layer < 3 ? out[i].layer : 2];
    d.hullCount = d.layerCount[0];
    u32 flags[kMaxSegmentsPerGlyph];
    jointFlags(out, n, flatBelowDegrees, flags);
    for (u32 i = 0; i < n; ++i) {
        GlyphVertexGpu v;
        v.x0 = out[i].a.x;
        v.y0 = out[i].a.y;
        v.x1 = out[i].b.x;
        v.y1 = out[i].b.y;
        v.info[0] = floatToHalf(d.extent);
        v.info[1] = floatToHalf(static_cast<float>(out[i].layer));
        v.info[2] = floatToHalf(static_cast<float>(flags[i]));
        v.info[3] = floatToHalf(d.facesHeading ? 0.0f : 1.0f);
        for (u32 c = 0; c < 4; ++c) vertices_[(segmentCount_ + i) * 4 + c] = v;
    }
    segmentCount_ += n;

    // Glass: the hull's first closed loop (design/tools/frame_mock.py draw_glyph), triangulated.
    d.glassFirst = glassTris_;
    d.glassCount = 0;
    if (d.hullCount >= 3 && d.kind == GlyphKind::Enemy) {
        Vec2 loop[64];
        u32 m = 0;
        bool closed = false;
        for (u32 i = 0; i < d.hullCount && m < 64; ++i) {
            loop[m++] = out[i].a;
            if (m > 2 && near(out[i].b, out[0].a)) {
                closed = true;
                break;
            }
        }
        if (closed) {
            if (glassTris_ + 64 > glassCapacity_) {
                const u32 cap = (glassTris_ + 64) * 2;
                void* g = std::realloc(glass_, sizeof(Vec2) * 3 * cap);
                if (!g) return false;
                glass_ = static_cast<Vec2*>(g);
                glassCapacity_ = cap;
            }
            d.glassCount = triangulate(loop, m, glass_ + glassTris_ * 3, 62);
            glassTris_ += d.glassCount;
        }
    }
    defs_[count_++] = d;
    return true;
}

bool GlyphSet::load(float flatBelowDegrees) {
    JsonDoc doc;
    if (!doc.parse(kGlyphsJson, sizeof kGlyphsJson - 1, "design/glyphs.json")) return false;
    const JsonValue* list = doc.find("glyphs");
    GlyphSegment segs[kMaxSegmentsPerGlyph];
    bool ok = true;
    for (const JsonValue* e = doc.firstChild(list); e; e = doc.nextSibling(e)) {
        GlyphDef d;
        doc.text("id", d.id, sizeof d.id, e);
        doc.text("family", d.family, sizeof d.family, e);
        char kind[16] = {};
        doc.text("kind", kind, sizeof kind, e);
        d.kind = std::strcmp(kind, "enemy") == 0    ? GlyphKind::Enemy
                 : std::strcmp(kind, "player") == 0 ? GlyphKind::Player
                 : std::strcmp(kind, "pickup") == 0 ? GlyphKind::Pickup
                 : std::strcmp(kind, "threat") == 0 ? GlyphKind::Threat
                 : std::strcmp(kind, "mark") == 0   ? GlyphKind::Mark
                                                    : GlyphKind::Shape;
        d.tier = static_cast<u8>(doc.number("tier", 0.0, nullptr, e));
        d.size = static_cast<float>(doc.number("size", 1.0, nullptr, e));
        d.extent = static_cast<float>(doc.number("extent", 1.0, nullptr, e));
        const JsonValue* fh = doc.member(e, "facesHeading");
        d.facesHeading = fh && fh->type == JsonValue::Type::Bool && fh->boolean;
        u32 n = 0;
        for (const JsonValue* s = doc.firstChild(doc.member(e, "segments")); s && n < kMaxSegmentsPerGlyph;
             s = doc.nextSibling(s)) {
            float v[5] = {};
            u32 k = 0;
            for (const JsonValue* x = doc.firstChild(s); x && k < 5; x = doc.nextSibling(x)) v[k++] = static_cast<float>(x->number);
            if (k != 5) {
                RF_ERR("glyphs: %s has a malformed segment", d.id);
                ok = false;
                break;
            }
            segs[n].a = {v[0], -v[1]};  // the file is y up, the world is y down
            segs[n].b = {v[2], -v[3]};
            segs[n].layer = static_cast<u8>(v[4]);
            ++n;
        }
        if (n == 0 || !addGlyph(d, segs, n, flatBelowDegrees)) ok = false;
    }
    // Play test 2 (look.md 6, Regen and Cells): the first k arcs of an n-arc halo ("halo_k_of_n"), so the halo can show k
    // whole cells in the places of n while the next arc grows back or draws itself in (a halo's arcs are stored in order).
    for (u32 n = 2; n <= 8; ++n) {
        char src[20];
        std::snprintf(src, sizeof src, "halo_%u", n);
        const int g = find(src);
        if (g < 0) continue;
        const GlyphDef base = defs_[g];
        const u32 per = base.segmentCount / n;
        if (per == 0 || per * n != base.segmentCount) continue;
        for (u32 k = 1; k < n; ++k) {
            GlyphDef d = base;
            std::snprintf(d.id, sizeof d.id, "halo_%u_of_%u", k, n);
            for (u32 i = 0; i < k * per; ++i) segs[i] = segments_[base.firstSegment + i];
            if (!addGlyph(d, segs, k * per, flatBelowDegrees)) ok = false;
        }
    }
    // Unit circles for rings (warp-in, shock, Riff and Drop fronts): flat joints all round.
    const u32 ringSides[3] = {32, 64, 128};
    const char* ringIds[3] = {"ring32", "ring64", "ring128"};
    for (u32 r = 0; r < 3; ++r) {
        GlyphDef d;
        std::snprintf(d.id, sizeof d.id, "%s", ringIds[r]);
        d.kind = GlyphKind::Shape;
        d.extent = 1.0f;
        const u32 n = ringSides[r];
        for (u32 i = 0; i < n; ++i) {
            segs[i].a = polar(1.0f, kTau * static_cast<float>(i) / static_cast<float>(n));
            segs[i].b = polar(1.0f, kTau * static_cast<float>(i + 1) / static_cast<float>(n));
            segs[i].layer = 0;
        }
        if (!addGlyph(d, segs, n, flatBelowDegrees)) ok = false;
    }
    if (ok) RF_LOG("glyphs: %u shapes, %u segments, %u glass triangles", count_, segmentCount_, glassTris_);
    return ok;
}

}  // namespace rf
