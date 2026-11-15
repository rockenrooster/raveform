// The owner, 2026-11-14: "Things that need more visual feedback: on beat crit, crescendo, blackhole (powerup and upgrade),
// syncopation, stormguard, the meteor crash (it just eases and disappears, it doesn't need to ease, its a meteor impact), the
// fire burn upgrade, event horizon death". What each shows now (look.md §5 "Feedback"); cosmetic only, nothing here touches the
// simulation:
// - On-Beat Crit: a beat shot's hit bursts as a gold star with a white core, a ring out to the burst's 80 wu, a pool of gold light
//   and sparks (the shot itself wears a bigger star with a glow; drawShots).
// - Crescendo: ten ticks round the ship lit gold to white as the damage climbs; each step flares its tick; full, a ring joins them
//   and pulses on the beat with a warm glow; a hit shatters it (sparks and a gold ring). Its shots brighten with it (drawShots).
// - Syncopation: an off-beat shot is accented, a ">" ahead of its head, a doubled tail and a brighter head (drawShots).
// - Burn: a burning body wears flames, tongues rising from it (more and taller by its stacks, yellow-white at the root to red at
//   the tip, flaring on the beat it burns), embers rising off it and a warm glow; a burning death's spread bursts in fire.
// - Singularity's well and Black Hole's singularity: the lattice falls into them; an accretion disc (streaks on spirals, faster
//   near the core), rings falling in, a white photon ring round a dark core, all quickening toward the blast; placed, a ring folds
//   in and light is drawn to it; blown, a white flash, debris of light, a ring, the lattice thrown out, a shake.
// - Storm Coil ("stormguard"; the owner then: "the ring for storm coil isn't needed, just a more visible lightning zap"): its zaps
//   (and Chain Storm's) brighter and held longer, with a pooled glow along each, each ending in a star. Aegis: the arc glows; a block
//   flashes white where it struck and the whole arc flares.
// - The meteor smashes (play_view_arena.cpp falls it faster and faster): its rock flies apart, a white star, a crater rim glowing
//   for a bar, the lattice punched, a hard shake.
// - Event Horizon's death: over the bar light is torn in toward the point and the lattice falls in after it, the screen trembling;
//   the beat of black holds one white point; then the burst: two jets of light along its axis, rings in its hues, debris.

#include <cmath>

#include "core/args.h"
#include "game/play_view.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
Vec3 whiteF() { return {1.0f, 1.0f, 1.0f}; }
Vec2 dirF(float a) { return {std::cos(a), std::sin(a)}; }
float clampF(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
float fractF(float x) { return x - std::floor(x); }
float hashF(u32 n) {  // 0..1 from an integer: the same body keeps the same flames
    n = (n ^ 61u) ^ (n >> 16);
    n *= 9u;
    n ^= n >> 4;
    n *= 0x27d4eb2du;
    n ^= n >> 15;
    return static_cast<float>(n & 0xFFFFu) / 65535.0f;
}
const Vec3 kFlameRoot{1.0f, 0.86f, 0.52f}, kFlameMid{1.0f, 0.42f, 0.08f}, kFlameTip{0.85f, 0.12f, 0.02f};
const Vec2 kUp{0.0f, -1.0f};  // the screen's up in the world (y grows down the screen)

enum FlashKind : u8 { kFlCrit = 1, kFlBlock, kFlZap, kFlImplode, kFlCrater, kFlRing, kFlStar };

// Chosen (the owner's "more visual feedback", 2026-11-14; look.md §5 "Feedback").
constexpr float kCritFlashWu = 46.0f, kCritFlashS = 0.22f, kCritPoolWu = 70.0f;
constexpr u32 kCrescTicks = 10;
constexpr float kCrescStepS = 0.3f;
constexpr float kAegisFlareS = 0.18f;
// The lattice's pull (wu at its peak, rising by half toward the blast), where it peaks and how far it reaches: a funnel round the
// core, hot only near it (a pull over the whole reach heated the whole frame white).
constexpr float kWellLatticePull = 12.0f, kWellLatticePeakWu = 60.0f, kWellLatticeGoneWu = 220.0f;
constexpr float kHoleLatticePull = 16.0f, kHoleLatticePeakWu = 90.0f, kHoleLatticeGoneWu = 320.0f;
constexpr float kHzPullWu = 30.0f, kHzPeakWu = 160.0f, kHzReachWu = 800.0f;  // Event Horizon's death bar (its pull grows as u^2)
constexpr float kDebrisDrag = 3.0f;                                   // 1/s
constexpr float kHzJetS = 0.8f, kHzJetWu = 1800.0f, kHzJetAxis = 1.05f;
}  // namespace

void PlayView::addFlash(u8 kind, Vec2 at, float size, float life, Vec3 c, float t, float rot) {
    Flash& f = flashes_[flashHead_];
    flashHead_ = (flashHead_ + 1u) % kFlashes;
    f.at = at;
    f.t = t;
    f.size = size;
    f.life = life;
    f.c = c;
    f.kind = kind;
    f.rot = rot;
}

