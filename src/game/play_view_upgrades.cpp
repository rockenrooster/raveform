// M5 part 3 (GAME_DESIGN 6.2-6.6: every upgrade visible within a bar of the pick): the new Mods, Riffs, Boosts and
// Fusions on screen, each in its player's colours (look.md 6; a teammate's at 0.6). Lines (Accent's lances, the Lancer,
// Downbeat Cannon, Bar Line, Beam, the Sweeper's arms) are capsules; bodies (wingmen, the decoy, mines, comets, bombs,
// wells) are glyphs; lightning is a jagged line that fades in 0.15 s. Cosmetic only.

#include <cmath>
#include <initializer_list>

#include "core/args.h"
#include "game/play_view.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
Vec3 whiteU() { return {1.0f, 1.0f, 1.0f}; }
Vec2 dirU(float a) { return {std::cos(a), std::sin(a)}; }
Vec2 perpU(Vec2 v) { return {-v.y, v.x}; }
float clampU(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
const Vec3 kIce{0.55f, 0.85f, 1.0f};
const Vec3 kFire{1.0f, 0.42f, 0.08f};
constexpr float kBoltS = 0.09f;       // look.md 5: a zap's 90 ms
constexpr float kBeatPeakS = 2.0f / 60.0f, kBeatFallS = 0.110f;  // look.md 5 (a beam on a beat), 7
// A held beam's core moves between 1.0 and 1.2 on the 1/32 grid (look.md v2.5 says 1.0-1.4; the lead's checkpoint after part 4:
// the core's bloom widened the Beam Boost's haze, so 1.2).
constexpr float kHeldCoreSwing = 0.2f;
}  // namespace

void PlayView::drawBeam(FrameDraw& d, Vec2 from, Vec2 dir, float len, float w, float corePx, Vec3 noseCol, Vec3 farCol, Vec3 glowCol,
                        float coreLevel, float glowLevel) {
    // look.md v2.5 ("Beams, lightning and areas"): a beam is a laser, one thin line and its glow. The core, the only thing
    // with an edge, on the axis (2, 3 or 4 px), from the nose's colour to the far end's; the glow a Gaussian strip of the
    // owner's colour, 0.25 on the axis, sigma w / 5, nothing left at +-w/2, drawn with the pools (round Gaussians a sigma
    // wide, two sigmas apart: their sum along the axis is the strip), coming up over the first 20 wu. Under 12 wu: the core
    // alone. No edge lines, no fill.
    if (coreLevel > 0.0f) d.scene.line(toScene(from), toScene(from + dir * len), 0.5f * corePx * refPx_, noseCol * coreLevel, farCol * coreLevel, 0);
    if (glowLevel <= 0.0f || w < 12.0f) return;
    const float sigma = w / 5.0f;
    const float step = 2.0f * sigma;
    const float radius = sigma * look_->glowRadiusOverSigma;
    // (a disc's peak c gives c x sigma x sqrt(2 pi) / step on the axis)
    const float c = glowLevel * step / (sigma * 2.5066283f);
    // look.md §3: soft light under 16 ref px is a sparkle (full resolution), from 16 up a pool. The checkpoint after part 4
    // measured the Beam Boost's glow (w 20 wu, sigma 4 wu) drawn as quarter-resolution pools at 25 wu full width at half
    // maximum, the haze three times its width; as sparkles it keeps its sigma.
    GlowBatch& soft = radius * pxPerWu_ < 16.0f * refPx_ ? d.sparkles : d.pools;
    for (float u = 0.0f; u <= len; u += step) {
        const Vec2 p = from + dir * u;
        if (!visible(p, radius)) continue;
        const float rise = u < 20.0f ? u / 20.0f : 1.0f;
        soft.glow(toScene(p), radius * pxPerWu_, glowCol * (c * rise));
    }
}
void PlayView::lethalBeam(FrameDraw& d, Vec2 a, Vec2 b, float killWu, float level) {
    // look.md v2.5 ("A lethal beam is a laser too"): danger red on the priority layer, its capsule exactly as wide as it kills
    // (a threat's picture is its hit box), a white-pink line of 2 px down its axis; round it a glow of the danger colour drawn
    // as the players' is, 0.25 on the axis (reduce flashing 0.15), sigma twice the kill width, that hurts nobody.
    const Vec3 danger = palettes_->danger, W1 = whiteU();
    const Vec3 pink = mix(danger, W1, 0.7f);
    const float len = length(b - a);
    if (len < 1e-3f) return;
    const Vec2 dir = (b - a) * (1.0f / len);
    if (shown(kMuteBullet)) {
        d.scene.line(toScene(a), toScene(b), refPx_, pink * (1.6f * level), pink * (1.6f * level), 0);
        const float sigma = 2.0f * killWu;
        const float step = 2.0f * sigma;
        const float radius = sigma * look_->glowRadiusOverSigma;
        const float c = (reduceFlashing_ ? 0.15f : 0.25f) * level * step / (sigma * 2.5066283f);
        for (float u = 0.0f; u <= len; u += step) {
            const Vec2 p = a + dir * u;
            if (visible(p, radius)) d.pools.glow(toScene(p), radius * pxPerWu_, danger * c);
        }
    }
    if (!shown(kMutePriority)) return;
    const float hw = 0.5f * killWu * pxPerWu_;
    d.threatRims.line(toScreen(a), toScreen(b), hw + look_->threat.telegraphRim * refPx_, Vec3{}, Vec3{}, 0);
    d.threatCores.line(toScreen(a), toScreen(b), hw, danger * level, danger * level, 0);
    d.threatCores.line(toScreen(a), toScreen(b), refPx_, pink * level, pink * level, 0);
}

