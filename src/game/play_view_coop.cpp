// M4 co-op in the view (design/look.md section 6 "Co-op (M4)"): the teammates' ships in their own colour sets at
// 0.7 of the ship's level with their pool at half (the local ship alone is on the priority layer), a 44 px
// chevron in their colour for one off screen, and a downed player's beacon: a dashed ring of 90 wu in its colour,
// the hull inside at a third of its size turning once every two bars, a thin vertical flare that swells on the
// beat, the revive filling as a solid arc while a teammate stands in the ring, and the close.

#include <cmath>

#include "game/play_view.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
constexpr float kMateLevel = 0.7f;         // look.md 6 (quoted): other players' ships at 0.7 of the ship's level
constexpr float kMatePool = 0.5f;          // ... their pool of light at half
constexpr float kBeaconRing = 1.2f;        // the dashed ring's level (quoted)
constexpr float kBeaconHullScale = 1.0f / 3.0f, kBeaconHull = 0.4f;  // the hull inside: a third of its size, at 0.4 (quoted)
constexpr float kBeaconFlarePx = 300.0f, kBeaconFlare = 0.8f, kBeaconFlareSwell = 0.2f;  // quoted
constexpr float kCloseFlashS = 0.12f, kCloseFlash = 0.9f;  // the ring's white flash as it closes (quoted)
constexpr u32 kBeaconDashes = 24;          // chosen: the lobby's open seat draws 24 dashes too (screens.py)
constexpr float kLeaveFadeS = 1.0f;        // chosen (stage B): a player that left fades over a second
Vec2 lerp2(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
float lerpAngle(float a, float b, float t) { return a + wrapAngle(b - a) * t; }
}  // namespace