void PlayView::addDebris(Vec2 at, u32 count, float speedLo, float speedHi, float lenLo, float lenHi, float life, Vec3 c0, Vec3 c1, float t) {
    for (u32 k = 0; k < count; ++k) {
        Debris& b = debris_[debrisHead_];
        debrisHead_ = (debrisHead_ + 1u) % kDebris;
        b.p = at;
        b.v = dirF(rng_.range(0.0f, kTau)) * rng_.range(speedLo, speedHi);
        b.ang = rng_.range(0.0f, kTau);
        b.spin = rng_.range(-14.0f, 14.0f);
        b.len = rng_.range(lenLo, lenHi);
        b.born = t;
        b.life = life * rng_.range(0.7f, 1.0f);
        b.c = mix(c0, c1, rng_.range(0.0f, 1.0f));
    }
}

float PlayView::crescendoLevel(const World& w, u32 q) const {
    const u8 st = w.player(q).loadout.mods[kCrescendo];
    if (!st) return 0.0f;
    const float cap = tuning::kCrescendoMax[st < 2 ? st : 2];
    return cap > 0.0f ? clampF((w.crescendo(q) - 1.0f) / cap) : 0.0f;
}

void PlayView::critBurst(const World& w, const Event& e, float t) {
    // On-Beat Crit (play test 2, look.md 5): the beat shot's hit bursts over the burst's 80 wu; since 2026-11-14 a gold star with a
    // white core, its ring, a pool of gold light, sparks and a small push of the lattice.
    const u32 q = e.a < kMaxPlayers ? e.a : w.viewPlayer();
    const bool mine = q == w.viewPlayer();
    const float lvl = (mine ? 1.0f : 0.6f) * (reduceFlashing_ ? 0.6f : 1.0f);
    const Vec2 at{e.x, e.y};
    addFlash(kFlCrit, at, e.f, kCritFlashS, buildGold_ * lvl, t, rng_.range(0.0f, kPi));
    addPool(at, 10.0f, kCritPoolWu, buildGold_, 0.9f * lvl, 0.0f, 0.18f, false, 1, false, false, t);
    SparkReq r{};
    r.at = at;
    r.count = mine ? 10 : 5;
    r.kind = 0;
    r.a = buildGold_ * lvl;
    r.b = whiteF() * lvl;
    r.t0 = t;
    sparkRequest(r);
    if (mine && !reduceFlashing_) push(kImpHit, at, 70.0f, 90.0f, t);
}

void PlayView::feedbackFx(const World& w, const Event& e, float t) {
    const u32 q = e.a < kMaxPlayers ? e.a : w.viewPlayer();
    const float lvl = q == w.viewPlayer() ? 1.0f : 0.6f;
    const Vec3 player = palettes_->player[q] * lvl, light = palettes_->playerLight[q] * lvl, W1 = whiteF() * lvl;
    const Vec2 at{e.x, e.y};
    switch (e.b) {
        case kFxAegis: {
            // The arc stopped a bullet: a white star where it struck, a small ring, sparks thrown back off it; the whole arc
            // flares (drawFeedbackFx).
            aegisHitT_[q] = t;
            addFlash(kFlBlock, at, 30.0f, 0.16f, mix(light, W1, 0.5f), t, rng_.range(0.0f, kPi));
            SparkReq r{};
            r.at = at;
            r.dir = normalizeOr(at - w.player(q).pos, Vec2{1.0f, 0.0f});
            r.count = 8;
            r.kind = 9;
            r.a = light;
            r.b = W1;
            r.t0 = t;
            sparkRequest(r);
            break;
        }
        case kFxBurnSpread: {
            // A burning death lit its neighbours: a burst of fire.
            SparkReq r{};
            r.at = at;
            r.count = static_cast<u16>(12u + 4u * (e.c < 8 ? e.c : 8u));
            r.kind = 5;
            r.a = kFlameMid;
            r.b = kFlameRoot;
            r.t0 = t;
            sparkRequest(r);
            addPool(at, 14.0f, tuning::kBurnSpreadWu * 1.6f, kFlameMid, 0.8f, 0.0f, 0.3f, false, 1, false, false, t);
            addFlash(kFlRing, at, tuning::kBurnSpreadWu + 20.0f, 0.25f, kFlameMid * 1.2f, t);
            break;
        }
        case kFxWell:
        case kFxHole: {
            // Placed: the space folds in (a ring falling to the point) and light is drawn into it.
            const bool hole = e.b == kFxHole;
            const Vec3 c = hole ? palettes_->player[3] * lvl : player;
            addFlash(kFlImplode, at, hole ? 560.0f : 280.0f, hole ? 0.45f : 0.3f, mix(c, W1, 0.3f), t);
            SparkReq r{};
            r.at = at;
            r.count = hole ? 60 : 28;
            r.kind = 6;
            r.a = c;
            r.b = W1;
            r.t0 = t;
            r.extra = 0.5f;
            sparkRequest(r);
            if (!reduceFlashing_) push(kImpNova, at, hole ? -110.0f : -60.0f, hole ? 280.0f : 160.0f, t);
            break;
        }
        default: break;
    }
}

