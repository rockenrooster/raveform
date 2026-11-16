#include "game/play_view.h"
#include "gfx/vector_font.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "audio/band.h"
#include "content/icons.h"
#include "core/args.h"
#include "core/half.h"
#include "core/log.h"
#include "sim/boss_prose.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
constexpr float kRefPxPerWu = 1.5f;       // look.md: 1 wu = 1.5 reference px at zoom 1
constexpr float kStandardEnemyWu = 14.0f;  // look.md §4: glyph sizes are relative to a 14 wu enemy

// Numbers chosen for the view (docs/PROGRESS.md M3, "numbers chosen"): the look bible gives none.
// GAME_DESIGN 9 (play test 1, 2026-11-12): half the first build's lead, which swung the view too far on every change
// of direction (0.25 s, 60 wu). The ease times stay.
constexpr float kLookAheadS = 0.125f;     // the camera leads the ship by its velocity x this (65 wu at full speed)
constexpr float kAimBiasWu = 30.0f;       // ... and leans this far along the aim while firing
constexpr float kLookAheadTau = 0.25f;    // s: the lead eases in and out
constexpr float kZoomCalm = 1.0f, kZoomBusy = 0.85f;  // GAME_DESIGN 9: breathes between 0.85x and 1.25x
constexpr float kZoomLoadLo = 60.0f, kZoomLoadHi = 300.0f, kZoomTau = 2.0f;
// GAME_DESIGN 9 (M3 stage C): while a boss lives within 1,800 wu of the ship the camera holds 0.85x and leans
// toward the boss by 30% of the offset, at most 320 wu. The lean's ease is chosen.
constexpr float kBossCamWu = 1800.0f, kBossZoom = 0.85f, kBossLeanShare = 0.30f, kBossLeanMaxWu = 320.0f;
constexpr float kBossLeanTau = 0.5f;
constexpr float kWallIntensity = 1.6f, kWallWidth = 2.6f, kWallSnare = 0.6f, kWallBass = 0.3f;
constexpr float kHitShakeRefPx = 4.0f, kNovaShakeRefPx = 3.0f, kDeathShakeRefPx = 6.0f;
constexpr float kCellHitShakeRefPx = 6.5f;  // (2026-11-14) a cell lost jolts harder than a guard ring's break
constexpr float kBigShakeTauS = 0.45f, kBigShakeMaxRefPx = 30.0f;  // (2026-11-14) the big shake's fall and its cap
constexpr float kMuzzleFlashS = 0.06f, kMuzzleFlashRefPx = 10.0f, kMuzzleFlash = 0.9f;
// look.md §3 (the owner, 2026-11-14: "a substantial glow" on every red enemy projectile): an enemy bullet's pooled glow on every
// tier, 26 ref px, 0.32, never under 0.6 of that in a crowd (the swarm gain's floor for it).
constexpr float kBulletGlowPx = 26.0f, kBulletGlow = 0.32f, kBulletGlowFloor = 0.6f;
constexpr float kNovaDim = 0.30f;         // exposure down by this much at the end of the inhale
constexpr float kNovaInhaleRingWu = 600.0f, kNovaFrontWidthRefPx = 6.0f, kNovaFront = 3.0f;
constexpr u32 kNovaInhaleSparks = 140, kNovaFrontSparks = 400;
constexpr float kPanicRingS = 0.25f;
constexpr u32 kDashGhosts = 5;            // look.md §5
constexpr float kDashGhostS = 0.25f;
// Edge chevrons (look.md §6 gives the height, the inset and the disc): their shape, chosen. A chevron is 0.4 of
// its height deep; the boss's two sit 0.32 of the height apart; the disc's radius is 0.68 of the height; the
// stroke's half width 0.055 of it, with the threat layer's dark rim.
// GAME_DESIGN 7, look.md 5 (quoted; play test 2: 0.80 and 50 degrees looked pale): the rainbow's lanes, and the band
// round the danger hue no lane may fall in.
constexpr float kPrismLaneL = 0.70f, kPrismLaneStepDeg = 60.0f, kPrismDangerBandDeg = 30.0f;
constexpr float kChevronDepth = 0.4f, kChevronPairGap = 0.32f, kChevronDisc = 0.68f, kChevronStroke = 0.055f;
// look.md §5, Telegraphs (0449eb3, the owner, quoted): a tell's dashes are 1.5 px in the danger colour at 0.35, rising to 0.6 over
// the charge's last beat, in the scene light only. The dash and the gap between dashes (30 wu each) are chosen: the beams' tells
// had them before.
constexpr float kTellWidthRefPx = 1.5f, kTellLevel = 0.35f, kTellLevelFiring = 0.6f;
constexpr float kTellDashWu = 30.0f, kTellGapWu = 30.0f;