void PlayView::drawCoop(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    const float t = static_cast<float>(in.time);
    const float a = in.alpha;
    const u32 me = w.viewPlayer();
    const Vec3 W1{1.0f, 1.0f, 1.0f};
    const float shipScale = 14.0f * glyphs_->def(static_cast<u32>(gShip_)).size;
    const LookLine& hull = look_->lines.playerHull;
    const LookLine& halo = look_->lines.playerHalo;
    const float eighthS = 30.0f / std::fmax(in.bpm, 30.0f);
    const float ringR = tuning::kReviveRingWu;
    const auto arc = [&](Vec2 c, float r, float u0, float u1, u32 segs, float widthRef, Vec3 col) {
        // A solid arc clockwise from twelve o'clock (the screen's y points down), u in turns.
        for (u32 k = 0; k < segs; ++k) {
            const float s0 = u0 + (u1 - u0) * static_cast<float>(k) / static_cast<float>(segs);
            const float s1 = u0 + (u1 - u0) * static_cast<float>(k + 1) / static_cast<float>(segs);
            const float a0 = -0.5f * kPi + s0 * kTau, a1 = -0.5f * kPi + s1 * kTau;
            d.scene.line(toScene(c + Vec2{std::cos(a0), std::sin(a0)} * r), toScene(c + Vec2{std::cos(a1), std::sin(a1)} * r),
                         widthRef * refPx_, col, col, 0);
        }
    };

    for (u32 q = 0; q < kMaxPlayers; ++q) {
        const PlayerState& pl = w.player(q);
        if (!pl.present) {
            mateTrailCount_[q] = 0;
            // M4 stage B: a player that left: its ship fades where it was over a second (its seat opened).
            const float age = t - leaveT_[q];
            if (age >= 0.0f && age < kLeaveFadeS && q != me) {
                const float f = 1.0f - age / kLeaveFadeS;
                const Vec3 c = palettes_->player[q], cd = palettes_->playerDeep[q];
                glyph(d.glyphs, gShip_, false, leavePos_[q], leaveHeading_[q], leaveHeading_[q], shipScale * (1.0f + 0.3f * age),
                      hull.intensity * kMateLevel * f, 0.5f * hull.width, kGlyphPlayer, c, cd);
            }
            continue;
        }
        const Vec3 c = palettes_->player[q], cd = palettes_->playerDeep[q], cl = palettes_->playerLight[q];

        if (pl.down) {
            mateTrailCount_[q] = 0;
            const Vec2 B = pl.beacon;
            if (!visible(B, ringR + 40.0f)) {
                // Off screen: its chevron carries a small ring at the tip and pulses on every beat.
                edgeChevron(d, B, c, false, 1.0f + 0.2f * kick_, 1);
                continue;
            }
            // The dashed ring.
            for (u32 k = 0; k < kBeaconDashes; ++k) {
                const float u0 = static_cast<float>(k) / static_cast<float>(kBeaconDashes);
                arc(B, ringR, u0, u0 + 0.5f / static_cast<float>(kBeaconDashes), 2, 0.5f * halo.width, c * kBeaconRing);
            }
            // The revive filling: a solid arc clockwise from twelve o'clock, and a line from the reviver.
            const float fill = clampf(static_cast<float>(pl.reviveTicks / (tuning::kReviveBeats * kTicksPerBeat)), 0.0f, 1.0f);
            if (fill > 0.0f) {
                arc(B, ringR, 0.0f, fill, 1 + static_cast<u32>(fill * 64.0f), 0.9f * halo.width, mix(c, W1, 0.3f) * 2.0f);
                i32 reviver = -1;
                float best = ringR * ringR;
                for (u32 o = 0; o < kMaxPlayers; ++o) {
                    const PlayerState& op = w.player(o);
                    if (o == q || !op.present || !op.alive) continue;
                    const float d2 = lengthSq(op.pos - B);
                    if (d2 <= best) {
                        best = d2;
                        reviver = static_cast<i32>(o);
                    }
                }
                if (reviver >= 0) {
                    const PlayerState& rp = w.player(static_cast<u32>(reviver));
                    const Vec2 from = static_cast<u32>(reviver) == me ? in.shipPos : lerp2(rp.prevPos, rp.pos, a);
                    d.scene.line(toScene(from), toScene(B), 0.6f * refPx_, c * 1.2f, c * 1.6f, 0);
                }
            }
            // The hull inside: a third of its size, at 0.4, turning once every two bars.
            const float turn = static_cast<float>(std::fmod(in.music, 2.0 * kTicksPerBar) / (2.0 * kTicksPerBar)) * kTau;
            glyph(d.glyphs, gShip_, false, B, turn, turn, shipScale * kBeaconHullScale, hull.intensity * kBeaconHull, 0.5f * hull.width,
                  kGlyphPlayer, c, cd);
            // A thin vertical flare, 300 px tall, swelling 20% on every beat, so it is found across the frame.
            const Vec2 sb = toScene(B);
            const float half = 0.5f * kBeaconFlarePx * refPx_ * (1.0f + kBeaconFlareSwell * kick_);
            d.scene.line(Vec2{sb.x, sb.y - half}, sb, 0.6f * refPx_, Vec3{}, c * kBeaconFlare, 0);
            d.scene.line(sb, Vec2{sb.x, sb.y + half}, 0.6f * refPx_, c * kBeaconFlare, Vec3{}, 0);
            d.pools.glow(sb, look_->soft.playerPool.radius[0] * pxPerWu_, c * (look_->soft.playerPool.intensity[0] * kMatePool));
            continue;
        }

        // The ring's close: white at 0.9 for 0.12 s (a revive by a teammate or at the Break).
        const float closeAge = t - reviveT_[q];
        if (closeAge >= 0.0f && closeAge < kCloseFlashS) arc(pl.pos, ringR, 0.0f, 1.0f, 64, 0.9f * halo.width, W1 * kCloseFlash);

        if (q == me || !pl.alive) continue;
        const Vec2 P = lerp2(pl.prevPos, pl.pos, a);
        // The teammate's trail (the view clock), as the local ship's.
        {
            u32& head = mateTrailHead_[q];
            mateTrailPos_[q][head] = P;
            mateTrailT_[q][head] = t;
            head = (head + 1) % kTrail;
            if (mateTrailCount_[q] < kTrail) ++mateTrailCount_[q];
        }
        if (!visible(P, 2.0f * shipScale)) {
            edgeChevron(d, P, c, false, 1.0f);  // look.md 6: a 44 px chevron in their colour, on the dark disc
            continue;
        }
        const bool blink = closeAge >= 0.0f && closeAge < 8.0f * eighthS && std::fmod(closeAge, 2.0f * eighthS) >= eighthS;
        if (blink) continue;
        const float h = lerpAngle(pl.prevHeading, pl.heading, a);
        glyph(d.glyphs, gShip_, false, P, h, h, shipScale, hull.intensity * kMateLevel, 0.5f * hull.width, kGlyphPlayer, c, cd);
        glyph(d.glyphs, gShip_, true, P, h, h, shipScale, look_->lines.playerWhiteCopy.intensity * kMateLevel,
              0.5f * look_->lines.playerWhiteCopy.width, kGlyphWhite, W1, W1);
        const u32 cells = pl.cells > 8 ? 8u : pl.cells;  // play test 2: up to eight arcs
        if (cells > 0) {
            const float rot = 0.3f * t;
            const Vec3 hc = cells == 1 ? palettes_->danger : c;
            glyph(d.glyphs, gHalo_[cells], false, P, rot, rot, shipScale, halo.intensity * kMateLevel * (1.0f + 0.25f * kick_),
                  0.5f * halo.width, cells == 1 ? kGlyphFlat : kGlyphRingA, hc, cells == 1 ? hc : cl);
        }
        d.pools.glow(toScene(P), look_->soft.playerPool.radius[0] * pxPerWu_, c * (look_->soft.playerPool.intensity[0] * kMatePool));
        // Trail: the last 100 wu of path in dashes, white -> its colour -> deep, fading, at the teammate's level.
        const LookLine& tr = look_->lines.playerTrail;
        Vec2 prev = P;
        float walked = 0.0f;
        const u32 n = mateTrailCount_[q];
        for (u32 k = 1; k < n && walked < 100.0f; ++k) {
            const Vec2 p = mateTrailPos_[q][(mateTrailHead_[q] + kTrail - 1 - k) % kTrail];
            const float seg = length(p - prev);
            if (seg < 1e-3f) continue;
            const float u0 = walked / 100.0f, u1 = std::fmin((walked + seg) / 100.0f, 1.0f);
            const auto field = [&](float u) {
                const float f = clampf(u, 0.0f, 1.0f) * 2.0f;
                const Vec3 col = f < 1.0f ? mix(W1, c, f) : mix(c, cd, f - 1.0f);
                return col * (tr.intensity * kMateLevel * (1.0f - u));
            };
            d.scene.line(toScene(prev), toScene(lerp2(prev, p, 0.6f)), 0.5f * tr.width * refPx_, field(u0), field(u0 + 0.6f * (u1 - u0)), 0);
            walked += seg;
            prev = p;
        }
    }
}

}  // namespace rf
