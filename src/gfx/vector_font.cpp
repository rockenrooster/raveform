#include "gfx/vector_font.h"

#include <cstring>

#include "core/log.h"

namespace rf {

namespace {

// design/vector_font.txt, embedded by cmake/EmbedText.cmake.
const char kFontSource[] =
#include "vector_font.txt.inc"
    ;

bool isDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

bool VectorFont::addSegment(int x0, int y0, int x1, int y1) {
    if (segmentCount_ >= countOf(segments_)) return false;
    segments_[segmentCount_++] = {static_cast<i8>(x0), static_cast<i8>(y0), static_cast<i8>(x1), static_cast<i8>(y1)};
    return true;
}

bool VectorFont::init() {
    segmentCount_ = 0;
    glyphCount_ = 0;
    for (Glyph& g : glyphs_) g = Glyph{};

    const char* p = kFontSource;
    int lineNo = 0;
    while (*p) {
        const char* line = p;
        const char* end = std::strchr(p, '\n');
        if (!end) end = p + std::strlen(p);
        p = *end ? end + 1 : end;
        ++lineNo;
        size_t len = static_cast<size_t>(end - line);
        if (len && line[len - 1] == '\r') --len;
        if (len == 0 || (len >= 2 && line[0] == '/' && line[1] == '/')) continue;

        const unsigned char ch = static_cast<unsigned char>(line[0]);
        if (ch >= 128 || (len >= 2 && line[1] != ' ')) {
            RF_ERR("vector font line %d: expected '<char> <strokes>'", lineNo);
            return false;
        }
        Glyph& glyph = glyphs_[ch];
        glyph.first = static_cast<u16>(segmentCount_);
        glyph.defined = true;

        // Strokes are '|'-separated polylines of two-digit points "xy"; one point is a dot.
        int px = 0, py = 0, points = 0;
        for (size_t i = 2; i <= len;) {
            const char c = i < len ? line[i] : '|';
            if (c == ' ') {
                ++i;
            } else if (c == '|') {
                if (points == 1 && !addSegment(px, py, px, py)) return false;
                points = 0;
                ++i;
            } else if (isDigit(c) && i + 1 < len && isDigit(line[i + 1])) {
                const int x = c - '0';
                const int y = line[i + 1] - '0';
                if (points > 0 && !addSegment(px, py, x, y)) return false;
                px = x;
                py = y;
                ++points;
                i += 2;
            } else {
                RF_ERR("vector font line %d: unexpected '%c'", lineNo, c);
                return false;
            }
        }
        glyph.count = static_cast<u16>(segmentCount_ - glyph.first);
        ++glyphCount_;
    }
    glyphs_[' '].defined = true;  // space has no strokes
    if (segmentCount_ >= countOf(segments_)) {
        RF_ERR("vector font: segment table full");
        return false;
    }
    RF_LOG("vector font: %u glyphs, %u segments", glyphCount_, segmentCount_);
    return glyphCount_ > 0;
}

const VectorFont::Glyph* VectorFont::lookup(char c) const {
    unsigned char u = static_cast<unsigned char>(c);
    if (u < 128 && glyphs_[u].defined) return &glyphs_[u];
    if (u >= 'a' && u <= 'z' && glyphs_[u - 32].defined) return &glyphs_[u - 32];  // lower case maps to upper
    return glyphs_['?'].defined ? &glyphs_['?'] : nullptr;
}

float VectorFont::draw(CapsuleBatch& batch, Vec2 origin, const TextStyle& style, const char* text) const {
    const float unit = style.capHeight / kCapUnits;
    const Stroke stroke{style.weight * 0.5f * unit, style.color, style.intensity, style.skirt};
    float x = origin.x;
    for (const char* c = text; *c; ++c) {
        if (const Glyph* g = lookup(*c)) {
            for (u32 k = 0; k < g->count; ++k) {
                const Segment& s = segments_[g->first + k];
                const Vec2 a{x + s.x0 * unit, origin.y + (kCapUnits - s.y0) * unit};
                const Vec2 b{x + s.x1 * unit, origin.y + (kCapUnits - s.y1) * unit};
                batch.line(a, b, stroke);
            }
        }
        x += kAdvanceUnits * unit;
    }
    return x - origin.x;
}

float VectorFont::drawField(CapsuleBatch& batch, Vec2 origin, float capHeight, float halfWidth, Vec3 top, Vec3 middle,
                            Vec3 bottom, const char* text) const {
    const float unit = capHeight / kCapUnits;
    // y in font units: 6 at the cap (top), 0 at the baseline.
    const auto field = [&](int y) {
        const float u = 1.0f - static_cast<float>(y) / kCapUnits;  // 0 top .. 1 baseline
        return u < 0.5f ? mix(top, middle, u * 2.0f) : mix(middle, bottom, u * 2.0f - 1.0f);
    };
    float x = origin.x;
    for (const char* c = text; *c; ++c) {
        if (const Glyph* g = lookup(*c)) {
            for (u32 k = 0; k < g->count; ++k) {
                const Segment& s = segments_[g->first + k];
                const Vec2 a{x + s.x0 * unit, origin.y + (kCapUnits - s.y0) * unit};
                const Vec2 b{x + s.x1 * unit, origin.y + (kCapUnits - s.y1) * unit};
                batch.line(a, b, halfWidth, field(s.y0), field(s.y1), 0);
            }
        }
        x += kAdvanceUnits * unit;
    }
    return x - origin.x;
}

float VectorFont::measure(const TextStyle& style, const char* text) const {
    return static_cast<float>(std::strlen(text)) * kAdvanceUnits * style.capHeight / kCapUnits;
}

}  // namespace rf
