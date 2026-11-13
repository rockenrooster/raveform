// M5 part 3 (GAME_DESIGN 6.2-6.6: every upgrade visible within a bar of the pick): the new Mods, Riffs, Boosts and
// Fusions on screen, each in its player's colours (look.md 6; a teammate's at 0.6). Lines (Accent's lances, the Lancer,
// Downbeat Cannon, Bar Line, Beam, the Sweeper's arms) are capsules; bodies (wingmen, the decoy, mines, comets, bombs,
// wells) are glyphs; lightning is a jagged line that fades in 0.15 s. Cosmetic only.

#include <cmath>

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
constexpr float kLanceFadeS = 0.28f;  // chosen: a lance's flash
constexpr float kBoltS = 0.15f;       // chosen: a lightning jump's life
}  // namespace

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
            addShockRings(at, 1, e.f, e.c == 1 ? light : player, W1, t, e.c == 1 ? 0.3f : 0.18f);
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

        // Lines: Accent's lances, the Lancer's beam, Downbeat Cannon, Bar Line's rays (Standing Wave's stay for a bar).
        for (const PlayerState::Lance& ln : pq.lances) {
            const float age = static_cast<float>((in.music - ln.tick) / tps);
            const float life = ln.standing ? static_cast<float>(kTicksPerBar / tps) : kLanceFadeS;
            if (age < 0.0f || age > life) continue;
            const float u = 1.0f - age / life;
            const float wpx = std::fmax(2.0f * ln.half * pxPerWu_ * (ln.standing ? 0.5f : (0.4f + 0.6f * u)), 1.2f * refPx_);
            Vec3 c = light, core = Wq;
            if (ln.kind == kLanceCannon) c = gold;
            if (ln.kind == kLanceBarLine) c = palette_->accent * lvl;
            const float k = (ln.standing ? 0.5f + 0.3f * beat_ : 1.6f) * u;
            const Vec2 A = toScene(ln.from), B = toScene(ln.from + ln.dir * ln.length);
            d.scene.line(A, B, wpx, c * k, c * (0.3f * k), 0);
            d.scene.line(A, B, wpx * 0.35f, core * k, core * (0.3f * k), 0);
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

        // Beam: the primary as a laser while it fires.
        if (pq.boost(kBeam) && (in.music - pq.beamTick) < 2.0 * w.leadGrid(q) + 2.0) {
            const float half = tuning::kBeamHalfWu + 4.0f * static_cast<float>(pq.beamLanes);
            const Vec2 A = toScene(ship + aim * tuning::kMuzzleWu), B = toScene(ship + aim * tuning::kBeamLengthWu);
            const float flick = 0.85f + 0.15f * std::sin(t * 60.0f);
            d.scene.line(A, B, 2.0f * half * pxPerWu_, player * (1.4f * flick), player * 0.4f, 0);
            d.scene.line(A, B, 0.7f * half * pxPerWu_, Wq * (1.8f * flick), Wq * 0.5f, 0);
            d.sparkles.glow(A, 3.0f * half * pxPerWu_, light * 0.8f);
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
                d.scene.line(toScene(ship + dirU(u0) * r), toScene(ship + dirU(u1) * r), 2.2f * refPx_, light * 1.6f, Wq * 1.2f, 0);
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
                d.scene.line(toScene(ship), toScene(ship + d0 * len), 2.2f * refPx_, player * 1.2f, light * 1.6f, 0);
                d.sparkles.glow(toScene(ship + d0 * len), 7.0f * refPx_, light * 0.8f);
            }
        }

        // Singularity's well and Black Hole's singularity: rings drawn inward, a dark core.
        const auto well = [&](Vec2 at, float r, float from, float to, Vec3 c) {
            const float span = to - from;
            const float u = span > 1.0f ? clampU((static_cast<float>(in.music) - from) / span) : 0.0f;
            for (u32 k = 0; k < 3; ++k) {
                const float ph = std::fmod(t * 1.5f + static_cast<float>(k) / 3.0f, 1.0f);
                glyph(d.glyphs, gRing128_, false, at, 0.0f, 0.0f, r * (1.0f - ph), 0.9f * ph * (0.5f + u), 0.5f, kGlyphRingW, c, Wq);
            }
            d.pools.glow(toScene(at), r * 0.3f * pxPerWu_, c * (0.3f + 0.4f * u));
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

    // Lightning (Arc, Tesla Bloom, Chain Storm, Storm Coil): a jagged line, white at its core, gone in 0.15 s.
    for (const Bolt& b : bolts_) {
        const float age = t - b.born;
        if (age < 0.0f || age > kBoltS) continue;
        const float u = 1.0f - age / kBoltS;
        const float lvl = b.owner == w.viewPlayer() ? 1.0f : 0.6f;
        const Vec3 c = (b.style == 2 ? palettes_->player[3] : palettes_->playerLight[b.owner < kMaxPlayers ? b.owner : 0]) * (lvl * 1.6f * u);
        const Vec2 dv = b.b - b.a;
        const Vec2 side = perpU(normalizeOr(dv, Vec2{1.0f, 0.0f}));
        Vec2 prev = b.a;
        const u32 segs = 5;
        for (u32 k = 1; k <= segs; ++k) {
            const float f = static_cast<float>(k) / segs;
            const float jag = k == segs ? 0.0f : std::sin(static_cast<float>(k) * 12.9898f + b.born * 78.233f) * 14.0f;
            const Vec2 p = b.a + dv * f + side * jag;
            d.scene.line(toScene(prev), toScene(p), 1.4f * refPx_, c, W1 * (lvl * u), 0);
            prev = p;
        }
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

}  // namespace rf
