#pragma once
// Palettes (design/look.md §2) loaded from design/palettes.json, embedded at build time. Colours are
// linear-light sRGB; multiply by a light level from look.md §3 for the HDR value.

#include "core/math.h"
#include "core/types.h"

namespace rf {

struct Palette {
    char id[16] = {};
    char name[24] = {};
    u32 track = 0;
    Vec3 bg0, bg1, grid, gridHot, accent;
    // The three enemy hues of a solo run (soloEnemies) and each one's partner (soloPartner).
    Vec3 hue[3];
    Vec3 partner[3];
};

struct Palettes {
    static constexpr u32 kMax = 12;  // M5: ten Tracks, ten palettes (and room for two more)
    Palette list[kMax];
    u32 count = 0;
    Vec3 danger, pickup, white{1.0f, 1.0f, 1.0f};
    Vec3 player[4], playerDeep[4], playerLight[4];

    const Palette* find(const char* id) const;
    u32 indexOf(const Palette* p) const { return p ? static_cast<u32>(p - list) : 0u; }
};

bool loadPalettes(Palettes& out);

}  // namespace rf
