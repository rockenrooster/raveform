#pragma once
// Upgrade icons (design/icons.json, embedded, parsed once): each icon is a list of additive layers in a
// unit frame (y up, frame radius 1): glows, filled polygons with gradients, line sets with gradients and
// flares. draw() puts one in the scene (HDR, bloomed) at a centre and radius in logical pixels; drawHud()
// draws its lines on the HUD layer (after tone mapping) through a colour function.

#include "core/types.h"
#include "gfx/renderer.h"

namespace rf {

class IconSet {
public:
    IconSet() = default;
    IconSet(const IconSet&) = delete;
    IconSet& operator=(const IconSet&) = delete;
    ~IconSet();

    bool load();
    i32 find(const char* id) const;
    float size(i32 icon) const;  // the icon's own scale (Mods 1, Riffs 1.14, Boosts 1.3)
    // `radiusPx`: the frame radius before the icon's own size; `intensity` scales every layer.
    void draw(FrameDraw& d, i32 icon, Vec2 centre, float radiusPx, float intensity) const;
    using ColourFn = Vec3 (*)(const void* user, Vec3 linear, float intensity);
    void drawHud(CapsuleBatch& hud, i32 icon, Vec2 centre, float radiusPx, float intensity, ColourFn fn, const void* user) const;

private:
    struct Layer {
        u8 type = 0;   // 0 glow, 1 fill, 2 lines, 3 flare
        u8 grad = 0;   // 0 solid, 1 linear, 2 radial, 3 angular
        u8 stops = 1;
        u8 arms = 4;
        Vec3 colour[4];
        float g[5] = {};
        float intensity = 1.0f, width = 1.0f;
        float x = 0, y = 0, r = 0, len = 0, rot = 0;
        u32 first = 0, count = 0;  // points (fill) or segment endpoints (lines) in pts_
    };
    struct Icon {
        char id[24] = {};
        float size = 1.0f;
        u32 firstLayer = 0, layers = 0;
    };
    Vec3 colourAt(const Layer& l, Vec2 p) const;

    Icon* icons_ = nullptr;
    u32 iconCount_ = 0;
    Layer* layers_ = nullptr;
    u32 layerCount_ = 0;
    Vec2* pts_ = nullptr;
    Vec3* ptColour_ = nullptr;  // colourAt(its layer, the point), once at load: gradients need atan2 or a square root a point
    u32 ptCount_ = 0;
};

}  // namespace rf