Vec2 dir(float a) { return {std::cos(a), std::sin(a)}; }
Vec2 perp(Vec2 v) { return {-v.y, v.x}; }
Vec2 rotate(Vec2 v, float c, float s) { return {v.x * c - v.y * s, v.x * s + v.y * c}; }
float smooth01(float x) {
    x = clampf(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}
Vec3 white() { return {1.0f, 1.0f, 1.0f}; }
Vec2 lerp2(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
float lerpAngle(float a, float b, float t) { return a + wrapAngle(b - a) * t; }

// Linear sRGB <-> OKLab (Ottosson's matrices), for Prism's rainbow (GAME_DESIGN 7, M3 stage C2).
Vec3 toOklab(Vec3 c) {
    const float l = std::cbrt(0.4122214708f * c.x + 0.5363325363f * c.y + 0.0514459929f * c.z);
    const float m = std::cbrt(0.2119034982f * c.x + 0.6806995451f * c.y + 0.1073969566f * c.z);
    const float s = std::cbrt(0.0883024619f * c.x + 0.2817188376f * c.y + 0.6299787005f * c.z);
    return {0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s, 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s,
            0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s};
}
Vec3 fromOklab(float L, float A, float B) {
    const float l_ = L + 0.3963377774f * A + 0.2158037573f * B, m_ = L - 0.1055613458f * A - 0.0638541728f * B,
                s_ = L - 0.0894841775f * A - 1.2914855480f * B;
    const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    return {4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s, -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
            -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s};
}
// The most saturated colour of lightness L and hue h (radians) inside the sRGB gamut.
Vec3 fullChroma(float L, float h) {
    float lo = 0.0f, hi = 0.4f;
    for (u32 k = 0; k < 24; ++k) {
        const float c = 0.5f * (lo + hi);
        const Vec3 v = fromOklab(L, c * std::cos(h), c * std::sin(h));
        const bool in = v.x >= 0.0f && v.x <= 1.0f && v.y >= 0.0f && v.y <= 1.0f && v.z >= 0.0f && v.z <= 1.0f;
        (in ? lo : hi) = c;
    }
    const Vec3 v = fromOklab(L, lo * std::cos(h), lo * std::sin(h));
    return {clampf(v.x, 0.0f, 1.0f), clampf(v.y, 0.0f, 1.0f), clampf(v.z, 0.0f, 1.0f)};
}
}  // namespace

struct PlayView::Pool {
    Vec2 at;
    float r0 = 0, r1 = 0, i0 = 0, i1 = 0, born = -100.0f, life = 0.0f;
    Vec3 colour;
    bool kill = false;
    u8 fade = 0;           // 0 linear i0 -> i1, 1 squared time left
    bool sparkle = false;  // radius in ref px, full resolution
    bool dimmed = true;    // scaled by the crowd gain
};
struct PlayView::Ember {
    Vec2 p0, v0;
    float born = -100.0f, life = 0.0f;
    Vec3 colour;
};
struct PlayView::Shatter {
    Vec2 a, b, centre, v;
    float spin = 0.0f, born = -100.0f, life = 0.5f;
    Vec3 colour;
    bool dimmed = true;
};
struct PlayView::DistortRing {
    Vec2 at;
    float born = -100.0f, life = 0.0f, toWu = 0.0f, amountRefPx = 0.0f, widthRefPx = 0.0f;
};

PlayView::~PlayView() {
    std::free(pools_);
    std::free(embers_);
    std::free(shatter_);
    std::free(rings_);
    std::free(sparkReqs_);
    std::free(stars_);
}

bool PlayView::init(const Look& look, const Palettes& palettes, const Palette& palette, const GlyphSet& glyphs,
                    const TierSettings& tier) {
    look_ = &look;
    palettes_ = &palettes;
    palette_ = &palette;
    glyphs_ = &glyphs;
    tier_ = tier;
    gShip_ = glyphs.find("ship");
    // Play test 2: up to eight arcs (halo_7, halo_8), and the first k arcs of each (content/glyphs.cpp).
    for (u32 k = 1; k <= 8; ++k) {
        char id[20];
        std::snprintf(id, sizeof id, "halo_%u", k);
        gHalo_[k] = glyphs.find(id);
        for (u32 n = k + 1; n <= 8; ++n) {
            std::snprintf(id, sizeof id, "halo_%u_of_%u", k, n);
            gHaloPart_[k][n] = glyphs.find(id);
        }
    }
    gShard_ = glyphs.find("shard");
    gBullet_ = glyphs.find("enemy_bullet");
    gMarkBHeading_ = glyphs.find("mark_b_heading");
    gMarkBSpin_ = glyphs.find("mark_b_spin");
    gMarkC_ = glyphs.find("mark_c");
    gRing32_ = glyphs.find("ring32");
    gRing64_ = glyphs.find("ring64");
    gRing128_ = glyphs.find("ring128");
    if (gShip_ < 0 || gHalo_[1] < 0 || gHalo_[8] < 0 || gHaloPart_[7][8] < 0 || gShard_ < 0 || gBullet_ < 0 || gRing32_ < 0 || gRing64_ < 0 ||
        gRing128_ < 0 || gMarkBHeading_ < 0 || gMarkBSpin_ < 0 || gMarkC_ < 0) {
        RF_ERR("play: a glyph the game needs is missing from design/glyphs.json");
        return false;
    }
    poolCap_ = 512;
    emberCap_ = 1024;
    shatterCap_ = 1024;
    ringCap_ = 8;
    sparkReqCap_ = 2048;
    pools_ = static_cast<Pool*>(std::calloc(poolCap_, sizeof(Pool)));
    embers_ = static_cast<Ember*>(std::calloc(emberCap_, sizeof(Ember)));
    shatter_ = static_cast<Shatter*>(std::calloc(shatterCap_, sizeof(Shatter)));
    rings_ = static_cast<DistortRing*>(std::calloc(ringCap_, sizeof(DistortRing)));
    sparkReqs_ = static_cast<SparkReq*>(std::calloc(sparkReqCap_, sizeof(SparkReq)));
    stars_ = static_cast<Star*>(std::calloc(512, sizeof(Star)));
    if (!pools_ || !embers_ || !shatter_ || !rings_ || !sparkReqs_ || !stars_) return false;
    resetEffects();

    // Background (look.md §7): clouds on a tile moving at a quarter of the camera's speed, star dust at half.
    cloudTile_ = {3600.0f, 1800.0f};
    cloudCount_ = 8;
    for (u32 i = 0; i < cloudCount_; ++i) {
        Cloud& c = clouds_[i];
        c.p = {rng_.range(0.0f, cloudTile_.x), rng_.range(0.0f, cloudTile_.y)};
        c.radius = rng_.range(look.back.nebulaRadiusWu[0], look.back.nebulaRadiusWu[1]);
        c.intensity = rng_.range(look.back.nebulaIntensity[0], look.back.nebulaIntensity[1]);
        c.colour = static_cast<u8>(rng_.below(5));
    }
    starTile_ = {2000.0f, 900.0f};
    starCount_ = static_cast<u32>(look.back.starsOnScreen) < 512 ? static_cast<u32>(look.back.starsOnScreen) : 512u;
    const float radii[4] = {1.6f, 1.6f, 2.2f, 3.0f};
    for (u32 i = 0; i < starCount_; ++i) {
        Star& s = stars_[i];
        s.p = {rng_.range(0.0f, starTile_.x), rng_.range(0.0f, starTile_.y)};
        s.radius = radii[rng_.below(4)];
        s.intensity = rng_.range(look.soft.starDust.intensity[0], look.soft.starDust.intensity[1]);
        s.colour = rng_.uniform() < look.back.starsWhiteShare ? 0u : static_cast<u8>(1u + rng_.below(3));
    }
    return setTier(tier);
}

bool PlayView::setTier(const TierSettings& tier) {
    const bool newLattice = tier.gridSpacingWu != lattice_.spacing() || !hasPrevView_;
    tier_ = tier;
    // The window covers the widest view (zoom 0.75 on a 20:9 screen) plus margins.
    if (newLattice && !lattice_.init(tier.gridSpacingWu, 1600.0f / 0.75f, 720.0f / 0.75f)) return false;
    // M8 (README.md 4): the Stage's, every tier alike: 20 wu, so every other line is a tile's side (the Stage and its margin).
    if (!stageLatticeReady_) {
        if (!stageLattice_.init(20.0f, 1900.0f, 1000.0f)) return false;
        stageLatticeReady_ = true;
    }
    return true;
}

void PlayView::setIcons(const IconSet* icons, const i32* upgradeIcons, const i32* boostIcons) {
    icons_ = icons;
    for (u32 k = 0; k < kUpgradeCount; ++k) upgradeIcon_[k] = upgradeIcons[k];
    for (u32 k = 0; k < kBoostCount; ++k) boostIcon_[k] = boostIcons[k];
    for (u32 k = 0; k < kFusionCount; ++k) fusionIcon_[k] = icons ? icons->find(fusionInfo(static_cast<u8>(k)).icon) : -1;  // M5
}

void PlayView::resetEffects() {
    for (u32 i = 0; i < poolCap_; ++i) pools_[i].born = -100.0f;
    for (u32 i = 0; i < emberCap_; ++i) embers_[i].born = -100.0f;
    for (u32 i = 0; i < shatterCap_; ++i) shatter_[i].born = -100.0f;
    for (u32 i = 0; i < ringCap_; ++i) rings_[i].born = -100.0f;
    for (ShockRing& s : shock_) s.start = -100.0f;
    for (Bolt& b : bolts_) b.born = -100.0f;  // M5
    for (Flash& f : flashes_) f.t = -100.0f;    // 2026-11-14
    for (Debris& b : debris_) b.born = -100.0f;
    hzBurstT_ = -100.0f;
    floorReady_ = false;  // M8 TRACERS: the floor, the words, the house and the count start afresh
    houseT_ = houseDarkT_ = countT_ = -100.0f;
    for (RingWords& r : ringWords_) r.t = -100.0f;
    for (u32 k = 0; k < sim::World::kArenaBodies; ++k) {
        vaultQuarters_[k] = 0;
        vaultAlive_[k] = false;
        vaultPlaced_[k] = -1.0;
    }
    cellHitT_ = rpWaveT_ = -100.0f;
    bigShake_ = 0.0f;
    rpBurstTick_ = rpDeathTick_ = -1.0;
    for (u32 p = 0; p < kMaxPlayers; ++p) invulPrev_[p] = invulMax_[p] = 0.0f;
    sparkReqCount_ = 0;
    trailCount_ = 0;
    for (u32 p = 0; p < kMaxPlayers; ++p) {
        dash_[p].t = -100.0f;
        novaPressT_[p] = novaBlastT_[p] = novaFrontT_[p] = -100.0f;
        mateTrailCount_[p] = 0;
        reviveT_[p] = -100.0f;
        leaveT_[p] = -100.0f;
        crescPrev_[p] = 0;
        crescStepT_[p] = crescBreakT_[p] = aegisHitT_[p] = -100.0f;
    }
    allDownT_ = -100.0f;
    allDownRest_ = 0.0f;
    hitT_ = pickupT_ = -100.0f;
}

float PlayView::beatAmount() const { return reduceFlashing_ ? look_->reduceFlashingScale : 1.0f; }

float PlayView::chainPulse(double t) const {
    const float age = static_cast<float>(t) - pickupT_;
    return age >= 0.0f && age < 0.4f ? std::exp(-age / 0.12f) : 0.0f;
}

// ---------------------------------------------------------------------------------------------- effects

void PlayView::addPool(Vec2 at, float r0, float r1, Vec3 colour, float i0, float i1, float life, bool kill, u8 fade,
                       bool sparkle, bool dimmed, float t) {
    if (kill) {
        // M5 (look.md: an area is drawn by its edge, and no fill is brighter than the bullets inside): a kill inside a live
        // kill pool renews that pool instead of stacking another on it. Part 4's Feedback pictures had a crowd killed in one
        // place sum a dozen pools into a cream-white disc a tenth of the screen wide that hid the bullets in it.
        for (u32 i = 0; i < poolCap_; ++i) {
            Pool& p = pools_[i];
            if (!p.kill || t - p.born >= p.life) continue;
            const float r = p.r0 + (p.r1 - p.r0) * clampf((t - p.born) / p.life, 0.0f, 1.0f);
            if (lengthSq(at - p.at) >= r * r) continue;
            p.r0 = r;
            p.r1 = std::fmax(p.r1, r1);
            p.i0 = std::fmax(p.i0, i0);
            p.i1 = std::fmax(p.i1, i1);
            p.born = t;
            p.life = life;
            return;
        }
        // At most `killPools` kill pools: the oldest fades out early (look.md §3).
        u32 alive = 0, oldest = poolCap_;
        float oldestBorn = 1e30f;
        for (u32 i = 0; i < poolCap_; ++i) {
            const Pool& p = pools_[i];
            if (!p.kill || t - p.born >= p.life) continue;
            ++alive;
            if (p.born < oldestBorn) {
                oldestBorn = p.born;
                oldest = i;
            }
        }
        if (alive >= tier_.killPools && oldest < poolCap_) {
            Pool& p = pools_[oldest];
            const float remaining = t - p.born + 0.1f;
            if (remaining < p.life) p.life = remaining;
        }
    }
    Pool& p = pools_[poolHead_];
    poolHead_ = (poolHead_ + 1) % poolCap_;
    p.at = at;
    p.r0 = r0;
    p.r1 = r1;
    p.i0 = i0;
    p.i1 = i1;
    p.born = t;
    p.life = life;
    p.colour = colour;
    p.kill = kill;
    p.fade = fade;
    p.sparkle = sparkle;
    p.dimmed = dimmed;
}

void PlayView::addShockRings(Vec2 at, u32 count, float toWu, Vec3 a, Vec3 b, float t, float lifeS) {
    // At most maxShockRings alive: a newer ring replaces the oldest (look.md §3, §6).
    const u32 cap = look_->boom.maxShockRings < kMaxShockRings ? look_->boom.maxShockRings : kMaxShockRings;
    const LookExplosion& x = look_->boom.medium;
    for (u32 k = 0; k < count; ++k) {
        u32 slot = 0;
        float oldest = 1e30f;
        for (u32 i = 0; i < cap; ++i) {
            if (shock_[i].start < oldest) {
                oldest = shock_[i].start;
                slot = i;
            }
        }
        ShockRing& r = shock_[slot];
        r.at = at;
        r.start = t + x.ringGapS * static_cast<float>(k);
        r.toWu = toWu;
        r.life = lifeS > 0.0f ? lifeS : (x.ringS > 0.0f ? x.ringS : 0.2f);
        r.a = a;
        r.b = b;
    }
}

void PlayView::addDistortion(Vec2 at, float toWu, float lifeS, float amountRefPx, float widthRefPx, float t) {
    DistortRing& r = rings_[ringHead_];
    ringHead_ = (ringHead_ + 1) % ringCap_;
    r = {at, t, lifeS, toWu, amountRefPx, widthRefPx};
}

void PlayView::sparkRequest(const SparkReq& r) {
    if (sparkReqCount_ < sparkReqCap_) {
        sparkReqs_[sparkReqCount_++] = r;
        return;
    }
    // Play test 3 (GAME_DESIGN 7.6): a boss hit's sparks take the place of the newest ordinary request.
    if (!r.prio) return;
    for (u32 k = sparkReqCount_; k-- > 0;)
        if (!sparkReqs_[k].prio) {
            sparkReqs_[k] = r;
            return;
        }
}

void PlayView::shatterGlyph(int g, Vec2 at, float heading, float scale, Vec3 colour, float speedLo, float speedHi, float t) {
    // The glyph's own hull segments fly apart as tumbling capsules (look.md §5 shatter).
    if (g < 0) return;
    const GlyphDef& gd = glyphs_->def(static_cast<u32>(g));
    const GlyphSegment* segs = glyphs_->segments() + gd.firstSegment;
    const float c = std::cos(heading), s = std::sin(heading);
    for (u32 k = 0; k < gd.hullCount; ++k) {
        Shatter& p = shatter_[shatterHead_];
        shatterHead_ = (shatterHead_ + 1) % shatterCap_;
        const Vec2 a = rotate(segs[k].a, c, s) * scale, b = rotate(segs[k].b, c, s) * scale;
        const Vec2 mid = (a + b) * 0.5f;
        const float ml = length(mid);
        p.a = a - mid;
        p.b = b - mid;
        p.centre = at + mid;
        p.v = (ml > 1e-3f ? mid * (1.0f / ml) : dir(rng_.range(0.0f, kTau))) * rng_.range(speedLo, speedHi);
        p.spin = rng_.range(-9.0f, 9.0f);
        p.born = t;
        p.life = look_->boom.shatterS;
        p.colour = colour;
        p.dimmed = true;
    }
}

void PlayView::shatterArcs(Vec2 at, u32 slots, u32 from, u32 to, Vec3 colour, float t) {
    if (slots < 1 || slots > 8 || gHalo_[slots] < 0 || from >= to) return;
    const GlyphDef& gd = glyphs_->def(static_cast<u32>(gHalo_[slots]));
    const GlyphSegment* segs = glyphs_->segments() + gd.firstSegment;
    const u32 per = gd.segmentCount / slots;
    const float rot = 0.3f * t, c = std::cos(rot), s = std::sin(rot);  // as the halo is drawn (render: haloRot)
    const float scale = kStandardEnemyWu * glyphs_->def(static_cast<u32>(gShip_)).size;
    for (u32 k = from * per; k < to * per && k < gd.segmentCount; ++k) {
        Shatter& p = shatter_[shatterHead_];
        shatterHead_ = (shatterHead_ + 1) % shatterCap_;
        const Vec2 a = rotate(segs[k].a, c, s) * scale, b = rotate(segs[k].b, c, s) * scale;
        const Vec2 mid = (a + b) * 0.5f;
        const float ml = length(mid);
        p.a = a - mid;
        p.b = b - mid;
        p.centre = at + mid;
        p.v = (ml > 1e-3f ? mid * (1.0f / ml) : dir(rng_.range(0.0f, kTau))) * rng_.range(160.0f, 320.0f);
        p.spin = rng_.range(-9.0f, 9.0f);
        p.born = t;
        p.life = look_->boom.shatterS;
        p.colour = colour;
        p.dimmed = false;
    }
}

void PlayView::killRecipe(Vec2 at, u8 size, u8 hue, float heading, int g, float radius, float t, i32 owner) {
    // look.md §5: small (tier I-II), medium (III-IV), large (V, elites).
    const Look::Explosions& boom = look_->boom;
    const LookExplosion& x = size == 0 ? boom.small : (size == 1 ? boom.medium : boom.large);
    const float density = tier_.particleDensity * density_;
    SparkReq r{};
    r.at = at;
    r.count = static_cast<u16>(rng_.range(x.sparks[0], x.sparks[1]) * density + 0.5f);
    r.kind = size == 2 ? 1u : (size == 1 ? 5u : 0u);
    r.a = palette_->hue[hue];
    r.b = palette_->partner[hue];
    r.t0 = t;
    sparkRequest(r);
    // The kill flash: a white sparkle for small kills, the white pool for medium and large (look.md §3).
    if (size == 0) {
        const LookSoftLight& f = look_->soft.killFlashSmall;
        addPool(at, f.radius[0], f.radius[0], white(), f.intensity[0], f.intensity[1], f.lifeS, false, f.squaredFade ? 1 : 0,
                true, true, t);
    } else {
        const LookSoftLight& f = look_->soft.killFlash;
        const float rr = f.radius[0] * (size == 2 ? f.largeScale : 1.0f);
        addPool(at, rr, rr, white(), f.intensity[0], f.intensity[1], f.lifeS, false, f.squaredFade ? 1 : 0, false, true, t);
        // The kill pool lives as long as the recipe's longest spark (as in the M1 stage and crowd.py).
        const LookSoftLight& pl = size == 1 ? look_->soft.killPoolMedium : look_->soft.killPoolLarge;
        addPool(at, pl.radius[0], pl.radius[1], owner >= 0 ? palettes_->player[owner] : palette_->hue[hue], pl.intensity[0],
                pl.intensity[1], x.lifeS[1], true, 0, false, true, t);
        for (u32 k = 0; k < x.embers; ++k) {
            Ember& m = embers_[emberHead_];
            emberHead_ = (emberHead_ + 1) % emberCap_;
            m.p0 = at;
            m.v0 = dir(rng_.range(0.0f, kTau)) * rng_.range(20.0f, 70.0f);
            m.born = t;
            m.life = x.emberLifeS;
            m.colour = palette_->partner[hue];
        }
    }
    if (x.rings) addShockRings(at, x.rings, x.ringToWu, palette_->hue[hue], palette_->partner[hue], t);
    push(kImpKill, at, x.gridImpulse * crowdGain_, size == 0 ? 60.0f : (size == 1 ? 90.0f : 130.0f), t);
    if (x.shakeRefPx > shakeAmp_) shakeAmp_ = x.shakeRefPx;
    if (size == 2) {
        shatterGlyph(g, at, heading, radius, mix(palette_->hue[hue], white(), 0.5f), 80.0f, 200.0f, t);
        const LookDistortionSource& ds = look_->distortion.largeKill;
        addDistortion(at, 200.0f, ds.lifeS, ds.refPx, ds.widthRefPx, t);
    }
}

void PlayView::onEvents(const World& w, double tickEnd) {
    const SpeciesTable& table = w.species();
    if (!speciesGlyphsReady_) {
        for (u32 i = 0; i < SpeciesTable::kCount; ++i) {
            const Species& s = table.get(static_cast<u16>(i));
            // (M5 part 4: a Wyrm's body is wyrm_body_N, its head wyrm_head_N)
            const char* fam = s.family == Family::Wyrm ? "wyrm_body" : familyName(s.family);
            int g = glyphs_->enemy(fam, s.tier);
            for (u32 t = 1; g < 0 && t <= 5; ++t) g = glyphs_->enemy(fam, t);
            speciesGlyph_[i] = g >= 0 ? g : gRing32_;
            if (s.family == Family::Wyrm) {
                int h = glyphs_->enemy("wyrm_head", s.tier);
                wyrmHeadGlyph_[i < SpeciesTable::kCount ? i : 0] = h >= 0 ? h : speciesGlyph_[i];
            }
        }
        speciesGlyphsReady_ = true;
    }
    const u32 me = w.viewPlayer();
    if (!buildReady_) buildColours(palettes_->player[me]);
    lat_ = w.onStage() ? &stageLattice_ : &lattice_;  // (M8: the Stage's lattice takes its impulses)
    for (u32 k = 0; k < w.eventCount(); ++k) {
        const Event& e = w.events()[k];
        const float t = static_cast<float>(tickEnd - e.age);
        const Vec2 at{e.x, e.y};
        // M4 (look.md 6): a player's effects are in that player's colours (solo: player 0, the local one).
        const u32 owner = e.a < kMaxPlayers ? e.a : me;
        const Vec3 player = palettes_->player[owner], light = palettes_->playerLight[owner], deep = palettes_->playerDeep[owner];
        const bool mine = owner == me;
        switch (e.type) {
            case EventType::Kill: {
                const Species& s = table.get(e.species);
                if (e.c) {  // a warp-in dissolved by a Nova: a small flash only
                    const LookSoftLight& f = look_->soft.killFlashSmall;
                    addPool(at, f.radius[0], f.radius[0], palette_->hue[s.hue], f.intensity[0], f.intensity[1], f.lifeS, false,
                            0, true, true, t);
                    break;
                }
                // Co-op: the kill pool in the killer's colour (look.md 6); solo keeps the enemy's hue.
                killRecipe(at, e.a, s.hue, e.f, speciesGlyph_[e.species], s.radius, t,
                           w.playerCount() > 1 && e.b < kMaxPlayers ? static_cast<i32>(e.b) : -1);
                break;
            }
            case EventType::Impact: {
                // A shot that hit without killing: 3-4 sparks back along it, 0.15 s (look.md §5).
                if (e.species == kBossSpecies) {
                    eventB(w, e, t);
                    break;
                }
                const Species& s = table.get(e.species);
                SparkReq r{};
                r.at = at;
                r.dir = Vec2{-e.x2, -e.y2};
                r.count = static_cast<u16>(look_->boom.impactSparks[0] + rng_.below(2));
                r.kind = 3;
                r.a = palette_->hue[s.hue];
                r.b = palette_->partner[s.hue];
                r.t0 = t;
                // Play test 2 (look.md 5): the sparks follow the build: a Prism lane's colour, else Overcharge's head; at
                // Overcharge 5 the impact flash doubles.
                u32 over = 0;
                if (e.b < kMaxPlayers) {
                    over = w.player(e.b).loadout.mods[kOvercharge] > 5 ? 5u : w.player(e.b).loadout.mods[kOvercharge];
                    if (e.c >= 1 && e.c <= 5 && e.b == me) {
                        r.a = laneColour_[e.c - 1];
                        r.b = white();
                    } else if (over >= 1) {
                        r.a = overHead_[over];
                        r.b = over == 3 ? buildGold_ : palette_->hue[s.hue];
                    }
                }
                sparkRequest(r);
                if (over >= 1) {
                    const LookSoftLight& f = look_->soft.killFlashSmall;
                    const float k = over >= 5 ? 2.0f : 1.0f;
                    addPool(at, f.radius[0] * 0.6f * k, f.radius[0] * 0.6f * k, mix(overHead_[over], white(), 0.5f), f.intensity[0] * 0.5f * k,
                            f.intensity[1], f.lifeS, false, f.squaredFade ? 1 : 0, true, true, t);
                }
                break;
            }
            case EventType::Shot: {
                // Muzzle flash (GAME_DESIGN 11.1, lead notes). Play test 2 (look.md 5): in the head's colour by Overcharge; a
                // spark flies off every second shot at 3 stacks, every shot at 4 and 5.
                const u32 over = w.player(owner).loadout.mods[kOvercharge] > 5 ? 5u : w.player(owner).loadout.mods[kOvercharge];
                const Vec3 flash = over == 0 ? mix(player, white(), 0.5f) : mix(overHead_[over], white(), 0.35f);
                addPool(at, kMuzzleFlashRefPx, kMuzzleFlashRefPx, flash, kMuzzleFlash * (mine ? 1.0f : 0.6f), 0.0f, kMuzzleFlashS, false, 1,
                        true, false, t);
                if (over >= 3 && (over >= 4 || (shotSparkTurn_[owner] & 1u) == 0)) {
                    // Off to the side of the shot, alternately left and right.
                    const float side = (shotSparkTurn_[owner] & 2u) ? 1.0f : -1.0f;
                    SparkReq r{};
                    r.at = at;
                    r.dir = normalizeOr(Vec2{e.x2, e.y2} * 0.5f + perp(Vec2{e.x2, e.y2}) * side, Vec2{e.x2, e.y2});
                    r.count = 1;
                    r.kind = 3;
                    r.a = over == 3 ? buildGold_ : overHead_[over];
                    r.b = white();
                    r.t0 = t;
                    sparkRequest(r);
                }
                ++shotSparkTurn_[owner];
                break;
            }
            case EventType::WarpLand: {
                // Solid on the beat, with the small white flash (look.md §5 warp-in).
                const LookSoftLight& f = look_->soft.killFlashSmall;
                addPool(at, f.radius[0], f.radius[0], white(), f.intensity[0], f.intensity[1], f.lifeS, false,
                        f.squaredFade ? 1 : 0, true, true, t);
                lastWarpLandT_ = t;
                break;
            }
            case EventType::Pickup: pickupT_ = t; break;
            case EventType::Hit: {
                // Shield cell lost (look.md §5): the arc shatters outward, a panic ring pushes out.
                if (mine) hitT_ = t;
                if (e.c == 1 || e.c == 2) {
                    // Play test 3: the Orbitals' guard ring took it (its shards fly, no arc is lost), or an Extra Life was spent
                    // (a gold ring: the ship is back with its cells).
                    if (e.c == 1) ringBreak(w, owner, at, t);
                    else addShockRings(at, 2, kPanicRadiusWu, palettes_->player[3], white(), t, kPanicRingS);
                    addShockRings(at, 1, kPanicRadiusWu, player, light, t, kPanicRingS);
                    push(kImpHit, at, 260.0f, 120.0f, t);
                    if (mine && kHitShakeRefPx > shakeAmp_) shakeAmp_ = kHitShakeRefPx;
                    break;
                }
                const u8 left = e.b;
                // The lost arc, the last of the halo it was (play test 2: the halo glyphs are ornament, which shatterGlyph skips).
                if (left < 8) shatterArcs(at, left + 1u, left, left + 1u, mix(player, white(), 0.3f) * 2.0f, t);
                addShockRings(at, 1, kPanicRadiusWu, player, light, t, kPanicRingS);
                // (2026-11-14, the owner: "visual effects when you are damaged are not visible enough"): a danger-coloured ring
                // and sparks too, the hull white for 0.1 s and the frame's edges flashing (play_view_feedback.cpp), a harder jolt.
                addShockRings(at, 1, 1.5f * kPanicRadiusWu, palettes_->danger, white(), t, 1.4f * kPanicRingS);
                {
                    SparkReq r{};
                    r.at = at;
                    r.count = mine ? 26 : 12;
                    r.kind = 5;
                    r.a = palettes_->danger;
                    r.b = white();
                    r.t0 = t;
                    sparkRequest(r);
                }
                if (mine) cellHitT_ = t;
                push(kImpHit, at, 340.0f, 140.0f, t);
                if (mine && kCellHitShakeRefPx > shakeAmp_) shakeAmp_ = kCellHitShakeRefPx;
                break;
            }
            case EventType::Down: {
                // M4 (look.md 6): the ship shatters with the large kill recipe in its own colour; the beacon stays.
                const LookExplosion& x = look_->boom.large;
                SparkReq r{};
                r.at = at;
                r.count = static_cast<u16>(x.sparks[1] * tier_.particleDensity * density_);
                r.kind = 1;
                r.a = player;
                r.b = light;
                r.t0 = t;
                sparkRequest(r);
                shatterGlyph(gShip_, at, rng_.range(0.0f, kTau), kStandardEnemyWu * 1.4f, mix(player, white(), 0.4f) * 2.0f, 60.0f,
                             260.0f, t);
                const LookSoftLight& f = look_->soft.killFlash;
                addPool(at, f.radius[0] * f.largeScale, f.radius[0] * f.largeScale, white(), f.intensity[0], f.intensity[1], f.lifeS,
                        false, f.squaredFade ? 1 : 0, false, true, t);
                const LookSoftLight& kp = look_->soft.killPoolLarge;
                addPool(at, kp.radius[0], kp.radius[1], player, kp.intensity[0], kp.intensity[1], x.lifeS[1], true, 0, false, true, t);
                if (x.rings) addShockRings(at, x.rings, x.ringToWu, player, light, t);
                push(kImpShipDown, at, x.gridImpulse, 130.0f, t);
                if (mine && x.shakeRefPx > shakeAmp_) shakeAmp_ = x.shakeRefPx;
                break;
            }
            case EventType::Revive: {
                // M4 (look.md 6): the ring closes: a white flash, a shock ring 90 -> 260 wu in 0.2 s, eight sparks.
                const u32 p = e.a < kMaxPlayers ? e.a : 0u;
                reviveT_[p] = t;
                reviveFill_[p] = e.b != 0;
                addShockRings(at, 1, 260.0f, player, light, t, 0.2f);
                SparkReq r{};
                r.at = at;
                r.count = 8;
                r.kind = 0;
                r.a = player;
                r.b = white();
                r.t0 = t;
                sparkRequest(r);
                break;
            }
            case EventType::Death: {
                if (w.playerCount() > 1) {
                    // M4 (look.md 6): all down. The simulation's time dips, the lattice drains, then game over.
                    allDownT_ = t;
                    break;
                }
                SparkReq r{};
                r.at = at;
                r.count = static_cast<u16>(look_->boom.large.sparks[1] * tier_.particleDensity * density_);
                r.kind = 8;
                r.a = player;
                r.b = deep;
                r.t0 = t;
                sparkRequest(r);
                shatterGlyph(gShip_, at, rng_.range(0.0f, kTau), kStandardEnemyWu * 1.4f, mix(player, white(), 0.4f) * 2.0f, 60.0f,
                             260.0f, t);
                addPool(at, 60.0f, 220.0f, player, 0.6f, 0.0f, 0.8f, false, 0, false, false, t);
                addShockRings(at, 3, 260.0f, player, light, t);
                push(kImpDeath, at, 420.0f, 200.0f, t);
                if (kDeathShakeRefPx > shakeAmp_) shakeAmp_ = kDeathShakeRefPx;
                break;
            }
            case EventType::NovaPress: {
                novaPressT_[owner] = t;
                novaAt_[owner] = at;
                const float tpS = w.ticksPerSecond();
                novaBlastT_[owner] = t + static_cast<float>((e.tick - (w.music() - static_cast<double>(e.age) * tpS)) / tpS);
                // Sparks appear on a 600 wu ring and fall inward until the beat (look.md §5).
                SparkReq r{};
                r.at = at;
                r.count = static_cast<u16>(kNovaInhaleSparks * (tier_.particleDensity > 0.5f ? 1.0f : 0.6f));
                r.kind = 6;
                r.a = player;
                r.b = light;
                r.t0 = t;
                r.extra = novaBlastT_[owner] - t;
                sparkRequest(r);
                break;
            }
            case EventType::NovaBlast: {
                // The thick white front to 1400 wu in 0.35 s with 400 radial sparks and the bloom pump.
                novaFrontT_[owner] = t;
                novaAt_[owner] = at;
                SparkReq r{};
                r.at = at;
                r.count = static_cast<u16>(kNovaFrontSparks * tier_.particleDensity * density_);
                r.kind = 7;
                r.a = white();
                r.b = player;
                r.t0 = t;
                sparkRequest(r);
                const LookDistortionSource& n = look_->distortion.nova;
                addDistortion(at, n.toWu > 0.0f ? n.toWu : tuning::kNovaFrontWu, n.lifeS, n.refPx, n.widthRefPx, t);
                for (int i = 0; i < 2; ++i) push(kImpNova, at, 420.0f, 260.0f, t);
                if (mine && kNovaShakeRefPx > shakeAmp_) shakeAmp_ = kNovaShakeRefPx;
                break;
            }
            case EventType::Dash: {
                Dash& ds = dash_[owner];
                ds.from = at;
                ds.to = Vec2{e.x2, e.y2};
                ds.t = t;
                ds.heading = angleOf(ds.to - ds.from);
                ds.perfect = e.b != 0;
                if (ds.perfect) addShockRings(ds.to, 1, 90.0f, player, light, t);
                push(kImpDash, ds.to, 120.0f, 70.0f, t);
                break;
            }
            case EventType::Restart: resetEffects(); break;
            case EventType::RingLock:
            case EventType::RingPop:
            case EventType::House:
            case EventType::TilesLost:
            case EventType::FloorCount:
            case EventType::GelTake: tracersEvent(w, e, t); break;  // M8 TRACERS (play_view_modes.cpp)
            default: eventB(w, e, t); break;
        }
    }
}

// ---------------------------------------------------------------------------------------------- drawing helpers

bool PlayView::visible(Vec2 w, float marginWu) const {
    const Vec2 d = w - cam_;
    return std::fabs(d.x) <= 0.5f * viewW_ + marginWu && std::fabs(d.y) <= 0.5f * viewH_ + marginWu;
}

void PlayView::glyph(GlyphBatch& b, int g, bool hullOnly, Vec2 pos, float rot0, float rot1, float scaleWu, float intensity,
                     float halfWidthRef, GlyphMode mode, Vec3 a, Vec3 c) {
    if (g < 0) return;
    GlyphInstanceGpu* gi = b.push(static_cast<u32>(g), hullOnly);
    if (!gi) return;
    GlyphInstanceGpu v;
    v.x = pos.x;
    v.y = pos.y;
    v.rot[0] = floatToHalf(std::cos(rot0));
    v.rot[1] = floatToHalf(std::sin(rot0));
    v.rot[2] = floatToHalf(std::cos(rot1));
    v.rot[3] = floatToHalf(std::sin(rot1));
    v.shape[0] = floatToHalf(scaleWu);
    v.shape[1] = floatToHalf(intensity);
    v.shape[2] = floatToHalf(halfWidthRef);
    v.shape[3] = floatToHalf(static_cast<float>(mode));
    v.a = packLinear(a);
    v.b = packLinear(c);
    std::memcpy(gi, &v, sizeof v);
}

void PlayView::hullEveryOther(FrameDraw& d, int g, Vec2 pos, float rot, float scale, float intensity, float halfWidthRef, Vec3 hue, Vec3 partner,
                              bool white) {
    // The hull's even segments as capsules, in the enemy hull's field (shaders/glyph.vert mode 0, layer 0) or white: a Phasing
    // elite off its solid half (M6, look.md 4).
    if (g < 0) return;
    const GlyphDef& gd = glyphs_->def(static_cast<u32>(g));
    const GlyphSegment* seg = glyphs_->segments() + gd.firstSegment;
    const float c = std::cos(rot), s = std::sin(rot), ext = std::fmax(gd.extent, 1e-3f);
    const Vec3 W{1.0f, 1.0f, 1.0f};
    const auto field = [&](Vec2 p) {
        if (white) return W * intensity;
        const auto stops3 = [](float t, Vec3 a, Vec3 b, Vec3 e) {
            t = clampf(t, 0.0f, 1.0f) * 2.0f;
            return t < 1.0f ? mix(a, b, t) : mix(b, e, t - 1.0f);
        };
        return (gd.facesHeading ? stops3((ext - p.x) / (2.0f * ext), mix(hue, W, 0.4f), hue, partner)
                                : stops3(length(p) / ext, partner, hue, mix(hue, W, 0.25f))) *
               intensity;
    };
    for (u32 k = 0; k < gd.hullCount; k += 2) {
        const Vec2 a = seg[k].a, b = seg[k].b;
        const Vec2 wa = pos + Vec2{a.x * c - a.y * s, a.x * s + a.y * c} * scale, wb = pos + Vec2{b.x * c - b.y * s, b.x * s + b.y * c} * scale;
        d.scene.line(toScene(wa), toScene(wb), halfWidthRef * refPx_, field(a), field(b), 0);
    }
}

void PlayView::flare(CapsuleBatch& out, Vec2 at, float angle, float lengthWu, u32 arms, Vec3 colour, float halfWidthRef) {
    // Star streaks through a centre that taper to nothing (look.md §4 flares, fade exponent 1.6).
    const float halfW = halfWidthRef * refPx_;
    const float mid = std::pow(0.5f, look_->flareFadeExponent);
    for (u32 k = 0; k < arms; ++k) {
        const Vec2 d = dir(angle + kPi * static_cast<float>(k) / static_cast<float>(arms));
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            const Vec2 m = at + d * (0.5f * lengthWu * static_cast<float>(sgn));
            const Vec2 tip = at + d * (lengthWu * static_cast<float>(sgn));
            out.line(toScene(at), toScene(m), halfW, colour, colour * mid, kFlatEnd);
            out.line(toScene(m), toScene(tip), halfW, colour * mid, Vec3{}, kFlatStart);
        }
    }
}