void PlayView::wellBlow(const Event& e, float t) {
    // Singularity's and Black Hole's blast, on top of the rings eventUpgrade draws: the white flash, debris of light thrown out,
    // a ring, the lattice pushed out, a harder shake for Black Hole.
    const bool hole = e.b == kFxHoleBlow;
    const u32 q = e.a < kMaxPlayers ? e.a : 0u;
    const Vec3 c = hole ? palettes_->player[3] : palettes_->playerLight[q];
    const Vec2 at{e.x, e.y};
    addPool(at, 20.0f, e.f * (hole ? 1.1f : 0.9f), whiteF(), reduceFlashing_ ? 0.5f : (hole ? 1.6f : 1.1f), 0.0f, hole ? 0.3f : 0.2f, false, 1,
            false, false, t);
    addDebris(at, hole ? 40u : 18u, hole ? 500.0f : 300.0f, hole ? 1300.0f : 800.0f, 10.0f, hole ? 34.0f : 22.0f, hole ? 0.8f : 0.55f, c,
              whiteF(), t);
    SparkReq r{};
    r.at = at;
    r.count = hole ? 120 : 50;
    r.kind = 1;
    r.a = c;
    r.b = whiteF();
    r.t0 = t;
    sparkRequest(r);
    addFlash(kFlRing, at, e.f * 1.3f, hole ? 0.55f : 0.4f, mix(c, whiteF(), 0.4f), t);
    if (!hole) addDistortion(at, e.f * 1.3f, 0.35f, 14.0f, 18.0f, t);
    push(kImpNova, at, hole ? 520.0f : 260.0f, e.f * 1.4f, t);
    shakeAmp_ = std::fmax(shakeAmp_, hole ? 7.0f : 3.0f);
}

void PlayView::zapHit(Vec2 at, u32 owner, bool gold, float t) {
    const Vec3 c = gold ? palettes_->player[3] : palettes_->playerLight[owner < kMaxPlayers ? owner : 0];
    addFlash(kFlZap, at, 24.0f, 0.16f, mix(c, whiteF(), 0.4f), t, rng_.range(0.0f, kPi));
}

void PlayView::meteorImpact(Vec2 at, float ringWu, float t) {
    // The rock smashes (the owner, 2026-11-14: "it doesn't need to ease, its a meteor impact"): its pieces fly out, a white star,
    // a crater rim glowing for a bar and more, a gold pool, the lattice punched, a hard shake (on top of eventArena's flash, rings,
    // sparks and distortion).
    const Vec3 gold = palettes_->player[3], W1 = whiteF(), danger = palettes_->danger;
    addDebris(at, 26, 400.0f, 1100.0f, 14.0f, 40.0f, 0.75f, gold, W1, t);
    addDebris(at, 14, 150.0f, 420.0f, 8.0f, 18.0f, 1.0f, mix(gold, danger, 0.5f), gold, t);
    addFlash(kFlCrater, at, ringWu * 0.85f, 1.6f, mix(danger, gold, 0.35f), t, rng_.range(0.0f, kTau));
    if (!reduceFlashing_) addFlash(kFlStar, at, ringWu * 1.6f, 0.2f, W1, t, rng_.range(0.0f, kPi));
    addPool(at, ringWu * 0.4f, ringWu * 2.2f, gold, reduceFlashing_ ? 0.3f : 0.7f, 0.0f, 0.5f, false, 1, false, false, t);
    push(kImpBoss, at, 220.0f, ringWu * 2.0f, t);
    shakeAmp_ = std::fmax(shakeAmp_, 8.0f);
}

