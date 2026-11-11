#include "tools/dev_overlay.h"

#include <cstdarg>
#include <cstdio>

#include "gfx/vector_font.h"

namespace rf {

void DevOverlay::addLine(u32 color, const char* fmt, ...) {
    if (count_ >= kMaxLines) return;
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(lines_[count_], kLineChars, fmt, ap);
    va_end(ap);
    colors_[count_] = color;
    ++count_;
}

float DevOverlay::draw(CapsuleBatch& batch, const VectorFont& font, Vec2 origin, float capHeight,
                       float lineStep) const {
    TextStyle style;
    style.capHeight = capHeight;
    Vec2 at = origin;
    for (u32 i = 0; i < count_; ++i) {
        style.color = colors_[i];
        font.draw(batch, at, style, lines_[i]);
        at.y += lineStep;
    }
    return at.y;
}

}  // namespace rf
