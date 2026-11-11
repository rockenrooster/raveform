#pragma once
// The glyph sheet (design/scenes.md §1): every glyph of design/glyphs.json drawn by the instanced glyph
// path with the colour rules of design/look.md §2, compared by eye with design/previews/glyphs_*.png.
// Three pages at 2400x1080: families 1-9, families 10-17, then the variant marks, elites, the ship with
// every halo in all four player colours, a Shard and an enemy bullet. A still: no time-driven effects.

#include "content/glyphs.h"
#include "content/look.h"
#include "content/palette.h"
#include "gfx/renderer.h"
#include "gfx/vector_font.h"

namespace rf {

class GlyphSheet {
public:
    void init(const Look& look, const Palettes& palettes, const Palette& palette, const GlyphSet& glyphs,
              const VectorFont& font, u32 page) {
        look_ = &look;
        palettes_ = &palettes;
        palette_ = &palette;
        glyphs_ = &glyphs;
        font_ = &font;
        page_ = page < 1 ? 1 : (page > 3 ? 3 : page);
    }
    void setPalette(const Palette& p) { palette_ = &p; }
    void render(FrameDraw& draw, FrameParams& params, Background& bg, float widthPx, float heightPx);

private:
    void enemy(FrameDraw& d, int glyph, Vec2 at, float scaleWu, float rot, u32 hue, bool elite, int mark);

    const Look* look_ = nullptr;
    const Palettes* palettes_ = nullptr;
    const Palette* palette_ = nullptr;
    const GlyphSet* glyphs_ = nullptr;
    const VectorFont* font_ = nullptr;
    u32 page_ = 1;
    float pxPerWu_ = 1.5f;
    float refPx_ = 1.0f;
};

}  // namespace rf
