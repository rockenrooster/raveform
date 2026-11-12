#include "game/play_view.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "audio/band.h"
#include "core/args.h"
#include "core/half.h"
#include "core/log.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
constexpr float kRefPxPerWu = 1.5f;       // look.md: 1 wu = 1.5 reference px at zoom 1
constexpr float kStandardEnemyWu = 14.0f;  // look.md §4: glyph sizes are relative to a 14 wu enemy

// Numbers chosen for the view (docs/PROGRESS.md M3, "numbers chosen"): the look bible gives none.
constexpr float kLookAheadS = 0.25f;      // the camera leads the ship by its velocity x this (scenes.md §2's 0.25 s)
constexpr float kAimBiasWu = 60.0f;       // ... and leans this far along the aim while firing
constexpr float kLookAheadTau = 0.25f;    // s: the lead eases in and out
constexpr float kZoomCalm = 1.0f, kZoomBusy = 0.85f;  // GAME_DESIGN 9: breathes between 0.85x and 1.25x
constexpr float kZoomLoadLo = 60.0f, kZoomLoadHi = 300.0f, kZoomTau = 2.0f;
constexpr float kWallIntensity = 1.6f, kWallWidth = 2.6f, kWallSnare = 0.6f, kWallBass = 0.3f;
constexpr float kHitShakeRefPx = 4.0f, kNovaShakeRefPx = 3.0f, kDeathShakeRefPx = 6.0f;
constexpr float kMuzzleFlashS = 0.06f, kMuzzleFlashRefPx = 10.0f, kMuzzleFlash = 0.9f;
constexpr float kNovaDim = 0.30f;         // exposure down by this much at the end of the inhale
constexpr float kNovaInhaleRingWu = 600.0f, kNovaFrontWidthRefPx = 6.0f, kNovaFront = 3.0f;
constexpr u32 kNovaInhaleSparks = 140, kNovaFrontSparks = 400;
constexpr float kPanicRingS = 0.25f;
constexpr u32 kDashGhosts = 5;            // look.md §5
constexpr float kDashGhostS = 0.25f;
constexpr float kIndicatorInsetRefPx = 46.0f, kIndicatorSizeRefPx = 16.0f;

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
    for (u32 k = 1; k <= 6; ++k) {
        char id[16];
        std::snprintf(id, sizeof id, "halo_%u", k);
        gHalo_[k] = glyphs.find(id);
    }
    gShard_ = glyphs.find("shard");
    gBullet_ = glyphs.find("enemy_bullet");
    gMarkBHeading_ = glyphs.find("mark_b_heading");
    gMarkBSpin_ = glyphs.find("mark_b_spin");
    gMarkC_ = glyphs.find("mark_c");
    gRing32_ = glyphs.find("ring32");
    gRing64_ = glyphs.find("ring64");
    gRing128_ = glyphs.find("ring128");
    if (gShip_ < 0 || gHalo_[1] < 0 || gHalo_[6] < 0 || gShard_ < 0 || gBullet_ < 0 || gRing32_ < 0 || gRing64_ < 0 ||
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
    return true;
}

