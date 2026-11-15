// The shots on screen (look.md v2.2 §3 streaks; play test 2, look.md §5 "The build on the shots"): a stronger build
// looks stronger. The tail of a shot is always its owner's colour, so it can be told from a teammate's; the head (the
// front 65% of the streak) shows the build: Overcharge changes its colour, width and length with every stack, On-Beat
// Crit puts a four-point star on a beat shot's head, Pierce a small chevron behind the head for each stack, and what a
// Riff throws takes its owner's base colour at levels 1-2, the light colour at 3-4 and a white core with the player's rim
// at 5. Prism's lanes are saturated end to end. The owner, 2026-11-14 ("more visual feedback"): the crit star is bigger with a
// gold glow; Crescendo brightens its owner's shots and warms their tails toward gold as it climbs; Syncopation's off-beat shots
// are accented (a ">" ahead of the head, a doubled tail, a brighter head). Cosmetic only.

#include <cmath>
#include <initializer_list>

#include "core/args.h"
#include "game/play_view.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
Vec3 whiteC() { return {1.0f, 1.0f, 1.0f}; }
Vec2 lerpP(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
Vec2 perpP(Vec2 v) { return {-v.y, v.x}; }

constexpr float kHeadShare = 0.65f;  // look.md 5 (quoted; the lead's review of play test 2: 45% read as a coloured tip)
constexpr float kOverWidth[6] = {1.0f, 1.15f, 1.3f, 1.45f, 1.6f, 1.8f};      // look.md 5 (quoted): width by stack
constexpr float kOverLength = 0.08f;                                         // look.md 5 (quoted): +8% length a stack
constexpr float kPrismWhite = 0.15f, kPrismLevel = 0.6f, kPrismWidth = 1.3f;  // look.md 5 (quoted)
constexpr float kCritStarWu[4] = {0.0f, 16.0f, 22.0f, 28.0f};               // look.md 5: the star by stack (10, 14, 18 until 2026-11-14)
// Chosen (the look bible gives the idea, not the numbers): Overcharge 3's gold rim is this much wider than its white head;
// 5's white core this much narrower than its violet one, its afterglow this many streak lengths behind at this level; a
// Pierce chevron's half width and spacing (wu).
constexpr float kRimWidth = 1.5f, kCoreWidth = 0.45f, kAfterglowLengths = 1.6f, kAfterglowLevel = 0.35f;
// A coloured head is drawn at Prism's 0.6 of the shot level (look.md 5): at the full level the tone map bleaches amber,
// ice blue and violet to white, as it did the first build's Prism fan.
constexpr float kHeadLevel = 0.6f;
constexpr float kChevronHalfWu = 4.0f, kChevronStepWu = 7.0f;
// Chosen (2026-11-14): Crescendo at its cap brightens its owner's shots by half and turns their tails 30% toward gold; Syncopation's
// accent, a ">" this far ahead of the head and this wide, its doubled tail this far either side.
constexpr float kCrescLight = 0.5f, kCrescGold = 0.3f;
constexpr float kAccentAheadWu = 9.0f, kAccentBackWu = 6.0f, kAccentHalfWu = 5.0f, kAccentTailWu = 2.6f;
}  // namespace