void PlayView::eventUpgrade(const World& w, const Event& e, float t) {
    const u32 q = e.a < kMaxPlayers ? e.a : w.viewPlayer();
    const float lvl = q == w.viewPlayer() ? 1.0f : 0.6f;
    const Vec3 player = palettes_->player[q] * lvl, light = palettes_->playerLight[q] * lvl, W1 = whiteU() * lvl;
    const Vec2 at{e.x, e.y};
    switch (e.b) {
        case kFxArc: {
            Bolt& b = bolts_[boltHead_];
            boltHead_ = (boltHead_ + 1u) % kBolts;
            b.a = at;
            b.b = Vec2{e.x2, e.y2};
            b.born = t;
            b.owner = static_cast<u8>(q);
            b.style = static_cast<u8>(e.f + 0.5f);
            break;
        }
        case kFxBlast:
            // (look.md 5: what a Boost draws is gold: Supernova's and the Carousel's blasts)
            addShockRings(at, 1, e.f, e.c == 1 ? light : ((e.c == 2 || e.c == 3) ? palettes_->player[3] * lvl : player), W1, t, e.c == 1 ? 0.3f : 0.18f);
            if (e.c == 1) addPool(at, 10.0f, e.f, light, 1.2f, 0.0f, 0.3f, false, 1, false, false, t);
            break;
        case kFxMineBlow:
        case kFxWellBlow:
        case kFxShockstep:
        case kFxLastLight:
        case kFxHoleBlow: {
            const bool big = e.b == kFxHoleBlow || e.b == kFxLastLight;
            addShockRings(at, big ? 2 : 1, e.f, e.b == kFxHoleBlow ? palettes_->player[3] : player, W1, t, big ? 0.5f : 0.3f);
            addPool(at, 20.0f, e.f, big ? W1 : light, big ? 1.6f : 1.0f, 0.0f, big ? 0.5f : 0.3f, false, 1, false, false, t);
            if (e.b == kFxHoleBlow) {
                addDistortion(at, e.f * 1.6f, 0.5f, 22.0f, 24.0f, t);
                shakeAmp_ = std::fmax(shakeAmp_, 4.0f);
            }
            break;
        }
        case kFxShatterFrozen: {
            SparkReq r{};
            r.at = at;
            r.count = 10;
            r.kind = 0;
            r.a = kIce;
            r.b = whiteU();
            r.t0 = t;
            sparkRequest(r);
            break;
        }
        case kFxFreeze: addShockRings(at, 2, 900.0f, kIce, whiteU(), t, 0.5f); break;
        case kFxCollector: addShockRings(at, 1, 160.0f, palettes_->player[3], whiteU(), t, 0.3f); break;
        case kFxFork:
        case kFxConfetti: {
            SparkReq r{};
            r.at = at;
            r.count = e.b == kFxFork ? 3 : 12;
            r.kind = 0;
            r.a = e.b == kFxConfetti ? palettes_->player[3] : light;
            r.b = W1;
            r.t0 = t;
            sparkRequest(r);
            break;
        }
        default: break;
    }
}

