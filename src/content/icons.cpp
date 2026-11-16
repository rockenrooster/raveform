#include "content/icons.h"

#include <cmath>
#include <cstring>
#include <new>

#include "core/json_doc.h"
#include "core/log.h"

namespace rf {

namespace {
const char kIconsJson[] =
#include "icons.json.inc"
    ;
}  // namespace

IconSet::~IconSet() {
    delete[] icons_;
    delete[] layers_;
    delete[] pts_;
    delete[] ptColour_;
}

bool IconSet::load() {
    JsonDoc doc;
    if (!doc.parse(kIconsJson, sizeof kIconsJson - 1, "design/icons.json")) return false;
    const JsonValue* sw = doc.find("swatches");
    const JsonValue* list = doc.find("icons");
    if (!sw || !list) {
        RF_ERR("icons: swatches or icons missing");
        return false;
    }
    const auto swatch = [&](const char* name) {
        Vec3 c{1.0f, 1.0f, 1.0f};
        const JsonValue* s = doc.member(sw, name);
        float v[3];
        if (s && doc.numbers("linear", v, 3, s) == 3) c = Vec3{v[0], v[1], v[2]};
        return c;
    };
    // Count, then fill.
    u32 icons = 0, layers = 0, pts = 0;
    for (const JsonValue* ic = doc.firstChild(list); ic; ic = doc.nextSibling(ic)) {
        ++icons;
        for (const JsonValue* l = doc.firstChild(doc.member(ic, "layers")); l; l = doc.nextSibling(l)) {
            ++layers;
            if (const JsonValue* p = doc.member(l, "pts")) pts += p->count;
            if (const JsonValue* s = doc.member(l, "segs")) pts += 2 * s->count;
        }
    }
    icons_ = new (std::nothrow) Icon[icons];
    layers_ = new (std::nothrow) Layer[layers];
    pts_ = new (std::nothrow) Vec2[pts ? pts : 1];
    ptColour_ = new (std::nothrow) Vec3[pts ? pts : 1];
    if (!icons_ || !layers_ || !pts_ || !ptColour_) return false;
    for (const JsonValue* ic = doc.firstChild(list); ic; ic = doc.nextSibling(ic)) {
        Icon& out = icons_[iconCount_++];
        doc.text("id", out.id, sizeof out.id, ic);
        out.size = static_cast<float>(doc.number("size", 1.0, nullptr, ic));
        out.firstLayer = layerCount_;
        for (const JsonValue* lj = doc.firstChild(doc.member(ic, "layers")); lj; lj = doc.nextSibling(lj)) {
            Layer& l = layers_[layerCount_++];
            ++out.layers;
            char t[8] = {};
            doc.text("t", t, sizeof t, lj);
            l.type = std::strcmp(t, "glow") == 0 ? 0 : (std::strcmp(t, "fill") == 0 ? 1 : (std::strcmp(t, "lines") == 0 ? 2 : 3));
            l.intensity = static_cast<float>(doc.number("i", 1.0, nullptr, lj));
            l.width = static_cast<float>(doc.number("w", 1.0, nullptr, lj));
            l.x = static_cast<float>(doc.number("x", 0.0, nullptr, lj));
            l.y = static_cast<float>(doc.number("y", 0.0, nullptr, lj));
            l.r = static_cast<float>(doc.number("r", 0.0, nullptr, lj));
            l.len = static_cast<float>(doc.number("len", 0.0, nullptr, lj));
            l.rot = static_cast<float>(doc.number("rot", 0.0, nullptr, lj));
            l.arms = static_cast<u8>(doc.number("arms", 4.0, nullptr, lj));
            if (const JsonValue* c = doc.member(lj, "colour")) l.colour[0] = swatch(doc.string(c));
            if (const JsonValue* cs = doc.member(lj, "colours")) {
                l.stops = 0;
                for (const JsonValue* c = doc.firstChild(cs); c && l.stops < 4; c = doc.nextSibling(c)) l.colour[l.stops++] = swatch(doc.string(c));
                if (!l.stops) l.stops = 1;
            }
            if (const JsonValue* g = doc.member(lj, "grad"); g && g->type == JsonValue::Type::Array) {
                const JsonValue* kind = doc.firstChild(g);
                const char* k = doc.string(kind);
                l.grad = std::strcmp(k, "lin") == 0 ? 1 : (std::strcmp(k, "rad") == 0 ? 2 : 3);
                u32 n = 0;
                for (const JsonValue* v = doc.nextSibling(kind); v && n < 5; v = doc.nextSibling(v)) l.g[n++] = static_cast<float>(v->number);
            }
            l.first = ptCount_;
            if (const JsonValue* p = doc.member(lj, "pts")) {
                for (const JsonValue* q = doc.firstChild(p); q; q = doc.nextSibling(q)) {
                    const JsonValue* a = doc.firstChild(q);
                    const JsonValue* b = doc.nextSibling(a);
                    pts_[ptCount_++] = Vec2{a ? static_cast<float>(a->number) : 0.0f, b ? static_cast<float>(b->number) : 0.0f};
                }
            }
            if (const JsonValue* s = doc.member(lj, "segs")) {
                for (const JsonValue* q = doc.firstChild(s); q; q = doc.nextSibling(q)) {
                    float v[4] = {};
                    u32 n = 0;
                    for (const JsonValue* e = doc.firstChild(q); e && n < 4; e = doc.nextSibling(e)) v[n++] = static_cast<float>(e->number);
                    pts_[ptCount_++] = Vec2{v[0], v[1]};
                    pts_[ptCount_++] = Vec2{v[2], v[3]};
                }
            }
            l.count = ptCount_ - l.first;
        }
    }
    // Every point's gradient colour once: the HUD draws its icons every frame, and the colour of a point never changes.
    for (u32 k = 0; k < layerCount_; ++k) {
        const Layer& l = layers_[k];
        for (u32 j = 0; j < l.count; ++j) ptColour_[l.first + j] = colourAt(l, pts_[l.first + j]);
    }
    RF_LOG("icons: %u icons, %u layers", iconCount_, layerCount_);
    return true;
}

i32 IconSet::find(const char* id) const {
    for (u32 i = 0; i < iconCount_; ++i)
        if (std::strcmp(icons_[i].id, id) == 0) return static_cast<i32>(i);
    return -1;
}

float IconSet::size(i32 icon) const { return icon >= 0 && static_cast<u32>(icon) < iconCount_ ? icons_[icon].size : 1.0f; }

Vec3 IconSet::colourAt(const Layer& l, Vec2 p) const {
    if (l.grad == 0 || l.stops < 2) return l.colour[0];
    float t = 0.0f;
    if (l.grad == 1) {
        const Vec2 a{l.g[0], l.g[1]}, b{l.g[2], l.g[3]};
        const Vec2 ab = b - a;
        const float d = ab.x * ab.x + ab.y * ab.y;
        t = d > 1e-9f ? ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / d : 0.0f;
    } else if (l.grad == 2) {
        const float dx = p.x - l.g[0], dy = p.y - l.g[1];
        t = l.g[2] > 1e-6f ? std::sqrt(dx * dx + dy * dy) / l.g[2] : 0.0f;
    } else {
        float a = std::atan2(p.y - l.g[1], p.x - l.g[0]) - l.g[2];
        a = a / kTau;
        t = a - std::floor(a);
    }
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    const float s = t * static_cast<float>(l.stops - 1);
    const u32 k = static_cast<u32>(s) < static_cast<u32>(l.stops - 1) ? static_cast<u32>(s) : static_cast<u32>(l.stops - 2);
    const float f = s - static_cast<float>(k);
    return l.colour[k] + (l.colour[k + 1] - l.colour[k]) * f;
}

void IconSet::draw(FrameDraw& d, i32 icon, Vec2 c, float radiusPx, float intensity) const {
    if (icon < 0 || static_cast<u32>(icon) >= iconCount_ || intensity <= 0.0f) return;
    const Icon& ic = icons_[icon];
    const float R = radiusPx * ic.size;
    const auto at = [&](Vec2 p) { return Vec2{c.x + p.x * R, c.y - p.y * R}; };  // y up -> screen y down
    for (u32 k = 0; k < ic.layers; ++k) {
        const Layer& l = layers_[ic.firstLayer + k];
        const float in = l.intensity * intensity;
        switch (l.type) {
            case 0: {
                const float r = l.r * R;
                if (r < 16.0f) {
                    d.sparkles.glow(at(Vec2{l.x, l.y}), r, l.colour[0] * in);
                } else {
                    d.pools.glow(at(Vec2{l.x, l.y}), r, l.colour[0] * in);
                }
                break;
            }
            case 1: {
                if (l.count < 3) break;
                const Vec2 p0 = pts_[l.first];
                const Vec3 c0 = ptColour_[l.first] * in;
                for (u32 j = 1; j + 1 < l.count; ++j) {
                    const Vec2 p1 = pts_[l.first + j], p2 = pts_[l.first + j + 1];
                    d.glass.tri(at(p0), c0, at(p1), ptColour_[l.first + j] * in, at(p2), ptColour_[l.first + j + 1] * in);
                }
                break;
            }
            case 2: {
                const float hw = 0.5f * l.width * R / 60.0f;
                for (u32 j = 0; j + 1 < l.count; j += 2) {
                    const Vec2 a = pts_[l.first + j], b = pts_[l.first + j + 1];
                    d.scene.line(at(a), at(b), hw, ptColour_[l.first + j] * in, ptColour_[l.first + j + 1] * in, 0);
                }
                break;
            }
            default: {
                const float hw = 0.5f * l.width * R / 60.0f;
                const Vec2 o = at(Vec2{l.x, l.y});
                for (u32 j = 0; j < l.arms; ++j) {
                    const float a = l.rot + kTau * 0.5f * static_cast<float>(j) / static_cast<float>(l.arms);
                    const Vec2 dv{std::cos(a) * l.len * R, -std::sin(a) * l.len * R};
                    d.scene.line(o, o + dv, hw, l.colour[0] * in, Vec3{}, 0);
                    d.scene.line(o, o - dv, hw, l.colour[0] * in, Vec3{}, 0);
                }
                break;
            }
        }
    }
}

void IconSet::drawHud(CapsuleBatch& hud, i32 icon, Vec2 c, float radiusPx, float intensity, ColourFn fn, const void* user) const {
    if (icon < 0 || static_cast<u32>(icon) >= iconCount_ || intensity <= 0.0f) return;
    const Icon& ic = icons_[icon];
    const float R = radiusPx * ic.size;
    const auto at = [&](Vec2 p) { return Vec2{c.x + p.x * R, c.y - p.y * R}; };
    for (u32 k = 0; k < ic.layers; ++k) {
        const Layer& l = layers_[ic.firstLayer + k];
        if (l.type != 2) continue;
        const float in = std::fmin(l.intensity, 2.4f) * intensity;
        const float hw = std::fmax(0.5f * l.width * R / 60.0f, 0.6f);
        for (u32 j = 0; j + 1 < l.count; j += 2) {
            const Vec2 a = pts_[l.first + j], b = pts_[l.first + j + 1];
            hud.line(at(a), at(b), hw, fn(user, ptColour_[l.first + j], in), fn(user, ptColour_[l.first + j + 1], in), 0);
        }
    }
}

}  // namespace rf