void PlayView::telegraphLine(FrameDraw& d, Vec2 a, Vec2 b, double fireTick, float sceneLight) {
    // look.md §5 (0449eb3, the owner: "the red dotted line before a laser needs to be transparent"): a tell is a warning, not a
    // threat, so it is see-through: in the scene light only, never on the priority layer (no dark rim, no opaque core), its
    // dashes 1.5 px in the danger colour at 0.35, so the lattice, enemies and shots under it show through; over the charge's last
    // beat it rises to 0.6. Every laser's and lethal beam's tell, the Longwave lunge's line and every other dashed tell come
    // here whole and are dashed here (a piece shorter than a dash is one dash).
    if (!shown(kMuteBullet)) return;
    const float len = length(b - a);
    if (len < 1e-3f) return;
    const float rise = fireTick >= 0.0 ? clampf(1.0f - static_cast<float>((fireTick - tellNow_) / kTicksPerBeat), 0.0f, 1.0f) : 0.0f;
    const Vec3 c = palettes_->danger * ((kTellLevel + (kTellLevelFiring - kTellLevel) * rise) * sceneLight);
    const float hw = 0.5f * kTellWidthRefPx * refPx_;
    const Vec2 dv = (b - a) * (1.0f / len);
    for (float u = 0.0f; u < len; u += kTellDashWu + kTellGapWu) {
        const Vec2 p0 = a + dv * u, p1 = a + dv * std::fmin(u + kTellDashWu, len);
        if (!visible(p0, 20.0f) && !visible(p1, 20.0f)) continue;
        d.scene.line(toScene(p0), toScene(p1), hw, c, c, 0);
    }
}