void PlayView::horizonCollapse(FrameDraw& d, const ViewInput& in, Vec2 c, float u, float black) {
    // horizon.md's death ("everything spirals into the point over a bar, a beat of black, then the flash"), with more to see (the
    // owner, 2026-11-14): light is torn in toward the point from far out, faster and brighter as the bar ends (streaks in the
    // Track's hues on spirals), the lattice falls in after it (feedbackWells), the screen trembles; in the beat of black one white
    // point holds, shrinking, with a quivering ring of light about to break.
    const Vec3 W1 = whiteF();
    const float t = static_cast<float>(in.time);
    if (black <= 0.0f) {
        const u32 n = reduceFlashing_ ? 36u : 72u;
        for (u32 k = 0; k < n; ++k) {
            const float h = hashF(k * 7u + 3u);
            const float ph = fractF(t * (0.35f + 1.4f * u * u) + h);  // its trip in
            const float r = 60.0f + 1100.0f * (1.0f - 0.45f * u) * (1.0f - ph) * (1.0f - ph);
            const float a = h * kTau + (1.0f - r / 1200.0f) * 4.0f + t * 0.6f;
            const float spin = 0.12f + 0.25f * u;  // how far round each streak reaches
            const Vec2 p0 = c + dirF(a) * r, p1 = c + dirF(a + spin) * (r * 0.82f);
            if (!visible(p0, 40.0f) && !visible(p1, 40.0f)) continue;
            const Vec3 hue = palette_->hue[k % 3];
            const float lv = (0.25f + 1.6f * u) * clampF(ph * 4.0f) * (0.5f + 0.5f * (1.0f - r / 1200.0f));
            d.scene.line(toScene(p0), toScene(p1), (0.5f + 0.5f * u) * refPx_, hue * (0.3f * lv), mix(hue, W1, 0.5f) * lv, 0);
        }
        d.pools.glow(toScene(c), (260.0f - 200.0f * u) * pxPerWu_, palette_->hue[0] * (0.10f + 0.25f * u));
        shakeAmp_ = std::fmax(shakeAmp_, (reduceFlashing_ ? 0.5f : 1.0f) * (1.0f + 4.0f * u * u));
        return;
    }
    const float r = 10.0f * (1.0f - black) + 2.0f;
    d.sparkles.glow(toScene(c), r * 2.5f * pxPerWu_, W1 * (1.2f + 1.5f * black));
    const u32 n = 40;
    const float rr = 36.0f * (1.0f - 0.6f * black);
    const Vec3 rc = W1 * (0.4f + 0.8f * black);
    for (u32 j = 0; j < n; ++j) {
        const float a0 = kTau * static_cast<float>(j) / n, a1 = kTau * static_cast<float>(j + 1) / n;
        const float wob = 1.0f + 0.06f * std::sin(t * 70.0f + static_cast<float>(j));
        d.scene.line(toScene(c + dirF(a0) * (rr * wob)), toScene(c + dirF(a1) * (rr * wob)), 0.5f * refPx_, rc, rc, 0);
    }
}

void PlayView::horizonBurst(Vec2 at, float t) {
    // The burst after the beat of black (fightBreak plays the boss's fall): two jets of white light along its axis for most of a
    // second (drawFeedbackFx), a ring of each of the Track's hues racing out, debris of light, the lattice blown out.
    hzBurstT_ = t;
    hzBurstAt_ = at;
    for (u32 k = 0; k < 3; ++k)
        addShockRings(at, 1, 1400.0f + 400.0f * static_cast<float>(k), palette_->hue[k], whiteF(), t + 0.05f * static_cast<float>(k), 0.7f);
    addDebris(at, 40, 700.0f, 1800.0f, 16.0f, 50.0f, 0.9f, palette_->hue[0], whiteF(), t);
    push(kImpBoss, at, 500.0f, 1600.0f, t);
    shakeAmp_ = std::fmax(shakeAmp_, 8.0f);
}

u32 PlayView::feedbackWells(const ViewInput& in, LatticeWell* out, u32 cap) const {
    // The lattice falls into Singularity's well and Black Hole's singularity (harder as they near the blast) and, over Event
    // Horizon's death bar, into its point.
    const World& w = *in.world;
    u32 n = 0;
    for (u32 q = 0; q < kMaxPlayers && n < cap; ++q) {
        const PlayerState& pq = w.player(q);
        if (!pq.present) continue;
        const u8 L = pq.loadout.riffs[riffIndex(kSingularity)];
        if (L && pq.wellFrom >= 0.0 && in.music >= pq.wellFrom && in.music < pq.wellBlow && n < cap) {
            const float u = pq.wellBlow - pq.wellFrom > 1.0 ? clampF(static_cast<float>((in.music - pq.wellFrom) / (pq.wellBlow - pq.wellFrom))) : 0.0f;
            out[n++] = LatticeWell{pq.wellAt, kWellLatticePull * (1.0f + 0.5f * u), kWellLatticePeakWu, kWellLatticeGoneWu};
        }
        if (pq.holeFrom >= 0.0 && in.music >= pq.holeFrom && in.music < pq.holeBlow && n < cap) {
            const float u = pq.holeBlow - pq.holeFrom > 1.0 ? clampF(static_cast<float>((in.music - pq.holeFrom) / (pq.holeBlow - pq.holeFrom))) : 0.0f;
            out[n++] = LatticeWell{pq.holeAt, kHoleLatticePull * (1.0f + 0.5f * u), kHoleLatticePeakWu, kHoleLatticeGoneWu};
        }
    }
    const BossState& b = w.boss();
    if (b.kind == BossKind::EventHorizon && b.dying && n < cap) {
        const double since = in.music - b.deathTick;
        if (since >= 0.0 && since < kTicksPerBar + kTicksPerBeat) {
            const float u = clampF(static_cast<float>(since / kTicksPerBar));
            out[n++] = LatticeWell{b.pos, kHzPullWu * u * u, kHzPeakWu, kHzReachWu};
        }
    }
    return n;
}

