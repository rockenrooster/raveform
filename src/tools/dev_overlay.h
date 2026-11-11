#pragma once
// Dev overlay text: fixed line storage formatted by the app a few times per second and drawn with
// the vector font every frame. No allocation.

#include "core/math.h"
#include "core/types.h"
#include "gfx/capsule.h"

namespace rf {

class VectorFont;

class DevOverlay {
public:
    static constexpr u32 kMaxLines = 14;
    static constexpr u32 kLineChars = 96;

    void clear() { count_ = 0; }
    void addLine(u32 color, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
    // Draws all lines from `origin` (top-left), `capHeight` pixels tall, `lineStep` apart.
    // Returns the y below the last line.
    float draw(CapsuleBatch& batch, const VectorFont& font, Vec2 origin, float capHeight, float lineStep) const;

private:
    char lines_[kMaxLines][kLineChars] = {};
    u32 colors_[kMaxLines] = {};
    u32 count_ = 0;
};

}  // namespace rf