void PlayView::tellRing(FrameDraw& d, Vec2 c, float r, double fireTick) {
    // A tell's moving front (a Volatile bomb's and a meteor's ring filling in from its edge): a tell, so see-through as the dashed
    // ones are (look.md §5, 0449eb3): 1.5 px of danger in the scene light only, at the tells' level rising over the last beat;
    // solid, since it moves (it had been a solid line on the priority layer: noticed in af4b101's review, changed 2026-11-14).
    if (!shown(kMuteBullet) || r <= 0.5f || !visible(c, r + 20.0f)) return;
    const float rise = fireTick >= 0.0 ? clampf(1.0f - static_cast<float>((fireTick - tellNow_) / kTicksPerBeat), 0.0f, 1.0f) : 0.0f;
    const Vec3 col = palettes_->danger * (kTellLevel + (kTellLevelFiring - kTellLevel) * rise);
    const float hw = 0.5f * kTellWidthRefPx * refPx_;
    const u32 n = r > 120.0f ? 40u : 24u;
    for (u32 k = 0; k < n; ++k) {
        const float a0 = 6.28318530718f * static_cast<float>(k) / n, a1 = 6.28318530718f * static_cast<float>(k + 1) / n;
        d.scene.line(toScene(c + Vec2{std::cos(a0), std::sin(a0)} * r), toScene(c + Vec2{std::cos(a1), std::sin(a1)} * r), hw, col, col, 0);
    }
}

void PlayView::tellMark(FrameDraw& d, Vec2 a, Vec2 b, float sceneLight) {
    // A glint or a chevron (look.md §5): danger in the scene at the telegraph line's level (look.md §3), never on the priority
    // layer (0449eb3: a tell is see-through).
    if (!shown(kMuteBullet)) return;
    const LookLine& tl = look_->lines.telegraph;
    const Vec3 c = palettes_->danger * (tl.intensity * sceneLight);
    d.scene.line(toScene(a), toScene(b), 0.5f * tl.width * refPx_, c, c, 0);
}

void PlayView::envelopes(const ViewInput& in) {
    // look.md §7: one envelope per drum. Heard events while the band plays; else from the musical
    // position (kick on every beat, snare or clap on 2 and 4, hats on the off-beat eighths).
    const float t = static_cast<float>(in.time);
    if (in.beat && in.beat->running) {
        using audio::BeatKind;
        const BeatView& v = *in.beat;
        kick_ = v.envelope(BeatKind::Kick, look_->kickDecayMs * 1e-3f);
        // M5 (Tracks 7, 9 and 10 have no kick on every beat): "on the beat" is the clock's beat, never the kick.
        beat_ = v.beat(look_->kickDecayMs * 1e-3f);
        snare_ = std::fmax(v.envelope(BeatKind::Clap, look_->snareDecayMs * 1e-3f), v.envelope(BeatKind::Snare, look_->snareDecayMs * 1e-3f));
        hat_ = v.envelope(BeatKind::Hat, look_->hatDecayMs * 1e-3f);
        bass_ = v.envelope(BeatKind::Bass, 0.18f);
        const double kickAge = v.since(BeatKind::Kick);
        if (t - kickAge > lastKickT_ + 0.05) {
            prevKickT_ = lastKickT_;
            lastKickT_ = t - kickAge;
        }
        lastHatT_ = t - v.since(BeatKind::Hat);
        return;
    }
    const float tps = in.bpm * 96.0f / 60.0f;
    const double beat = std::floor(in.music / kTicksPerBeat);
    const float sinceBeat = static_cast<float>((in.music - beat * kTicksPerBeat) / tps);
    lastKickT_ = t - sinceBeat;
    prevKickT_ = lastKickT_ - 96.0f / tps;
    kick_ = std::exp(-sinceBeat * 1000.0f / look_->kickDecayMs);
    beat_ = kick_;  // (no band: the musical position's beat)
    const i64 b =static_cast<i64>(beat);
    const i64 inBar = ((b % 4) + 4) % 4;
    const float sinceSnare = sinceBeat + ((inBar % 2) == 0 ? 96.0f / tps : 0.0f);  // the last of beats 2 and 4
    snare_ = b >= 1 ? std::exp(-sinceSnare * 1000.0f / look_->snareDecayMs) : 0.0f;
    const double hatTick = std::floor((in.music - 48.0) / kTicksPerBeat) * kTicksPerBeat + 48.0;
    const float sinceHat = static_cast<float>((in.music - hatTick) / tps);
    lastHatT_ = t - sinceHat;
    hat_ = std::exp(-sinceHat * 1000.0f / look_->hatDecayMs);
    bass_ = std::exp(-sinceHat / 0.18f);
}

// ---------------------------------------------------------------------------------------------- render