void PlayView::drawUpgrades(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    const float t = static_cast<float>(in.time);
    const float a = in.alpha;
    const Vec3 W1 = whiteU();
    const double tps = static_cast<double>(w.ticksPerSecond());
    const float shipScale = 14.0f * glyphs_->def(static_cast<u32>(gShip_)).size;

    for (u32 q = 0; q < kMaxPlayers; ++q) {
        const PlayerState& pq = w.player(q);
        if (!pq.present || (q != w.viewPlayer() && w.playerCount() < 2)) continue;
        const bool local = q == w.viewPlayer();
        const float lvl = local ? 1.0f : 0.6f;
        const Vec3 player = palettes_->player[q] * lvl, light = palettes_->playerLight[q] * lvl, Wq = W1 * lvl;
        const Vec3 gold = palettes_->player[3] * lvl;
        const Vec2 ship = local ? in.shipPos : pq.prevPos + (pq.pos - pq.prevPos) * a;
        const Loadout& l = pq.loadout;

        // Beams (look.md v2.5): Accent's lances, the Lancer's, Downbeat Cannon, Bar Line's rays: on a beat, at their peak for
        // two frames and gone over 110 ms, the glow falling with the core; Standing Wave's held for a bar (a steady glow, the
        // core between 1.0 and 1.4 on the 1/32 grid). The core 70% white at the nose running to the owner's light colour; a
        // Fusion's beam (Downbeat Cannon, Standing Wave) a gold glow and a core half white, half the owner's colour. Reduce
        // flashing: a beat beam peaks at 1.0, no white in its core, its glow 0.15.
        const float shotLvl = look_->lines.playerShot.intensity * lvl;
        const Vec3 own = palettes_->player[q], ownLight = palettes_->playerLight[q];
        for (const PlayerState::Lance& ln : pq.lances) {
            const float age = static_cast<float>((in.music - ln.tick) / tps);
            if (age < 0.0f) continue;
            const bool fusion = ln.kind == kLanceCannon || ln.standing;
            const Vec3 glowCol = fusion ? palettes_->player[3] : own;
            const Vec3 nose = reduceFlashing_ ? own : mix(own, W1, fusion ? 0.5f : 0.7f);
            const float corePx = ln.kind == kLanceCannon ? 4.0f : (ln.kind == kLanceLancer ? 3.0f : 2.0f);
            const float peak = reduceFlashing_ ? 1.0f : (ln.kind == kLanceCannon ? 2.0f : 1.6f);
            const float glowPeak = reduceFlashing_ ? 0.15f : 0.25f;
            float gl = 0.0f, cl = 0.0f;
            if (ln.standing) {
                if (age > static_cast<float>(kTicksPerBar / tps)) continue;
                const u32 step32 = static_cast<u32>(std::floor(in.music / (kTicksPerStep * 0.5)));
                gl = glowPeak * lvl;
                cl = shotLvl * (1.0f + kHeldCoreSwing * static_cast<float>((step32 * 2654435761u >> 28) & 15u) / 15.0f);
            } else {
                const float e = age <= kBeatPeakS ? 1.0f : clampU(1.0f - (age - kBeatPeakS) / kBeatFallS);
                if (e <= 0.0f) continue;
                gl = glowPeak * lvl * e;
                cl = shotLvl * peak * e;
            }
            drawBeam(d, ln.from, ln.dir, ln.length, 2.0f * ln.half, corePx, nose, ownLight * lvl, glowCol, cl, gl);
        }
        if (!pq.alive) continue;
        const Vec2 aim = local ? in.shipAim : normalizeOr(pq.aim, dirU(pq.heading));

        // The Lancer charging: a thin line along the aim that brightens over the three beats before it fires.
        if (const u8 L = l.riffs[riffIndex(kLancer)]) {
            const double beatPos = std::fmod(in.music, kTicksPerBar) / kTicksPerBeat;  // 0..4 in the bar
            const double every = (pq.boost(kHyper) ? 1.0 : (L >= 3 ? 2.0 : 4.0));
            const float charge = static_cast<float>(std::fmod(beatPos + 1.0, every) / every);
            const Vec3 c = light * (0.15f + 0.5f * charge * charge);
            d.scene.line(toScene(ship), toScene(ship + aim * tuning::kLancerLengthWu), 0.8f * refPx_, c, c * 0.1f, 0);
        }

        // Beam: the primary as a held laser while it fires (look.md 5: edges and fill steady, the core on the 1/32 grid).
        if (pq.boost(kBeam) && (in.music - pq.beamTick) < 2.0 * w.leadGrid(q) + 2.0) {
            const float half = tuning::kBeamHalfWu + 4.0f * static_cast<float>(pq.beamLanes > 1u ? pq.beamLanes - 1u : 0u);  // (as World::beamVolley)
            const float shotLvl = look_->lines.playerShot.intensity * lvl;
            const u32 step32 = static_cast<u32>(std::floor(in.music / (kTicksPerStep * 0.5)));
            const float cl = shotLvl * (1.0f + kHeldCoreSwing * static_cast<float>((step32 * 2654435761u >> 28) & 15u) / 15.0f);
            const Vec3 own = palettes_->player[q];
            drawBeam(d, ship + aim * tuning::kMuzzleWu, aim, tuning::kBeamLengthWu - tuning::kMuzzleWu, 2.0f * half, 2.0f,
                     reduceFlashing_ ? own : mix(own, W1, 0.7f), palettes_->playerLight[q] * lvl, own, cl, (reduceFlashing_ ? 0.15f : 0.25f) * lvl);
        }

        // Wingmen (Mirror, Quartet) and Chorus's ghosts: the ship's glyph, smaller and dimmer, in formation.
        {
            Vec2 wp[5], wd[5];
            float ws[5];
            const u32 n = w.wingmen(q, wp, wd, ws, 5);
            const Vec2 off = ship - pq.pos;  // the drawn ship's place, not the tick's
            for (u32 k = 0; k < n; ++k) {
                const float h = angleOf(wd[k]);
                const float s = ws[k] >= 1.0f && !l.riffs[riffIndex(kMirror)] ? 0.85f : 0.65f;
                glyph(d.glyphs, gShip_, false, wp[k] + off, h, h, shipScale * s, 1.4f * lvl, 0.5f, kGlyphFlat, light, player);
                d.sparkles.glow(toScene(wp[k] + off), 8.0f * refPx_, player * 0.5f);
            }
        }

        // Aegis: the arc, turning once a bar.
        if (const u8 st = l.mods[kAegis]) {
            const float half = 0.5f * tuning::kAegisArcDeg[st < 3 ? st : 3] * (kTau / 360.0f);
            const float c0 = w.aegisAngle(q), r = 0.5f * (tuning::kAegisInWu + tuning::kAegisOutWu);
            const u32 segs = 10;
            for (u32 s = 0; s < segs; ++s) {
                const float u0 = c0 - half + 2.0f * half * static_cast<float>(s) / segs, u1 = c0 - half + 2.0f * half * static_cast<float>(s + 1) / segs;
                d.scene.line(toScene(ship + dirU(u0) * r), toScene(ship + dirU(u1) * r), 1.1f * refPx_, light * 1.6f, Wq * 1.2f, 0);
            }
        }

        // Afterimage's decoy: a flickering ghost of the ship where the Dash began.
        if (pq.decoyUntil > in.music) {
            const float left = clampU(static_cast<float>((pq.decoyUntil - in.music) / kTicksPerBeat));
            const float flick = 0.6f + 0.4f * std::sin(t * 30.0f);
            glyph(d.glyphs, gShip_, false, pq.decoyAt, pq.heading, pq.heading, shipScale, 1.2f * flick * (0.4f + 0.6f * left) * lvl, 0.5f,
                  kGlyphFlat, light, player);
        }

        // Minelayer's mines: a small ring with a cross, blinking faster toward the bar line.
        for (const PlayerState::Mine& m : pq.mines) {
            if (m.blowTick < 0.0) continue;
            const float left = static_cast<float>((m.blowTick - in.music) / kTicksPerBar);
            const float blink = 0.5f + 0.5f * std::cos(t * (6.0f + 30.0f * (1.0f - clampU(left))));
            glyph(d.glyphs, gRing32_, false, m.at, 0.0f, 0.0f, 10.0f, 1.4f, 0.6f, kGlyphFlat, light, player);
            const Vec3 c = mix(player, Wq, blink) * 1.5f;
            d.scene.line(toScene(m.at + Vec2{-6.0f, 0.0f}), toScene(m.at + Vec2{6.0f, 0.0f}), 1.0f * refPx_, c, c, 0);
            d.scene.line(toScene(m.at + Vec2{0.0f, -6.0f}), toScene(m.at + Vec2{0.0f, 6.0f}), 1.0f * refPx_, c, c, 0);
            if (l.fused(kCollapseField)) glyph(d.glyphs, gRing128_, false, m.at, 0.0f, 0.0f, tuning::kCollapsePullWu * (0.6f + 0.4f * clampU(left)), 0.5f, 0.4f, kGlyphRingW, player, player);
        }

        // Comet's discs.
        for (u32 k = 0; k < PlayerState::kComets; ++k) {
            Vec2 at;
            float r = 0.0f;
            if (!w.cometAt(q, k, at, r)) continue;
            glyph(d.glyphs, gRing32_, false, at, t * 12.0f, t * 12.0f, r, 2.0f * lvl, 0.9f, kGlyphFlat, light, Wq);
            d.sparkles.glow(toScene(at), r * pxPerWu_ * 1.6f, light * 0.6f);
        }

        // The Sweeper's arm (two from level 3).
        if (const u8 L = l.riffs[riffIndex(kSweeper)]) {
            const float len = tuning::kSweeperLengthWu[L < 5 ? L : 5];
            const float a0 = w.sweeperAngle(q);
            for (u32 arm = 0; arm < (L >= 3 ? 2u : 1u); ++arm) {
                const Vec2 d0 = dirU(a0 + static_cast<float>(arm) * kPi);
                d.scene.line(toScene(ship), toScene(ship + d0 * len), 1.1f * refPx_, player * 1.2f, light * 1.6f, 0);
                d.sparkles.glow(toScene(ship + d0 * len), 7.0f * refPx_, light * 0.8f);
            }
        }

        // Singularity's well and Black Hole's singularity: rings drawn inward, a dark core.
        const auto well = [&](Vec2 at, float r, float from, float to, Vec3 c) {
            // An area is drawn by its edge (look.md 5); the pull shows in the lattice and two faint rings falling inward.
            const float span = to - from;
            const float u = span > 1.0f ? clampU((static_cast<float>(in.music) - from) / span) : 0.0f;
            glyph(d.glyphs, gRing128_, false, at, 0.0f, 0.0f, r, 1.2f, 0.6f, kGlyphRingW, c, Wq);
            for (u32 k = 0; k < 2; ++k) {
                const float ph = std::fmod(t * 1.5f + static_cast<float>(k) * 0.5f, 1.0f);
                glyph(d.glyphs, gRing128_, false, at, 0.0f, 0.0f, r * (1.0f - ph), 0.35f * ph * (0.5f + u), 0.4f, kGlyphRingW, c, c);
            }
        };
        if (const u8 L = l.riffs[riffIndex(kSingularity)]; L && pq.wellFrom >= 0.0 && in.music >= pq.wellFrom)
            well(pq.wellAt, tuning::kWellPullWu[L < 5 ? L : 5], static_cast<float>(pq.wellFrom), static_cast<float>(pq.wellBlow), player);
        if (pq.holeFrom >= 0.0) well(pq.holeAt, tuning::kHolePullWu, static_cast<float>(pq.holeFrom), static_cast<float>(pq.holeBlow), gold);

        // Carousel's six bombs.
        if (pq.boost(kCarousel)) {
            const Vec2 off = ship - pq.pos;
            for (u32 k = 0; k < 6; ++k) {
                const Vec2 b = w.carouselBomb(q, k) + off;
                glyph(d.glyphs, gRing32_, false, b, 0.0f, 0.0f, 7.0f, 1.8f, 0.8f, kGlyphFlat, gold, Wq);
                d.sparkles.glow(toScene(b), 6.0f * refPx_, gold * 0.7f);
            }
        }

        // Colossus: the giant's outline, three times the ship.
        if (pq.boost(kColossus)) {
            glyph(d.glyphs, gShip_, false, ship, pq.heading, pq.heading, shipScale * tuning::kColossusScale, 1.2f * lvl, 0.9f, kGlyphFlat, gold, Wq);
            d.pools.glow(toScene(ship), 60.0f * pxPerWu_, gold * 0.25f);
        }

        // Kick Pulse's ring and Shockstep's.
        const auto ring = [&](double when, Vec2 at, float r, float lifeS, Vec3 c) {
            const float age = static_cast<float>((in.music - when) / tps);
            if (age < 0.0f || age > lifeS || r <= 0.0f) return;
            const float u = age / lifeS;
            glyph(d.glyphs, gRing128_, false, at, 0.0f, 0.0f, r * (0.3f + 0.7f * u), 1.6f * (1.0f - u), 0.7f, kGlyphRingW, c, Wq);
        };
        ring(pq.kickTick, ship, pq.kickR, 0.2f, player);
        ring(pq.shockTick, pq.shockAt, pq.shockR, 0.25f, light);
    }

    // Lightning (look.md 5, one recipe for Arc, Storm Coil, Chain Storm and Tesla Bloom): three strands of 1.2 px between the
    // two bodies, each a polyline of pieces 18 to 30 wu pushed up to 10 wu aside, drawn afresh every frame for the zap's 90
    // ms; the middle strand white, the outer two the owner's colour (gold for Chain Storm, a Boost); a sparkle at each end.
    for (const Bolt& b : bolts_) {
        const float age = t - b.born;
        if (age < 0.0f || age > kBoltS) continue;
        const float lvl = (b.owner == w.viewPlayer() ? 1.0f : 0.6f) * 1.4f;
        const Vec3 outer = (b.style == 2 ? palettes_->player[3] : palettes_->player[b.owner < kMaxPlayers ? b.owner : 0]) * lvl;
        const Vec2 dv = b.b - b.a;
        const float len = length(dv);
        if (len < 1.0f) continue;
        const Vec2 dir = dv * (1.0f / len), side = perpU(dir);
        for (u32 strand = 0; strand < 3; ++strand) {
            const Vec3 c = strand == 1 ? W1 * lvl : outer;
            Vec2 prev = b.a;
            float along = 0.0f;
            while (along < len) {
                along = std::fmin(len, along + rng_.range(18.0f, 30.0f));
                const float push = along >= len ? 0.0f : rng_.range(-10.0f, 10.0f);
                const Vec2 p = b.a + dir * along + side * push;
                d.scene.line(toScene(prev), toScene(p), 0.6f * refPx_, c, c, 0);
                prev = p;
            }
        }
        d.sparkles.glow(toScene(b.a), 5.0f * refPx_, outer * 0.6f);
        d.sparkles.glow(toScene(b.b), 5.0f * refPx_, outer * 0.6f);
    }

    // Frozen bodies wear a ring of ice; burning ones glow like embers.
    const Enemies& e = w.enemies();
    for (u32 i = 0; i < e.count; ++i) {
        if (e.flags[i] & kFlagDead) continue;
        const bool ice = w.frozen(i), fire = w.burning(i);
        if (!ice && !fire) continue;
        const Vec2 p{e.x[i], e.y[i]};
        if (!visible(p, 40.0f)) continue;
        if (ice) glyph(d.glyphs, gRing32_, false, p, 0.0f, 0.0f, e.radius[i] * 1.5f, 1.4f, 0.5f, kGlyphFlat, kIce, W1);
        if (fire) d.sparkles.glow(toScene(p), e.radius[i] * 1.8f * pxPerWu_, kFire * (0.6f + 0.4f * std::sin(t * 25.0f + static_cast<float>(i))));
    }
}