void PlayView::resetEffects() {
    for (u32 i = 0; i < poolCap_; ++i) pools_[i].born = -100.0f;
    for (u32 i = 0; i < emberCap_; ++i) embers_[i].born = -100.0f;
    for (u32 i = 0; i < shatterCap_; ++i) shatter_[i].born = -100.0f;
    for (u32 i = 0; i < ringCap_; ++i) rings_[i].born = -100.0f;
    for (ShockRing& s : shock_) s.start = -100.0f;
    sparkReqCount_ = 0;
    trailCount_ = 0;
    dash_.t = -100.0f;
    novaPressT_ = novaBlastT_ = novaFrontT_ = -100.0f;
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
    if (sparkReqCount_ < sparkReqCap_) sparkReqs_[sparkReqCount_++] = r;
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

void PlayView::killRecipe(Vec2 at, u8 size, u8 hue, float heading, int g, float radius, float t) {
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
        addPool(at, pl.radius[0], pl.radius[1], palette_->hue[hue], pl.intensity[0], pl.intensity[1], x.lifeS[1], true, 0, false,
                true, t);
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
    lattice_.impulse(at, x.gridImpulse * crowdGain_, size == 0 ? 60.0f : (size == 1 ? 90.0f : 130.0f));
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
            int g = glyphs_->enemy(familyName(s.family), s.tier);
            for (u32 t = 1; g < 0 && t <= 5; ++t) g = glyphs_->enemy(familyName(s.family), t);
            speciesGlyph_[i] = g >= 0 ? g : gRing32_;
        }
        speciesGlyphsReady_ = true;
    }
    const Vec3 player = palettes_->player[0], light = palettes_->playerLight[0], deep = palettes_->playerDeep[0];
    for (u32 k = 0; k < w.eventCount(); ++k) {
        const Event& e = w.events()[k];
        const float t = static_cast<float>(tickEnd - e.age);
        const Vec2 at{e.x, e.y};
        switch (e.type) {
            case EventType::Kill: {
                const Species& s = table.get(e.species);
                if (e.c) {  // a warp-in dissolved by a Nova: a small flash only
                    const LookSoftLight& f = look_->soft.killFlashSmall;
                    addPool(at, f.radius[0], f.radius[0], palette_->hue[s.hue], f.intensity[0], f.intensity[1], f.lifeS, false,
                            0, true, true, t);
                    break;
                }
                killRecipe(at, e.a, s.hue, e.f, speciesGlyph_[e.species], s.radius, t);
                break;
            }
            case EventType::Impact: {
                // A shot that hit without killing: 3-4 sparks back along it, 0.15 s (look.md §5).
                const Species& s = table.get(e.species);
                SparkReq r{};
                r.at = at;
                r.dir = Vec2{-e.x2, -e.y2};
                r.count = static_cast<u16>(look_->boom.impactSparks[0] + rng_.below(2));
                r.kind = 3;
                r.a = palette_->hue[s.hue];
                r.b = palette_->partner[s.hue];
                r.t0 = t;
                sparkRequest(r);
                break;
            }
            case EventType::Shot:
                // Muzzle flash (GAME_DESIGN 11.1, lead notes).
                addPool(at, kMuzzleFlashRefPx, kMuzzleFlashRefPx, mix(player, white(), 0.5f), kMuzzleFlash, 0.0f, kMuzzleFlashS,
                        false, 1, true, false, t);
                break;
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
                hitT_ = t;
                const u8 left = e.b;
                const int arcs = gHalo_[left + 1 <= 6 ? left + 1 : 6];
                shatterGlyph(arcs, at, rng_.range(0.0f, kTau), kStandardEnemyWu, mix(player, white(), 0.3f) * 2.0f, 160.0f, 320.0f,
                             t);
                addShockRings(at, 1, kPanicRadiusWu, player, light, t, kPanicRingS);
                lattice_.impulse(at, 260.0f, 120.0f);
                if (kHitShakeRefPx > shakeAmp_) shakeAmp_ = kHitShakeRefPx;
                break;
            }
            case EventType::Death: {
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
                lattice_.impulse(at, 420.0f, 200.0f);
                if (kDeathShakeRefPx > shakeAmp_) shakeAmp_ = kDeathShakeRefPx;
                break;
            }
            case EventType::NovaPress: {
                novaPressT_ = t;
                novaAt_ = at;
                const float tpS = w.ticksPerSecond();
                novaBlastT_ = t + static_cast<float>((e.tick - (w.music() - static_cast<double>(e.age) * tpS)) / tpS);
                // Sparks appear on a 600 wu ring and fall inward until the beat (look.md §5).
                SparkReq r{};
                r.at = at;
                r.count = static_cast<u16>(kNovaInhaleSparks * (tier_.particleDensity > 0.5f ? 1.0f : 0.6f));
                r.kind = 6;
                r.a = player;
                r.b = light;
                r.t0 = t;
                r.extra = novaBlastT_ - t;
                sparkRequest(r);
                break;
            }
            case EventType::NovaBlast: {
                // The thick white front to 1400 wu in 0.35 s with 400 radial sparks and the bloom pump.
                novaFrontT_ = t;
                novaAt_ = at;
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
                for (int i = 0; i < 2; ++i) lattice_.impulse(at, 420.0f, 260.0f);
                if (kNovaShakeRefPx > shakeAmp_) shakeAmp_ = kNovaShakeRefPx;
                break;
            }
            case EventType::Dash:
                dash_.from = at;
                dash_.to = Vec2{e.x2, e.y2};
                dash_.t = t;
                dash_.heading = angleOf(dash_.to - dash_.from);
                dash_.perfect = e.b != 0;
                if (dash_.perfect) addShockRings(dash_.to, 1, 90.0f, player, light, t);
                lattice_.impulse(dash_.to, 120.0f, 70.0f);
                break;
            case EventType::Restart: resetEffects(); break;
            default: break;
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

void PlayView::telegraphLine(FrameDraw& d, Vec2 a, Vec2 b, float sceneLight) {
    // Telegraphs (look.md §3): danger in the scene at 1.3 x the swarm gain, and on the threat layer.
    const Vec3 danger = palettes_->danger;
    const LookLine& tl = look_->lines.telegraph;
    if (shown(kMuteBullet)) {
        const Vec3 c = danger * (tl.intensity * sceneLight);
        d.scene.line(toScene(a), toScene(b), 0.5f * tl.width * refPx_, c, c, 0);
    }
    if (!shown(kMutePriority)) return;
    const float w = 0.5f * look_->threat.telegraphWidth * refPx_;
    const Vec3 core = danger * 0.9f;
    d.threatRims.line(toScreen(a), toScreen(b), w + look_->threat.telegraphRim * refPx_, Vec3{}, Vec3{}, 0);
    d.threatCores.line(toScreen(a), toScreen(b), w, core, core, 0);
}

void PlayView::envelopes(const ViewInput& in) {
    // look.md §7: one envelope per drum. Heard events while the band plays; else from the musical
    // position (kick on every beat, snare or clap on 2 and 4, hats on the off-beat eighths).
    const float t = static_cast<float>(in.time);
    if (in.beat && in.beat->running) {
        using audio::BeatKind;
        const BeatView& v = *in.beat;
        kick_ = v.envelope(BeatKind::Kick, look_->kickDecayMs * 1e-3f);
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
    const i64 b = static_cast<i64>(beat);
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
    const World& w = *in.world;
    const float t = static_cast<float>(in.time);
    now_ = in.time;
    refPx_ = H / 1080.0f;
    const Vec3 W1 = white();
    const Vec3 player = palettes_->player[0], deep = palettes_->playerDeep[0], light = palettes_->playerLight[0];
    const Vec3 danger = palettes_->danger;
    const Vec3 accent = palette_->accent;
    const float a = in.alpha;
    const float tps = in.bpm * 96.0f / 60.0f;
    const float barS = 384.0f / tps;
    envelopes(in);
    const float beatAmt = beatAmount();
    if (!speciesGlyphsReady_) onEvents(w, in.time);  // builds the species -> glyph table (no events yet)

    // ---- Camera (GAME_DESIGN 9): follows the ship with look-ahead along its velocity and a lean toward
    // its aim; zoom breathes with the crowd; the kick punches it a little (look.md §7).
    const Vec2 S = in.shipPos;
    {
        // The lead follows the course at cruising speed at most (a Dash does not swing the camera).
        const float sp = length(in.shipVel);
        Vec2 target = (sp > kPlayerSpeed ? in.shipVel * (kPlayerSpeed / sp) : in.shipVel) * kLookAheadS;
        if (in.shipFiring) target = target + in.shipAim * kAimBiasWu;
        const float k = 1.0f - std::exp(-in.dt / kLookAheadTau);
        lookAhead_ = lookAhead_ + (target - lookAhead_) * k;
        const float busy = clampf((stats_.load - kZoomLoadLo) / (kZoomLoadHi - kZoomLoadLo), 0.0f, 1.0f);
        const float zt = kZoomCalm + (kZoomBusy - kZoomCalm) * busy;
        zoomBase_ += (zt - zoomBase_) * (1.0f - std::exp(-in.dt / kZoomTau));
        zoomBase_ = clampf(zoomBase_, look_->zoomMin, look_->zoomMax);
    }
    zoom_ = zoomBase_ * (1.0f + look_->zoomKick * beatAmt * kick_);
    pxPerWu_ = kRefPxPerWu * refPx_ * zoom_;
    centre_ = {0.5f * W, 0.5f * H};
    viewW_ = W / pxPerWu_;
    viewH_ = H / pxPerWu_;
    cam_ = S + lookAhead_;
    // The view may look up to 240 wu past a wall, no further.
    const float mx = kArenaHalfW + 240.0f - 0.5f * viewW_, my = kArenaHalfH + 240.0f - 0.5f * viewH_;
    if (mx > 0.0f) cam_.x = clampf(cam_.x, -mx, mx);
    if (my > 0.0f) cam_.y = clampf(cam_.y, -my, my);
    shakeAmp_ *= std::exp(-in.dt / 0.15f);
    const float shakePx = clampf(shakeAmp_, 0.0f, look_->shakeMaxRefPx) * refPx_;
    shake_ = shakePx > 0.05f ? dir(rng_.range(0.0f, kTau)) * shakePx : Vec2{};
    lattice_.recentre(cam_);
    lattice_.step(in.dt * w.timeScale());
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
    const float crowd = crowdGain_, swarm = swarmGain_;

    // ---- Sparks born since the last frame (stateless; TECH_SPEC 5.3).
    stats_.sparksBorn = stats_.sparksDropped = 0;
    if (d.particles) {
        const Look::Explosions& boom = look_->boom;
        const float levels[4] = {2.4f, 3.4f, 4.6f, 5.6f};
        for (u32 r = 0; r < sparkReqCount_; ++r) {
            const SparkReq& q = sparkReqs_[r];
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
    if (!shown(kMuteBackground)) bg.edge = bg.centre = Vec3{};
    if (tier_.background && shown(kMuteBackground)) {
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
    lv.boundsMin = Vec2{-kArenaHalfW, -kArenaHalfH};
    lv.boundsMax = Vec2{kArenaHalfW, kArenaHalfH};
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
    lattice_.setWells(nullptr, 0, 0.0f);
    if (shown(kMuteGrid)) lattice_.draw(d.scene, lv, ls);

    // ---- Arena walls (GAME_DESIGN 9): glowing lines in the accent; the snare flashes them, the bass
    // breathes their width (GAME_DESIGN 11.1).
    {
        const float i = kWallIntensity * (1.0f + kWallSnare * beatAmt * snare_);
        const float hw = 0.5f * kWallWidth * (1.0f + kWallBass * beatAmt * bass_) * refPx_;
        const Vec3 c = accent * i;
        const Vec2 corners[4] = {{-kArenaHalfW, -kArenaHalfH}, {kArenaHalfW, -kArenaHalfH}, {kArenaHalfW, kArenaHalfH}, {-kArenaHalfW, kArenaHalfH}};
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
        const Vec2 pos = lerp2(Vec2{e.px[i], e.py[i]}, Vec2{e.x[i], e.y[i]}, a);
        const Species& s = table.get(e.species[i]);
        const int g = speciesGlyph_[e.species[i]];
        const GlyphDef& gd = glyphs_->def(static_cast<u32>(g));
        const float scale = e.radius[i];
        const bool inView = visible(pos, 0.0f);
        // Edge indicators (look.md §6): an engaged shooter outside the view, in the danger colour.
        if (!inView && s.ranged() && (e.state[i] == kGlint || e.state[i] == kFiring) && stats_.indicators < 8 &&
            shown(kMutePriority)) {
            const Vec2 dsc = toScreen(pos) - centre_;
            const float inset = kIndicatorInsetRefPx * refPx_;
            const float hx = 0.5f * W - inset, hy = 0.5f * H - inset;
            const float k = std::fmin(hx / std::fmax(std::fabs(dsc.x), 1e-3f), hy / std::fmax(std::fabs(dsc.y), 1e-3f));
            const Vec2 c = centre_ + dsc * k;
            const float ang = angleOf(dsc);
            const float sz = kIndicatorSizeRefPx * refPx_;
            const Vec2 p0 = c + rotate(Vec2{-0.6f, 0.8f}, std::cos(ang), std::sin(ang)) * sz;
            const Vec2 p1 = c + rotate(Vec2{0.6f, 0.0f}, std::cos(ang), std::sin(ang)) * sz;
            const Vec2 p2 = c + rotate(Vec2{-0.6f, -0.8f}, std::cos(ang), std::sin(ang)) * sz;
            const float hw = 0.5f * look_->threat.bulletWidth * refPx_;
            const Vec3 core = mix(danger, W1, 0.25f);
            d.threatRims.line(p0, p1, hw + look_->threat.bulletRim * refPx_, Vec3{}, Vec3{}, 0);
            d.threatRims.line(p1, p2, hw + look_->threat.bulletRim * refPx_, Vec3{}, Vec3{}, 0);
            d.threatCores.line(p0, p1, hw, core, core, 0);
            d.threatCores.line(p1, p2, hw, core, core, 0);
            d.pools.glow(c, 22.0f * refPx_, danger * 0.3f);
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
        const Vec3 hue = palette_->hue[s.hue], partner = palette_->partner[s.hue];
        float intensity = (look_->lines.enemyHullTier1.intensity + le.hullIntensityPerTier * (static_cast<float>(s.tier) - 1.0f)) * crowd;
        const float halfWidth = 0.5f * look_->lines.enemyHullTier1.width;
        const float phase = static_cast<float>(e.serial[i] % 997u) * 0.61803f * kTau;
        float rot0, rot1;
        if (gd.facesHeading) {
            rot0 = rot1 = lerpAngle(e.pheading[i], e.heading[i], a);
        } else {
            rot0 = phase + kTau * le.hullTurnsPerBar * static_cast<float>(hullBars);
            rot1 = phase + kTau * le.ornamentTurnsPerBar * static_cast<float>(steppedBars);
        }
        const bool lod = scale * pxPerWu_ < tier_.lodRadiusPx;
        if (shown(kMuteEnemy)) {
            glyph(d.glyphs, g, lod, pos, rot0, rot1, scale, intensity, halfWidth, kGlyphEnemy, hue, partner);
            if (s.variant != kVariantA) {
                const int mark = s.variant == kVariantC ? gMarkC_ : (gd.facesHeading ? gMarkBHeading_ : gMarkBSpin_);
                glyph(d.glyphs, mark, false, pos, rot0, rot0, scale, look_->lines.variantMark.intensity * crowd,
                      0.5f * look_->lines.variantMark.width, kGlyphWhite, W1, W1);
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
                telegraphLine(d, c - dd * 10.0f + n * 10.0f, c, swarm);
                telegraphLine(d, c - dd * 10.0f - n * 10.0f, c, swarm);
            }
        } else if (e.state[i] == kGlint) {
            const float progress = clampf(1.0f - static_cast<float>((e.until[i] - in.music) / kTicksPerBeat), 0.0f, 1.0f);
            const float len = 12.0f + 26.0f * progress;
            const float ang = 0.25f * kPi + progress * 0.5f * kPi;
            for (u32 k = 0; k < 2; ++k) {
                const Vec2 dd = dir(ang + 0.5f * kPi * static_cast<float>(k));
                telegraphLine(d, pos - dd * len, pos + dd * len, swarm);
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
            if (shown(kMuteBullet)) {
                glyph(d.glyphs, gBullet_, false, p, 0.0f, 0.0f, bulletScale, look_->lines.enemyBullet.intensity * swarm,
                      0.5f * look_->lines.enemyBullet.width, kGlyphFlat, danger, danger);
                if (tier_.bulletSparkles)
                    d.sparkles.glow(toScene(p), look_->soft.enemyBullet.radius[0] * refPx_, danger * (look_->soft.enemyBullet.intensity[0] * swarm));
            }
            if (shown(kMutePriority)) {
                glyph(d.threatGlyphRims, gBullet_, false, p, 0.0f, 0.0f, bulletScale, 1.0f, rimHalf, kGlyphFlat, Vec3{}, Vec3{});
                glyph(d.threatGlyphCores, gBullet_, false, p, 0.0f, 0.0f, bulletScale, 1.0f, coreHalf, kGlyphFlat, threatCore, threatCore);
            }
        }
    }

    // ---- The player: two-tone ship, halo (one arc per Shield cell), trail, pool, priority layer.
    const PlayerState& me = w.player(0);
    const float shipScale = kStandardEnemyWu * glyphs_->def(static_cast<u32>(gShip_)).size;
    const bool playerShown = shown(kMutePlayer);
    // Trail history (the view clock).
    if (me.alive) {
        trailPos_[trailHead_] = S;
        trailT_[trailHead_] = t;
        trailHead_ = (trailHead_ + 1) % kTrail;
        if (trailCount_ < kTrail) ++trailCount_;
    } else {
        trailCount_ = 0;
    }
    if (me.alive) {
        const float heading = in.shipHeading;
        const LookLine& hull = look_->lines.playerHull;
        const u32 cells = me.cells > 6 ? 6u : me.cells;
        const float haloRot = 0.3f * t;
        const float haloPulse = 1.0f + 0.25f * kick_;
        if (playerShown) {
            glyph(d.glyphs, gShip_, false, S, heading, heading, shipScale, hull.intensity, 0.5f * hull.width, kGlyphPlayer, player, deep);
            glyph(d.glyphs, gShip_, true, S, heading, heading, shipScale, look_->lines.playerWhiteCopy.intensity,
                  0.5f * look_->lines.playerWhiteCopy.width, kGlyphWhite, W1, W1);
            if (cells == 1) {
                // The last arc pulses in the danger colour (look.md §4).
                glyph(d.glyphs, gHalo_[1], false, S, haloRot, haloRot, shipScale, look_->lines.playerHalo.intensity * (0.8f + 1.2f * kick_),
                      0.5f * look_->lines.playerHalo.width, kGlyphFlat, danger, danger);
            } else if (cells > 0) {
                glyph(d.glyphs, gHalo_[cells], false, S, haloRot, haloRot, shipScale, look_->lines.playerHalo.intensity * haloPulse,
                      0.5f * look_->lines.playerHalo.width, kGlyphRingA, player, light);
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
        if (shown(kMutePriority)) {
            // The ship on the priority layer (look.md §6, §9): hull and halo after tone mapping with a rim.
            const Look::PriorityShip& ph = look_->priorityHull;
            const Look::PriorityShip& pa = look_->priorityHalo;
            const Vec3 hullCore = light * 0.55f + W1 * 0.45f;
            const Vec3 haloCore = cells == 1 ? mix(danger, W1, 0.25f) : player * 0.75f + W1 * 0.25f;
            glyph(d.threatGlyphRims, gShip_, true, S, heading, heading, shipScale, 1.0f, 0.5f * ph.width + ph.rim, kGlyphFlat, Vec3{}, Vec3{});
            glyph(d.priorityShipCores, gShip_, true, S, heading, heading, shipScale, 1.0f, 0.5f * ph.width, kGlyphFlat, hullCore, hullCore);
            if (cells > 0) {
                glyph(d.threatGlyphRims, gHalo_[cells], false, S, haloRot, haloRot, shipScale, 1.0f, 0.5f * pa.width + pa.rim, kGlyphFlat,
                      Vec3{}, Vec3{});
                glyph(d.priorityShipCores, gHalo_[cells], false, S, haloRot, haloRot, shipScale, 1.0f, 0.5f * pa.width, kGlyphFlat,
                      haloCore, haloCore);
            }
            // GAME_DESIGN 11.3: the local player's hit point is always marked by a small bright dot.
            d.threatRims.line(toScreen(S), toScreen(S), 2.2f * refPx_, Vec3{}, Vec3{}, kDisc);
            d.threatCores.line(toScreen(S), toScreen(S), 1.3f * refPx_, W1, W1, kDisc);
        }
    }
    // Dash: five fading ghost copies along the path; a Perfect Dash adds a four-point sparkle (look.md §5).
    {
        const float age = t - dash_.t;
        if (age >= 0.0f && age < kDashGhostS && playerShown) {
            const float fade = 1.0f - age / kDashGhostS;
            for (u32 k = 0; k < kDashGhosts; ++k) {
                const float u = static_cast<float>(k) / static_cast<float>(kDashGhosts);
                const Vec2 p = lerp2(dash_.from, dash_.to, u);
                const float i = look_->lines.playerHull.intensity * 0.6f * fade * (0.35f + 0.65f * u);
                glyph(d.glyphs, gShip_, true, p, dash_.heading, dash_.heading, shipScale, i, 0.5f * look_->lines.playerHull.width,
                      kGlyphPlayer, player, deep);
            }
            if (dash_.perfect) {
                flare(d.scene, dash_.to, 0.0f, 46.0f * (0.6f + 0.4f * fade), 2, W1 * (2.4f * fade), 0.7f);
                d.sparkles.glow(toScene(dash_.to), 14.0f * refPx_, mix(player, W1, 0.6f) * (1.6f * fade));
            }
        }
    }
    // The Nova: the inhale (sparks falling in, the screen dimming) and the front.
    float novaDim = 0.0f;
    {
        // One ring each, as M1's Drop front: both move far more than the afterglow's blur in a frame (the
        // front about 100 px), so at low load (swarm gain 1) the afterglow leaves soft copies behind them
        // (docs/PROGRESS.md M3: a question for the lead; drawing them swept like shots filled the disc).
        if (t >= novaPressT_ && t < novaBlastT_ && novaBlastT_ > novaPressT_) {
            const float span = novaBlastT_ - novaPressT_;
            novaDim = kNovaDim * smooth01((t - novaPressT_) / span);
            if (playerShown) {
                const float u = (t - novaPressT_) / span;
                glyph(d.glyphs, gRing128_, false, S, 0.0f, 0.0f, kNovaInhaleRingWu * (1.0f - u), 1.2f * u, 1.0f, kGlyphRingA, player, light);
            }
        }
        const float age = t - novaFrontT_;
        if (age >= 0.0f && age < tuning::kNovaFrontS && shown(kMuteFx)) {
            const float u = age / tuning::kNovaFrontS;
            glyph(d.glyphs, gRing128_, false, novaAt_, 0.0f, 0.0f, tuning::kNovaFrontWu * u, kNovaFront * (1.0f - u),
                  0.5f * kNovaFrontWidthRefPx, kGlyphFlat, W1, W1);
        }
        const float pumpAge = t - novaFrontT_;
        novaPump_ = (pumpAge >= 0.0f && pumpAge < look_->novaPumpMs * 1e-3f)
                        ? (reduceFlashing_ ? look_->novaPumpReduced : look_->novaPumpBloom)
                        : 0.0f;
    }

    // ---- Shots: a capsule as long as the shot travels in a frame (look.md v2.2 §3), a sparkle at the head.
    const Shots& sh = w.shots();
    {
        const Vec3 shotHead = W1 * look_->soft.shotHead.intensity[0];
        const Look::ShotStreak& ss = look_->shotStreak;
        const LookLine& pl = look_->lines.playerShot;
        for (u32 i = 0; i < sh.count; ++i) {
            if (sh.dead[i]) continue;
            const Vec2 head = lerp2(Vec2{sh.px[i], sh.py[i]}, Vec2{sh.x[i], sh.y[i]}, a);
            const float len = std::fmax(sh.speed[i] * ss.frameS + ss.overlapWu, ss.minWu);
            if (!visible(head, len + 4.0f) || !playerShown) continue;
            const Vec2 tail = head - Vec2{sh.dx[i], sh.dy[i]} * len;
            d.scene.line(toScene(tail), toScene(head), 0.5f * pl.width * refPx_, player * pl.intensity, W1 * pl.intensity, 0);
            d.sparkles.glow(toScene(head), look_->soft.shotHead.radius[0] * refPx_, shotHead);
        }
    }

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
            // Loose Shards fade over their last bar.
            const float left = static_cast<float>((s.born[i] + lifeTicks - in.music) / kTicksPerBar);
            const float fade = clampf(left, 0.0f, 1.0f);
            const float i0 = sl.intensity * swarm * fade;
            glyph(d.glyphs, gShard_, false, p, static_cast<float>(i) * 0.7f + 4.0f * t, 0.0f, shardScale, i0, 0.5f * sl.width,
                  kGlyphFlat, shardColour, shardColour);
            if (tier_.bulletSparkles)
                d.sparkles.glow(toScene(p), look_->soft.shard.radius[0] * refPx_, palettes_->pickup * (look_->soft.shard.intensity[0] * swarm * fade));
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

    // ---- Composite controls (look.md §7).
    const float pump = shown(kMutePump) ? kick_ * beatAmt : 0.0f;
    params.bloomPump = (1.0f + look_->bloomKick * pump) * (1.0f + novaPump_);
    params.exposurePump = (1.0f + look_->exposureKick * pump) * (1.0f - novaDim);
    params.vignette = shown(kMuteVignette);
    params.vignetteAmount = look_->vignetteAmount;
    params.drain = 0.0f;
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
    params.particleMaxLife = 2.0f;
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

}  // namespace rf