void PlayView::render(FrameDraw& d, FrameParams& params, Background& bg, float W, float H, const ViewInput& in) {
    frameMute_ = in.backgroundOnly ? ~(kMuteBackground | kMuteGrid | kMuteVignette | kMuteAberration) : 0u;
    // The co-op game over (screens.md): the run behind at a fifth; the threat layer is drawn after the tone map, where
    // the exposure cannot dim it, so it is left out there (with the beacons).
    if (in.noBeacons) frameMute_ |= kMutePriority;
    const World& w = *in.world;
    const float t = static_cast<float>(in.time);
    now_ = in.time;
    tellNow_ = in.music;
    refPx_ = H / 1080.0f;
    const Vec3 W1 = white();
    const Vec3 player = palettes_->player[w.viewPlayer()], deep = palettes_->playerDeep[w.viewPlayer()], light = palettes_->playerLight[w.viewPlayer()];
    const Vec3 danger = palettes_->danger;
    const Vec3 accent = palette_->accent;
    const float a = in.alpha;
    const float tps = in.bpm * 96.0f / 60.0f;
    const float barS = 384.0f / tps;
    envelopes(in);
    const float beatAmt = beatAmount();
    lastBeatAmt_ = beatAmt;
    if (!speciesGlyphsReady_) onEvents(w, in.time);  // builds the species -> glyph table (no events yet)

    // ---- Camera (GAME_DESIGN 9): follows the ship with look-ahead along its velocity and a lean toward
    // its aim; zoom breathes with the crowd; the kick punches it a little (look.md §7).
    const Vec2 S = in.shipPos;
    Vec2 leanTarget{};
    bool bossNear = false;
    // M5: Longwave and the Choir pull the camera back to their page's zoom for the whole fight (longwave.md, choir.md
    // "Camera": 0.75, below look.json's zoomMin; the pages win), eased like every zoom; Longwave without the lean (its head
    // swims a path 3,000 wu wide).
    float fightZoom = 0.0f;
    if (w.boss().active && !w.boss().dying) {
        if (w.boss().kind == sim::BossKind::Longwave) fightZoom = w.species().longwave.zoom;
        if (w.boss().kind == sim::BossKind::Choir) fightZoom = w.species().choir.zoom;
        if (w.boss().kind == sim::BossKind::EventHorizon) fightZoom = w.species().horizon.zoom;  // M5 part 4 (horizon.json camera)
        if (w.boss().kind == sim::BossKind::Polyrhythm) fightZoom = w.species().poly.zoom;
        if (w.boss().kind == sim::BossKind::Feedback) fightZoom = w.species().feedback.zoom;
        if (w.boss().kind == sim::BossKind::Reprise) fightZoom = sim::prose::kRpZoom;  // (reprise.md "Camera": 0.7 since the wheel grew)
    }
    if (w.boss().active && !w.boss().dying && w.boss().kind != sim::BossKind::Longwave) {
        const Vec2 off = lerp2(w.boss().prevPos, w.boss().pos, a) - S;
        if (lengthSq(off) < kBossCamWu * kBossCamWu) {
            bossNear = true;
            leanTarget = off * kBossLeanShare;
            const float l = length(leanTarget);
            if (l > kBossLeanMaxWu) leanTarget = leanTarget * (kBossLeanMaxWu / l);
        }
    }
    const bool stage = w.onStage();
    lat_ = stage ? &stageLattice_ : &lattice_;
    if (stage) {
        // M8 (design/modes/README.md 4): the Stage's camera, fixed on its centre at the largest zoom at which the Stage and
        // fitMarginWu round it fit the screen; no look-ahead, no lean, no breathing (the kick's punch and the shake stay). (On a
        // 16:10 screen that zoom is under the page's 0.75: the whole room wins; PROGRESS, M8 part 4.)
        const ModesData::Stage& st = w.species().modes.stage;
        const float zx = W / (kRefPxPerWu * refPx_ * (st.widthWu + 2.0f * st.fitMarginWu));
        const float zy = H / (kRefPxPerWu * refPx_ * (st.heightWu + 2.0f * st.fitMarginWu));
        zoomBase_ = std::fmin(zx, zy);
        lookAhead_ = Vec2{};
        bossLean_ = Vec2{};
    } else {
        // The lead follows the course at cruising speed at most (a Dash does not swing the camera).
        const float sp = length(in.shipVel);
        Vec2 target = (sp > kPlayerSpeed ? in.shipVel * (kPlayerSpeed / sp) : in.shipVel) * kLookAheadS;
        if (in.shipFiring) target = target + in.shipAim * kAimBiasWu;
        const float k = 1.0f - std::exp(-in.dt / kLookAheadTau);
        lookAhead_ = lookAhead_ + (target - lookAhead_) * k;
        const float busy = clampf((stats_.load - kZoomLoadLo) / (kZoomLoadHi - kZoomLoadLo), 0.0f, 1.0f);
        const float zt = fightZoom > 0.0f ? fightZoom : (bossNear ? kBossZoom : kZoomCalm + (kZoomBusy - kZoomCalm) * busy);
        bossLean_ = bossLean_ + (leanTarget - bossLean_) * (1.0f - std::exp(-in.dt / kBossLeanTau));
        zoomBase_ += (zt - zoomBase_) * (1.0f - std::exp(-in.dt / kZoomTau));
        zoomBase_ = clampf(zoomBase_, fightZoom > 0.0f ? std::fmin(look_->zoomMin, fightZoom) : std::fmin(look_->zoomMin, zoomBase_), look_->zoomMax);
    }
    sectionLook(in);
    if (stage) sectionZoom_ = 1.0f;
    zoom_ = zoomBase_ * sectionZoom_ * (1.0f + look_->zoomKick * beatAmt * kick_);
    pxPerWu_ = kRefPxPerWu * refPx_ * zoom_;
    centre_ = {0.5f * W, 0.5f * H};
    viewW_ = W / pxPerWu_;
    viewH_ = H / pxPerWu_;
    cam_ = stage ? Vec2{} : (in.camFixed ? in.camPos : S + lookAhead_ + bossLean_);
    // The view may look up to 240 wu past a wall, no further.
    const float mx = kArenaHalfW + 240.0f - 0.5f * viewW_, my = kArenaHalfH + 240.0f - 0.5f * viewH_;
    if (!stage && mx > 0.0f) cam_.x = clampf(cam_.x, -mx, mx);
    if (!stage && my > 0.0f) cam_.y = clampf(cam_.y, -my, my);
    shakeAmp_ *= std::exp(-in.dt / 0.15f);
    bigShake_ *= std::exp(-in.dt / kBigShakeTauS);
    const float shakePx = (clampf(shakeAmp_, 0.0f, look_->shakeMaxRefPx) + clampf(bigShake_, 0.0f, kBigShakeMaxRefPx)) * refPx_;
    shake_ = shakePx > 0.05f ? dir(rng_.range(0.0f, kTau)) * shakePx : Vec2{};
    lat_->recentre(cam_);
    lat_->step(in.dt * w.timeScale());
    if (stage) tracersFloor(w, t);  // M8: the floor's light this frame
    stats_.zoom = zoom_;

    // ---- Light budget (look.md §3) from the previous frame's load.
    {
        const Look::Crowd& cr = look_->crowd;
        const float load = stats_.load;
        const float target = load > 1.0f ? clampf(std::pow(cr.restLoad / load, cr.exponent), cr.minGain, 1.0f) : 1.0f;
        const float follow = cr.followSeconds > 0.0f ? 1.0f - std::exp(-in.dt / cr.followSeconds) : 1.0f;
        crowdGain_ += (target - crowdGain_) * follow;
        swarmGain_ = std::pow(crowdGain_, cr.swarmExponent);
        stats_.crowdGain = crowdGain_;
        stats_.swarmGain = swarmGain_;
    }
    // M5 part 5 (arena.md, Eclipse): the scene's light x 0.15 between beats, back for the kick's flash (reduce flashing: x 0.4,
    // the flash adds 0.3); the players' own light untouched.
    eclipseK_ = 1.0f;
    if (in.world->eclipse())
        eclipseK_ = reduceFlashing_ ? prose::kArEclipseCalm + prose::kArEclipseCalmFlash * kick_ : prose::kArEclipseScene + (1.0f - prose::kArEclipseScene) * kick_;
    const float crowd = crowdGain_ * eclipseK_ * in.runLight, swarm = swarmGain_ * eclipseK_ * in.runLight;  // (M6: the transitions' run light)

    // ---- Sparks born since the last frame (stateless; TECH_SPEC 5.3).
    stats_.sparksBorn = stats_.sparksDropped = 0;
    if (d.particles) {
        const Look::Explosions& boom = look_->boom;
        const float levels[4] = {2.4f, 3.4f, 4.6f, 5.6f};
        // (play test 3: a boss hit's sparks first, so a full pool drops others)
        for (u32 pass = 0; pass < 2; ++pass)
        for (u32 r = 0; r < sparkReqCount_; ++r) {
            const SparkReq& q = sparkReqs_[r];
            if ((q.prio != 0) != (pass == 0)) continue;
            for (u32 k = 0; k < q.count; ++k) {
                ParticleGpu* sp = d.particles->spawn();
                if (!sp) {
                    stats_.sparksDropped += q.count - k;
                    break;
                }
                Vec2 at = q.at, v;
                float life, drag = boom.sparkDrag, halfW = 0.7f, t0 = q.t0, level = levels[rng_.below(4)] * crowd;
                Vec3 own = mix(mix(q.a, q.b, rng_.uniform()), W1, 0.4f * rng_.uniform());
                switch (q.kind) {
                    case 1: {  // large: a fast thin band and a slow thick band
                        const bool fast = (k & 1u) != 0;
                        v = dir(rng_.range(0.0f, kTau)) * (fast ? rng_.range(500.0f, boom.large.speedWu[1]) : rng_.range(boom.large.speedWu[0], 400.0f));
                        life = rng_.range(boom.large.lifeS[0], boom.large.lifeS[1]);
                        halfW = fast ? 0.55f : 1.0f;
                        break;
                    }
                    case 3:  // shot impact: back along the shot
                        v = (q.dir + dir(rng_.range(0.0f, kTau)) * 0.5f) * rng_.range(150.0f, 300.0f);
                        life = boom.impactLifeS;
                        break;
                    case 9: {  // play test 3 (GAME_DESIGN 7.6): armour's sparks, thrown sideways off the shot's line
                        const Vec2 side = Vec2{-q.dir.y, q.dir.x} * ((k & 1u) ? 1.0f : -1.0f);
                        v = (side + q.dir * 0.25f + dir(rng_.range(0.0f, kTau)) * 0.2f) * rng_.range(160.0f, 280.0f);
                        life = boom.impactLifeS;
                        level = 1.6f * crowd;
                        break;
                    }
                    case 6: {  // Nova inhale: from a 600 wu ring to the centre, arriving on the beat
                        const float ang = rng_.range(0.0f, kTau);
                        const float span = q.extra > 0.05f ? q.extra : 0.05f;
                        at = q.at + dir(ang) * kNovaInhaleRingWu * rng_.range(0.85f, 1.0f);
                        drag = 0.05f;
                        v = (q.at - at) * (1.0f / span);
                        life = span;
                        level = levels[rng_.below(4)];  // the player's: never dimmed
                        own = mix(mix(q.a, q.b, rng_.uniform()), W1, 0.3f);
                        break;
                    }
                    case 7:  // Nova front: radial, fast
                        v = dir(rng_.range(0.0f, kTau)) * rng_.range(600.0f, 1400.0f);
                        life = rng_.range(0.4f, 0.9f);
                        level = levels[rng_.below(4)];
                        own = mix(mix(q.a, q.b, rng_.uniform()), W1, 0.5f);
                        break;
                    case 8:  // the player's death
                        v = dir(rng_.range(0.0f, kTau)) * rng_.range(150.0f, 900.0f);
                        life = rng_.range(0.5f, 1.4f);
                        level = levels[rng_.below(4)];
                        break;
                    default: {  // 0 small, 5 medium
                        const LookExplosion& x = q.kind == 5 ? boom.medium : boom.small;
                        v = dir(rng_.range(0.0f, kTau)) * rng_.range(x.speedWu[0], x.speedWu[1]);
                        life = rng_.range(x.lifeS[0], x.lifeS[1]);
                        break;
                    }
                }
                ParticleGpu g;
                g.x = at.x;
                g.y = at.y;
                g.t0 = t0;
                g.motion[0] = floatToHalf(v.x);
                g.motion[1] = floatToHalf(v.y);
                g.motion[2] = floatToHalf(life);
                g.motion[3] = floatToHalf(drag);
                g.light[0] = floatToHalf(level * (shown(kMuteSparks) ? 1.0f : 0.0f));
                g.light[1] = floatToHalf(halfW);
                g.colour = packLinear(own);
                g.flags = 0;
                std::memcpy(sp, &g, sizeof g);
                ++stats_.sparksBorn;
            }
        }
    }
    sparkReqCount_ = 0;

    // ---- Background (look.md §7).
    const Vec3 bgEdge = palette_->bg0 * look_->bgLevel;
    const Vec3 bgCentre = palette_->bg1 * look_->bgLevel;
    const float bassLift = 1.0f + look_->backgroundBass * beatAmt * bass_;
    const float drift = look_->backgroundHueDriftDegrees * beatAmt / 60.0f * bass_;
    bg.edge = mix(bgEdge, accent * (bgEdge.x + bgEdge.y + bgEdge.z) * (1.0f / 3.0f), drift) * bassLift;
    bg.centre = mix(bgCentre, accent * (bgCentre.x + bgCentre.y + bgCentre.z) * (1.0f / 3.0f), drift) * bassLift;
    bg.cx = look_->bgCentre[0];
    bg.cy = look_->bgCentre[1];
    bg.radius = look_->bgRadius;
    bg.power = look_->bgPower;
    bg.cloudCount = 0;
    if (!shown(kMuteBackground) || eclipseK_ < 1.0f) bg.edge = bg.centre = Vec3{};  // (an Eclipse: the background off)
    if (tier_.background && shown(kMuteBackground) && eclipseK_ >= 1.0f) {
        const Vec2 pc = cam_ * look_->back.nebulaParallax;
        for (u32 i = 0; i < cloudCount_ && bg.cloudCount < 8 && tier_.nebula; ++i) {
            const Cloud& c = clouds_[i];
            Vec2 rel = c.p - pc;
            rel.x = rel.x - cloudTile_.x * std::floor(rel.x / cloudTile_.x + 0.5f);
            rel.y = rel.y - cloudTile_.y * std::floor(rel.y / cloudTile_.y + 0.5f);
            Background::Cloud& o = bg.clouds[bg.cloudCount++];
            o.x = centre_.x + rel.x * pxPerWu_;
            o.y = centre_.y + rel.y * pxPerWu_;
            o.radius = c.radius * pxPerWu_;
            const Vec3 col = c.colour < 3 ? palette_->hue[c.colour] : (c.colour == 3 ? palette_->partner[1] : accent);
            o.colour = col * (c.intensity * (1.0f + look_->back.nebulaBassSwell * bass_));
        }
        const Vec2 ps = cam_ * look_->back.starsParallax;
        const float snareFlare = tier_.starTwinkle ? look_->back.starsSnareFlare * snare_ : 0.0f;
        const float hatTwinkle = tier_.starTwinkle ? look_->back.starsHatTwinkle * hat_ : 0.0f;
        for (u32 i = 0; i < starCount_; ++i) {
            const Star& s = stars_[i];
            Vec2 rel = s.p - ps;
            rel.x = rel.x - starTile_.x * std::floor(rel.x / starTile_.x + 0.5f);
            rel.y = rel.y - starTile_.y * std::floor(rel.y / starTile_.y + 0.5f);
            const Vec2 p = centre_ + rel * (pxPerWu_ / zoom_);
            if (p.x < -4.0f || p.y < -4.0f || p.x > W + 4.0f || p.y > H + 4.0f) continue;
            const float twinkle = 1.0f + hatTwinkle * ((i & 1u) ? 1.0f : -1.0f);
            const Vec3 col = s.colour == 0 ? W1 : (s.colour == 3 ? palette_->partner[1] : palette_->hue[s.colour == 1 ? 0 : 2]);
            d.sparkles.glow(p, s.radius * refPx_, col * (s.intensity * twinkle * (1.0f + snareFlare)));
        }
    }

    // ---- The lattice: world-locked, kick ripples leave the ship at 550 wu/s, inside the arena only.
    LatticeView lv;
    lv.camera = cam_;
    lv.pxPerWu = pxPerWu_;
    lv.screenCentre = centre_ + shake_;
    lv.widthPx = W;
    lv.heightPx = H;
    lv.refPx = refPx_;
    lv.player = S;
    lv.hatShimmer = look_->latticeHatShimmer * beatAmt * hat_;
    lv.bounded = true;
    lv.boundsMin = Vec2{-w.halfW(), -w.halfH()};  // (M8: the Stage's walls on the Stage)
    lv.boundsMax = Vec2{w.halfW(), w.halfH()};
    if (stage) stageLatticeView(lv, w);
    for (double kt : {lastKickT_, prevKickT_}) {
        const float age = t - static_cast<float>(kt);
        if (age < 0.0f || age > 2.5f || lv.rippleCount >= 4) continue;
        const float ampWu = look_->latticeRippleRefPx * beatAmt / kRefPxPerWu * std::exp(-age / 1.2f);
        lv.ripples[lv.rippleCount++] = {age * look_->latticeRippleSpeedWu, ampWu, 40.0f};
    }
    LatticeStyle ls;
    ls.grid = palette_->grid;
    ls.gridHot = palette_->gridHot;
    ls.player = player;
    ls.thirdHue = palette_->hue[2];
    ls.restIntensity = look_->lines.gridRest.intensity;
    ls.restWidth = look_->lines.gridRest.width;
    ls.warmIntensity = look_->lines.gridWarm.intensity;
    ls.warmWidth = look_->lines.gridWarm.width;
    ls.hotIntensity = look_->lines.gridHot.intensity;
    ls.hotPeakIntensity = look_->lines.gridHotPeak.intensity;
    ls.hotWidth = look_->lines.gridHot.width;
    ls.warmFromWu = look_->grid.warmFromWu;
    ls.hotFromWu = look_->grid.hotFromWu;
    ls.warmTowardHot = look_->grid.warmTowardHot;
    ls.tintRadiusWu = look_->grid.tintRadiusWu;
    ls.tintNearPlayerMix = look_->grid.tintNearPlayerMix;
    ls.tintFarThirdHueMix = look_->grid.tintFarThirdHueMix;
    ls.tint = tier_.gridTint;
    ls.heatGain = shown(kMuteHeat) ? crowd : 0.0f;
    if (trioRest_ > 0.0f) {
        // screens.md, the Trio: the fight steps back, the lattice rests at 0.07.
        ls.restIntensity += (0.07f - ls.restIntensity) * trioRest_;
        ls.warmIntensity *= 1.0f - 0.8f * trioRest_;
        ls.hotIntensity *= 1.0f - 0.8f * trioRest_;
        ls.heatGain *= 1.0f - trioRest_;
    }
    {
        // Event Horizon's beat of black (2026-11-14): the lattice dims over the last quarter of its death bar and stays dark through
        // the beat of black (the burst lights it again).
        const BossState& b = w.boss();
        if (rpBurstTick_ >= 0.0 && rpDeathTick_ >= 0.0 && in.music >= rpDeathTick_ && in.music < rpBurstTick_) {
            // (Kaleidos Reprise's implosion: the lattice dims over its last third, all the light gone into the point)
            const float u = clampf(static_cast<float>((in.music - rpDeathTick_) / std::fmax(rpBurstTick_ - rpDeathTick_, 1.0)), 0.0f, 1.0f);
            const float dim = 1.0f - 0.8f * clampf((u - 0.67f) / 0.33f, 0.0f, 1.0f);
            ls.restIntensity *= dim;
            ls.warmIntensity *= dim;
            ls.hotIntensity *= dim;
            ls.hotPeakIntensity *= dim;
        }
        if (b.kind == BossKind::EventHorizon && b.dying) {
            const double since = in.music - b.deathTick;
            if (since >= 0.75 * kTicksPerBar && since < kTicksPerBar + kTicksPerBeat) {
                const float dim = 1.0f - 0.85f * clampf(static_cast<float>((since - 0.75 * kTicksPerBar) / (0.25 * kTicksPerBar)), 0.0f, 1.0f);
                ls.restIntensity *= dim;
                ls.warmIntensity *= dim;
                ls.hotIntensity *= dim;
                ls.hotPeakIntensity *= dim;
            }
        }
    }
    if (eclipseK_ < 1.0f) {  // (an Eclipse: the lattice at 0.05)
        ls.restIntensity = prose::kArEclipseLattice;
        ls.warmIntensity *= eclipseK_;
        ls.hotIntensity *= eclipseK_;
    }
    {
        // morgana.md "The storm" and "The oasis" (M5 part 6's rest, 2026-11-14): her wind whips the lattice 10 wu along it (the
        // streaks' wind, drawMorgana), the oasis at rest in the accent's tint; at her death the storm drops at once and the whole
        // lattice takes the tint for her death's bar.
        const BossState& b = w.boss();
        if (b.active && b.kind == BossKind::Morgana && in.music >= b.enterTick) {
            const float now = static_cast<float>(in.time);
            const float entrance = clampf(static_cast<float>((in.music - b.enterTick) / kTicksPerBar), 0.0f, 1.0f);
            lv.wind = true;
            lv.windDir = dir(0.35f + 0.25f * std::sin(now * 0.13f));
            lv.windAmpWu = b.dying ? 0.0f : prose::kMgLatticeWindWu * entrance;
            lv.windPhase = now * (6.28318530718f * prose::kMgGustHz);
            lv.calmAt = w.morganaOasis(in.music);
            lv.calmRadiusWu = b.dying ? 1.0e6f : w.species().morgana.oasisRadius;
            lv.calmTint = palette_->accent;
            lv.calmTintMix = prose::kMgOasisTint * (b.dying ? clampf(1.0f - static_cast<float>((in.music - b.deathTick) / kTicksPerBar), 0.0f, 1.0f) : entrance);
        }
    }
    {
        // (2026-11-14) Singularity's well, Black Hole's singularity and Event Horizon's death bar pull the lattice (from the next step).
        LatticeWell wells[4];
        lat_->setWells(wells, feedbackWells(in, wells, 4));
    }
    if (shown(kMuteGrid)) lat_->draw(d.scene, lv, ls);

    // ---- Arena walls (GAME_DESIGN 9): glowing lines in the accent; the snare flashes them, the bass
    // breathes their width (GAME_DESIGN 11.1).
    {
        const float i = kWallIntensity * (1.0f + kWallSnare * beatAmt * snare_);
        const float hw = 0.5f * kWallWidth * (1.0f + kWallBass * beatAmt * bass_) * refPx_;
        const Vec3 c = accent * i;
        const float hwx = w.halfW(), hwy = w.halfH();  // (M8: the Stage's walls on the Stage)
        const Vec2 corners[4] = {{-hwx, -hwy}, {hwx, -hwy}, {hwx, hwy}, {-hwx, hwy}};
        for (u32 k = 0; k < 4; ++k) {
            Vec2 p0 = corners[k], p1 = corners[(k + 1) % 4];
            // Clip to the view (plus a margin) so a 9,600 wu line stays a short capsule.
            const float x0 = cam_.x - 0.5f * viewW_ - 50.0f, x1 = cam_.x + 0.5f * viewW_ + 50.0f;
            const float y0 = cam_.y - 0.5f * viewH_ - 50.0f, y1 = cam_.y + 0.5f * viewH_ + 50.0f;
            if (p0.y == p1.y) {
                if (p0.y < y0 || p0.y > y1) continue;
                const float a0 = clampf(std::fmin(p0.x, p1.x), x0, x1), a1 = clampf(std::fmax(p0.x, p1.x), x0, x1);
                if (a1 <= a0) continue;
                p0.x = a0;
                p1.x = a1;
            } else {
                if (p0.x < x0 || p0.x > x1) continue;
                const float a0 = clampf(std::fmin(p0.y, p1.y), y0, y1), a1 = clampf(std::fmax(p0.y, p1.y), y0, y1);
                if (a1 <= a0) continue;
                p0.y = a0;
                p1.y = a1;
            }
            if (shown(kMuteGrid)) d.scene.line(toScene(p0), toScene(p1), hw, c, c, 0);
        }
    }

    drawArena(d, in);  // M5 part 5: the structures and the event, under the bodies
    if (stage) drawStageFloor(d, in);  // M8: the rig and the floor's edges, under the bodies

    // ---- Enemies.
    const Enemies& e = w.enemies();
    const SpeciesTable& table = w.species();
    const Look::Enemy& le = look_->enemy;
    stats_.enemiesInView = 0;
    stats_.indicators = 0;
    const double hullBars = in.music / kTicksPerBar;
    const double steppedBars = std::floor(in.music / 48.0) * 48.0 / kTicksPerBar;  // ornaments step on the eighths
    for (u32 i = 0; i < e.count; ++i) {
        if (e.flags[i] & kFlagDead) continue;
        // M4: a co-op client draws a corrected enemy where it was, the offset decaying (ox, oy; 0 otherwise).
        const Vec2 pos = lerp2(Vec2{e.px[i], e.py[i]}, Vec2{e.x[i], e.y[i]}, a) + Vec2{e.ox[i], e.oy[i]};
        const Species& s = table.get(e.species[i]);
        // (M5 part 4: a Wyrm's head)
        const int g = (s.family == Family::Wyrm && e.a[i] > 0.5f) ? wyrmHeadGlyph_[e.species[i]] : speciesGlyph_[e.species[i]];
        const GlyphDef& gd = glyphs_->def(static_cast<u32>(g));
        const float scale = e.radius[i];
        const bool inView = visible(pos, 0.0f);
        // Edge indicators (look.md §6): an engaged shooter outside the view, in the danger colour.
        if (!inView && s.ranged() && (e.state[i] == kGlint || e.state[i] == kFiring) && stats_.indicators < 8 &&
            shown(kMutePriority)) {
            edgeChevron(d, pos, danger, false, 1.0f);
            ++stats_.indicators;
        }
        // M5 part 5 (look.md 6): a white edge chevron for an elite off screen.
        const bool elite = (e.traits[i] & kEliteBit) != 0;
        if (!inView && elite && stats_.indicators < 8 && shown(kMutePriority)) {
            edgeChevron(d, pos, W1, false, 1.0f);
            ++stats_.indicators;
        }
        if (!visible(pos, 2.5f * scale + 40.0f)) continue;
        if (inView) ++stats_.enemiesInView;
        if (e.state[i] == kWarping) {
            // Warp-in (look.md §5): a ring in the enemy hue contracting to nothing over two beats, from 2.5 x
            // the glyph's extent.
            const float left = clampf(static_cast<float>((e.until[i] - in.music) / (kWarpBeats * kTicksPerBeat)), 0.0f, 1.0f);
            const float r = look_->boom.warpFromGlyphExtents * gd.extent * scale * left;
            if (r > 0.5f && shown(kMuteWarp))
                glyph(d.glyphs, r > 40.0f ? gRing64_ : gRing32_, false, pos, 0.0f, 0.0f, r, look_->lines.warpRing.intensity * crowd,
                      0.5f * look_->lines.warpRing.width, kGlyphFlat, palette_->hue[s.hue], W1);
            continue;
        }
        Vec3 hue = palette_->hue[s.hue], partner = palette_->partner[s.hue];
        const bool caught = stage && (e.flags[i] & kFlagCaught) && caughtBy_[i] < kMaxPlayers;  // (M8 TRACERS, tracers.md "A lock")
        if (caught) {
            hue = palettes_->player[caughtBy_[i]];
            partner = palettes_->playerLight[caughtBy_[i]];
            d.sparkles.glow(toScene(pos), 3.5f * refPx_, W1 * 1.4f);
        }
        if (w.worldStretch() < 1.0f) {
            // look.md 5 (Time Stretch on the world): enemy hulls lose a third of their chroma; the room slows, the player does not.
            const auto grey = [](Vec3 c) { const float l = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; return c + (Vec3{l, l, l} - c) * (1.0f / 3.0f); };
            hue = grey(hue);
            partner = grey(partner);
        }
        float intensity = (look_->lines.enemyHullTier1.intensity + le.hullIntensityPerTier * (static_cast<float>(s.tier) - 1.0f)) * crowd * enemyLight_ *
                          (caught ? 0.8f : 1.0f);
        float halfWidth = 0.5f * look_->lines.enemyHullTier1.width;
        float eliteLevel = 1.0f;
        if (elite) {
            // look.md 3: hull + 0.6 at 2.3, never dimmed by the light budget (an Eclipse dims it); Cloaked at 8% between beats and
            // full on the beat's flash; Phasing faint off its solid half.
            const u16 tr = e.traits[i];
            eliteLevel = eclipseK_;
            if (hasTrait(tr, kCloaked)) eliteLevel *= w.species().traits.cloaked + (1.0f - w.species().traits.cloaked) * kick_;
            intensity = (look_->lines.enemyHullTier1.intensity + le.hullIntensityPerTier * (static_cast<float>(s.tier) - 1.0f) + le.eliteIntensityAdd) *
                        enemyLight_ * eliteLevel;
            halfWidth = 0.5f * le.eliteWidth;
        }
        const float phase = static_cast<float>(e.serial[i] % 997u) * 0.61803f * kTau;
        float rot0, rot1;
        if (gd.facesHeading) {
            rot0 = rot1 = lerpAngle(e.pheading[i], e.heading[i], a);
        } else {
            rot0 = phase + kTau * le.hullTurnsPerBar * static_cast<float>(hullBars);
            rot1 = phase + kTau * le.ornamentTurnsPerBar * static_cast<float>(steppedBars);
        }
        const bool lod = scale * pxPerWu_ < tier_.lodRadiusPx;
        if (elite) drawElite(d, in, i, pos, rot0, rot1, scale, hue, g, eliteLevel);  // M5 part 5
        if (shown(elite ? kMuteElite : kMuteEnemy)) {
            // M6 (look.md 4): a Phasing elite off its solid half has every other hull segment hidden (was: the whole hull at 35%).
            if (elite && w.elitePhased(i)) hullEveryOther(d, g, pos, rot0, scale, intensity, halfWidth, hue, partner, false);
            else glyph(d.glyphs, g, lod && !elite, pos, rot0, rot1, scale, intensity, halfWidth, kGlyphEnemy, hue, partner);
            if (s.variant != kVariantA) {
                const int mark = s.variant == kVariantC ? gMarkC_ : (gd.facesHeading ? gMarkBHeading_ : gMarkBSpin_);
                glyph(d.glyphs, mark, false, pos, rot0, rot0, scale, look_->lines.variantMark.intensity * crowd,
                      0.5f * look_->lines.variantMark.width, kGlyphWhite, W1, W1);
            }
            if (s.family == Family::Hound && s.variant == kVariantC) {
                // M5 (GAME_DESIGN 7.2): the Hound C's frontal shield, a white arc where shots are shed.
                const float half = 0.5f * table.family(Family::Hound).v.shieldArcDeg * kTau / 360.0f;
                const float r = scale * 1.75f;
                const u32 n = 6;
                for (u32 k = 0; k < n; ++k) {
                    const float a0 = e.b[i] - half + 2.0f * half * static_cast<float>(k) / n, a1 = e.b[i] - half + 2.0f * half * static_cast<float>(k + 1) / n;
                    d.scene.line(toScene(pos + dir(a0) * r), toScene(pos + dir(a1) * r), 0.9f * refPx_, W1 * (1.4f * crowd), W1 * (1.4f * crowd), 0);
                }
            }
            if (s.family == Family::Carom) {  // a short motion streak (geometry, look.md §7)
                const Vec2 v{e.vx[i], e.vy[i]};
                const float sp = length(v);
                if (sp > 1.0f) {
                    const Vec2 u = v * (1.0f / sp);
                    d.scene.line(toScene(pos - u * (scale * 1.3f)), toScene(pos - u * (scale * 4.5f)), 1.0f * refPx_, hue * (1.6f * crowd),
                                 Vec3{}, 0);
                }
            }
        }
        // Telegraphs (R4, look.md §5): a Ram shows chevrons along its path; shooters glint one beat ahead.
        if (e.state[i] == kTelegraph) {
            const Vec2 dd{e.a[i], e.b[i]};
            const Vec2 n = perp(dd);
            for (float along = 50.0f; along <= tuning::kRamPathWu; along += 52.0f) {
                const Vec2 c = pos + dd * along;
                tellMark(d, c - dd * 10.0f + n * 10.0f, c, swarm);
                tellMark(d, c - dd * 10.0f - n * 10.0f, c, swarm);
            }
        } else if (e.state[i] == kGlint) {
            const float progress = clampf(1.0f - static_cast<float>((e.until[i] - in.music) / kTicksPerBeat), 0.0f, 1.0f);
            const float len = 12.0f + 26.0f * progress;
            const float ang = 0.25f * kPi + progress * 0.5f * kPi;
            for (u32 k = 0; k < 2; ++k) {
                const Vec2 dd = dir(ang + 0.5f * kPi * static_cast<float>(k));
                tellMark(d, pos - dd * len, pos + dd * len, swarm);
            }
        }
    }

    // ---- Enemy bullets: upright diamonds, scene light on the swarm gain, and the threat layer.
    const Bullets& bl = w.bullets();
    stats_.bulletsInView = 0;
    {
        const Vec3 threatCore = mix(danger, W1, 0.25f);
        const float bulletScale = kStandardEnemyWu * glyphs_->def(static_cast<u32>(gBullet_)).size;
        const float rimHalf = 0.5f * look_->threat.bulletWidth + look_->threat.bulletRim;
        const float coreHalf = 0.5f * look_->threat.bulletWidth;
        for (u32 i = 0; i < bl.count; ++i) {
            if (bl.dead[i]) continue;
            const Vec2 p = lerp2(Vec2{bl.px[i], bl.py[i]}, Vec2{bl.x[i], bl.y[i]}, a);
            if (!visible(p, 10.0f)) continue;
            if (visible(p, 0.0f)) ++stats_.bulletsInView;
            if (bl.echo[i]) {
                // M5 part 4 (feedback.md): an echo shot is the player's shot, turned: a short streak in the danger colour with
                // the threat rim.
                const Vec2 dv = normalizeOr(Vec2{bl.vx[i], bl.vy[i]}, Vec2{1.0f, 0.0f});
                const Vec2 tail = p - dv * w.species().feedback.streak;
                // look.md §5 (the owner's red-line pass, 2026-11-14, "thinner ... and the glow more"): a thin core, a strong glow
                // at its head and down its middle.
                const float echoCore = coreHalf, echoRim = echoCore + look_->threat.bulletRim;
                if (shown(kMuteBullet)) {
                    d.scene.line(toScene(tail), toScene(p), 0.5f * look_->lines.enemyBullet.width * refPx_, danger * 0.4f, danger * 1.6f, 0);
                    d.sparkles.glow(toScene(p), 16.0f * refPx_, danger * 0.9f);
                    d.sparkles.glow(toScene((tail + p) * 0.5f), 12.0f * refPx_, danger * 0.45f);
                }
                if (shown(kMutePriority)) {
                    d.threatRims.line(toScreen(tail), toScreen(p), echoRim * refPx_, Vec3{}, Vec3{}, 0);
                    d.threatCores.line(toScreen(tail), toScreen(p), echoCore * refPx_, threatCore, threatCore, 0);
                }
                continue;
            }
            if (shown(kMuteBullet)) {
                glyph(d.glyphs, gBullet_, false, p, 0.0f, 0.0f, bulletScale, look_->lines.enemyBullet.intensity * swarm,
                      0.5f * look_->lines.enemyBullet.width, kGlyphFlat, danger, danger);
                // look.md §3 (the owner, 2026-11-14: "do a pass on any red enemy projectiles ... make sure they have a substantial
                // glow"): every bullet glows on every tier: a pool (quarter resolution, cheap) never under 0.6 of its light in a
                // crowd; the sharp sparkle on top from High up, as before. Until then Low and Medium (the phones) had no glow at all.
                d.pools.glow(toScene(p), kBulletGlowPx * refPx_, danger * (kBulletGlow * std::fmax(swarm, kBulletGlowFloor)));
                if (tier_.bulletSparkles)
                    d.sparkles.glow(toScene(p), look_->soft.enemyBullet.radius[0] * refPx_, danger * (look_->soft.enemyBullet.intensity[0] * swarm));
            }
            if (shown(kMutePriority)) {
                glyph(d.threatGlyphRims, gBullet_, false, p, 0.0f, 0.0f, bulletScale, 1.0f, rimHalf, kGlyphFlat, Vec3{}, Vec3{});
                glyph(d.threatGlyphCores, gBullet_, false, p, 0.0f, 0.0f, bulletScale, 1.0f, coreHalf, kGlyphFlat, threatCore, threatCore);
            }
        }
    }

    if (stage) drawStageLines(d, in);  // M8: the tracers, the closing hint, the rings (over the bodies, under the ships)

    // ---- The player: two-tone ship, halo (one arc per Shield cell), trail, pool, priority layer.
    const PlayerState& me = w.player(w.viewPlayer());
    const float shipScale = kStandardEnemyWu * glyphs_->def(static_cast<u32>(gShip_)).size;
    const bool playerShown = shown(kMutePlayer) && !in.noShips;
    // Trail history (the view clock).
    if (me.alive) {
        trailPos_[trailHead_] = S;
        trailT_[trailHead_] = t;
        trailHead_ = (trailHead_ + 1) % kTrail;
        if (trailCount_ < kTrail) ++trailCount_;
    } else {
        trailCount_ = 0;
    }
    // M4 (look.md 6): a revived ship blinks on the eighths for a bar (it cannot be hurt meanwhile).
    const float eighthS = 30.0f / std::fmax(in.bpm, 30.0f);
    const auto blinkOff = [&](u32 q) {
        const float age = t - reviveT_[q];
        return age >= 0.0f && age < 8.0f * eighthS && std::fmod(age, 2.0f * eighthS) >= eighthS;
    };
    if (me.alive) {
        const float heading = in.shipHeading;
        const LookLine& hull = look_->lines.playerHull;
        const u32 cells = me.cells > 8 ? 8u : me.cells;
        // (2026-11-14, the owner: "it's not clear when you are damaged and when you can be damaged again"): while it cannot be hurt
        // (invulS) the hull blinks on the sixteenths (down to 0.35; reduce flashing: steady at 0.7), its bubble draining round it
        // (drawFeedbackFx); a lost cell turns the hull white for 0.1 s.
        const bool safe = me.invulS > 0.0f;
        const bool dimStep = safe && !reduceFlashing_ && (static_cast<i64>(std::floor(in.music / (0.25 * kTicksPerBeat))) & 1) != 0;
        const float hullLv = safe ? (reduceFlashing_ ? 0.7f : (dimStep ? 0.35f : 1.0f)) : 1.0f;
        const float hitWhite = clampf(1.0f - (t - cellHitT_) / 0.1f, 0.0f, 1.0f) * (t >= cellHitT_ ? 1.0f : 0.0f);
        const float haloRot = 0.3f * t;
        const float haloPulse = 1.0f + 0.25f * kick_;
        // Play test 2 (look.md 6): up to eight arcs. Regen's arc growing back is a dashed outline that fills clockwise in
        // the place of the next arc; a Cell taken draws its arc in over a quarter of a beat. `whole` arcs are drawn in the
        // places of `slots`, and the arc in place `whole` grows (grow: its fill, < 0 none).
        u32 whole = cells, slots = cells;
        float grow = -1.0f;
        bool growDashed = false;
        {
            const float quarterS = 15.0f / std::fmax(in.bpm, 30.0f);
            const float backAge = t - cellBackT_;
            const u32 cap = me.cellCap > 8 ? 8u : me.cellCap;
            const u32 every = w.regenPeriod(w.viewPlayer());  // (the loop stretches it)
            if (backAge >= 0.0f && backAge < quarterS && cellBackTo_ == cells && cells >= 1) {
                whole = cells - 1;
                grow = backAge / quarterS;
            } else if (every && cells < cap) {
                const float inBar = clampf(static_cast<float>((in.music - w.tickOfBar(w.phrase().bar)) / kTicksPerBar), 0.0f, 1.0f);
                slots = cells + 1;
                grow = clampf((static_cast<float>(me.regenBars) + inBar) / static_cast<float>(every), 0.0f, 1.0f);
                growDashed = true;
            }
        }
        const int haloG = whole == 0 ? -1 : (whole == slots ? gHalo_[whole] : gHaloPart_[whole][slots]);
        if (playerShown && !blinkOff(w.viewPlayer())) {
            glyph(d.glyphs, gShip_, false, S, heading, heading, shipScale, hull.intensity * hullLv, 0.5f * hull.width, kGlyphPlayer, player, deep);
            glyph(d.glyphs, gShip_, true, S, heading, heading, shipScale, look_->lines.playerWhiteCopy.intensity * hullLv,
                  0.5f * look_->lines.playerWhiteCopy.width, kGlyphWhite, W1, W1);
            if (hitWhite > 0.0f)
                glyph(d.glyphs, gShip_, true, S, heading, heading, shipScale, 3.0f * hitWhite, 0.8f * hull.width, kGlyphWhite, W1, W1);
            if (cells == 1 && haloG >= 0) {
                // The last arc pulses in the danger colour (look.md §4).
                glyph(d.glyphs, haloG, false, S, haloRot, haloRot, shipScale, look_->lines.playerHalo.intensity * (0.8f + 1.2f * kick_),
                      0.5f * look_->lines.playerHalo.width, kGlyphFlat, danger, danger);
            } else if (haloG >= 0) {
                glyph(d.glyphs, haloG, false, S, haloRot, haloRot, shipScale, look_->lines.playerHalo.intensity * haloPulse,
                      0.5f * look_->lines.playerHalo.width, kGlyphRingA, player, light);
            }
            if (grow >= 0.0f) {
                // The growing arc, as content/glyphs.cpp lays the halo out (design/tools/glyphs.py: arc j of n centred at
                // 90 + j x 360 / n degrees, y up, spanning 360 / n - 26), clockwise on screen from its start.
                const float deg = kTau / 360.0f;
                const float n = static_cast<float>(slots);
                const float span = (slots > 1 ? 360.0f / n - 26.0f : 320.0f) * deg;
                const float mid = (90.0f + static_cast<float>(whole) * 360.0f / n) * deg;
                const float phi0 = -(mid + 0.5f * span) + haloRot;
                const float R = 1.55f * shipScale;
                const float hw = 0.5f * look_->lines.playerHalo.width * refPx_;
                const Vec3 lit = (cells == 1 && growDashed ? danger : mix(player, light, 0.5f)) * (look_->lines.playerHalo.intensity * haloPulse);
                // Dashes about 8 degrees long (two at least: at eight cells an arc spans 19 degrees).
                const u32 dashes = static_cast<u32>(clampf(span / (8.0f * deg), 2.0f, 8.0f));
                const u32 segs = growDashed ? 2 * dashes - 1 : 14;
                for (u32 s = 0; s < segs; ++s) {
                    const float u0 = static_cast<float>(s) / segs, u1 = static_cast<float>(s + 1) / segs;
                    const Vec2 p0 = S + dir(phi0 + u0 * span) * R, p1 = S + dir(phi0 + u1 * span) * R;
                    if (growDashed) {
                        if (s % 2) continue;  // the gaps
                        const bool on = u0 < grow;
                        const Vec2 q1 = on && u1 > grow ? S + dir(phi0 + grow * span) * R : p1;
                        d.scene.line(toScene(p0), toScene(p1), 0.8f * hw, W1 * 0.6f, W1 * 0.6f, 0);  // the outline
                        if (on) d.scene.line(toScene(p0), toScene(q1), hw, lit * 1.3f, lit * 1.3f, 0);
                    } else if (u0 < grow) {
                        const Vec2 q1 = u1 <= grow ? p1 : S + dir(phi0 + grow * span) * R;
                        d.scene.line(toScene(p0), toScene(q1), hw, lit * 1.4f, lit * 1.4f, 0);
                    }
                }
            }
            d.pools.glow(toScene(S), look_->soft.playerPool.radius[0] * pxPerWu_, player * look_->soft.playerPool.intensity[0]);
            // Trail: 13 dashes over the last 100 wu of path, white -> player -> deep, fading to nothing.
            const LookLine& tr = look_->lines.playerTrail;
            Vec2 path[14];
            u32 have = 0;
            {
                float want = 0.0f, walked = 0.0f;
                Vec2 prev = S;
                path[have++] = S - dir(heading) * (shipScale * 0.9f);
                for (u32 k = 1; k < trailCount_ && have < 14; ++k) {
                    const Vec2 p = trailPos_[(trailHead_ + kTrail - 1 - k) % kTrail];
                    const float seg = length(p - prev);
                    while (have < 14 && seg > 1e-4f && walked + seg >= (want = 100.0f * static_cast<float>(have) / 13.0f)) {
                        const float u = (want - walked) / seg;
                        path[have++] = lerp2(prev, p, u) - dir(heading) * (shipScale * 0.9f);
                    }
                    walked += seg;
                    prev = p;
                }
            }
            const auto field = [&](float u) {
                const float f = clampf(u, 0.0f, 1.0f) * 2.0f;
                const Vec3 c = f < 1.0f ? mix(W1, player, f) : mix(player, deep, f - 1.0f);
                return c * (tr.intensity * (1.0f - u));
            };
            for (u32 k = 0; k + 1 < have; ++k) {
                const float u0 = static_cast<float>(k) / 13.0f, u1 = (static_cast<float>(k) + 0.6f) / 13.0f;
                const Vec2 b = lerp2(path[k], path[k + 1], 0.6f);
                d.scene.line(toScene(path[k]), toScene(b), 0.5f * tr.width * refPx_, field(u0), field(u1), 0);
            }
        }
        if (shown(kMutePriority) && !in.noShips) {
            // The ship on the priority layer (look.md §6, §9): hull and halo after tone mapping with a rim.
            const Look::PriorityShip& ph = look_->priorityHull;
            const Look::PriorityShip& pa = look_->priorityHalo;
            const Vec3 hullCore = mix(light * 0.55f + W1 * 0.45f, W1, hitWhite) * hullLv;
            const Vec3 haloCore = cells == 1 ? mix(danger, W1, 0.25f) : player * 0.75f + W1 * 0.25f;
            glyph(d.threatGlyphRims, gShip_, true, S, heading, heading, shipScale, 1.0f, 0.5f * ph.width + ph.rim, kGlyphFlat, Vec3{}, Vec3{});
            glyph(d.priorityShipCores, gShip_, true, S, heading, heading, shipScale, 1.0f, 0.5f * ph.width, kGlyphFlat, hullCore, hullCore);
            if (haloG >= 0) {
                glyph(d.threatGlyphRims, haloG, false, S, haloRot, haloRot, shipScale, 1.0f, 0.5f * pa.width + pa.rim, kGlyphFlat,
                      Vec3{}, Vec3{});
                glyph(d.priorityShipCores, haloG, false, S, haloRot, haloRot, shipScale, 1.0f, 0.5f * pa.width, kGlyphFlat,
                      haloCore, haloCore);
            }
            // GAME_DESIGN 11.3: the local player's hit point is always marked by a small bright dot.
            d.threatRims.line(toScreen(S), toScreen(S), 2.2f * refPx_, Vec3{}, Vec3{}, kDisc);
            d.threatCores.line(toScreen(S), toScreen(S), 1.3f * refPx_, W1, W1, kDisc);
        }
    }
    // M4 co-op (look.md 6): the teammates' ships, the beacons and the revives (play_view_coop.cpp).
    if (w.playerCount() > 1 && playerShown) drawCoop(d, in);
    // Dash: five fading ghost copies along the path; a Perfect Dash adds a four-point sparkle (look.md §5). M4: each
    // player's own, in its colours.
    for (u32 q = 0; q < kMaxPlayers; ++q) {
        const Dash& ds = dash_[q];
        const float age = t - ds.t;
        if (age < 0.0f || age >= kDashGhostS || !playerShown) continue;
        const Vec3 pc = palettes_->player[q], pd = palettes_->playerDeep[q];
        const float fade = 1.0f - age / kDashGhostS;
        for (u32 k = 0; k < kDashGhosts; ++k) {
            const float u = static_cast<float>(k) / static_cast<float>(kDashGhosts);
            const Vec2 p = lerp2(ds.from, ds.to, u);
            const float i = look_->lines.playerHull.intensity * 0.6f * fade * (0.35f + 0.65f * u);
            glyph(d.glyphs, gShip_, true, p, ds.heading, ds.heading, shipScale, i, 0.5f * look_->lines.playerHull.width, kGlyphPlayer, pc,
                  pd);
        }
        if (ds.perfect) {
            flare(d.scene, ds.to, 0.0f, 46.0f * (0.6f + 0.4f * fade), 2, W1 * (2.4f * fade), 0.7f);
            d.sparkles.glow(toScene(ds.to), 14.0f * refPx_, mix(pc, W1, 0.6f) * (1.6f * fade));
        }
    }
    // The Nova: the inhale (sparks falling in, the screen dimming) and the front.
    float novaDim = 0.0f;
    novaPump_ = 0.0f;
    for (u32 q = 0; q < kMaxPlayers; ++q) {
        // One ring each, as M1's Drop front: both move far more than the afterglow's blur in a frame (the
        // front about 100 px), so at low load (swarm gain 1) the afterglow leaves soft copies behind them
        // (docs/PROGRESS.md M3: a question for the lead; drawing them swept like shots filled the disc).
        const Vec3 pc = palettes_->player[q], pl = palettes_->playerLight[q];
        const bool mine = q == w.viewPlayer();
        if (t >= novaPressT_[q] && t < novaBlastT_[q] && novaBlastT_[q] > novaPressT_[q]) {
            const float span = novaBlastT_[q] - novaPressT_[q];
            if (mine) novaDim = kNovaDim * smooth01((t - novaPressT_[q]) / span);
            if (playerShown) {
                const float u = (t - novaPressT_[q]) / span;
                const PlayerState& o = w.player(q);
                const Vec2 at = mine ? S : lerp2(o.prevPos, o.pos, a);
                glyph(d.glyphs, gRing128_, false, at, 0.0f, 0.0f, kNovaInhaleRingWu * (1.0f - u), 1.2f * u, 1.0f, kGlyphRingA, pc, pl);
            }
        }
        const float age = t - novaFrontT_[q];
        if (age >= 0.0f && age < tuning::kNovaFrontS && shown(kMuteFx)) {
            const float u = age / tuning::kNovaFrontS;
            glyph(d.glyphs, gRing128_, false, novaAt_[q], 0.0f, 0.0f, tuning::kNovaFrontWu * u, kNovaFront * (1.0f - u),
                  0.5f * kNovaFrontWidthRefPx, kGlyphFlat, W1, W1);
        }
        const float pumpAge = t - novaFrontT_[q];
        if (pumpAge >= 0.0f && pumpAge < look_->novaPumpMs * 1e-3f)
            novaPump_ = reduceFlashing_ ? look_->novaPumpReduced : look_->novaPumpBloom;
    }

    // ---- Shots: a capsule as long as the shot travels in a frame (look.md v2.2 §3), a sparkle at the head; play test 2:
    // the build on the shots and the saturated Prism (play_view_shots.cpp).
    if (!buildReady_) buildColours(player);
    drawShots(d, in, a);

    // ---- Shards: the pickup diamond; homing ones faster than 480 wu/s carry a tail (look.md §7).
    {
        const Shards& s = w.shards();
        const Vec3 shardColour = mix(palettes_->pickup, W1, 0.4f);
        const float shardScale = kStandardEnemyWu * glyphs_->def(static_cast<u32>(gShard_)).size;
        const LookLine& sl = look_->lines.shard;
        const Look::ShotStreak& ss = look_->shotStreak;
        const double lifeTicks = kShardLifeBars * kTicksPerBar;
        for (u32 i = 0; i < s.count; ++i) {
            if (s.dead[i]) continue;
            const Vec2 p = lerp2(Vec2{s.px[i], s.py[i]}, Vec2{s.x[i], s.y[i]}, a);
            if (!visible(p, 30.0f) || !shown(kMuteShard)) continue;
            // Loose Shards fade over their last bar (one flying in is whole).
            const float left = static_cast<float>((s.born[i] + lifeTicks - in.music) / kTicksPerBar);
            const float fade = s.homing[i] ? 1.0f : clampf(left, 0.0f, 1.0f);
            // A merged Shard (a crowded field's, or a kill's in one: world.cpp, 2026-11-15; the owner: "the value of the shard
            // should be reflected by color and size") is bigger and golder with what it is worth: 2 a third bigger and a third of
            // the way to gold, 8 about twice the size and gold, the most 2.6 times and gold to white; a slow pulse from 4 up.
            const u32 worth = s.value[i] ? s.value[i] : 1u;
            const float lv = worth > 1u ? std::log2(static_cast<float>(worth)) : 0.0f;  // 1 -> 0, 2 -> 1, 8 -> 3, 64 -> 6
            const float size = std::fmin(1.0f + 0.33f * lv, 2.6f);
            const float toGold = std::fmin(lv / 3.0f, 1.0f), toWhite = clampf((lv - 3.0f) / 4.0f, 0.0f, 0.6f);
            const Vec3 colour = mix(mix(shardColour, palettes_->player[3], toGold), W1, toWhite);
            const float pulse = worth >= 4u ? 1.0f + 0.25f * std::sin(t * 6.0f + static_cast<float>(i)) : 1.0f;
            const float i0 = sl.intensity * swarm * fade * (1.0f + 0.15f * lv) * pulse;
            glyph(d.glyphs, gShard_, false, p, static_cast<float>(i) * 0.7f + 4.0f * t, 0.0f, shardScale * size, i0, 0.5f * sl.width * (1.0f + 0.3f * lv),
                  kGlyphFlat, colour, colour);
            if (tier_.bulletSparkles)
                d.sparkles.glow(toScene(p), look_->soft.shard.radius[0] * refPx_ * size,
                                mix(palettes_->pickup, palettes_->player[3], toGold) * (look_->soft.shard.intensity[0] * swarm * fade * (1.0f + 0.25f * lv)));
            const Vec2 v{s.vx[i], s.vy[i]};
            const float sp = length(v);
            if (sp > 480.0f) {
                const float len = std::fmax(sp * ss.frameS + ss.overlapWu, ss.minWu);
                const Vec2 tail = p - v * (len / sp);
                d.scene.line(toScene(tail), toScene(p), 0.5f * sl.width * refPx_, Vec3{}, shardColour * i0, 0);
            }
        }
    }

    // ---- Effects: flashes and pools, shock rings, embers, shatter, distortion.
    for (u32 i = 0; i < poolCap_; ++i) {
        const Pool& p = pools_[i];
        const float age = t - p.born;
        if (age < 0.0f || age >= p.life) continue;
        if (!visible(p.at, p.sparkle ? 10.0f : p.r1) || !shown(p.kill ? kMutePool : kMuteFlash)) continue;
        const float u = age / p.life;
        const float level = (p.fade == 1 ? p.i0 * (1.0f - u) * (1.0f - u) : p.i0 + (p.i1 - p.i0) * u) * (p.dimmed ? crowd : 1.0f);
        const float radius = p.r0 + (p.r1 - p.r0) * u;
        if (p.sparkle)
            d.sparkles.glow(toScene(p.at), radius * refPx_, p.colour * level);
        else
            d.pools.glow(toScene(p.at), radius * pxPerWu_, p.colour * level);
    }
    const bool fxShown = shown(kMuteFx);
    for (const ShockRing& sr : shock_) {
        const float age = t - sr.start;
        if (age < 0.0f || age >= sr.life || !visible(sr.at, sr.toWu) || !fxShown) continue;
        const float u = age / sr.life;
        const float r = sr.toWu * (0.25f + 0.75f * std::sqrt(u));
        glyph(d.glyphs, gRing64_, false, sr.at, 0.0f, 0.0f, r, look_->lines.shockRing.intensity * (1.0f - u) * crowd,
              0.5f * look_->lines.shockRing.width, kGlyphRingW, sr.a, sr.b);
    }
    for (u32 i = 0; i < emberCap_; ++i) {
        const Ember& m = embers_[i];
        const float age = t - m.born;
        if (age < 0.0f || age >= m.life) continue;
        const Vec2 p = m.p0 + m.v0 * ((1.0f - std::exp(-2.0f * age)) / 2.0f);
        if (!visible(p, 10.0f) || !fxShown) continue;
        d.sparkles.glow(toScene(p), look_->soft.ember.radius[0] * refPx_, m.colour * (look_->soft.ember.intensity[0] * (1.0f - age / m.life) * crowd));
    }
    for (u32 i = 0; i < shatterCap_; ++i) {
        const Shatter& s = shatter_[i];
        const float age = t - s.born;
        if (age < 0.0f || age >= s.life || !fxShown) continue;
        const Vec2 c = s.centre + s.v * age;
        const float ang = s.spin * age, cs = std::cos(ang), sn = std::sin(ang);
        const float fade = 1.0f - age / s.life;
        const Vec3 col = s.colour * (look_->lines.shatter.intensity * fade * (s.dimmed ? crowd : 1.0f));
        d.scene.line(toScene(c + rotate(s.a, cs, sn)), toScene(c + rotate(s.b, cs, sn)), 0.5f * look_->lines.shatter.width * refPx_, col, col, 0);
    }
    params.wellCount = 0;
    params.ringCount = 0;
    for (u32 i = 0; i < ringCap_; ++i) {
        const DistortRing& r = rings_[i];
        const float age = t - r.born;
        if (age < 0.0f || age >= r.life || params.ringCount >= 8 || !shown(kMuteDistortion)) continue;
        const float u = age / r.life;
        DistortionRing& o = params.rings[params.ringCount++];
        const Vec2 sp = toScene(r.at);
        o.x = sp.x;
        o.y = sp.y;
        o.radius = r.toWu * u * pxPerWu_;
        o.width = r.widthRefPx * refPx_;
        o.amount = r.amountRefPx * refPx_ * (1.0f - u);
    }

    // ---- M3 stage B: Riffs, Boosts, the boss, the Trio (play_view_b.cpp).
    drawStageB(d, in, W, H);

    // ---- M6 (screens.md "First-run hints"): one line of world text 70 wu above the ship, cap 22, white at 0.85 with the
    // priority layer's dark rim.
    if (in.hint && in.hintLevel > 0.01f && !in.noShips) {
        const float cap = 22.0f * pxPerWu_;
        const float wText = font_->measure(TextStyle{cap}, in.hint);
        const Vec2 sc = toScreen(in.shipPos);  // (the priority layer: the threat view, which does not shake)
        const Vec2 at{sc.x - 0.5f * wText, sc.y - 70.0f * pxPerWu_ - cap};
        const Vec3 rim{};
        font_->drawField(d.threatRims, at, cap, 2.2f * refPx_, rim, rim, rim, in.hint);
        const Vec3 c = white() * (0.85f * in.hintLevel);
        font_->drawField(d.threatCores, at, cap, 0.9f * refPx_, c, c, c, in.hint);
    }

    // ---- Composite controls (look.md §7).
    const float pump = shown(kMutePump) ? kick_ * beatAmt : 0.0f;
    params.bloomPump = (1.0f + look_->bloomKick * pump) * (1.0f + novaPump_);
    params.exposurePump = (1.0f + look_->exposureKick * pump) * (1.0f - novaDim) * exposureSection_;
    params.vignette = shown(kMuteVignette);
    params.vignetteAmount = look_->vignetteAmount + (w.worldStretch() < 1.0f ? 0.15f : 0.0f);  // look.md 5: Time Stretch closes it by 0.15
    params.drain = drain_;
    params.afterglow = shown(kMuteAfterglow);
    params.afterglowGain = look_->afterglowGain == Look::Gain::Swarm ? swarm : (look_->afterglowGain == Look::Gain::Crowd ? crowd : 1.0f);
    params.dtSeconds = in.dt;
    params.aberration = shown(kMuteAberration);
    params.aberrationSnarePx = look_->aberrationSnareRefPx * beatAmt * refPx_ * snare_;
    params.world = {pxPerWu_, centre_.x + shake_.x - cam_.x * pxPerWu_, centre_.y + shake_.y - cam_.y * pxPerWu_};
    params.worldThreat = {pxPerWu_, centre_.x - cam_.x * pxPerWu_, centre_.y - cam_.y * pxPerWu_};
    params.accent = accent;
    params.enemySwell = 1.0f + look_->enemy.kickSwell * pump;
    params.particleTime = t;
    params.particleMaxLife = in.backgroundOnly ? 0.0f : 2.0f;  // (M4 stage B: no spark behind the co-op setup screens)
    params.dimAt = S;
    params.dimRadius = 120.0f;  // GAME_DESIGN 11.3: particles within 120 wu of the local player at half
    params.dimFactor = 0.5f;
    const float off0x = params.world.offsetX, off0y = params.world.offsetY;
    if (hasPrevView_) {
        const float k = pxPerWu_ / prevScale_;
        params.cameraScale = k;
        params.cameraShiftX = off0x - prevOffset_.x * k;
        params.cameraShiftY = off0y - prevOffset_.y * k;
    } else {
        params.cameraScale = 1.0f;
        params.cameraShiftX = params.cameraShiftY = 0.0f;
    }
    prevScale_ = pxPerWu_;
    prevOffset_ = {off0x, off0y};
    hasPrevView_ = true;
    stats_.glyphs = d.glyphs.count();
    stats_.load = static_cast<float>(stats_.enemiesInView) + 0.25f * static_cast<float>(stats_.bulletsInView);
    (void)barS;
    (void)lastWarpLandT_;
}

