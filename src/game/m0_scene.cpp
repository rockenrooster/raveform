#include "game/m0_scene.h"

#include <cstdio>

#include "gfx/vector_font.h"

namespace rf {

namespace {
// Placeholder palette until the lead's palettes arrive in design/.
constexpr u32 kCyan = rgba(0, 240, 255);
constexpr u32 kMagenta = rgba(255, 43, 214);
constexpr u32 kYellow = rgba(255, 220, 60);
constexpr u32 kWhite = rgba(255, 255, 255);
constexpr u32 kGood = rgba(60, 255, 120);
constexpr u32 kLate = rgba(255, 190, 40);
constexpr u32 kMiss = rgba(255, 60, 60);
constexpr u32 kDim = rgba(90, 110, 140);

void drawFigure(CapsuleBatch& b, const SceneInput& in) {
    const float s = in.scale;
    const Vec2 c{in.width * 0.70f, in.height * 0.52f};
    const float r = 300.0f * s;
    const float a = in.time * 0.35f;
    b.polygon(c, r, 6, a, {3.0f * s, kCyan, 1.2f, 10.0f * s});
    b.polygon(c, r * 0.55f, 3, -a * 1.7f, {3.0f * s, kMagenta, 1.2f, 8.0f * s});
    for (u32 i = 0; i < 6; ++i) {
        const float k = a + kTau * static_cast<float>(i) / 6.0f;
        b.line(c + polar(r * 0.62f, k), c + polar(r * 0.95f, k), {1.5f * s, kCyan, 0.5f, 0.0f});
    }
    // An orbiting dot makes uneven pacing visible on the device.
    b.dot(c + polar(r * 0.8f, in.time * 1.3f), {9.0f * s, kYellow, 1.4f, 14.0f * s});
}

void drawTouches(CapsuleBatch& b, const VectorFont& font, const SceneInput& in) {
    const float s = in.scale;
    TextStyle label;
    label.capHeight = 22.0f * s;
    label.color = kWhite;
    for (u32 i = 0; i < in.touchCount; ++i) {
        const TouchPoint& t = in.touches[i];
        if (!t.active) continue;
        // Pressed state: a bright white ring and a cyan crosshair.
        b.polygon(t.pos, 70.0f * s, 40, 0.0f, {4.0f * s, kWhite, 1.6f, 12.0f * s});
        b.line(t.pos - Vec2{110.0f * s, 0}, t.pos + Vec2{110.0f * s, 0}, {2.0f * s, kCyan, 1.0f, 0.0f});
        b.line(t.pos - Vec2{0, 110.0f * s}, t.pos + Vec2{0, 110.0f * s}, {2.0f * s, kCyan, 1.0f, 0.0f});
        char text[16];
        std::snprintf(text, sizeof text, "T%u", i);
        font.draw(b, t.pos + Vec2{80.0f * s, -100.0f * s}, label, text);
    }
}

void drawPulse(CapsuleBatch& b, const SceneInput& in) {
    constexpr float kLife = 0.5f;
    if (in.pulseAge < 0.0f || in.pulseAge > kLife) return;
    const float k = in.pulseAge / kLife;
    const float s = in.scale;
    b.polygon(in.pulseAt, (40.0f + 420.0f * k) * s, 64, 0.0f, {3.0f * s, kCyan, 1.8f * (1.0f - k), 16.0f * s});
}

void drawFrameGraph(CapsuleBatch& b, const VectorFont& font, const SceneInput& in) {
    if (!in.intervals) return;
    const float s = in.scale;
    const float w = 600.0f * s;
    const float h = 140.0f * s;
    const float x0 = in.insetMin.x + 40.0f * s;
    const float y0 = in.height - in.insetMax.y - 40.0f * s;  // baseline
    const u32 n = in.intervals->size();
    const u32 cap = FrameHistory::capacity();
    const float step = w / static_cast<float>(cap);
    const Stroke frame{1.0f * s, kDim, 0.6f, 0.0f};
    b.line({x0, y0}, {x0 + w, y0}, frame);
    b.line({x0, y0 - h * 0.5f}, {x0 + w, y0 - h * 0.5f}, frame);  // the target interval
    for (u32 i = 0; i < n; ++i) {
        const float ms = in.intervals->at(i);
        float k = ms / (2.0f * in.targetMs);
        if (k > 1.0f) k = 1.0f;
        const u32 color = ms <= in.targetMs * 1.2f ? kGood : (ms <= in.targetMs * 1.5f ? kLate : kMiss);
        const float x = x0 + (static_cast<float>(cap - n + i) + 0.5f) * step;
        b.line({x, y0}, {x, y0 - h * k}, {step * 0.35f, color, 1.0f, 0.0f});
    }
    TextStyle label;
    label.capHeight = 18.0f * s;
    label.color = kDim;
    char text[40];
    std::snprintf(text, sizeof text, "FRAME INTERVAL  TARGET %.2f MS", in.targetMs);
    font.draw(b, {x0, y0 - h - 34.0f * s}, label, text);
}

}  // namespace

void drawM0Scene(CapsuleBatch& batch, const VectorFont& font, const SceneInput& in) {
    drawFigure(batch, in);
    drawPulse(batch, in);
    drawTouches(batch, font, in);
    drawFrameGraph(batch, font, in);

    TextStyle hint;
    hint.capHeight = 24.0f * in.scale;
    hint.color = in.toneHold ? kMagenta : kDim;
    const char* text = in.toneHold ? "TEST TONE: HOLD" : "TAP: TEST TONE";
    const float tw = font.measure(hint, text);
    font.draw(batch, {in.width - in.insetMax.x - tw - 30.0f * in.scale, in.height - in.insetMax.y - 64.0f * in.scale},
              hint, text);
}

}  // namespace rf