void PlayView::drawWell(FrameDraw& d, const ViewInput& in, Vec2 at, float r, double from, double to, Vec3 c, bool hole, float lvl) {
    // Singularity's well and Black Hole's singularity (the owner, 2026-11-14: "blackhole (powerup and upgrade)"): an area is drawn by
    // its edge (look.md 5); inside it an accretion disc, streaks on spirals faster near the core, rings falling in, a white photon
    // ring round a dark core; all quicken and brighten toward the blast; the lattice falls in (feedbackWells).
    if (!visible(at, r)) return;
    const float t = static_cast<float>(in.time);
    const Vec3 W1 = whiteF() * lvl;
    const double span = to - from;
    const float u = span > 1.0 ? clampF(static_cast<float>((in.music - from) / span)) : 0.0f;
    glyph(d.glyphs, gRing128_, false, at, 0.0f, 0.0f, r, (1.2f + 0.8f * u) * (1.0f + 0.3f * beat_), 0.6f, kGlyphRingW, c, W1);
    for (u32 k = 0; k < 4; ++k) {
        const float ph = fractF(t * (1.2f + 1.5f * u) + 0.25f * static_cast<float>(k));
        const float rr = r * (1.0f - ph) * (1.0f - 0.4f * ph);
        glyph(d.glyphs, gRing128_, false, at, 0.0f, 0.0f, rr, 0.45f * ph * (0.6f + u), 0.4f, kGlyphRingW, c, c);
    }
    const float core = hole ? 28.0f : 16.0f;
    const u32 n = hole ? 56u : 30u;
    for (u32 k = 0; k < n; ++k) {
        const float h = hashF(k * 31u + (hole ? 7u : 3u));
        const float ph = fractF(t * (0.22f + 0.3f * u) + h);
        const float rad = core + (r * 0.9f - core) * (1.0f - ph) * (1.0f - ph);
        const float omega = (1.4f + 2.5f * u) * std::sqrt(r / rad);  // radians a second, faster near the core
        const float a = h * kTau + t * omega;
        const float arc = std::fmin(0.6f, omega * 0.05f);
        const Vec2 p0 = at + dirF(a) * rad, p1 = at + dirF(a + arc) * (rad * 0.97f);
        const float near = 1.0f - (rad - core) / (r * 0.9f - core);  // 0 far out, 1 at the core
        const Vec3 col = mix(c, W1, near * near) * ((0.35f + 1.3f * near) * (0.6f + 0.6f * u) * clampF(ph * 5.0f));
        d.scene.line(toScene(p0), toScene(p1), (0.5f + 0.4f * near) * refPx_, col * 0.4f, col, 0);
    }
    const u32 m = 28;
    const float pr = core * (1.0f - 0.25f * u);
    const Vec3 pc = W1 * ((1.4f + 1.2f * u) * (1.0f + 0.5f * beat_));
    for (u32 j = 0; j < m; ++j) {
        const float a0 = kTau * static_cast<float>(j) / m, a1 = kTau * static_cast<float>(j + 1) / m;
        d.scene.line(toScene(at + dirF(a0) * pr), toScene(at + dirF(a1) * pr), 0.8f * refPx_, pc, pc, 0);
    }
    d.pools.glow(toScene(at), pr * 4.0f * pxPerWu_, c * (0.25f + 0.25f * u));
}