void PlayView::buildColours(Vec3 player) {
    // Prism's fan (GAME_DESIGN 7, look.md 5; play test 2: saturated): the centre lane in the player's colour, the others
    // 60 and 120 degrees round the OKLCH hue wheel to either side at lightness 0.70 and full chroma; a lane within 30
    // degrees of the danger hue moves to the nearest edge of that band.
    const float deg = kTau / 360.0f;
    const Vec3 lab = toOklab(player), dl = toOklab(palettes_->danger);
    const float h0 = std::atan2(lab.z, lab.y), hd = std::atan2(dl.z, dl.y);
    for (i32 k = -2; k <= 2; ++k) {
        float h = h0 + static_cast<float>(k) * kPrismLaneStepDeg * deg;
        const float off = wrapAngle(h - hd);
        if (k != 0 && std::fabs(off) < kPrismDangerBandDeg * deg) h = hd + (off < 0.0f ? -1.0f : 1.0f) * kPrismDangerBandDeg * deg;
        laneColour_[k + 2] = k == 0 ? player : fullChroma(kPrismLaneL, h);
    }
    laneReady_ = true;
    // The build on the shots (look.md 5): Overcharge's heads, full chroma: white, amber (OKLCH 0.82 at 70), gold (0.88 at
    // 95), white (with a gold rim), ice blue (0.85 at 230), electric violet (0.72 at 295, with a white core).
    overHead_[0] = white();
    overHead_[1] = fullChroma(0.82f, 70.0f * deg);
    overHead_[2] = fullChroma(0.88f, 95.0f * deg);
    overHead_[3] = white();
    overHead_[4] = fullChroma(0.85f, 230.0f * deg);
    overHead_[5] = fullChroma(0.72f, 295.0f * deg);
    buildGold_ = overHead_[2];
    buildReady_ = true;
}

