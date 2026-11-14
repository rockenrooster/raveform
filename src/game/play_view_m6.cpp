// The view of M5 part 6: the bosses of Tracks 7 to 10 (design/bosses/*.md "Parts"). Cosmetic only.

#include <cmath>
#include <initializer_list>

#include "core/args.h"
#include "game/play_view.h"
#include "gfx/vector_font.h"
#include "sim/boss_prose.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
Vec2 dirv(float a) { return {std::cos(a), std::sin(a)}; }
Vec3 whiteV() { return {1.0f, 1.0f, 1.0f}; }
float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
}  // namespace

// ---------------------------------------------------------------------------------------------- Cadence

void PlayView::drawCadence(FrameDraw& d, const ViewInput& in) {
    // cadence.md "Parts": four pillars (a ring of twelve petals, radius 90, the numeral inside, cap 60 wu), each its own hue;
    // sounding: petals spread to 120, the numeral white at 2.6, a pool of its hue; silent: closed, the numeral at 0.8. A thin
    // line in the grid's colour joins them; the side leading to the next chord brightens over the bar.
    const World& w = *in.world;
    const BossState& s = w.boss();
    const CadenceData& C = w.species().cadence;
    const double t = in.music;
    const float now = static_cast<float>(in.time);
    const Vec3 W1 = whiteV();
    const Vec3 hues[4] = {palette_->hue[0], palette_->hue[1], palette_->hue[2], palette_->accent};
    static const char* const kNumerals[4] = {"I", "V", "vi", "IV"};
    const float entrance = clamp01(static_cast<float>((t - s.enterTick) / kTicksPerBar));
    const u32 chord = w.cadenceChord(t);
    const double inBar = t - std::floor(t / kTicksPerBar) * kTicksPerBar;
    const float barU = static_cast<float>(inBar / kTicksPerBar);
    const bool inversion = s.cd.phase == 2;
    Vec2 p[4];
    for (u32 k = 0; k < 4; ++k) p[k] = s.bodyPrev[1 + k] + (s.body[1 + k] - s.bodyPrev[1 + k]) * in.alpha;
    if (!visible(s.pos, C.squareSide) && !s.dying) fightChevron(d, in, s.pos, hues[chord]);
    // The square, the side to the next chord brightening over the bar.
    for (u32 k = 0; k < 4; ++k) {
        const u32 n = (k + 1) % 4;
        const float lit = k == chord ? barU : 0.0f;
        const Vec3 col = palette_->grid * ((0.9f + 1.6f * lit) * entrance);
        d.scene.line(toScene(p[k]), toScene(p[n]), 0.6f * refPx_, col, col, 0);
    }
    for (u32 k = 0; k < 4; ++k) {
        // The death: the four burst in the song's order on the last bar's beats.
        float level = entrance;
        if (s.dying && s.payoutTick >= 0.0) {
            const double burst = s.payoutTick - kTicksPerBar + static_cast<double>(k) * kTicksPerBeat;
            if (t >= burst) continue;
        }
        const bool sounding = k == chord && !s.dying;
        const bool open = inversion ? !sounding : sounding;
        const float petal = sounding ? 120.0f : C.pillarRadius;
        const Vec3 hue = hues[k];
        for (u32 j = 0; j < 12; ++j) {
            const float a = kTau * static_cast<float>(j) / 12.0f + now * (sounding ? 0.6f : 0.15f);
            const Vec2 a0 = p[k] + dirv(a) * (petal * 0.62f), a1 = p[k] + dirv(a + 0.18f) * petal;
            d.scene.line(toScene(a0), toScene(a1), 0.9f * refPx_, hue * (1.8f * level), hue * (1.2f * level), 0);
        }
        if (open) {
            // The open one: a white ring (it can be hurt; x 2 on its One in the first phases).
            const float one = (!inversion && inBar < kTicksPerBeat) ? 1.0f - static_cast<float>(inBar / kTicksPerBeat) : 0.0f;
            const u32 n = 40;
            for (u32 j = 0; j < n; ++j) {
                const Vec2 q0 = p[k] + dirv(kTau * j / n) * (C.pillarRadius * 0.5f), q1 = p[k] + dirv(kTau * (j + 1) / n) * (C.pillarRadius * 0.5f);
                d.scene.line(toScene(q0), toScene(q1), 0.6f * refPx_, W1 * ((1.0f + 1.4f * one) * level), W1 * ((1.0f + 1.4f * one) * level), 0);
            }
        }
        if (sounding) d.pools.glow(toScene(p[k]), 2.2f * C.pillarRadius * pxPerWu_, hue * (0.12f * level));
        const char* text = kNumerals[k];
        const float cap = 60.0f * pxPerWu_ * 0.6f;
        const float wText = font_->measure(TextStyle{cap}, text);
        const Vec2 sc = toScene(p[k]);
        const Vec3 col = sounding ? W1 * (2.6f * level) : hue * (0.8f * level);
        font_->drawField(d.scene, Vec2{sc.x - 0.5f * wText, sc.y - 0.5f * cap}, cap, 1.0f * refPx_, col, col, col, text);
    }
}

}  // namespace rf