void PlayView::burnFlames(FrameDraw& d, const ViewInput& in, u32 i, Vec2 p) {
    // Burn (the owner, 2026-11-14: "the fire burn upgrade"): the body wears a flame, drawn as fire is drawn in line art, a
    // teardrop outline rising from under it to a swaying tip (orange to red) with a smaller yellow-white one inside; from two stacks
    // a smaller flame either side, at three all of them taller; flickering, flaring on each beat it burns; embers rising off it and
    // a warm glow under it.
    const Enemies& e = in.world->enemies();
    const float t = static_cast<float>(in.time);
    const u32 raw = e.burnLevel[i] & ~kBurnSpread;
    const u32 st = raw == 0 ? 1u : (raw > 3 ? 3u : raw);
    const float r = e.radius[i];
    const float beatK = 1.0f + 0.5f * beat_;
    const float lvl = in.runLight;
    const float fi = static_cast<float>(i);
    // One flame: centre x offset, half width, height, its phase; the outline from the round bottom up each side to the tip.
    const auto flame = [&](float dx, float halfW, float height, float ph, float level) {
        constexpr u32 kSteps = 9;
        const float sway = std::sin(t * 7.0f + ph) * 0.22f * halfW + std::sin(t * 13.0f + 1.7f * ph) * 0.08f * halfW;
        const float H = height * (0.85f + 0.15f * std::sin(t * 15.0f + 2.3f * ph)) * beatK;
        const float bottom = 0.75f * r;  // (the flame stands under and around the body)
        for (u32 layer = 0; layer < 2; ++layer) {
            const float w = layer ? 0.5f * halfW : halfW, h = layer ? 0.62f * H : H;
            const Vec3 c0 = layer ? kFlameRoot * 1.9f : kFlameMid * 1.5f, c1 = layer ? kFlameMid * 0.9f : kFlameTip * 0.5f;
            const float wpx = (layer ? 0.75f : 0.9f) * refPx_;
            for (int sg = -1; sg <= 1; sg += 2) {
                Vec2 prev = p + Vec2{dx, bottom};
                for (u32 k = 1; k <= kSteps; ++k) {
                    const float u = static_cast<float>(k) / kSteps;
                    // (half width: s^0.5 (1 - s), its peak 0.385 at a third of the way up)
                    const float hw = w * std::sqrt(u) * (1.0f - u) / 0.385f;
                    const Vec2 q = p + Vec2{dx + static_cast<float>(sg) * hw + sway * u * u, bottom - (bottom + h) * u};
                    const float u0 = static_cast<float>(k - 1) / kSteps;
                    d.scene.line(toScene(prev), toScene(q), wpx * (1.0f - 0.4f * u), mix(c0, c1, u0) * level, mix(c0, c1, u) * level, 0);
                    prev = q;
                }
            }
        }
    };
    const float tall = 1.5f + 0.35f * static_cast<float>(st);
    flame(0.0f, 1.05f * r, tall * r, fi * 1.3f, lvl);
    if (st >= 2) {
        flame(-0.75f * r, 0.5f * r, 0.6f * tall * r, fi * 1.3f + 2.1f, 0.85f * lvl);
        flame(0.75f * r, 0.5f * r, 0.55f * tall * r, fi * 1.3f + 4.2f, 0.85f * lvl);
    }
    d.pools.glow(toScene(p + kUp * (0.3f * r)), r * 2.8f * pxPerWu_, kFlameMid * ((0.16f + 0.06f * static_cast<float>(st)) * beatK * lvl));
    const Vec2 side{1.0f, 0.0f};
    for (u32 j = 0; j < 1u + st; ++j) {
        const float h = hashF(i * 13u + j * 7u + 1u);
        const float ph = fractF(t * (0.9f + 0.4f * h) + h);
        const Vec2 q = p + side * (r * 1.2f * (h - 0.5f) + r * 0.3f * std::sin(ph * 6.0f + h * 6.0f)) + kUp * (r * tall * 0.6f + ph * r * 2.5f);
        d.sparkles.glow(toScene(q), 3.5f * refPx_ * (1.0f - ph), mix(kFlameMid, kFlameRoot, 1.0f - ph) * ((1.0f - ph) * 1.6f * lvl));
    }
}