void PlayView::drawHazards(FrameDraw& d, const ViewInput& in) {
    // M5 part 4 (look.md 2, 5): what is lethal is the danger colour and nothing else. A Tether's beam is one line (under 20
    // wu: edges and core are one) with a white core; a Weaver's trail (and Ram B's wake) a danger polyline fading with its
    // age; a Well is drawn by its glyph and the edge of its pull (an area by its edge, no fill).
    const World& w = *in.world;
    const Vec3 danger = palettes_->danger, W1 = whiteU();
    const float t = static_cast<float>(in.time);
    {
        Vec2 a[64], b[64];
        const u32 n = w.tetherBeams(a, b, 64);
        const float width = w.species().family(Family::Tether).beamWidth;
        for (u32 k = 0; k < n; ++k) {
            if (!visible((a[k] + b[k]) * 0.5f, length(b[k] - a[k]) * 0.5f + 20.0f)) continue;
            const float flick = 0.85f + 0.15f * std::sin(t * 40.0f + static_cast<float>(k));
            lethalBeam(d, a[k], b[k], width, flick);  // (look.md v2.5: 6 wu, a laser like every lethal beam)
        }
    }
    const World::Trail* tr = w.trails();
    for (u32 k = 0; k < World::kTrails; ++k) {
        const World::Trail& tl = tr[k];
        if (tl.n < 2) continue;
        for (u32 j = 1; j < tl.n; ++j) {
            const u32 j0 = (tl.head + World::Trail::kCap - j) % World::Trail::kCap, j1 = (tl.head + World::Trail::kCap - j - 1) % World::Trail::kCap;
            const Vec2 p0{tl.x[j0], tl.y[j0]}, p1{tl.x[j1], tl.y[j1]};
            if (!visible(p0, 40.0f)) continue;
            const float age0 = static_cast<float>((in.music - tl.t[j0]) / tl.life), age1 = static_cast<float>((in.music - tl.t[j1]) / tl.life);
            const float l0 = 1.4f * (1.0f - clampU(age0)), l1 = 1.4f * (1.0f - clampU(age1));
            d.scene.line(toScene(p0), toScene(p1), std::fmax(0.5f * tl.width * pxPerWu_, 0.8f * refPx_), danger * l0, danger * l1, 0);
            if (tl.kind == 2) d.scene.line(toScene(p0), toScene(p1), 0.8f * refPx_, W1 * (0.5f * l0), W1 * (0.5f * l1), 0);
        }
    }
    // Skitter B's mines (a danger ring and a core that blinks faster as it ages), Blink C's decoys (white copies), Sentry C's
    // domes (an area: its edge in the enemy's hue).
    for (u32 k = 0; k < World::kSpots; ++k) {
        const World::Spot& m = w.enemyMines()[k];
        if (m.until < in.music || !visible(m.at, m.r)) continue;
        const float blink = 0.6f + 0.4f * std::sin(t * 9.0f + static_cast<float>(k));
        glyph(d.glyphs, gRing64_, false, m.at, 0.0f, 0.0f, m.r, 0.9f, 0.4f, kGlyphRingW, danger, danger);
        glyph(d.glyphs, gRing32_, false, m.at, 0.0f, 0.0f, 6.0f, 1.8f * blink, 0.7f, kGlyphFlat, danger, W1);
    }
    for (u32 k = 0; k < World::kSpots; ++k) {
        const World::Spot& dc = w.decoys()[k];
        if (dc.until < in.music || !visible(dc.at, dc.r)) continue;
        const float left = clampU(static_cast<float>((dc.until - in.music) / kTicksPerBeat));
        glyph(d.glyphs, gRing32_, false, dc.at, t, t, dc.r, 1.4f * (0.4f + 0.6f * left), 0.6f, kGlyphFlat, W1, W1);
    }
    {
        Vec2 da[16];
        float dr[16];
        const u32 nd = w.domes(da, dr, 16);
        for (u32 k = 0; k < nd; ++k)
            if (visible(da[k], dr[k])) glyph(d.glyphs, gRing128_, false, da[k], 0.0f, 0.0f, dr[k], 1.0f, 0.5f, kGlyphRingW, palette_->hue[2], W1);
    }
    const Enemies& e = w.enemies();
    const FamilyData& wf = w.species().family(Family::Well);
    for (u32 i = 0; i < e.count; ++i) {
        if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping || w.species().get(e.species[i]).family != Family::Well) continue;
        const Vec2 p{e.x[i], e.y[i]};
        const float sc = e.b[i] > 0.0f ? e.b[i] : 1.0f;
        if (!visible(p, wf.pullRadius * sc)) continue;
        const Vec3 hue = palette_->hue[2];
        glyph(d.glyphs, gRing128_, false, p, 0.0f, 0.0f, wf.pullRadius * sc, 0.7f, 0.35f, kGlyphRingW, hue, hue);
        const float ph = std::fmod(t * 0.8f + static_cast<float>(i % 7) * 0.13f, 1.0f);
        glyph(d.glyphs, gRing128_, false, p, 0.0f, 0.0f, wf.pullRadius * sc * (1.0f - ph), 0.5f * ph, 0.3f, kGlyphRingW, hue, W1);
    }
}

}  // namespace rf