void PlayView::dropRing(Vec2 at, double t) {
    if (!reduceFlashing_) push(kImpDropWave, at, 140.0f, 260.0f, static_cast<float>(t));
}

void PlayView::push(u8 kind, Vec2 at, float amp, float radius, float t) {
    lat_->impulse(at, amp, radius);
    ImpulseNote& n = impulses_[impulseHead_];
    impulseHead_ = (impulseHead_ + 1) % kImpulseNotes;
    n.t = t;
    n.amp = amp;
    n.radius = radius;
    n.at = at;
    n.kind = kind;
    lastViewT_ = t;
}

void PlayView::latticeTrace(char* out, u32 capacity) const {
    static const char* const kNames[kImpKinds] = {"kills", "hits", "ship down", "ship lost", "Novas", "Dashes", "the Drop's wave"};
    float sum[kImpKinds] = {};
    u32 count[kImpKinds] = {};
    float newest = lastViewT_;
    for (const ImpulseNote& n : impulses_) newest = std::fmax(newest, n.t);
    for (const ImpulseNote& n : impulses_) {
        if (newest - n.t > 1.0f || !visible(n.at, n.radius)) continue;
        sum[n.kind] += n.amp;
        ++count[n.kind];
    }
    u32 o = static_cast<u32>(std::snprintf(out, capacity, "lattice impulses in this frame over the last second:"));
    for (u32 k = 0; k < kImpKinds && o < capacity; ++k)
        o += static_cast<u32>(std::snprintf(out + o, capacity - o, " %s %u (sum %.0f)%s", kNames[k], count[k], static_cast<double>(sum[k]),
                                            k + 1 < kImpKinds ? "," : ""));
    const Lattice::HeatStats& h = lat_->heatStats();
    const float n = h.nodes ? static_cast<float>(h.nodes) : 1.0f;
    if (o < capacity)
        o += static_cast<u32>(std::snprintf(out + o, capacity - o,
                                            "; the middle third's lattice: %u nodes, %.0f%% warm, %.0f%% hot, mean heat %.1f wu from the springs (the "
                                            "impulses) and %.1f wu from the kick ripples",
                                            h.nodes, static_cast<double>(100.0f * static_cast<float>(h.warm) / n),
                                            static_cast<double>(100.0f * static_cast<float>(h.hot) / n), static_cast<double>(h.fromSprings / n),
                                            static_cast<double>(h.fromRipples / n)));
    if (o < capacity)
        std::snprintf(out + o, capacity - o, "; crowd gain %.2f, swarm gain %.2f, the beat's amount %.2f", static_cast<double>(crowdGain_),
                      static_cast<double>(swarmGain_), static_cast<double>(lastBeatAmt_));
}