void PlayView::drawShots(FrameDraw& d, const ViewInput& in, float a) {
    const World& w = *in.world;
    const Shots& sh = w.shots();
    if (!shown(kMutePlayer)) return;
    const Vec3 W1 = whiteC();
    const Look::ShotStreak& ss = look_->shotStreak;
    const LookLine& pl = look_->lines.playerShot;
    const float headR = look_->soft.shotHead.radius[0] * refPx_, headI = look_->soft.shotHead.intensity[0];
    const u32 me = w.viewPlayer();
    float cresc[kMaxPlayers];
    for (u32 q = 0; q < kMaxPlayers; ++q) cresc[q] = w.player(q).present ? crescendoLevel(w, q) : 0.0f;
    for (u32 i = 0; i < sh.count; ++i) {
        if (sh.dead[i]) continue;
        const u32 owner = sh.owner[i] < kMaxPlayers ? sh.owner[i] : 0u;
        const Loadout& lo = w.player(owner).loadout;
        const u32 over = lo.mods[kOvercharge] > 5 ? 5u : lo.mods[kOvercharge];
        const bool primary = sh.kind[i] == kShotPrimary;
        float len = std::fmax(sh.speed[i] * ss.frameS + ss.overlapWu, ss.minWu);
        if (primary) len *= 1.0f + kOverLength * static_cast<float>(over);
        const Vec2 head = lerpP(Vec2{sh.px[i], sh.py[i]}, Vec2{sh.x[i], sh.y[i]}, a);
        if (!visible(head, len * (over >= 5 ? 1.0f + kAfterglowLengths : 1.0f) + 4.0f)) continue;
        const Vec2 dirv{sh.dx[i], sh.dy[i]};
        const Vec2 tail = head - dirv * len;
        const bool mine = owner == me;
        const float ck = primary ? cresc[owner] : 0.0f;  // Crescendo (2026-11-14)
        const Vec3 base = ck > 0.0f ? mix(palettes_->player[owner], buildGold_, kCrescGold * ck) : palettes_->player[owner], light = palettes_->playerLight[owner];
        // M4 (look.md 6): a teammate's shots at 0.6 of the level and 0.8 of the width. M5: Echo's ghosts at 0.45.
        const float ghost = (sh.mods[i] & kShotGhost) ? 0.45f : 1.0f;
        const float lvl = (mine ? 1.0f : 0.6f) * pl.intensity * ghost * in.runLight * (1.0f + kCrescLight * ck), wid = (mine ? 1.0f : 0.8f) * 0.5f * pl.width * refPx_;
        if (sh.kind[i] == kShotOrb || sh.kind[i] == kShotBomb) {
            // M5: Heavy Step's slow orb (its light colour, a white core, pulsing) and Carousel's bombs (gold).
            const bool orb = sh.kind[i] == kShotOrb;
            const Vec3 c = orb ? light : palettes_->player[3];
            const float r = sh.radius[i] * (orb ? 1.2f : 1.0f) * (1.0f + 0.15f * beat_);
            glyph(d.glyphs, gRing32_, false, head, 0.0f, 0.0f, r, 2.0f * lvl, 0.8f, kGlyphFlat, c, W1);
            d.sparkles.glow(toScene(head), r * 2.2f * pxPerWu_, c * (0.9f * lvl));
            d.scene.line(toScene(tail), toScene(head), wid * 2.0f, c * (0.4f * lvl), c * lvl, 0);
            continue;
        }
        if (sh.kind[i] == kShotMissile || sh.kind[i] == kShotPellet) {
            // M5: Swarm's missiles (a short bright streak, a hot head), Flak's, Shatter's and Confetti's pellets (a fleck).
            const bool missile = sh.kind[i] == kShotMissile;
            const Vec3 c = missile ? light : mix(base, W1, 0.4f);
            const Vec2 t2 = head - dirv * (len * (missile ? 0.6f : 0.35f));
            d.scene.line(toScene(t2), toScene(head), wid * (missile ? 0.9f : 0.6f), c * (0.5f * lvl), W1 * lvl, 0);
            if (missile) d.sparkles.glow(toScene(head), headR * 0.8f, light * (headI * 0.8f * lvl));
            continue;
        }
        if (sh.kind[i] == kShotSentinel) {
            // What a Riff throws (Sentinel's bursts) by its level.
            const u32 lv = lo.riffs[riffIndex(kSentinel)];
            const Vec3 c = lv >= 5 ? W1 : (lv >= 3 ? light : base);
            if (lv >= 5) d.scene.line(toScene(tail), toScene(head), wid * kRimWidth, base * lvl, base * lvl, 0);
            d.scene.line(toScene(tail), toScene(head), wid, c * lvl, mix(c, W1, 0.5f) * lvl, 0);
            d.sparkles.glow(toScene(head), headR * (mine ? 1.0f : 0.8f), c * (headI * (mine ? 1.0f : 0.6f)));
            continue;
        }
        const float wv = wid * (primary ? kOverWidth[over] : 1.0f);
        Vec3 tip;  // the head's colour, for its glow
        // The owner's play of f0624e5 (Fractal "isn't visually clear at all"): a half that a Fork or Fractal split made is drawn
        // as a Prism lane is, turned round the hue wheel: 60 degrees while it can split again, 120 for the last halves.
        const bool forked = mine && primary && (sh.flags[i] & kShotForked) && !shotHasLane(sh.flags[i]);
        if (forked || (mine && shotHasLane(sh.flags[i]))) {
            // Prism: the lane's colour end to end, white only in its front 15%, at 0.6 of the shot level and 1.3 of its
            // width (the tone map bleaches anything brighter, which is why the first build's fan looked pale).
            const Vec3 lane = forked ? laneColour_[sh.forks[i] > 0 ? 3 : 4] : laneColour_[shotLane(sh.flags[i]) + 2];
            const float pv = lvl * kPrismLevel, pw = wv * kPrismWidth;
            const Vec2 cut = head - dirv * (len * kPrismWhite);
            d.scene.line(toScene(tail), toScene(cut), pw, lane * pv, lane * pv, 0);
            d.scene.line(toScene(cut), toScene(head), pw, lane * pv, W1 * pv, 0);
            d.sparkles.glow(toScene(head), headR, lane * (headI * kPrismLevel));
            tip = lane;
        } else {
            // The tail in the owner's colour; the head by Overcharge (white, amber, gold, white with a gold rim, ice blue,
            // electric violet with a white core and an afterglow).
            const Vec3 hc = primary ? overHead_[over] : W1;
            const Vec2 cut = head - dirv * (len * kHeadShare);
            if (primary && over >= 5) {
                const Vec2 back = tail - dirv * (len * kAfterglowLengths);
                d.scene.line(toScene(back), toScene(tail), wv * 0.8f, Vec3{}, hc * (lvl * kAfterglowLevel), 0);
            }
            d.scene.line(toScene(tail), toScene(cut), wv, base * lvl, base * lvl, 0);
            const float hl = lvl * kHeadLevel;
            if (!primary || over == 0) {
                d.scene.line(toScene(cut), toScene(head), wv, base * lvl, W1 * lvl, 0);
            } else if (over == 3) {
                d.scene.line(toScene(cut), toScene(head), wv * kRimWidth, buildGold_ * hl, buildGold_ * hl, 0);
                d.scene.line(toScene(cut), toScene(head), wv * 0.7f, W1 * lvl, W1 * lvl, 0);
            } else {
                d.scene.line(toScene(cut), toScene(head), wv, mix(base, hc, 0.5f) * hl, hc * hl, 0);
                if (over >= 5) d.scene.line(toScene(cut + dirv * (0.3f * len * kHeadShare)), toScene(head), wv * kCoreWidth, W1 * lvl, W1 * lvl, 0);
            }
            d.sparkles.glow(toScene(head), headR * (mine ? 1.0f : 0.8f) * (primary ? kOverWidth[over] : 1.0f),
                            (over == 0 || !primary ? W1 : hc * kHeadLevel) * (headI * (mine ? 1.0f : 0.6f)));
            tip = hc;
        }
        if (!primary) continue;
        // On-Beat Crit: a beat shot carries a four-point star at its head, 10, 14, 18 wu by stack, white to gold.
        if ((sh.flags[i] & kShotCrit) && lo.mods[kOnBeatCrit]) {
            const u32 cs = lo.mods[kOnBeatCrit] > 3 ? 3u : lo.mods[kOnBeatCrit];
            const Vec3 sc = mix(W1, buildGold_, 0.5f * static_cast<float>(cs - 1)) * (1.8f * (mine ? 1.0f : 0.6f));
            flare(d.scene, head, angleOf(dirv) + 0.25f * kPi, kCritStarWu[cs], 2, sc, 0.55f * (mine ? 1.0f : 0.8f));
            d.sparkles.glow(toScene(head), (8.0f + 3.0f * static_cast<float>(cs)) * refPx_, buildGold_ * (mine ? 1.2f : 0.7f));
        }
        // Syncopation (2026-11-14): an off-beat shot is accented: a ">" ahead of its head, a doubled tail, a brighter head.
        if (sh.accent[i]) {
            const float al = mine ? 1.0f : 0.6f;
            const Vec3 ac = mix(light, W1, 0.35f) * (1.7f * al);
            const Vec2 side = perpP(dirv);
            const Vec2 tipA = head + dirv * kAccentAheadWu, backA = tipA - dirv * kAccentBackWu;
            d.scene.line(toScene(backA + side * kAccentHalfWu), toScene(tipA), 0.7f * refPx_, ac, ac, 0);
            d.scene.line(toScene(backA - side * kAccentHalfWu), toScene(tipA), 0.7f * refPx_, ac, ac, 0);
            const Vec2 cut2 = head - dirv * (len * kHeadShare);
            for (float sg : {-1.0f, 1.0f}) {
                const Vec2 off = side * (kAccentTailWu * sg);
                d.scene.line(toScene(tail + off), toScene(cut2 + off), 0.5f * refPx_, light * (0.2f * lvl), light * (0.8f * lvl), 0);
            }
            d.sparkles.glow(toScene(head), headR * 1.5f, light * (headI * 0.9f * al));
        }
        // Pierce: one small chevron behind the head for each stack, pointing along the shot.
        if (const u32 ps = lo.mods[kPierce]) {
            const Vec2 side = perpP(dirv) * kChevronHalfWu;
            const Vec3 cc = mix(tip, W1, 0.3f) * (lvl * 0.9f);
            for (u32 c = 0; c < ps && c < 3; ++c) {
                const Vec2 at = head - dirv * (len * kHeadShare + kChevronStepWu * static_cast<float>(c + 1));
                const Vec2 back = at - dirv * kChevronHalfWu;
                d.scene.line(toScene(back + side), toScene(at), 0.6f * wid, cc, cc, 0);
                d.scene.line(toScene(back - side), toScene(at), 0.6f * wid, cc, cc, 0);
            }
        }
    }
}

}  // namespace rf
