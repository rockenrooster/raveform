#pragma once
// The game's original stroke font ("RF Cut", design/vector_font.txt). The design file is embedded
// verbatim at build time, parsed once at start-up and drawn as capsules (TECH_SPEC 5.3).

#include "core/math.h"
#include "core/types.h"
#include "gfx/capsule.h"

namespace rf {

struct TextStyle {
    float capHeight = 24.0f;  // pixels for the 6-unit cap height
    u32 color = rgba(255, 255, 255);
    float intensity = 1.0f;
    float weight = 0.55f;     // stroke thickness in font units (the design's HUD weight)
    float skirt = 0.0f;       // soft glow in pixels
};

class VectorFont {
public:
    static constexpr float kCapUnits = 6.0f;      // grid y 0..6
    static constexpr float kAdvanceUnits = 6.0f;  // per character

    bool init();
    // Draws with the top-left of the first cap box at `origin`; returns the advance in pixels.
    float draw(CapsuleBatch& batch, Vec2 origin, const TextStyle& style, const char* text) const;
    float measure(const TextStyle& style, const char* text) const;
    u32 glyphCount() const { return glyphCount_; }
    u32 segmentCount() const { return segmentCount_; }

private:
    struct Segment {
        i8 x0, y0, x1, y1;
    };
    struct Glyph {
        u16 first = 0;
        u16 count = 0;
        bool defined = false;
    };
    const Glyph* lookup(char c) const;
    bool addSegment(int x0, int y0, int x1, int y1);

    Glyph glyphs_[128];
    Segment segments_[1024];
    u32 segmentCount_ = 0;
    u32 glyphCount_ = 0;
};

}  // namespace rf