Vec2 PlayView::edgePlace(Vec2 dsc, float hx, float hy, float r) const {
    // The point on the box (half sizes hx, hy around the centre) toward dsc; then, if its disc comes within 20 px of a
    // HUD block, the nearest free point along the box's edge either way (screens.md "In the run"). The edge is walked as
    // a perimeter coordinate: 0 at the top left corner, clockwise.
    const float k = std::fmin(hx / std::fmax(std::fabs(dsc.x), 1e-3f), hy / std::fmax(std::fabs(dsc.y), 1e-3f));
    const Vec2 p0 = dsc * k;
    if (!keepOut_.count) return p0;
    const float clear = 20.0f * refPx_ + r;
    const auto free = [&](Vec2 p) {
        const Vec2 sp = centre_ + p;
        for (u32 i = 0; i < keepOut_.count; ++i) {
            const float dx = std::fmax(std::fmax(keepOut_.a[i].x - sp.x, sp.x - keepOut_.b[i].x), 0.0f);
            const float dy = std::fmax(std::fmax(keepOut_.a[i].y - sp.y, sp.y - keepOut_.b[i].y), 0.0f);
            if (dx * dx + dy * dy < clear * clear) return false;
        }
        return true;
    };
    if (free(p0)) return p0;
    const float wTop = 2.0f * hx, hSide = 2.0f * hy, perim = 2.0f * (wTop + hSide);
    const auto at = [&](float u) {
        u = std::fmod(u, perim);
        if (u < 0.0f) u += perim;
        if (u < wTop) return Vec2{-hx + u, -hy};
        u -= wTop;
        if (u < hSide) return Vec2{hx, -hy + u};
        u -= hSide;
        if (u < wTop) return Vec2{hx - u, hy};
        u -= wTop;
        return Vec2{-hx, hy - u};
    };
    float u0 = 0.0f;
    if (std::fabs(p0.y + hy) < 0.5f) u0 = p0.x + hx;
    else if (std::fabs(p0.x - hx) < 0.5f) u0 = wTop + p0.y + hy;
    else if (std::fabs(p0.y - hy) < 0.5f) u0 = wTop + hSide + hx - p0.x;
    else u0 = 2.0f * wTop + hSide + hy - p0.y;
    const float step = 4.0f * refPx_;
    for (float d = step; d < 0.5f * perim; d += step) {
        if (free(at(u0 - d))) return at(u0 - d);
        if (free(at(u0 + d))) return at(u0 + d);
    }
    return p0;
}

void PlayView::edgeChevron(FrameDraw& d, Vec2 world, Vec3 colour, bool boss, float swell, u8 tipMark) {
    const Look::EdgeIndicator& ei = look_->edge;
    const float h = (boss ? ei.bossChevron : ei.chevron) * refPx_;
    const float disc = kChevronDisc * h;
    const float W = 2.0f * centre_.x, H = 2.0f * centre_.y;
    // The disc sits the inset inside the edge, on the line from the centre toward the target.
    const Vec2 dsc = toScreen(world) - centre_;
    const float inset = ei.inset * refPx_ + disc;
    const float hx = 0.5f * W - inset, hy = 0.5f * H - inset;
    if (hx <= 0.0f || hy <= 0.0f) return;
    const Vec2 c = centre_ + edgePlace(dsc, hx, hy, disc);
    const float ang = angleOf(dsc), cs = std::cos(ang), sn = std::sin(ang);
    d.threatRims.line(c, c, disc, Vec3{}, Vec3{}, kDisc | capsuleOpacity(ei.discAlpha));
    const float s = h * swell;
    const float hw = kChevronStroke * s, rim = look_->threat.bulletRim * refPx_;
    const Vec3 core{clampf(colour.x, 0.0f, 1.0f), clampf(colour.y, 0.0f, 1.0f), clampf(colour.z, 0.0f, 1.0f)};
    const float depth = (boss ? 0.75f : 1.0f) * kChevronDepth * s;  // the boss's pair is shallower, to stay on its disc
    const auto chevron = [&](float along) {
        const Vec2 tip = c + rotate(Vec2{along + 0.5f * depth, 0.0f}, cs, sn);
        const Vec2 a0 = c + rotate(Vec2{along - 0.5f * depth, 0.5f * s}, cs, sn);
        const Vec2 a1 = c + rotate(Vec2{along - 0.5f * depth, -0.5f * s}, cs, sn);
        d.threatRims.line(a0, tip, hw + rim, Vec3{}, Vec3{}, 0);
        d.threatRims.line(tip, a1, hw + rim, Vec3{}, Vec3{}, 0);
        d.threatCores.line(a0, tip, hw, core, core, 0);
        d.threatCores.line(tip, a1, hw, core, core, 0);
    };
    if (boss) {
        chevron(0.5f * kChevronPairGap * s);
        chevron(-0.5f * kChevronPairGap * s);
    } else {
        chevron(0.0f);
    }
    if (tipMark == 1) {
        // M4 (look.md 6): a downed teammate's chevron carries a small ring at its tip.
        const Vec2 tip = c + rotate(Vec2{0.5f * depth + 0.3f * s, 0.0f}, cs, sn);
        const float rr = 0.22f * s;
        for (u32 k = 0; k < 12; ++k) {
            const float a0 = kTau * static_cast<float>(k) / 12.0f, a1 = kTau * static_cast<float>(k + 1) / 12.0f;
            const Vec2 p0 = tip + Vec2{std::cos(a0), std::sin(a0)} * rr, p1 = tip + Vec2{std::cos(a1), std::sin(a1)} * rr;
            d.threatRims.line(p0, p1, hw + rim, Vec3{}, Vec3{}, 0);
            d.threatCores.line(p0, p1, hw, core, core, 0);
        }
    } else if (tipMark == 2) {
        // Play test 2 (look.md 6): a Cell's chevron carries a plus at its tip (the only plus sign in the game).
        const Vec2 tip = c + rotate(Vec2{0.5f * depth + 0.32f * s, 0.0f}, cs, sn);
        const float arm = 0.2f * s;
        const Vec2 ends[4] = {tip + Vec2{-arm, 0.0f}, tip + Vec2{arm, 0.0f}, tip + Vec2{0.0f, -arm}, tip + Vec2{0.0f, arm}};
        for (u32 k = 0; k < 4; k += 2) {
            d.threatRims.line(ends[k], ends[k + 1], hw + rim, Vec3{}, Vec3{}, 0);
            d.threatCores.line(ends[k], ends[k + 1], hw, core, core, 0);
        }
    }
    const LookSoftLight& glow = look_->soft.edgeIndicator;
    d.pools.glow(c, glow.radius[0] * refPx_, colour * glow.intensity[0]);
}
}  // namespace rf