void PlayView::drawFeedbackFx(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    const float t = static_cast<float>(in.time);
    const Vec3 W1 = whiteF();
    const float shipScale = 14.0f * glyphs_->def(static_cast<u32>(gShip_)).size;
    const bool fx = shown(kMuteFx);

    if (fx) {
        for (const Flash& f : flashes_) {
            const float age = t - f.t;
            if (age < 0.0f || age > f.life || !visible(f.at, f.size + 40.0f)) continue;
            const float u = age / f.life, fade = 1.0f - u;
            switch (f.kind) {
                case kFlCrit: {
                    // A four-point star shrinking from 46 wu, gold arms round a white core, turning a little; a second smaller star
                    // across it; a ring out to the burst's width.
                    const float star = kCritFlashWu * (1.0f - 0.5f * u);
                    flare(d.scene, f.at, f.rot + 0.8f * u, star, 2, mix(f.c, W1, 0.35f) * (2.4f * fade * fade), 0.9f);
                    flare(d.scene, f.at, f.rot + 0.25f * kPi - 0.8f * u, star * 0.55f, 2, f.c * (1.6f * fade * fade), 0.6f);
                    glyph(d.glyphs, gRing64_, false, f.at, 0.0f, 0.0f, f.size * (0.25f + 0.75f * std::sqrt(u)), 1.8f * fade, 0.7f, kGlyphRingW, f.c, W1);
                    d.sparkles.glow(toScene(f.at), 14.0f * refPx_ * (1.0f - 0.5f * u), W1 * (1.4f * fade));
                    break;
                }
                case kFlBlock:
                case kFlZap:
                case kFlStar: {
                    flare(d.scene, f.at, f.rot, f.size * (1.0f - 0.4f * u), 2, f.c * (2.2f * fade), f.kind == kFlStar ? 1.4f : 0.7f);
                    d.sparkles.glow(toScene(f.at), (f.kind == kFlStar ? 30.0f : 9.0f) * refPx_, f.c * (1.2f * fade));
                    if (f.kind == kFlBlock) glyph(d.glyphs, gRing32_, false, f.at, 0.0f, 0.0f, f.size * (0.4f + 0.8f * u), 1.6f * fade, 0.6f, kGlyphRingW, f.c, W1);
                    break;
                }
                case kFlImplode: {
                    const float rr = f.size * (1.0f - u) * (1.0f - u) + 6.0f;
                    glyph(d.glyphs, gRing128_, false, f.at, 0.0f, 0.0f, rr, (0.6f + 1.6f * u) * fade + 0.3f, 0.6f, kGlyphRingW, f.c, W1);
                    break;
                }
                case kFlRing:
                    glyph(d.glyphs, gRing128_, false, f.at, 0.0f, 0.0f, f.size * (0.2f + 0.8f * std::sqrt(u)), 1.4f * fade, 0.6f, kGlyphRingW, f.c, W1);
                    break;
                case kFlCrater: {
                    // The meteor's crater: a rough rim glowing hot, cooling from gold-white to its colour; embers rising off it.
                    const Vec3 c = mix(mix(f.c, W1, 0.5f), f.c, clampF(u * 3.0f)) * (1.6f * fade);
                    const u32 n = 40;
                    for (u32 j = 0; j < n; ++j) {
                        const float a0 = kTau * static_cast<float>(j) / n, a1 = kTau * static_cast<float>(j + 1) / n;
                        const float r0 = f.size * (1.0f + 0.07f * std::sin(static_cast<float>(j) * 2.7f + f.rot));
                        const float r1 = f.size * (1.0f + 0.07f * std::sin(static_cast<float>(j + 1) * 2.7f + f.rot));
                        d.scene.line(toScene(f.at + dirF(a0) * r0), toScene(f.at + dirF(a1) * r1), refPx_, c, c, 0);
                    }
                    d.pools.glow(toScene(f.at), f.size * 1.3f * pxPerWu_, f.c * (0.22f * fade));
                    for (u32 j = 0; j < 8; ++j) {
                        const float ph = fractF(t * 0.7f + hashF(j + 11u));
                        const Vec2 p = f.at + dirF(kTau * hashF(j * 5u + 2u)) * (f.size * (0.9f + 0.15f * hashF(j))) + kUp * (ph * 50.0f);
                        d.sparkles.glow(toScene(p), 4.0f * refPx_ * (1.0f - ph), mix(f.c, W1, 0.4f) * (fade * (1.0f - ph) * 1.4f));
                    }
                    break;
                }
                default: break;
            }
        }
        for (const Debris& b : debris_) {
            const float age = t - b.born;
            if (age < 0.0f || age > b.life) continue;
            const Vec2 p = b.p + b.v * ((1.0f - std::exp(-kDebrisDrag * age)) / kDebrisDrag);
            if (!visible(p, b.len + 10.0f)) continue;
            const float fade = 1.0f - age / b.life;
            const Vec2 dv = dirF(b.ang + b.spin * age) * (0.5f * b.len);
            d.scene.line(toScene(p - dv), toScene(p + dv), 0.9f * refPx_, b.c * (1.8f * fade), b.c * (1.8f * fade), 0);
        }
        // Event Horizon's jets.
        const float jet = t - hzBurstT_;
        if (jet >= 0.0f && jet < kHzJetS) {
            const float grow = clampF(jet / 0.15f), fade = jet < 0.3f ? 1.0f : clampF(1.0f - (jet - 0.3f) / (kHzJetS - 0.3f));
            for (u32 s = 0; s < 2; ++s) {
                const Vec2 dv = dirF(kHzJetAxis + (s ? kPi : 0.0f));
                drawBeam(d, hzBurstAt_, dv, kHzJetWu * grow, 90.0f, 4.0f, W1, palette_->hue[s], palette_->hue[2], (reduceFlashing_ ? 1.0f : 2.0f) * fade,
                         (reduceFlashing_ ? 0.15f : 0.3f) * fade);
            }
        }
    }

    for (u32 q = 0; q < kMaxPlayers; ++q) {
        const PlayerState& pq = w.player(q);
        if (!pq.present || (q != w.viewPlayer() && w.playerCount() < 2)) continue;
        const bool local = q == w.viewPlayer();
        const float lvl = local ? 1.0f : 0.6f;
        const Vec3 light = palettes_->playerLight[q] * lvl, gold = buildGold_ * lvl, Wq = W1 * lvl;
        const Vec2 ship = local ? in.shipPos : pq.prevPos + (pq.pos - pq.prevPos) * in.alpha;
        const Loadout& l = pq.loadout;

        // Crescendo: the meter.
        if (const u8 st = l.mods[kCrescendo]) {
            const float cap = tuning::kCrescendoMax[st < 2 ? st : 2];
            const auto litOf = [&](u32 bars) {
                const float k = cap > 0.0f ? clampF(std::fmin(tuning::kCrescendoStep * static_cast<float>(bars), cap) / cap) : 0.0f;
                return static_cast<u32>(std::floor(k * static_cast<float>(kCrescTicks) + 1e-3f));
            };
            const u32 lit = litOf(pq.cleanBars), litBefore = litOf(crescPrev_[q]);
            if (lit > litBefore) crescStepT_[q] = t;
            if (pq.cleanBars < crescPrev_[q] && litBefore > 0 && fx) {
                // A hit: the meter shatters.
                crescBreakT_[q] = t;
                SparkReq r{};
                r.at = ship;
                r.count = static_cast<u16>(6u + 3u * litBefore);
                r.kind = 0;
                r.a = gold;
                r.b = Wq;
                r.t0 = t;
                sparkRequest(r);
                addFlash(kFlRing, ship, 1.55f * shipScale + 40.0f, 0.3f, gold * 1.4f, t);
            }
            crescPrev_[q] = pq.cleanBars;
            if (pq.alive) {
                const float r0 = 1.55f * shipScale + 5.0f, r1 = r0 + 6.0f;
                const float stepAge = t - crescStepT_[q];
                const float flash = stepAge >= 0.0f && stepAge < kCrescStepS ? 1.0f - stepAge / kCrescStepS : 0.0f;
                const bool full = lit >= kCrescTicks;
                const float pulse = full ? 1.0f + 0.8f * beat_ : 1.0f;
                for (u32 j = 0; j < kCrescTicks; ++j) {
                    const Vec2 dv = dirF(-0.5f * kPi + kTau * static_cast<float>(j) / static_cast<float>(kCrescTicks));  // clockwise from the top
                    const bool on = j < lit;
                    Vec3 c = Wq * 0.12f;
                    if (on) {
                        const float f = static_cast<float>(j + 1) / static_cast<float>(kCrescTicks);
                        c = mix(gold, Wq, f * f) * (1.3f * pulse * (j + 1 == lit ? 1.0f + 2.0f * flash : 1.0f));
                    }
                    d.scene.line(toScene(ship + dv * r0), toScene(ship + dv * r1), (on ? 0.9f : 0.6f) * refPx_, c, c, 0);
                }
                if (lit > 0 && flash > 0.0f) {
                    const Vec2 dv = dirF(-0.5f * kPi + kTau * static_cast<float>(lit - 1) / static_cast<float>(kCrescTicks));
                    d.sparkles.glow(toScene(ship + dv * (0.5f * (r0 + r1))), 10.0f * refPx_, mix(gold, Wq, 0.5f) * (1.5f * flash));
                }
                if (full) {
                    // Full: a ring joins the ticks and pulses on the beat; a warm glow round the ship.
                    const float rr = 0.5f * (r0 + r1);
                    const Vec3 c = mix(gold, Wq, 0.5f) * (0.5f + 0.7f * beat_);
                    const u32 n = 40;
                    for (u32 j = 0; j < n; ++j) {
                        const float a0 = kTau * static_cast<float>(j) / n, a1 = kTau * static_cast<float>(j + 1) / n;
                        d.scene.line(toScene(ship + dirF(a0) * rr), toScene(ship + dirF(a1) * rr), 0.5f * refPx_, c, c, 0);
                    }
                    d.pools.glow(toScene(ship), r1 * 2.6f * pxPerWu_, gold * (0.08f + 0.08f * beat_));
                }
            }
        }
        if (!pq.alive) continue;

        // (Storm Coil's reach ring is gone: the owner, 2026-11-14, "the ring for storm coil isn't needed, just a more visible
        // lightning zap"; its zaps glow instead, play_view_upgrades.cpp.)

        // Aegis: the arc glows along its length and at its two ends; a block flares the whole arc white.
        if (const u8 st = l.mods[kAegis]) {
            const float hitAge = t - aegisHitT_[q];
            const float flare = hitAge >= 0.0f && hitAge < kAegisFlareS ? 1.0f - hitAge / kAegisFlareS : 0.0f;
            const float half = 0.5f * tuning::kAegisArcDeg[st < 3 ? st : 3] * (kTau / 360.0f);
            const float c0 = w.aegisAngle(q), r = 0.5f * (tuning::kAegisInWu + tuning::kAegisOutWu);
            const u32 segs = 8;
            for (u32 s = 0; s <= segs; ++s) {
                const float a = c0 - half + 2.0f * half * static_cast<float>(s) / segs;
                d.sparkles.glow(toScene(ship + dirF(a) * r), (9.0f + 8.0f * flare) * refPx_, light * (0.35f + flare));
            }
            for (int sg = -1; sg <= 1; sg += 2)
                d.sparkles.glow(toScene(ship + dirF(c0 + static_cast<float>(sg) * half) * r), 7.0f * refPx_, mix(light, Wq, 0.5f) * 0.9f);
            if (flare > 0.0f) {
                for (u32 s = 0; s < segs; ++s) {
                    const float a0 = c0 - half + 2.0f * half * static_cast<float>(s) / segs, a1 = c0 - half + 2.0f * half * static_cast<float>(s + 1) / segs;
                    d.scene.line(toScene(ship + dirF(a0) * r), toScene(ship + dirF(a1) * r), 1.6f * refPx_, Wq * (2.0f * flare), Wq * (2.0f * flare), 0);
                }
            }
        }
    }
}

}  // namespace rf
