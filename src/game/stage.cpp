#include "game/stage.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "audio/band.h"
#include "core/args.h"
#include "core/half.h"
#include "core/log.h"

namespace rf {

namespace {
enum Family : u8 {
    kShoal,
    kDrifter,
    kHound,
    kFlanker,
    kRam,
    kCarom,
    kSentry,
    kWyrmHead,
    kWyrmBody,
    kMoth,
    kTether,
    kBlink,
    kWeaver,
    kBoss,
    kFamilies
};
const char* const kFamilyGlyph[kFamilies] = {"shoal", "drifter", "hound",     "flanker", "ram",    "carom",  "sentry",
                                             "wyrm_head", "wyrm_body", "moth", "tether", "blink", "weaver", "carom"};
enum EnemyFlags : u8 { kElite = 1, kMarkB = 2, kMarkC = 4, kBossPart = 8, kWarping = 16 };

constexpr float kEnemyRadiusWu = 14.0f;  // the standard enemy (look.md §4)
constexpr float kBulletSpeed = 240.0f;   // design/species.json enemyBullet.speed
constexpr float kShardFlightS = 0.6f;
constexpr float kShotSpeed = 1400.0f;     // docs/GAME_DESIGN.md §4, primary weapon
constexpr float kShotLife = 0.8f;
constexpr float kRefPxPerWu = 1.5f;      // at zoom 1

// The endgame's formations fit this box around the ship (world units): the 20:9 view at zoom 1 is
// 1600 x 720, and zoom breathing, the Build's zoom-in and the camera's look-ahead take about 15% off it.
// design/scenes.md §4 counts what is in view, so everything is kept inside (as in the endgame target,
// design/tools/crowd.py, which squashes its rings to the screen the same way).
constexpr float kFitX = 680.0f, kFitY = 300.0f;

Vec2 dir(float a) { return {std::cos(a), std::sin(a)}; }
Vec2 perp(Vec2 v) { return {-v.y, v.x}; }
Vec2 rotate(Vec2 v, float c, float s) { return {v.x * c - v.y * s, v.x * s + v.y * c}; }
float angleOf(Vec2 v) { return std::atan2(v.y, v.x); }
float frac(float x) { return x - std::floor(x); }
float smooth01(float x) {
    x = clampf(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}
// Mirror x into [-half, half] (a ball bouncing between two walls).
float bounce(float x, float half) {
    float u = std::fmod(x + half, 4.0f * half);
    if (u < 0.0f) u += 4.0f * half;
    return u < 2.0f * half ? u - half : 3.0f * half - u;
}
// Deterministic per-(a, b) pseudo-random in [0, 1).
float hash01(u32 a, u32 b) {
    u32 h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
}
Vec3 white() { return {1.0f, 1.0f, 1.0f}; }
// Soft fit of an offset from the ship into the endgame box: unchanged near the ship, squeezed toward the
// edges (per axis), never outside.
Vec2 fitView(Vec2 o) { return {kFitX * std::tanh(o.x / kFitX), kFitY * std::tanh(o.y / kFitY)}; }
// Distance from the ship to the edge of the endgame box's inscribed ellipse along the unit direction d.
float fitRadius(Vec2 d) { return 1.0f / std::sqrt(d.x * d.x / (kFitX * kFitX) + d.y * d.y / (kFitY * kFitY)); }

// Arc-length table of the shoal's figure of eight (x = A sin u, y = B sin 2u).
constexpr float kShoalA = 650.0f, kShoalB = 260.0f;
constexpr u32 kShoalSamples = 512;
float gShoalLength = 0.0f;
float gShoalU[kShoalSamples + 1];  // u at arc length k * L / kShoalSamples
void buildShoalTable() {
    if (gShoalLength > 0.0f) return;
    constexpr u32 fine = 8192;
    float cum[fine + 1];
    cum[0] = 0.0f;
    for (u32 i = 1; i <= fine; ++i) {
        const float u0 = kTau * static_cast<float>(i - 1) / fine, u1 = kTau * static_cast<float>(i) / fine;
        const Vec2 a{kShoalA * std::sin(u0), kShoalB * std::sin(2.0f * u0)};
        const Vec2 b{kShoalA * std::sin(u1), kShoalB * std::sin(2.0f * u1)};
        cum[i] = cum[i - 1] + length(b - a);
    }
    gShoalLength = cum[fine];
    u32 j = 0;
    for (u32 k = 0; k <= kShoalSamples; ++k) {
        const float s = gShoalLength * static_cast<float>(k) / kShoalSamples;
        while (j < fine && cum[j + 1] < s) ++j;
        const float seg = cum[j + 1] - cum[j];
        const float f = seg > 0.0f ? (s - cum[j]) / seg : 0.0f;
        gShoalU[k] = kTau * (static_cast<float>(j) + f) / fine;
    }
}
}  // namespace

struct Stage::Enemy {
    u8 family = 0, tier = 1, hue = 0, flags = 0;
    u16 group = 0, index = 0;
    int glyph = -1;
    float a = 0, b = 0, c = 0, d = 0;  // formation parameters (re-drawn on respawn)
    float warpUntil = -1.0f;           // song time when the warp-in ends
    float radius = kEnemyRadiusWu;     // world units
    Vec2 pos, vel;
    float heading = 0.0f;
    bool inView = false;
};
struct Stage::Bullet {
    Vec2 p0, v;
    float born = -100.0f, life = 0.0f;
};
struct Stage::Shot {
    Vec2 p0, dir;
    float born = -100.0f, speed = 0.0f, intensity = 0.0f, halfWidth = 0.0f;
};
struct Stage::Shard {
    Vec2 p0, v0;
    float born = -100.0f, spin = 0.0f;
};
struct Stage::Ember {
    Vec2 p0, v0;
    float born = -100.0f, life = 0.0f;
    u8 hue = 0;
};
struct Stage::Shatter {
    Vec2 a, b;  // relative to the centre, world units
    Vec2 centre, v;
    float spin = 0.0f, born = -100.0f;
    u8 hue = 0;
};
struct Stage::Pool {
    Vec2 at;
    float r0 = 0, r1 = 0, i0 = 0, i1 = 0, born = -100.0f, life = 0.0f;
    Vec3 colour;
    bool kill = false;
    u8 fade = 0;           // 0 linear i0 -> i1, 1 squared time left (i0 -> nothing)
    bool sparkle = false;  // radius in ref px, full resolution
    bool dimmed = true;    // scaled by the crowd gain
};
struct Stage::DistortRing {
    Vec2 at;
    float born = -100.0f, life = 0.0f, toWu = 0.0f, amountRefPx = 0.0f, widthRefPx = 0.0f;
};

Stage::~Stage() {
    std::free(enemies_);
    std::free(farPos_);
    std::free(farParam_);
    std::free(bullets_);
    std::free(shots_);
    std::free(shards_);
    std::free(embers_);
    std::free(shatter_);
    std::free(pools_);
    std::free(rings_);
    std::free(sparkReqs_);
    std::free(stars_);
}

bool Stage::init(StageKind kind, const Look& look, const Palettes& palettes, const Palette& palette,
                 const GlyphSet& glyphs, const TierSettings& tier) {
    kind_ = kind;
    look_ = &look;
    palettes_ = &palettes;
    palette_ = &palette;
    glyphs_ = &glyphs;
    tier_ = tier;
    rng_.reseed(1);
    buildShoalTable();

    gShip_ = glyphs.find("ship");
    gHalo3_ = glyphs.find("halo_3");
    gShard_ = glyphs.find("shard");
    gBullet_ = glyphs.find("enemy_bullet");
    gMarkBHeading_ = glyphs.find("mark_b_heading");
    gMarkBSpin_ = glyphs.find("mark_b_spin");
    gMarkC_ = glyphs.find("mark_c");
    gRing32_ = glyphs.find("ring32");
    gRing64_ = glyphs.find("ring64");
    gRing128_ = glyphs.find("ring128");
    gCarom3_ = glyphs.find("carom_3");
    if (gShip_ < 0 || gHalo3_ < 0 || gShard_ < 0 || gBullet_ < 0 || gMarkBHeading_ < 0 || gRing32_ < 0 || gRing64_ < 0 ||
        gRing128_ < 0 || gCarom3_ < 0) {
        RF_ERR("stage: a glyph it needs is missing from design/glyphs.json");
        return false;
    }

    enemyCap_ = 1024;
    bulletCap_ = 4096;
    shotCap_ = 1024;
    shardCap_ = 2048;
    emberCap_ = 1024;
    shatterCap_ = 1024;
    poolCap_ = 256;
    ringCap_ = 8;
    sparkReqCap_ = 512;
    enemies_ = static_cast<Enemy*>(std::calloc(enemyCap_, sizeof(Enemy)));
    farPos_ = static_cast<Vec2*>(std::calloc(3072, sizeof(Vec2)));
    farParam_ = static_cast<float*>(std::calloc(3072 * 3, sizeof(float)));
    bullets_ = static_cast<Bullet*>(std::calloc(bulletCap_, sizeof(Bullet)));
    shots_ = static_cast<Shot*>(std::calloc(shotCap_, sizeof(Shot)));
    shards_ = static_cast<Shard*>(std::calloc(shardCap_, sizeof(Shard)));
    embers_ = static_cast<Ember*>(std::calloc(emberCap_, sizeof(Ember)));
    shatter_ = static_cast<Shatter*>(std::calloc(shatterCap_, sizeof(Shatter)));
    pools_ = static_cast<Pool*>(std::calloc(poolCap_, sizeof(Pool)));
    rings_ = static_cast<DistortRing*>(std::calloc(ringCap_, sizeof(DistortRing)));
    sparkReqs_ = static_cast<SparkReq*>(std::calloc(sparkReqCap_, sizeof(SparkReq)));
    stars_ = static_cast<Star*>(std::calloc(512, sizeof(Star)));
    if (!enemies_ || !farPos_ || !farParam_ || !bullets_ || !shots_ || !shards_ || !embers_ || !shatter_ || !pools_ ||
        !rings_ || !sparkReqs_ || !stars_)
        return false;
    for (u32 i = 0; i < bulletCap_; ++i) bullets_[i].born = -100.0f;
    for (u32 i = 0; i < shotCap_; ++i) shots_[i].born = -100.0f;
    for (u32 i = 0; i < shardCap_; ++i) shards_[i].born = -100.0f;
    for (u32 i = 0; i < emberCap_; ++i) embers_[i].born = -100.0f;
    for (u32 i = 0; i < shatterCap_; ++i) shatter_[i].born = -100.0f;
    for (u32 i = 0; i < poolCap_; ++i) pools_[i].born = -100.0f;
    for (u32 i = 0; i < ringCap_; ++i) rings_[i].born = -100.0f;

    if (kind == StageKind::Mid)
        buildMid();
    else
        buildEndgame();

    // Background (look.md §7): clouds on a tile moving at a quarter of the camera's speed, star dust at half.
    cloudTile_ = {3600.0f, 1800.0f};
    cloudCount_ = 8;
    for (u32 i = 0; i < cloudCount_; ++i) {
        Cloud& c = clouds_[i];
        c.p = {rng_.range(0.0f, cloudTile_.x), rng_.range(0.0f, cloudTile_.y)};
        c.radius = rng_.range(look.back.nebulaRadiusWu[0], look.back.nebulaRadiusWu[1]);
        c.intensity = rng_.range(look.back.nebulaIntensity[0], look.back.nebulaIntensity[1]);
        c.colour = static_cast<u8>(rng_.below(5));  // hue 1-3, partner 2, accent
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
    if (!setTier(tier)) return false;
    RF_LOG("stage: %s, %u enemies in formation, %u far, lattice %.0f wu", kind == StageKind::Mid ? "bench_mid" : "bench_endgame",
           enemyCount_, farCount_, tier.gridSpacingWu);
    return true;
}

bool Stage::setTier(const TierSettings& tier) {
    const bool newLattice = tier.gridSpacingWu != lattice_.spacing() || !stats_.latticeSegments;
    tier_ = tier;
    if (newLattice) {
        // The window covers the widest view: zoom 0.75 on a 20:9 screen, plus margins.
        if (!lattice_.init(tier.gridSpacingWu, 1600.0f / 0.75f, 720.0f / 0.75f)) return false;
    }
    return true;
}

u32 Stage::addEnemy(u8 family, u8 tier, u8 hue, u16 group, u16 index) {
    if (enemyCount_ >= enemyCap_) return enemyCap_;
    Enemy& e = enemies_[enemyCount_];
    e = Enemy{};
    e.family = family;
    e.tier = tier;
    e.hue = hue;
    e.group = group;
    e.index = index;
    int g = glyphs_->enemy(kFamilyGlyph[family], tier);
    for (u32 t = 1; g < 0 && t <= 5; ++t) g = glyphs_->enemy(kFamilyGlyph[family], t);
    e.glyph = g >= 0 ? g : gRing32_;
    e.radius = kEnemyRadiusWu * glyphs_->def(static_cast<u32>(e.glyph)).size;
    randomiseSlot(e);
    return enemyCount_++;
}

// A fresh place in its formation (design/scenes.md §2 kills).
void Stage::randomiseSlot(Enemy& e) {
    switch (e.family) {
        case kShoal:
            e.a = rng_.range(-20.0f, 20.0f);
            e.b = rng_.range(-20.0f, 20.0f);
            break;
        case kDrifter: {
            // bench_mid: scattered over +-900 x +-450 (scenes.md §3); the endgame's are scattered over the screen.
            const bool mid = kind_ == StageKind::Mid;
            e.a = mid ? rng_.range(-900.0f, 900.0f) : rng_.range(-(kFitX - 30.0f), kFitX - 30.0f);
            e.b = mid ? rng_.range(-450.0f, 450.0f) : rng_.range(-(kFitY - 30.0f), kFitY - 30.0f);
            e.c = rng_.range(15.0f, 30.0f);  // loop radius (30-60 wu across)
            e.d = rng_.range(5.0f, 9.0f);    // loop period
            break;
        }
        case kHound:
            e.a = rng_.range(0.0f, 360.0f);
            e.b = rng_.range(0.0f, kTau);
            e.c = rng_.range(-6.0f, 6.0f);
            break;
        case kFlanker:
            e.a = rng_.range(-10.0f, 10.0f);
            break;
        case kCarom:
            e.a = rng_.range(-750.0f, 750.0f);
            e.b = rng_.range(-325.0f, 325.0f);
            e.c = rng_.range(0.0f, kTau);
            break;
        case kSentry:
            e.a = rng_.range(0.0f, 1.0f);
            break;
        default:
            e.a = rng_.range(-1.0f, 1.0f);
            break;
    }
    e.d = e.family == kDrifter ? e.d : rng_.range(0.0f, kTau);  // spin phase
}

void Stage::buildMid() {
    // design/scenes.md §3: 500 enemies, moderate effects.
    for (u16 s = 0; s < 2; ++s)
        for (u16 i = 0; i < 110; ++i) addEnemy(kShoal, static_cast<u8>(1 + rng_.below(2)), 0, s, i);
    for (u16 i = 0; i < 110; ++i) {
        const float r = rng_.uniform();
        addEnemy(kDrifter, r < 0.6f ? 1 : (r < 0.9f ? 2 : 3), 0, 0, i);
    }
    for (u16 i = 0; i < 90; ++i) addEnemy(kHound, static_cast<u8>(1 + rng_.below(3)), 1, 0, i);
    for (u16 g = 0; g < 2; ++g)
        for (u16 i = 0; i < 20; ++i) addEnemy(kFlanker, static_cast<u8>(1 + rng_.below(2)), 1, g, i);
    for (u16 p = 0; p < 10; ++p)
        for (u16 i = 0; i < 2; ++i) addEnemy(kRam, static_cast<u8>(2 + rng_.below(2)), 1, p, i);
    for (u16 i = 0; i < 20; ++i) addEnemy(kCarom, static_cast<u8>(1 + rng_.below(3)), 2, 0, i);
    for (u16 i = 0; i < 3; ++i) addEnemy(kSentry, 3, 2, 0, i);
    // Two elites: one Hound, one Carom.
    for (u32 i = 0; i < enemyCount_; ++i) {
        Enemy& e = enemies_[i];
        if ((e.family == kHound && e.index == 7) || (e.family == kCarom && e.index == 3)) e.flags |= kElite;
    }
    farCount_ = 0;
}

void Stage::buildEndgame() {
    // design/scenes.md §4: about 700 enemies in view, the rest of the pool (3,072) outside it.
    for (u16 s = 0; s < 3; ++s)
        for (u16 i = 0; i < 87; ++i) addEnemy(kShoal, static_cast<u8>(2 + rng_.below(3)), 0, s, i);
    for (u16 g = 0; g < 2; ++g)
        for (u16 i = 0; i < 55; ++i) addEnemy(kHound, static_cast<u8>(2 + rng_.below(3)), 1, g, i);
    for (u16 i = 0; i < 90; ++i) addEnemy(kDrifter, static_cast<u8>(2 + rng_.below(4)), 0, 0, i);
    for (u16 c = 0; c < 6; ++c) {
        addEnemy(kWyrmHead, 3, 1, c, 0);
        for (u16 k = 1; k < 12; ++k) addEnemy(kWyrmBody, 3, 1, c, k);
    }
    for (u16 r = 0; r < 4; ++r)
        for (u16 i = 0; i < 9; ++i) addEnemy(kMoth, static_cast<u8>(2 + rng_.below(3)), 1, r, i);
    for (u16 i = 0; i < 30; ++i) addEnemy(kCarom, static_cast<u8>(2 + rng_.below(3)), 2, 0, i);
    for (u16 p = 0; p < 12; ++p)
        for (u16 i = 0; i < 2; ++i) addEnemy(kTether, 3, 2, p, i);
    for (u16 i = 0; i < 16; ++i) addEnemy(kBlink, static_cast<u8>(3 + rng_.below(2)), 2, 0, i);
    for (u16 i = 0; i < 12; ++i) addEnemy(kWeaver, 3, 2, 0, i);
    for (u16 i = 0; i < 10; ++i) {
        const u32 k = addEnemy(kSentry, static_cast<u8>(3 + rng_.below(2)), 2, 0, i);
        if (k < enemyCap_) enemies_[k].flags |= kMarkB;
    }
    // The boss stand-in is built from carom_3, so it wears the Caroms' colour: hue 3 (index 2).
    for (u16 a = 0; a < 8; ++a)
        for (u16 k = 0; k < 6; ++k) {
            const u32 i = addEnemy(kBoss, 3, 2, a, k);
            if (i < enemyCap_) {
                enemies_[i].glyph = gCarom3_;
                enemies_[i].flags |= kBossPart;
            }
        }
    // Twenty elites.
    u32 elites = 0;
    const u8 eliteFamilies[] = {kHound, kHound, kHound, kHound, kDrifter, kDrifter, kDrifter, kDrifter, kMoth, kMoth,
                                kMoth,  kCarom, kCarom, kCarom, kBlink,   kBlink,   kSentry,  kSentry,  kWyrmHead, kWyrmHead};
    for (u8 f : eliteFamilies) {
        for (u32 i = 0; i < enemyCount_; ++i) {
            Enemy& e = enemies_[i];
            if (e.family == f && !(e.flags & kElite) && hash01(i, 77) < 0.5f) {
                e.flags |= kElite;
                ++elites;
                break;
            }
        }
    }
    // The rest of the pool lives outside the view: far-mode updates and culling only.
    farCount_ = 3072u - enemyCount_;
    for (u32 i = 0; i < farCount_; ++i) {
        farParam_[i * 3 + 0] = rng_.range(1500.0f, 4200.0f);
        farParam_[i * 3 + 1] = rng_.range(0.0f, kTau);
        farParam_[i * 3 + 2] = rng_.range(-0.05f, 0.05f);
    }
    (void)elites;
}

Vec2 Stage::shipAt(float t) const {
    return {1400.0f * std::sin(kTau * t / 41.0f), 700.0f * std::sin(2.0f * kTau * t / 41.0f)};
}

Vec2 Stage::shipVelocity(float t) const {
    return {1400.0f * kTau / 41.0f * std::cos(kTau * t / 41.0f),
            700.0f * 2.0f * kTau / 41.0f * std::cos(2.0f * kTau * t / 41.0f)};
}

Vec2 Stage::bossOffset(float t) const {
    // 600 wu from the ship across the view (scenes.md §4), circling slowly; its vertical swing is squashed
    // to 0.3 so the 420 wu wheel stays on the 20:9 screen.
    const Vec2 d = dir(-0.6f + 0.05f * t);
    return {600.0f * d.x, 180.0f * d.y};
}

Vec2 Stage::shoalCurve(u32 stream, float s, float t, Vec2* tangent) const {
    const float L = gShoalLength;
    float w = std::fmod(s, L);
    if (w < 0.0f) w += L;
    const float k = w / L * kShoalSamples;
    const u32 i = static_cast<u32>(k) < kShoalSamples ? static_cast<u32>(k) : kShoalSamples - 1;
    const float u = gShoalU[i] + (gShoalU[i + 1] - gShoalU[i]) * (k - static_cast<float>(i));
    const u32 streams = kind_ == StageKind::Mid ? 2u : 3u;
    const float turn = (stream & 1u ? -1.0f : 1.0f) * kTau * t / 60.0f + kTau * static_cast<float>(stream) / streams;
    const float c = std::cos(turn), sn = std::sin(turn);
    if (tangent) *tangent = rotate({kShoalA * std::cos(u), 2.0f * kShoalB * std::cos(2.0f * u)}, c, sn);
    return rotate({kShoalA * std::sin(u), kShoalB * std::sin(2.0f * u)}, c, sn);
}

float Stage::spearBlend(float t) const {
    // Endgame shoal: snap into a spear on the first beat of each bar, then scatter back over two beats.
    const float phase = std::fmod(t, barLen());
    const float b = beatLen();
    if (phase < 0.12f) return smooth01(phase / 0.12f);
    if (phase < b) return 1.0f;
    return 1.0f - smooth01((phase - b) / (2.0f * b));
}

void Stage::place(Enemy& e, float t, Vec2 S) const {
    const float bar = barLen(), beat = beatLen();
    switch (e.family) {
        case kShoal: {
            Vec2 tan;
            const u32 streams = kind_ == StageKind::Mid ? 2u : 3u;
            const float s = 190.0f * t - 18.0f * static_cast<float>(e.index) + gShoalLength * e.group / streams;
            const Vec2 p = shoalCurve(e.group, s, t, &tan);
            const float tl = length(tan);
            const Vec2 tu = tl > 1e-3f ? tan * (1.0f / tl) : Vec2{1.0f, 0.0f};
            Vec2 pos = S + p + tu * e.a + perp(tu) * e.b;
            float heading = angleOf(tu);
            if (kind_ == StageKind::Endgame) {
                pos = S + fitView(pos - S);  // the turning figure of eight, kept on screen
                const float sigma = spearBlend(t);
                if (sigma > 0.0f) {
                    // A straight spear aimed at the ship, from a third of the way to the screen's edge out to it.
                    const float barIndex = std::floor(t / bar);
                    const Vec2 d = dir(kTau * e.group / 3.0f + barIndex * 0.7f);
                    const float reach = fitRadius(d);
                    const float along = reach * (0.3f + 0.7f * static_cast<float>(e.index) / 86.0f);
                    const Vec2 spear = S + d * along + perp(d) * (e.b * 0.3f);
                    pos = pos + (spear - pos) * sigma;
                    if (sigma > 0.5f) heading = angleOf(d * -1.0f);
                }
            }
            e.pos = pos;
            e.heading = heading;
            break;
        }
        case kDrifter: {
            const float a = kTau * t / e.d + e.a * 0.01f;
            e.pos = S + Vec2{e.a, e.b} + dir(a) * e.c;
            break;
        }
        case kHound: {
            if (kind_ == StageKind::Mid) {
                const float r = 620.0f - std::fmod(60.0f * t + e.a, 360.0f);
                e.pos = S + dir(e.b + 0.22f * t) * r;
            } else {
                // Two packs charging through the ship's position in waves, one wave per bar, from one edge of
                // the screen to the other (the pack's seven rows trail its centre by up to 132 wu).
                const float k = std::floor(t / bar);
                const float u = std::fmod(t, bar) / bar;
                const Vec2 d = dir(e.group * kPi * 0.5f + k * 0.65f);
                const float reach = fitRadius(d);
                const Vec2 centre = S + d * (-(reach - 140.0f) + (2.0f * reach - 140.0f) * u);
                const float row = static_cast<float>(e.index / 8), col = static_cast<float>(e.index % 8) - 3.5f;
                e.pos = centre - d * (row * 22.0f) + perp(d) * (col * 20.0f + e.c);
            }
            break;
        }
        case kFlanker: {
            const float sgn = e.group ? -1.0f : 1.0f;
            const float a = sgn * 0.5f * t + e.group * kPi + static_cast<float>(e.index) * 0.075f;
            e.pos = S + dir(a) * (420.0f + e.a);
            break;
        }
        case kRam: {
            // Pairs crossing the view in straight dashes: one beat of telegraph, then 0.6 s at 900 wu/s.
            const float cycle = 2.0f * bar;
            const float tt = t + static_cast<float>(e.group) * 0.35f;
            const u32 n = static_cast<u32>(std::floor(tt / cycle));
            const float tau = std::fmod(tt, cycle);
            const Vec2 start{(hash01(e.group, n) - 0.5f) * 1100.0f, (hash01(e.group + 50u, n) - 0.5f) * 520.0f};
            const Vec2 d = dir(hash01(e.group + 100u, n) * kTau);
            const Vec2 side = perp(d) * (e.index ? 20.0f : -20.0f);
            const float run = clampf((tau - beat) * 900.0f, 0.0f, 540.0f);
            e.pos = S + start + side + d * run;
            e.c = tau < beat ? 1.0f : 0.0f;  // telegraphing
            e.b = angleOf(d);
            break;
        }
        case kCarom: {
            // Bouncing in a 1500 x 650 box (scenes.md §3); the endgame's box is the screen.
            const Vec2 v = dir(e.c) * 260.0f;
            const bool mid = kind_ == StageKind::Mid;
            e.pos = S + Vec2{bounce(e.a + v.x * t, mid ? 750.0f : kFitX - 20.0f), bounce(e.b + v.y * t, mid ? 325.0f : kFitY - 20.0f)};
            break;
        }
        case kSentry: {
            if (kind_ == StageKind::Mid) {
                e.pos = S + dir(kTau * e.index / 3.0f + 0.1f * t) * 520.0f;
            } else {
                // A ring 480-560 wu out, squashed to the screen (0.55 high).
                const Vec2 d = dir(kTau * e.index / 10.0f + 0.05f * t);
                const float r = 480.0f + 80.0f * frac(e.index * 0.618f);
                e.pos = S + Vec2{d.x * r, d.y * r * 0.55f};
            }
            break;
        }
        case kWyrmHead:
        case kWyrmBody: {
            // Six chains on sine paths back and forth through the view.
            const float tau = t - static_cast<float>(e.index) * 0.11f;
            const float X = bounce(200.0f * tau + e.group * 300.0f, 700.0f);
            const float Y = 100.0f * std::sin(1.9f * tau + e.group) + (static_cast<float>(e.group) - 2.5f) * 70.0f;
            const float turn = 0.12f * (static_cast<float>(e.group) - 2.5f);
            e.pos = S + fitView(rotate({X, Y}, std::cos(turn), std::sin(turn)));
            break;
        }
        case kMoth: {
            // Four rings of nine spiralling around the ship between 300 and 360 wu (0.82 high on the 20:9 screen).
            const float r = 300.0f + 60.0f * (0.5f + 0.5f * std::sin(0.7f * t + e.group * 1.3f));
            const float a = kTau * e.index / 9.0f + (e.group & 1u ? -0.45f : 0.45f) * t + e.group * 0.4f;
            const Vec2 d = dir(a);
            e.pos = S + Vec2{d.x * r, d.y * r * 0.82f};
            break;
        }
        case kTether: {
            // Twelve pairs 160 wu apart, slowly rotating, around the ship on a screen-shaped ring.
            const Vec2 d = dir(kTau * e.group / 12.0f + 0.04f * t);
            const Vec2 centre = S + Vec2{d.x * 500.0f, d.y * 200.0f};
            e.pos = centre + dir(e.group + 0.35f * t) * (e.index ? 80.0f : -80.0f);
            break;
        }
        case kBlink: {
            // Jumping on the beat along a five-point star 400 wu across.
            const Vec2 c = S + Vec2{-330.0f, 120.0f};
            const float q = static_cast<float>(e.index) * 5.0f / 16.0f + std::floor(t / beat);
            const u32 k = static_cast<u32>(std::floor(q)) % 5u;
            const float f = frac(q);
            const Vec2 a = c + dir(-kPi * 0.5f + static_cast<float>(k) * 4.0f * kPi / 5.0f) * 200.0f;
            const Vec2 b = c + dir(-kPi * 0.5f + static_cast<float>(k + 1) * 4.0f * kPi / 5.0f) * 200.0f;
            e.pos = a + (b - a) * f;
            break;
        }
        case kWeaver: {
            // Curving paths around the ship, on a screen-shaped ring.
            const float sgn = e.index & 1u ? -1.0f : 1.0f;
            const float r = 520.0f + 100.0f * std::sin(0.9f * t + e.index);
            const Vec2 d = dir(kTau * e.index / 12.0f + sgn * 0.35f * t);
            e.pos = S + Vec2{d.x * r, d.y * r * 0.45f};
            break;
        }
        case kBoss: {
            const Vec2 B = S + bossOffset(t);
            const float arm = e.group * kPi * 0.25f + kTau * t / (4.0f * bar);
            e.pos = B + dir(arm) * (50.0f + 30.0f * static_cast<float>(e.index));
            e.b = arm;
            break;
        }
        default:
            e.pos = S;
            break;
    }
}

float Stage::beatAmount() const { return reduceFlashing_ ? look_->reduceFlashingScale : 1.0f; }

float Stage::beatLen() const {
    return beat_ && beat_->running ? static_cast<float>(60.0 / beat_->bpm) : 60.0f / 132.0f;
}

void Stage::followBand() {
    const BeatView& v = *beat_;
    if (!v.running) return;  // the song clock waits for the band's
    if (!beatAnchored_) {
        // The stage's song starts on the band's next bar line (at once when the band's song has not begun).
        const double bar = static_cast<double>(audio::kTicksPerBar);
        beatTick0_ = std::ceil((v.heardTick > 0.0 ? v.heardTick : 0.0) / bar) * bar;
        if (!songStarted_) {  // the song clock: minus the time until that bar line
            songClock_ = -(beatTick0_ - v.heardTick) / static_cast<double>(audio::kPpq) * 60.0 / v.bpm;
            songStarted_ = true;
        }
        lastHeardSeconds_ = v.songSeconds;
        beatAnchored_ = true;
        if (beatLogs_ < 8) {
            ++beatLogs_;
            RF_LOG("stage: on the band's clock from bar %.0f (heard tick %.1f, %.1f BPM)", beatTick0_ / bar, v.heardTick, v.bpm);
        }
    }
    // The heard time's progress, at most 0.1 s a frame and never backwards: a heard clock that jumps
    // (resynchronised, a new stream) moves the stage's song by 0.1 s at most.
    double d = v.songSeconds - lastHeardSeconds_;
    lastHeardSeconds_ = v.songSeconds;
    d = d < 0.0 ? 0.0 : (d > 0.1 ? 0.1 : d);
    songClock_ += d;
    const float t = static_cast<float>(songClock_);
    if (t > songT_) songT_ = t;
}

float Stage::stepTime(u32 step) const {
    if (heard()) {
        // Now, less the ticks heard since the step, at the present tempo.
        const double ago = beat_->heardTick - (beatTick0_ + static_cast<double>(audio::kTicksPerStep) * step);
        return songT_ - static_cast<float>(ago / static_cast<double>(audio::kPpq) * 60.0 / beat_->bpm);
    }
    return static_cast<float>(step) * beatLen() * 0.25f;
}

int Stage::beatIndex() const {
    if (heard()) return static_cast<int>(std::floor((beat_->heardTick - beatTick0_) / static_cast<double>(audio::kPpq)));
    return static_cast<int>(std::floor(songT_ / beatLen()));
}

void Stage::envelopes() {
    if (beat_) {
        // The heard events drive the pumps (look.md §7): what the player hears, when it is heard.
        using audio::BeatKind;
        const BeatView& v = *beat_;
        kick_ = v.envelope(BeatKind::Kick, look_->kickDecayMs * 1e-3f);
        snare_ = std::fmax(v.envelope(BeatKind::Clap, look_->snareDecayMs * 1e-3f),
                           v.envelope(BeatKind::Snare, look_->snareDecayMs * 1e-3f));
        hat_ = v.envelope(BeatKind::Hat, look_->hatDecayMs * 1e-3f);
        bass_ = v.envelope(BeatKind::Bass, 0.18f);
        const double kick = v.lastFrame[static_cast<u32>(BeatKind::Kick)];
        if (kick != kickFrame_) {
            prevKickFrame_ = kickFrame_;
            kickFrame_ = kick;
        }
        // Song time of a heard frame (the lattice ripples, the Pulse Ring, the ornaments' steps).
        const auto at = [&](double frame) {
            return frame < -1e17 ? -10.0f : songT_ - static_cast<float>((v.heardFrame - frame) / v.rate);
        };
        lastKickT_ = at(kickFrame_);
        prevKickT_ = at(prevKickFrame_);
        lastHatT_ = at(v.lastFrame[static_cast<u32>(BeatKind::Hat)]);
        lastBeatT_ = at(v.lastFrame[static_cast<u32>(BeatKind::Beat)]);  // M5: the clock's beat (no kick on every beat)
        return;
    }
    const float beat = beatLen(), t = songT_;
    const float b = std::floor(t / beat);
    lastKickT_ = b * beat;
    prevKickT_ = (b - 1.0f) * beat;
    lastBeatT_ = lastKickT_;
    kick_ = std::exp(-(t - lastKickT_) * 1000.0f / look_->kickDecayMs);
    float snareT = -10.0f;
    for (int k = 0; k < 4; ++k) {
        const int beatIndex = static_cast<int>(b) - k;
        if (beatIndex >= 0 && (beatIndex % 4 == 1 || beatIndex % 4 == 3)) {
            snareT = static_cast<float>(beatIndex) * beat;
            break;
        }
    }
    snare_ = std::exp(-(t - snareT) * 1000.0f / look_->snareDecayMs);
    const float hb = std::floor(t / beat - 0.5f);
    lastHatT_ = hb >= 0.0f ? (hb + 0.5f) * beat : -10.0f;
    hat_ = std::exp(-(t - lastHatT_) * 1000.0f / look_->hatDecayMs);
    bass_ = std::exp(-(t - lastHatT_) / 0.18f);  // the bass bus: off-beat eighths, a longer tail
}

void Stage::advance(float dt) {
    lastDt_ = dt;
    if (beat_) {
        followBand();
    } else {
        songT_ += dt;
    }
    if (songT_ >= hitStopUntil_) simT_ += dt;  // hit-stop freezes the action, not the song
    envelopes();
    if (heard()) {
        // Sixteenths on the band's grid, as they are heard, at most four a frame. The heard clock can
        // jump (resynchronised, a new stream): more than a bar back, the stage carries on from where it
        // is heard (from the next bar line when that is before the stage's song began); more than a bar
        // ahead for half a second (the app was stopped), the missed sixteenths are skipped.
        const double step = static_cast<double>(audio::kTicksPerStep), bar = static_cast<double>(audio::kTicksPerBar);
        const double tick = beat_->heardTick, next = beatTick0_ + static_cast<double>(nextSixteenth_) * step;
        beatBehindFrames_ = tick > next + bar ? beatBehindFrames_ + 1 : 0;
        if (tick < next - step - bar) {
            if (beatLogs_ < 8) {
                ++beatLogs_;
                RF_LOG("stage: the heard clock jumped back to tick %.1f (next sixteenth at %.1f)", tick, next);
            }
            if (tick >= beatTick0_) {
                nextSixteenth_ = static_cast<u32>((tick - beatTick0_) / step) + 1;
            } else {
                beatAnchored_ = false;  // anchored again on the next frame
                nextSixteenth_ = 0;
            }
        } else if (beatBehindFrames_ >= 30) {
            if (beatLogs_ < 8) {
                ++beatLogs_;
                RF_LOG("stage: sixteenths from tick %.1f to %.1f skipped", next, tick);
            }
            nextSixteenth_ = static_cast<u32>((tick - beatTick0_) / step);
            beatBehindFrames_ = 0;
        }
        for (u32 n = 0; n < 4 && heard() && beatTick0_ + static_cast<double>(nextSixteenth_) * step <= tick; ++n)
            onSixteenth(nextSixteenth_++);
    } else if (!beat_) {
        const float sixteenth = beatLen() * 0.25f;
        while (static_cast<float>(nextSixteenth_) * sixteenth <= songT_) onSixteenth(nextSixteenth_++);
    }

    // Build bar before each Drop (endgame): drain, vignette, stiffer lattice, slow zoom-in.
    buildAmount_ = 0.0f;
    if (kind_ == StageKind::Endgame && heard()) {
        const double bars = (beat_->heardTick - beatTick0_) / static_cast<double>(audio::kTicksPerBar);
        const double barIndex = std::floor(bars);
        if (bars >= 0.0 && static_cast<int>(barIndex) % 8 == 7) buildAmount_ = smooth01(static_cast<float>(bars - barIndex));
    } else if (kind_ == StageKind::Endgame && !beat_) {
        const float bar = barLen();
        const float barIndex = std::floor(songT_ / bar);
        if (static_cast<int>(barIndex) % 8 == 7) buildAmount_ = smooth01((songT_ - barIndex * bar) / bar);
    }
    if (kind_ == StageKind::Endgame) {
        // Loose spirals: a steady stream from the boss.
        static constexpr float kSpiralRate = 50.0f;
        const float prev = simT_ - dt;
        const int n0 = static_cast<int>(std::floor(prev * kSpiralRate)), n1 = static_cast<int>(std::floor(simT_ * kSpiralRate));
        const Vec2 B = shipAt(simT_) + bossOffset(simT_);
        for (int n = n0 + 1; n <= n1 && songT_ >= hitStopUntil_; ++n) {
            const float a = 2.4f * static_cast<float>(n) / kSpiralRate * 6.0f;
            spawnBullet(B, dir(a) * 200.0f, 3.0f);
        }
    }

    // Formations and the far pool.
    const Vec2 S = shipAt(simT_);
    const Vec2 Sp = shipAt(simT_ - 1.0f / 30.0f);
    for (u32 i = 0; i < enemyCount_; ++i) {
        Enemy& e = enemies_[i];
        place(e, simT_ - 1.0f / 30.0f, Sp);
        const Vec2 prev = e.pos;
        place(e, simT_, S);
        e.vel = (e.pos - prev) * 30.0f;
        const float sp = length(e.vel);
        if (sp > 1.0f) e.heading = angleOf(e.vel);
        if ((e.flags & kWarping) && songT_ >= e.warpUntil) {
            // Solid on the beat, with the small white flash (look.md §5 warp-in).
            e.flags &= static_cast<u8>(~kWarping);
            const LookSoftLight& f = look_->soft.killFlashSmall;
            addPool(e.pos, f.radius[0], f.radius[0], white(), f.intensity[0], f.intensity[1], f.lifeS, false,
                    f.squaredFade ? 1 : 0, true);
        }
    }
    for (u32 i = 0; i < farCount_; ++i) {
        float* p = farParam_ + i * 3;
        p[1] += p[2] * dt;
        farPos_[i] = S + dir(p[1]) * p[0];
    }
    lattice_.step(simT_ >= hitStopUntil_ ? dt : 0.0f);
    shakeAmp_ *= std::exp(-dt / 0.15f);
}

void Stage::onSixteenth(u32 step) {
    const float beat = beatLen();
    const u32 inBeat = step % 4u, beatIndex = step / 4u, inBar = beatIndex % 4u, bar = step / 16u;
    const float when = stepTime(step);
    const bool endgame = kind_ == StageKind::Endgame;
    const float tSim = simT_;
    const Vec2 S = shipAt(tSim);
    const Vec2 v = shipVelocity(tSim);
    const float heading = angleOf(v);
    const float aim = heading + 35.0f * kPi / 180.0f * std::sin(kTau * tSim / 7.0f);

    // Events first (endgame): the Drop every 8 bars, the boss phase break every 16.
    if (endgame && inBeat == 0 && inBar == 0) {
        if (bar > 0 && bar % 8 == 0) dropBurst();
        if (bar % 16 == 12) bossBreak();
    }

    // Kills on the sixteenth grid.
    const u32 small = endgame ? 10u : 2u;
    const u32 mediumByStep[4] = {2, 2, 1, 1};
    const u32 medium = endgame ? mediumByStep[inBeat] : (inBeat == 0 ? 1u : 0u);
    const bool large = endgame ? (inBeat == 0 && (inBar % 2u) == 0) : (inBeat == 0 && inBar == 0 && (bar % 2u) == 0);
    const auto pick = [&](u32 size) {
        for (u32 attempt = 0; attempt < 48; ++attempt) {
            Enemy& e = enemies_[rng_.below(enemyCount_)];
            if (!e.inView || (e.flags & (kWarping | kBossPart))) continue;
            const bool fits = size == 0 ? e.tier <= 2 : (size == 1 ? (e.tier >= 3 && e.tier <= 4) : (e.tier >= 5 || (e.flags & kElite)));
            if (fits || attempt >= 24) {
                kill(e, size);
                return;
            }
        }
    };
    for (u32 k = 0; k < small; ++k) pick(0);
    for (u32 k = 0; k < medium; ++k) pick(1);
    if (large) pick(2);

    // The player (design/scenes.md §3, §4).
    const Vec2 wing = S + perp(dir(heading)) * 60.0f;
    const auto volley = [&](float delay) {
        const u32 lanes = endgame ? 5u : 3u;
        for (u32 l = 0; l < lanes; ++l) {
            const float spread = (static_cast<float>(l) - 0.5f * static_cast<float>(lanes - 1)) * (endgame ? 0.07f : 0.0f);
            const Vec2 d = dir(aim + spread);
            const Vec2 lane = perp(d) * ((static_cast<float>(l) - 0.5f * static_cast<float>(lanes - 1)) * 12.0f);
            const u32 at = shotHead_;
            spawnShot(S + lane, d, kShotSpeed, look_->lines.playerShot.intensity, 0.5f * look_->lines.playerShot.width);
            shots_[at].born = simT_ + (when - songT_) + delay;
            if (endgame) {  // the mirrored wingman
                const Vec2 dm = dir(aim - spread);
                const u32 at2 = shotHead_;
                spawnShot(wing + lane, dm, kShotSpeed, look_->lines.playerShot.intensity, 0.5f * look_->lines.playerShot.width);
                shots_[at2].born = simT_ + (when - songT_) + delay;
            }
        }
    };
    volley(0.0f);
    if (endgame) volley(beat * 0.125f);  // thirty-second notes
    // Shot impacts: a few sparks back along the shots (no kill).
    for (u32 k = 0; k < (endgame ? 3u : 1u); ++k) {
        Enemy& e = enemies_[rng_.below(enemyCount_)];
        if (!e.inView || (e.flags & kWarping) || sparkReqCount_ >= sparkReqCap_) continue;
        SparkReq& r = sparkReqs_[sparkReqCount_++];
        r = {e.pos, static_cast<u16>(3 + rng_.below(2)), e.hue, 3, dir(aim) * -1.0f};
    }
    // Sentinel: a shot on each off-beat (endgame: bursts of three).
    if (inBeat == 2) {
        const Vec2 drone = S + dir(tSim * 1.7f) * 42.0f;
        const u32 burst = endgame ? 3u : 1u;
        for (u32 k = 0; k < burst; ++k) {
            const u32 at = shotHead_;
            spawnShot(drone, dir(aim + 0.05f * static_cast<float>(k)), kShotSpeed * 0.8f, look_->lines.sentinelShot.intensity,
                      0.5f * look_->lines.sentinelShot.width);
            shots_[at].born = simT_ + (when - songT_) + 0.04f * static_cast<float>(k);
        }
    }

    // Enemy bullets (endgame; bench_mid's are the Sentry arms only).
    if (endgame && (step % 2u) == 0) {
        const float bar4 = 4.0f * barLen();
        const Vec2 B = S + bossOffset(tSim);
        for (u32 a = 0; a < 8; ++a) {  // boss fans of seven from each arm
            const float arm = static_cast<float>(a) * kPi * 0.25f + kTau * tSim / bar4;
            const Vec2 tip = B + dir(arm) * 200.0f;
            for (int k = -3; k <= 3; ++k) spawnBullet(tip, dir(arm + static_cast<float>(k) * 0.09f) * kBulletSpeed, 2.0f);
        }
        const u32 ring = (step / 2u) % 4u;  // moth petals: one ring of nine fires
        for (u32 i = 0; i < enemyCount_; ++i) {
            const Enemy& e = enemies_[i];
            if (e.family != kMoth || e.group != ring || (e.flags & kWarping)) continue;
            const Vec2 out = e.pos - S;
            const float l = length(out);
            spawnBullet(e.pos, (l > 1e-3f ? out * (1.0f / l) : Vec2{1.0f, 0.0f}) * kBulletSpeed, 4.5f);
        }
    }
    if (endgame && inBeat == 0 && inBar == 0) {  // Hound bursts of three on beat 1
        u32 fired = 0;
        for (u32 i = 0; i < enemyCount_ && fired < 20; ++i) {
            const Enemy& e = enemies_[i];
            if (e.family != kHound || !e.inView || (e.flags & kWarping)) continue;
            const float a = angleOf(S - e.pos);
            for (int k = -1; k <= 1; ++k) spawnBullet(e.pos, dir(a + static_cast<float>(k) * 0.1f) * kBulletSpeed, 3.6f);
            ++fired;
        }
    }
}

void Stage::spawnBullet(Vec2 at, Vec2 velocity, float life) {
    Bullet& b = bullets_[bulletHead_];
    bulletHead_ = (bulletHead_ + 1) % bulletCap_;
    b.p0 = at;
    b.v = velocity;
    b.born = simT_;
    b.life = life;
}

void Stage::spawnShot(Vec2 from, Vec2 d, float speed, float intensity, float width) {
    Shot& s = shots_[shotHead_];
    shotHead_ = (shotHead_ + 1) % shotCap_;
    s.p0 = from;
    s.dir = d;
    s.born = simT_;
    s.speed = speed;
    s.intensity = intensity;
    s.halfWidth = width;
}

void Stage::addShockRings(Vec2 at, u32 count, float toWu, u8 hue) {
    // At most maxShockRings alive: a newer ring replaces the oldest (look.md v2.1 §6).
    const u32 cap = look_->boom.maxShockRings < kMaxShockRings ? look_->boom.maxShockRings : kMaxShockRings;
    const LookExplosion& x = look_->boom.medium;
    for (u32 k = 0; k < count; ++k) {
        u32 slot = 0;
        float oldest = 1e9f;
        for (u32 i = 0; i < cap; ++i) {
            if (shock_[i].start < oldest) {
                oldest = shock_[i].start;
                slot = i;
            }
        }
        shock_[slot] = {at, songT_ + x.ringGapS * static_cast<float>(k), toWu, hue};
    }
}

void Stage::addPool(Vec2 at, float r0, float r1, Vec3 colour, float i0, float i1, float life, bool kill, u8 fade,
                    bool sparkle, bool dimmed) {
    if (kill) {
        // At most `killPools` kill pools: the oldest fades out early (look.md §3).
        u32 alive = 0, oldest = poolCap_;
        float oldestBorn = 1e9f;
        for (u32 i = 0; i < poolCap_; ++i) {
            const Pool& p = pools_[i];
            if (!p.kill || songT_ - p.born >= p.life) continue;
            ++alive;
            if (p.born < oldestBorn) {
                oldestBorn = p.born;
                oldest = i;
            }
        }
        if (alive >= tier_.killPools && oldest < poolCap_) {
            Pool& p = pools_[oldest];
            const float remaining = songT_ - p.born + 0.1f;
            if (remaining < p.life) p.life = remaining;
        }
    }
    Pool& p = pools_[poolHead_];
    poolHead_ = (poolHead_ + 1) % poolCap_;
    p = {at, r0, r1, i0, i1, songT_, life, colour, kill, fade, sparkle, dimmed};
}

void Stage::addDistortion(Vec2 at, float toWu, float lifeS, float amountRefPx, float widthRefPx) {
    DistortRing& r = rings_[ringHead_];
    ringHead_ = (ringHead_ + 1) % ringCap_;
    r = {at, songT_, lifeS, toWu, amountRefPx, widthRefPx};
}

void Stage::kill(Enemy& e, u32 size) {
    const Look::Explosions& boom = look_->boom;
    const LookExplosion& x = size == 0 ? boom.small : (size == 1 ? boom.medium : boom.large);
    const Vec3 hue = palette_->hue[e.hue];
    const float density = tier_.particleDensity * governorDensity_;
    const u32 n = static_cast<u32>(rng_.range(x.sparks[0], x.sparks[1]) * density + 0.5f);
    const u8 kind = size == 2 ? 1u : (size == 1 ? 5u : 0u);
    if (sparkReqCount_ < sparkReqCap_) sparkReqs_[sparkReqCount_++] = {e.pos, static_cast<u16>(n), e.hue, kind, {}};
    // Kill flash (look.md v2.1 §3): a white sparkle for small kills, the white pool for medium and large.
    if (size == 0) {
        const LookSoftLight& f = look_->soft.killFlashSmall;
        addPool(e.pos, f.radius[0], f.radius[0], white(), f.intensity[0], f.intensity[1], f.lifeS, false, f.squaredFade ? 1 : 0,
                true);
    } else {
        const LookSoftLight& f = look_->soft.killFlash;
        const float r = f.radius[0] * (size == 2 ? f.largeScale : 1.0f);
        addPool(e.pos, r, r, white(), f.intensity[0], f.intensity[1], f.lifeS, false, f.squaredFade ? 1 : 0, false);
    }
    if (size >= 1) {
        // look.md gives the pool's radius and intensity over its life, not the life: it lasts as long as the
        // recipe's longest spark (medium 0.9 s, large 1.2 s), as in the endgame target (design/tools/crowd.py).
        const LookSoftLight& pl = size == 1 ? look_->soft.killPoolMedium : look_->soft.killPoolLarge;
        addPool(e.pos, pl.radius[0], pl.radius[1], hue, pl.intensity[0], pl.intensity[1], x.lifeS[1], true);
        for (u32 k = 0; k < x.embers; ++k) {
            Ember& m = embers_[emberHead_];
            emberHead_ = (emberHead_ + 1) % emberCap_;
            m = {e.pos, dir(rng_.range(0.0f, kTau)) * rng_.range(20.0f, 70.0f), songT_, x.emberLifeS, e.hue};
        }
    }
    if (x.rings) addShockRings(e.pos, x.rings, x.ringToWu, e.hue);
    // Lattice impulses scale with the crowd gain (look.md v2.1 §3); the Build stiffens the lattice.
    lattice_.impulse(e.pos, x.gridImpulse * crowdGain_ * (buildAmount_ > 0.0f ? 0.5f : 1.0f),
                     size == 0 ? 60.0f : (size == 1 ? 90.0f : 130.0f));
    if (x.shakeRefPx > shakeAmp_) shakeAmp_ = x.shakeRefPx;
    if (size == 2) {
        // Shatter: the enemy's own hull segments fly apart as tumbling capsules.
        const GlyphDef& g = glyphs_->def(static_cast<u32>(e.glyph));
        const GlyphSegment* segs = glyphs_->segments() + g.firstSegment;
        const float c = std::cos(e.heading), s = std::sin(e.heading);
        for (u32 k = 0; k < g.hullCount; ++k) {
            Shatter& p = shatter_[shatterHead_];
            shatterHead_ = (shatterHead_ + 1) % shatterCap_;
            const Vec2 a = rotate(segs[k].a, c, s) * e.radius, b = rotate(segs[k].b, c, s) * e.radius;
            const Vec2 mid = (a + b) * 0.5f;
            const float ml = length(mid);
            p = {a - mid, b - mid, e.pos + mid, (ml > 1e-3f ? mid * (1.0f / ml) : dir(rng_.range(0.0f, kTau))) * rng_.range(80.0f, 200.0f),
                 rng_.range(-9.0f, 9.0f), songT_, e.hue};
        }
        const LookDistortionSource& ds = look_->distortion.largeKill;
        addDistortion(e.pos, 200.0f, ds.lifeS, ds.refPx, ds.widthRefPx);
    }
    // Shards fly to the ship.
    const u32 shards = size == 0 ? 8u : (size == 1 ? 20u : 40u);
    for (u32 k = 0; k < shards; ++k) {
        Shard& sh = shards_[shardHead_];
        shardHead_ = (shardHead_ + 1) % shardCap_;
        sh = {e.pos, dir(rng_.range(0.0f, kTau)) * rng_.range(60.0f, 160.0f), simT_, rng_.range(0.0f, kTau)};
    }
    // Replaced at once: a warp-in ring at a fresh place in its formation for exactly two beats.
    randomiseSlot(e);
    e.flags |= kWarping;
    e.warpUntil = songT_ + look_->boom.warpBeats * beatLen();
    ++stats_.kills;
}

void Stage::dropBurst() {
    dropT_ = songT_;
    accentSwapped_ = !accentSwapped_;
    ++stats_.drops;
    const LookDistortionSource& n = look_->distortion.nova;
    addDistortion(shipAt(simT_), n.toWu > 0.0f ? n.toWu : 1400.0f, n.lifeS, n.refPx, n.widthRefPx);
    const u32 confetti = static_cast<u32>(2000.0f * tier_.particleDensity * governorDensity_);
    for (u32 k = 0; k < confetti && sparkReqCount_ < sparkReqCap_;) {
        const u16 batch = static_cast<u16>(confetti - k < 200 ? confetti - k : 200);
        sparkReqs_[sparkReqCount_++] = {{}, batch, static_cast<u8>(rng_.below(3)), 4, {}};
        k += batch;
    }
    for (int i = 0; i < 2; ++i) lattice_.impulse(shipAt(simT_), 420.0f, 260.0f);
}

void Stage::bossBreak() {
    bossT_ = songT_;
    ++stats_.bossBreaks;
    const Vec2 B = shipAt(simT_) + bossOffset(simT_);
    const u32 n = static_cast<u32>(look_->boom.bossSparks * tier_.particleDensity * governorDensity_);
    if (sparkReqCount_ < sparkReqCap_) sparkReqs_[sparkReqCount_++] = {B, static_cast<u16>(n), 1, 2, {}};
    addPool(B, 240.0f, 420.0f, white(), 0.9f, 0.0f, 0.5f, false, 0, false, false);  // the clamped white flash (a boss's: never dimmed)
    const LookDistortionSource& d = look_->distortion.bossBreak;
    addDistortion(B, 700.0f, 0.35f, d.refPx, d.widthRefPx);
    if (look_->boom.bossShakeRefPx > shakeAmp_) shakeAmp_ = look_->boom.bossShakeRefPx;
    hitStopUntil_ = songT_ + look_->boom.bossHitStopMs * 0.001f;
    lattice_.impulse(B, 300.0f, 160.0f);
}

void Stage::logCounts() const {
    static const char* const kNames[kFamilies] = {"shoal", "drifter", "hound",  "flanker", "ram",   "carom",  "sentry",
                                                  "wyrm",  "wyrm body", "moth", "tether",  "blink", "weaver", "boss"};
    char line[512];
    int n = 0;
    for (u32 f = 0; f < kFamilies && n < static_cast<int>(sizeof line) - 32; ++f) {
        if (!stats_.familyInView[f]) continue;
        n += std::snprintf(line + n, sizeof line - static_cast<size_t>(n), "%s%s %u", n ? ", " : "", kNames[f],
                           stats_.familyInView[f]);
    }
    line[n < static_cast<int>(sizeof line) ? n : static_cast<int>(sizeof line) - 1] = '\0';
    RF_LOG("stage: in view by family: %s", n ? line : "none");
}

bool Stage::visible(Vec2 w, float marginWu) const {
    const Vec2 d = w - cam_;
    return std::fabs(d.x) <= 0.5f * viewW_ + marginWu && std::fabs(d.y) <= 0.5f * viewH_ + marginWu;
}

void Stage::glyph(GlyphBatch& b, int g, bool hullOnly, Vec2 pos, float rot0, float rot1, float scaleWu, float intensity,
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

void Stage::flare(CapsuleBatch& out, Vec2 at, float angle, float lengthWu, u32 arms, Vec3 colour, float halfWidthRef) {
    // Star streaks through a centre that taper to nothing (look.md §4 flares, fade exponent 1.6).
    const float halfW = halfWidthRef * refPx_;
    const float mid = std::pow(0.5f, look_->flareFadeExponent);
    for (u32 k = 0; k < arms; ++k) {
        const Vec2 d = dir(angle + kPi * static_cast<float>(k) / static_cast<float>(arms));
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            const Vec2 m = at + d * (0.5f * lengthWu * static_cast<float>(sgn));
            const Vec2 tip = at + d * (lengthWu * static_cast<float>(sgn));
            out.line(toScreen(at), toScreen(m), halfW, colour, colour * mid, kFlatEnd);
            out.line(toScreen(m), toScreen(tip), halfW, colour * mid, Vec3{}, kFlatStart);
        }
    }
}

void Stage::lethalLine(FrameDraw& d, Vec2 a, Vec2 b) {
    // Lethal light (tether beams, weaver trails): danger in the scene and redrawn on the threat layer.
    const Vec3 danger = palettes_->danger;
    const LookLine& l = look_->lines.enemyBullet;
    const float i = l.intensity * swarmGain_;  // scene light follows the swarm gain; the priority copy does not
    if (shown(kMuteBullet))
        d.scene.line(toScreen(a), toScreen(b), 0.5f * l.width * refPx_, danger * i, danger * i, kFlatStart | kFlatEnd);
    if (!shown(kMutePriority)) return;
    const Vec3 core = mix(danger, white(), 0.25f);
    const float w = 0.5f * look_->threat.bulletWidth * refPx_;
    d.threatRims.line(toThreat(a), toThreat(b), w + look_->threat.bulletRim * refPx_, Vec3{}, Vec3{}, 0);
    d.threatCores.line(toThreat(a), toThreat(b), w, core, core, 0);
}

void Stage::drawEnemy(FrameDraw& d, const Enemy& e) {
    const Look::Enemy& le = look_->enemy;
    const GlyphDef& g = glyphs_->def(static_cast<u32>(e.glyph));
    const Vec3 hue = palette_->hue[e.hue];
    const Vec3 partner = palette_->partner[e.hue];
    const bool elite = (e.flags & kElite) != 0;
    const float bar = barLen();
    const float tier = static_cast<float>(e.tier);
    float intensity = look_->lines.enemyHullTier1.intensity + le.hullIntensityPerTier * (tier - 1.0f);
    float halfWidth = 0.5f * look_->lines.enemyHullTier1.width;
    if (elite) {
        intensity += le.eliteIntensityAdd;
        halfWidth = 0.5f * le.eliteWidth;
    }
    // Light budget (look.md v2.1 §3): the horde dims, elites and bosses do not.
    const float gain = (elite || e.family == kBoss) ? 1.0f : crowdGain_;
    intensity *= gain;
    float scale = e.radius;
    if (e.family == kBoss) scale = kEnemyRadiusWu * (1.5f - 0.17f * static_cast<float>(e.index));
    float rot0, rot1;
    if (g.facesHeading) {
        rot0 = rot1 = e.family == kSentry ? angleOf(shipAt(simT_) - e.pos) : e.heading;
    } else {
        // Spinning shapes: the hull a quarter turn per bar, the ornament the other way at twice the rate,
        // stepping on each hat (look.md §4).
        rot0 = e.d + kTau * le.hullTurnsPerBar * simT_ / bar;
        const float stepped = lastHatT_ > 0.0f ? lastHatT_ : 0.0f;
        rot1 = e.d + kTau * le.ornamentTurnsPerBar * stepped / bar;
        if (e.family == kBoss) rot0 = rot1 = e.b * ((e.group & 1u) ? -1.0f : 1.0f);
    }
    const float radiusPx = scale * pxPerWu_;
    const bool lod = radiusPx < tier_.lodRadiusPx;
    // Auras: elites and bosses (look.md v2.1 §3); auraByTier is zero in look.json since v2.1. The boss
    // stand-in is 48 ordinary glyphs, not a boss design (scenes.md §4): like the endgame target frame
    // (design/tools/crowd.py) its parts carry no aura each - 48 auras would be the fog §3 removes.
    const float auraPool =
        elite ? le.eliteAura : (e.tier >= 3 && tier_.aurasBelowElite ? le.auraByTier[e.tier - 1] * gain : 0.0f);
    if (auraPool > 0.0f && shown(kMuteAura))
        d.pools.glow(toScreen(e.pos), le.auraRadiusOfExtent * g.extent * scale * pxPerWu_, hue * auraPool);
    if (!shown(e.family == kBoss ? kMuteBoss : (elite ? kMuteElite : kMuteEnemy))) return;
    glyph(d.glyphs, e.glyph, lod, e.pos, rot0, rot1, scale, intensity, halfWidth, kGlyphEnemy, hue, partner);
    if (elite) {
        glyph(d.glyphs, e.glyph, true, e.pos, rot0, rot1, scale, look_->lines.eliteWhiteCopy.intensity,
              0.5f * look_->lines.eliteWhiteCopy.width, kGlyphWhite, white(), white());
        if (tier_.glass && g.glassCount) {
            const Vec2* tri = glyphs_->glass() + g.glassFirst * 3;
            const float c = std::cos(rot0) * scale, s = std::sin(rot0) * scale;
            const Vec3 tint = hue * le.eliteGlass;
            for (u32 k = 0; k < g.glassCount; ++k) {
                const Vec2 p0 = toScreen(e.pos + rotate(tri[k * 3], c, s));
                const Vec2 p1 = toScreen(e.pos + rotate(tri[k * 3 + 1], c, s));
                const Vec2 p2 = toScreen(e.pos + rotate(tri[k * 3 + 2], c, s));
                d.glass.tri(p0, tint, p1, tint, p2, tint);
            }
        }
        if (e.family == kHound) {  // a charging elite: two flare arms along its heading
            flare(d.scene, e.pos, e.heading, 80.0f, 1, white() * look_->lines.eliteFlare.intensity,
                  0.5f * look_->lines.eliteFlare.width);
        }
    }
    if (e.flags & (kMarkB | kMarkC)) {
        const int mark = (e.flags & kMarkC) ? gMarkC_ : (g.facesHeading ? gMarkBHeading_ : gMarkBSpin_);
        glyph(d.glyphs, mark, false, e.pos, rot0, rot0, scale, look_->lines.variantMark.intensity * gain,
              0.5f * look_->lines.variantMark.width, kGlyphWhite, white(), white());
    }
    if (e.family == kBlink) {
        flare(d.scene, e.pos, kPi * 0.25f, 2.2f * scale, 2, partner * (look_->lines.blinkFlare.intensity * gain),
              0.5f * look_->lines.blinkFlare.width);
    }
    if (e.family == kCarom) {  // a short motion streak
        const float sp = length(e.vel);
        if (sp > 1.0f) {
            const Vec2 u = e.vel * (1.0f / sp);
            d.scene.line(toScreen(e.pos - u * 18.0f), toScreen(e.pos - u * 64.0f), 1.0f * refPx_, hue * (1.6f * gain), Vec3{}, 0);
        }
    }
    if (e.family == kRam && e.c > 0.5f) {
        // One beat of telegraph chevrons along the dash path (danger; scene and threat layer).
        const Vec2 dd = dir(e.b);
        const Vec2 n = perp(dd);
        const Vec3 danger = palettes_->danger;
        const LookLine& tl = look_->lines.telegraph;
        const Vec3 sceneLight = danger * (tl.intensity * swarmGain_);
        for (u32 k = 1; k <= 8; ++k) {
            const Vec2 c = e.pos + dd * (60.0f * static_cast<float>(k));
            const Vec2 a = c - dd * 10.0f + n * 10.0f, b = c - dd * 10.0f - n * 10.0f;
            d.scene.line(toScreen(a), toScreen(c), 0.5f * tl.width * refPx_, sceneLight, sceneLight, 0);
            d.scene.line(toScreen(b), toScreen(c), 0.5f * tl.width * refPx_, sceneLight, sceneLight, 0);
            const float w = 0.5f * look_->threat.telegraphWidth * refPx_;
            const Vec3 core = danger * 0.9f;
            d.threatRims.line(toThreat(a), toThreat(c), w + look_->threat.telegraphRim * refPx_, Vec3{}, Vec3{}, 0);
            d.threatRims.line(toThreat(b), toThreat(c), w + look_->threat.telegraphRim * refPx_, Vec3{}, Vec3{}, 0);
            d.threatCores.line(toThreat(a), toThreat(c), w, core, core, 0);
            d.threatCores.line(toThreat(b), toThreat(c), w, core, core, 0);
        }
    }
}

void Stage::render(FrameDraw& d, FrameParams& params, Background& bg, float W, float H) {
    const float t = simT_, song = songT_;
    const float beat = beatLen(), bar = barLen();
    refPx_ = H / 1080.0f;
    const Vec2 S = shipAt(t);
    const Vec2 V = shipVelocity(t);
    const float heading = angleOf(V);
    const float aim = heading + 35.0f * kPi / 180.0f * std::sin(kTau * t / 7.0f);
    const bool endgame = kind_ == StageKind::Endgame;
    const Vec3 W1 = white();
    const Vec3 player = palettes_->player[0], deep = palettes_->playerDeep[0], light = palettes_->playerLight[0];
    Vec3 accent = palette_->accent;
    if (accentSwapped_) accent = palette_->hue[2];  // the Drop swaps the accent with the third enemy hue

    // ---- Camera (design/scenes.md §2): 0.25 s of look-ahead, zoom breathing 1.0 +- 0.1 over 29 s.
    float eventZoom = 1.0f;
    if (endgame) {
        const float sinceDrop = song - dropT_;
        if (buildAmount_ > 0.0f) eventZoom = 1.0f + 0.1f * buildAmount_;
        if (sinceDrop >= 0.0f && sinceDrop < bar) eventZoom = 0.9f + 0.1f * smooth01(sinceDrop / bar);
    }
    // Beat response amounts (look.md §7 table), halved in reduce-flashing mode.
    const float beatAmt = beatAmount();
    zoom_ = (1.0f + 0.1f * std::sin(kTau * t / 29.0f)) * eventZoom * (1.0f + look_->zoomKick * beatAmt * kick_);
    cam_ = S + V * 0.25f;
    pxPerWu_ = kRefPxPerWu * refPx_ * zoom_;
    centre_ = {0.5f * W, 0.5f * H};
    viewW_ = W / pxPerWu_;
    viewH_ = H / pxPerWu_;
    const float shakePx = clampf(shakeAmp_, 0.0f, look_->shakeMaxRefPx) * refPx_;
    shake_ = shakePx > 0.05f ? dir(rng_.range(0.0f, kTau)) * shakePx : Vec2{};
    lattice_.recentre(cam_);

    // ---- Light budget (look.md v2.1 §3), from the previous frame's load inside the camera rectangle,
    // followed with its time constant.
    {
        const Look::Crowd& cr = look_->crowd;
        const float load = stats_.load;
        const float target = load > 1.0f ? clampf(std::pow(cr.restLoad / load, cr.exponent), cr.minGain, 1.0f) : 1.0f;
        const float follow = cr.followSeconds > 0.0f ? 1.0f - std::exp(-lastDt_ / cr.followSeconds) : 1.0f;
        crowdGain_ += (target - crowdGain_) * follow;
        swarmGain_ = std::pow(crowdGain_, cr.swarmExponent);
        stats_.crowdGain = crowdGain_;
        stats_.swarmGain = swarmGain_;
    }
    const float crowd = crowdGain_, swarm = swarmGain_;

    // ---- Sparks born this frame (stateless; TECH_SPEC 5.3).
    stats_.sparksBorn = 0;
    stats_.sparksDropped = 0;
    if (d.particles) {
        const Look::Explosions& boom = look_->boom;
        const float levels[4] = {2.4f, 3.4f, 4.6f, 5.6f};
        for (u32 r = 0; r < sparkReqCount_; ++r) {
            const SparkReq& q = sparkReqs_[r];
            const Vec3 h = palette_->hue[q.hue], p = palette_->partner[q.hue];
            for (u32 k = 0; k < q.count; ++k) {
                ParticleGpu* sp = d.particles->spawn();
                if (!sp) {
                    stats_.sparksDropped += q.count - k;
                    break;
                }
                Vec2 at = q.at, v;
                float life, drag = boom.sparkDrag, halfW = 0.7f, delay = 0.0f;
                Vec3 own = mix(mix(h, p, rng_.uniform()), W1, 0.4f * rng_.uniform());
                switch (q.kind) {
                    case 1: {  // large: a fast thin band and a slow thick band
                        const bool fast = (k & 1u) != 0;
                        v = dir(rng_.range(0.0f, kTau)) * (fast ? rng_.range(500.0f, boom.large.speedWu[1]) : rng_.range(boom.large.speedWu[0], 400.0f));
                        life = rng_.range(boom.large.lifeS[0], boom.large.lifeS[1]);
                        halfW = fast ? 0.55f : 1.0f;
                        break;
                    }
                    case 2:  // boss phase break
                        v = dir(rng_.range(0.0f, kTau)) * rng_.range(200.0f, boom.bossSpeedMax);
                        life = rng_.range(0.6f, boom.bossLifeMax);
                        own = mix(own, W1, 0.5f);
                        break;
                    case 3:  // shot impact: back along the shot
                        v = (q.dir + dir(rng_.range(0.0f, kTau)) * 0.5f) * rng_.range(150.0f, 300.0f);
                        life = boom.impactLifeS;
                        break;
                    case 4: {
                        // Drop confetti (look.md v2.2 §5): thrown in toward the ship from 0-200 wu beyond
                        // the view edge, 0-250 ms after the Drop, so it arrives as a shower and not as a
                        // frame around the screen. Spread evenly along the edge (by its length).
                        const Look::DropConfetti& dc = look_->dropConfetti;
                        const float out = rng_.range(dc.outsideWu[0], dc.outsideWu[1]);
                        const float hx = 0.5f * viewW_ + out, hy = 0.5f * viewH_ + out;
                        if (rng_.uniform() < hx / (hx + hy)) {
                            at = cam_ + Vec2{rng_.range(-hx, hx), rng_.uniform() < 0.5f ? -hy : hy};
                        } else {
                            at = cam_ + Vec2{rng_.uniform() < 0.5f ? -hx : hx, rng_.range(-hy, hy)};
                        }
                        const Vec2 in = S - at;
                        const float l = length(in);
                        v = (l > 1e-3f ? in * (1.0f / l) : Vec2{1.0f, 0.0f}) * rng_.range(dc.speedWu[0], dc.speedWu[1]);
                        life = rng_.range(dc.lifeS[0], dc.lifeS[1]);
                        delay = rng_.range(dc.delayS[0], dc.delayS[1]);
                        drag = 1.0f;
                        own = rng_.uniform() < 0.3f ? W1 : mix(palette_->hue[rng_.below(3)], accent, rng_.uniform() * 0.5f);
                        break;
                    }
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
                g.t0 = t + delay;  // stateless: a spark born later is simply not drawn before its birth
                g.motion[0] = floatToHalf(v.x);
                g.motion[1] = floatToHalf(v.y);
                g.motion[2] = floatToHalf(life);
                g.motion[3] = floatToHalf(drag);
                // Sparks follow the crowd gain at birth.
                g.light[0] = floatToHalf(levels[rng_.below(4)] * crowd * (shown(kMuteSparks) ? 1.0f : 0.0f));
                g.light[1] = floatToHalf(halfW);
                g.colour = packLinear(own);
                g.flags = 0;
                std::memcpy(sp, &g, sizeof g);
                ++stats_.sparksBorn;
            }
        }
    }
    sparkReqCount_ = 0;

    // ---- Background (look.md §7): gradient, nebula at a quarter of the camera's speed, bass swell.
    const Vec3 bgEdge = palette_->bg0 * look_->bgLevel;
    const Vec3 bgCentre = palette_->bg1 * look_->bgLevel;
    const float bassLift = 1.0f + look_->backgroundBass * beatAmt * bass_;
    const float drift = look_->backgroundHueDriftDegrees * beatAmt / 60.0f * bass_;  // toward the accent, a little
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
        // Star dust at half the camera's speed: twinkle on hats, flare on snares.
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

    // ---- Lattice (world-locked): kick ripples leave the player at 550 wu/s.
    LatticeView lv;
    lv.camera = cam_;
    lv.pxPerWu = pxPerWu_;
    lv.screenCentre = centre_ + shake_;
    lv.widthPx = W;
    lv.heightPx = H;
    lv.refPx = refPx_;
    lv.player = S;
    lv.hatShimmer = look_->latticeHatShimmer * beatAmt * hat_;
    for (float kt : {lastKickT_, prevKickT_}) {
        const float age = song - kt;
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
    ls.heatGain = shown(kMuteHeat) ? crowd : 0.0f;  // only the heat above the rest level dims
    // Wells pull the lattice.
    Vec2 wells[3];
    const u32 wellCount = endgame ? 3u : 1u;
    for (u32 i = 0; i < wellCount; ++i) wells[i] = S + dir(0.3f * t + kTau * i / 3.0f) * (endgame ? 380.0f : 300.0f);
    lattice_.setWells(wells, wellCount, buildAmount_ > 0.0f ? 12.0f : 24.0f);
    stats_.latticeSegments = shown(kMuteGrid) ? lattice_.draw(d.scene, lv, ls) : 0u;

    // ---- Enemies. "In view" for the counts and the light budget = inside the camera rectangle; the
    // drawing keeps a margin so shapes do not pop at the edge.
    stats_.enemiesInView = 0;
    stats_.enemies = enemyCount_ + farCount_;
    for (u32& n : stats_.familyInView) n = 0;
    for (u32 i = 0; i < farCount_; ++i)
        if (visible(farPos_[i], 0.0f)) ++stats_.enemiesInView;  // far mode: culled, counted if ever inside
    for (u32 i = 0; i < enemyCount_; ++i) {
        Enemy& e = enemies_[i];
        e.inView = visible(e.pos, 2.5f * e.radius);
        if (!e.inView) continue;
        if (visible(e.pos, 0.0f)) {
            ++stats_.enemiesInView;
            ++stats_.familyInView[e.family];
        }
        if (e.flags & kWarping) {
            // Warp-in (look.md v2.1 §5): a ring in the enemy hue contracting to nothing over two beats,
            // from 2.5 x the glyph's drawn extent (elites and bosses: 90 wu).
            const float left = clampf((e.warpUntil - song) / (look_->boom.warpBeats * beat), 0.0f, 1.0f);
            const float extent = glyphs_->def(static_cast<u32>(e.glyph)).extent * e.radius;
            const float from = (e.flags & (kElite | kBossPart)) ? look_->boom.warpEliteFromWu
                                                                : look_->boom.warpFromGlyphExtents * extent;
            const float r = from * left;
            if (r > 0.5f && shown(kMuteWarp))
                glyph(d.glyphs, r > 40.0f ? gRing64_ : gRing32_, false, e.pos, 0.0f, 0.0f, r,
                      look_->lines.warpRing.intensity * crowd, 0.5f * look_->lines.warpRing.width, kGlyphFlat,
                      palette_->hue[e.hue], W1);
            continue;
        }
        drawEnemy(d, e);
    }
    // Tether beams and weaver trails: lethal light.
    if (endgame) {
        for (u32 i = 0; i + 1 < enemyCount_; ++i) {
            const Enemy& a = enemies_[i];
            if (a.family == kTether && a.index == 0) {
                const Enemy& b = enemies_[i + 1];
                if ((a.inView || b.inView) && !((a.flags | b.flags) & kWarping)) lethalLine(d, a.pos, b.pos);
            }
            if (a.family == kWeaver && a.inView) {
                Enemy probe = a;
                Vec2 prev = a.pos;
                const float step = 2.0f * bar / 40.0f;
                for (u32 k = 1; k <= 40; ++k) {
                    place(probe, t - step * static_cast<float>(k), S);
                    lethalLine(d, prev, probe.pos);
                    prev = probe.pos;
                }
            }
        }
    }

    // ---- Enemy bullets: the Sentry arms (rigid), then the free ones. Scene and threat layer.
    stats_.bullets = 0;
    stats_.bulletsInView = 0;
    const Vec3 danger = palettes_->danger;
    const Vec3 threatCore = mix(danger, W1, 0.25f);
    const float bulletScale = kEnemyRadiusWu * glyphs_->def(static_cast<u32>(gBullet_)).size;
    const float rimHalf = 0.5f * look_->threat.bulletWidth + look_->threat.bulletRim;
    const float coreHalf = 0.5f * look_->threat.bulletWidth;
    // A bullet's scene light follows the swarm gain; its priority-layer copy never dims. The danger shape
    // is the hollow diamond of glyphs.json (points on the axes, facesHeading false): it is drawn upright
    // and never turns, like every bullet in the reference pictures. Turned to its travel, the square
    // diamond reads as a square on every diagonal path.
    const auto bullet = [&](Vec2 p) {
        ++stats_.bullets;
        if (!visible(p, 10.0f)) return;
        if (visible(p, 0.0f)) ++stats_.bulletsInView;
        if (shown(kMuteBullet)) {
            glyph(d.glyphs, gBullet_, false, p, 0.0f, 0.0f, bulletScale, look_->lines.enemyBullet.intensity * swarm,
                  0.5f * look_->lines.enemyBullet.width, kGlyphFlat, danger, danger);
            if (tier_.bulletSparkles)
                d.sparkles.glow(toScreen(p), look_->soft.enemyBullet.radius[0] * refPx_,
                                danger * (look_->soft.enemyBullet.intensity[0] * swarm));
        }
        if (shown(kMutePriority)) {
            glyph(d.threatGlyphRims, gBullet_, false, p, 0.0f, 0.0f, bulletScale, 1.0f, rimHalf, kGlyphFlat, Vec3{},
                  Vec3{});
            glyph(d.threatGlyphCores, gBullet_, false, p, 0.0f, 0.0f, bulletScale, 1.0f, coreHalf, kGlyphFlat,
                  threatCore, threatCore);
        }
    };
    const u32 perArm = endgame ? 20u : 16u;
    for (u32 i = 0; i < enemyCount_; ++i) {
        const Enemy& e = enemies_[i];
        if (e.family != kSentry || (e.flags & kWarping)) continue;
        const float turn = kTau * t / (4.0f * bar) + static_cast<float>(e.index);
        for (u32 a = 0; a < 3; ++a) {
            const float ang = turn + kTau * static_cast<float>(a) / 3.0f;
            const Vec2 dd = dir(ang);
            // Bullets 16 wu apart: a 330 wu arm stays mostly on screen around the squashed ring.
            for (u32 k = 0; k < perArm; ++k) bullet(e.pos + dd * (30.0f + 16.0f * static_cast<float>(k)));
        }
    }
    for (u32 i = 0; i < bulletCap_; ++i) {
        const Bullet& b = bullets_[i];
        const float age = t - b.born;
        if (age < 0.0f || age >= b.life) continue;
        bullet(b.p0 + b.v * age);
    }

    // ---- The player: ship (two-tone), halo, trail, pool, shots, Riffs.
    // The ship's size in glyphs.json is relative to the standard enemy (look.md §4): 14 wu at size 1.
    const float shipScale = kEnemyRadiusWu * glyphs_->def(static_cast<u32>(gShip_)).size;
    const LookLine& hull = look_->lines.playerHull;
    const bool playerShown = shown(kMutePlayer);
    if (playerShown) {
        glyph(d.glyphs, gShip_, false, S, heading, heading, shipScale, hull.intensity, 0.5f * hull.width, kGlyphPlayer, player,
              deep);
        glyph(d.glyphs, gShip_, true, S, heading, heading, shipScale, look_->lines.playerWhiteCopy.intensity,
              0.5f * look_->lines.playerWhiteCopy.width, kGlyphWhite, W1, W1);
        const float haloPulse = 1.0f + 0.25f * kick_;
        glyph(d.glyphs, gHalo3_, false, S, 0.3f * t, 0.3f * t, shipScale, look_->lines.playerHalo.intensity * haloPulse,
              0.5f * look_->lines.playerHalo.width, kGlyphRingA, player, light);
    }
    // The ship on the priority layer (look.md v2.1 §6, §9): hull and halo again after tone mapping, each
    // with a dark rim, never shaken. Rims go with every other rim, cores before the threats' cores.
    const auto priorityShip = [&](Vec2 at, float facing, float scale) {
        const Look::PriorityShip& ph = look_->priorityHull;
        const Look::PriorityShip& pa = look_->priorityHalo;
        const Vec3 hullCore = light * 0.55f + W1 * 0.45f;
        const Vec3 haloCore = player * 0.75f + W1 * 0.25f;
        glyph(d.threatGlyphRims, gShip_, true, at, facing, facing, scale, 1.0f, 0.5f * ph.width + ph.rim, kGlyphFlat, Vec3{},
              Vec3{});
        glyph(d.threatGlyphRims, gHalo3_, false, at, 0.3f * t, 0.3f * t, scale, 1.0f, 0.5f * pa.width + pa.rim, kGlyphFlat,
              Vec3{}, Vec3{});
        glyph(d.priorityShipCores, gShip_, true, at, facing, facing, scale, 1.0f, 0.5f * ph.width, kGlyphFlat, hullCore,
              hullCore);
        glyph(d.priorityShipCores, gHalo3_, false, at, 0.3f * t, 0.3f * t, scale, 1.0f, 0.5f * pa.width, kGlyphFlat, haloCore,
              haloCore);
    };
    if (shown(kMutePriority)) priorityShip(S, heading, shipScale);
    if (playerShown)
        d.pools.glow(toScreen(S), look_->soft.playerPool.radius[0] * pxPerWu_, player * look_->soft.playerPool.intensity[0]);
    if (playerShown) {  // trail: 13 dashes over 100 wu, white -> player -> deep, fading to nothing
        const LookLine& tr = look_->lines.playerTrail;
        const float span = 100.0f / (length(V) > 1.0f ? length(V) : 1.0f);
        for (u32 k = 0; k < 13; ++k) {
            const float u0 = static_cast<float>(k) / 13.0f, u1 = (static_cast<float>(k) + 0.6f) / 13.0f;
            const Vec2 a = shipAt(t - span * u0) - dir(heading) * (shipScale * 0.9f);
            const Vec2 b = shipAt(t - span * u1) - dir(heading) * (shipScale * 0.9f);
            const auto field = [&](float u) {
                const float f = clampf(u, 0.0f, 1.0f) * 2.0f;
                const Vec3 c = f < 1.0f ? mix(W1, player, f) : mix(player, deep, f - 1.0f);
                return c * (tr.intensity * (1.0f - u));
            };
            d.scene.line(toScreen(a), toScreen(b), 0.5f * tr.width * refPx_, field(u0), field(u1), 0);
        }
    }
    stats_.shots = 0;
    const Vec3 shotHead = W1 * look_->soft.shotHead.intensity[0];
    for (u32 i = 0; i < shotCap_; ++i) {
        const Shot& s = shots_[i];
        const float age = t - s.born;
        if (age < 0.0f || age >= kShotLife) continue;
        const Vec2 head = s.p0 + s.dir * (s.speed * age);
        // look.md v2.2 §3: the capsule runs back over the distance the shot covers in a frame plus an
        // overlap (never under the minimum), so one frame's capsule overlaps the next and the afterglow
        // behind it is one even tail. Player, wingman and Sentinel shots alike.
        const Look::ShotStreak& ss = look_->shotStreak;
        const float len = std::fmax(s.speed * ss.frameS + ss.overlapWu, ss.minWu);
        if (!visible(head, len + 4.0f)) continue;
        ++stats_.shots;
        if (!playerShown) continue;
        const Vec2 tail = head - s.dir * len;
        d.scene.line(toScreen(tail), toScreen(head), s.halfWidth * refPx_, player * s.intensity, W1 * s.intensity, 0);
        d.sparkles.glow(toScreen(head), look_->soft.shotHead.radius[0] * refPx_, shotHead);
    }
    const u32 orbitals = endgame ? 5u : 3u;
    for (u32 k = 0; k < orbitals && playerShown; ++k) {
        const float a = kTau * (t / 1.2f + static_cast<float>(k) / static_cast<float>(orbitals));
        const Vec2 p = S + dir(a) * 70.0f;
        glyph(d.glyphs, gRing32_, false, p, 0.0f, 0.0f, 5.0f, look_->lines.orbital.intensity, 0.5f * look_->lines.orbital.width,
              kGlyphFlat, player, player);
        d.sparkles.glow(toScreen(p), look_->soft.orbital.radius[0] * refPx_, player * look_->soft.orbital.intensity[0]);
        for (u32 s = 1; s <= 6; ++s) {  // an arc of trail behind it
            const Vec2 p0 = S + dir(a - 0.12f * static_cast<float>(s - 1)) * 70.0f;
            const Vec2 p1 = S + dir(a - 0.12f * static_cast<float>(s)) * 70.0f;
            const float f0 = 1.0f - static_cast<float>(s - 1) / 6.0f, f1 = 1.0f - static_cast<float>(s) / 6.0f;
            d.scene.line(toScreen(p0), toScreen(p1), 0.5f * look_->lines.orbitalArc.width * refPx_,
                         player * (look_->lines.orbitalArc.intensity * f0), player * (look_->lines.orbitalArc.intensity * f1),
                         kFlatStart | kFlatEnd);
        }
    }
    if (playerShown) {  // the Sentinel drone
        const Vec2 drone = S + dir(t * 1.7f) * 42.0f;
        glyph(d.glyphs, gRing32_, false, drone, 0.0f, 0.0f, 4.0f, look_->lines.sentinel.intensity, 0.5f * look_->lines.sentinel.width,
              kGlyphFlat, player, player);
        d.sparkles.glow(toScreen(drone), look_->soft.orbital.radius[0] * refPx_, player * look_->soft.orbital.intensity[0]);
    }
    if (playerShown) {  // Pulse Ring: every second beat (bench_mid), a double ring every beat (endgame); 250 wu in 0.4 s
        const float sinceBeat = song - lastBeatT_;
        const bool on = endgame || (beatIndex() % 2 == 0);
        for (u32 k = 0; k < (endgame ? 2u : 1u) && on; ++k) {
            const float age = sinceBeat - 0.1f * static_cast<float>(k);
            if (age < 0.0f || age > 0.4f) continue;
            const float u = age / 0.4f;
            const float r = 250.0f * (1.0f - (1.0f - u) * (1.0f - u));
            const float fade = std::sqrt(1.0f - u);
            glyph(d.glyphs, gRing64_, false, S, 0.0f, 0.0f, r, look_->lines.pulseRing.intensity * fade,
                  0.5f * look_->lines.pulseRing.width, kGlyphRingA, player, light);
            glyph(d.glyphs, gRing64_, false, S, 0.5f, 0.5f, r * 0.85f, look_->lines.pulseRingEcho.intensity * fade,
                  0.5f * look_->lines.pulseRingEcho.width, kGlyphRingA, player, light);
        }
    }
    if (endgame && playerShown) {
        // Lancer beam across the whole view every second beat (scenes.md §4: 6 px wide, intensity 4, 150 ms),
        // half player colour and half white, as in the endgame target (design/tools/crowd.py).
        const float sinceBeat = song - lastBeatT_;
        if ((beatIndex() % 2) == 1 && sinceBeat < 0.15f) {
            const Vec2 a = S, b = S + dir(aim) * 2400.0f;
            const Vec3 beam = (player * 0.5f + W1 * 0.5f) * 4.0f;
            d.scene.line(toScreen(a), toScreen(b), 3.0f * refPx_, beam, beam, 0);
        }
        // The Sweeper arm: 600 wu, once round every two bars; drawn like the target's (Riff light 2.2, 2.6 px,
        // white -> player -> deep along its length).
        const float sa = kTau * t / (2.0f * bar);
        const auto sweep = [&](float f) {
            return (f < 0.5f ? mix(W1, player, 2.0f * f) : mix(player, deep, 2.0f * f - 1.0f)) * 2.2f;
        };
        for (u32 k = 0; k < 6; ++k) {
            const Vec2 a = S + dir(sa) * (40.0f + 93.0f * static_cast<float>(k));
            const Vec2 b = S + dir(sa) * (40.0f + 93.0f * static_cast<float>(k + 1));
            const float f0 = (40.0f + 93.0f * static_cast<float>(k)) / 600.0f, f1 = (40.0f + 93.0f * static_cast<float>(k + 1)) / 600.0f;
            d.scene.line(toScreen(a), toScreen(b), 1.3f * refPx_, sweep(f0), sweep(f1),
                         (k ? kFlatStart : 0u) | (k < 5 ? kFlatEnd : 0u));
        }
        // The wingman: a second ship, mirrored.
        glyph(d.glyphs, gShip_, false, S + perp(dir(heading)) * 60.0f, heading, heading, shipScale * 0.7f, hull.intensity,
              0.5f * hull.width, kGlyphPlayer, player, deep);
    }
    if (endgame && shown(kMutePriority)) priorityShip(S + perp(dir(heading)) * 60.0f, heading, shipScale * 0.7f);

    // ---- Shards fly to the ship in 0.6 s.
    stats_.shards = 0;
    const Vec3 shardColour = mix(palettes_->pickup, W1, 0.4f);
    const float shardScale = kEnemyRadiusWu * glyphs_->def(static_cast<u32>(gShard_)).size;
    for (u32 i = 0; i < shardCap_; ++i) {
        const Shard& s = shards_[i];
        const float age = t - s.born;
        if (age < 0.0f || age >= kShardFlightS) continue;
        ++stats_.shards;
        const float u = age / kShardFlightS;
        const Vec2 burst = s.p0 + s.v0 * (age * (1.0f - u));
        const Vec2 p = burst + (S - burst) * (u * u * (3.0f - 2.0f * u));
        if (!visible(p, 10.0f) || !shown(kMuteShard)) continue;
        glyph(d.glyphs, gShard_, false, p, s.spin + 6.0f * age, 0.0f, shardScale, look_->lines.shard.intensity * swarm,
              0.5f * look_->lines.shard.width, kGlyphFlat, shardColour, shardColour);
        if (tier_.bulletSparkles)
            d.sparkles.glow(toScreen(p), look_->soft.shard.radius[0] * refPx_,
                            palettes_->pickup * (look_->soft.shard.intensity[0] * swarm));
    }

    // ---- Effects: flashes and kill pools, shock rings (at most maxShockRings), embers, shatter, wells.
    // All of them follow the crowd gain except the boss's flash.
    for (u32 i = 0; i < poolCap_; ++i) {
        const Pool& p = pools_[i];
        const float age = song - p.born;
        if (age < 0.0f || age >= p.life) continue;
        if (!visible(p.at, p.sparkle ? 10.0f : p.r1) || !shown(p.kill ? kMutePool : kMuteFlash)) continue;
        const float u = age / p.life;
        const float level = (p.fade == 1 ? p.i0 * (1.0f - u) * (1.0f - u) : p.i0 + (p.i1 - p.i0) * u) * (p.dimmed ? crowd : 1.0f);
        const float radius = p.r0 + (p.r1 - p.r0) * u;
        if (p.sparkle)
            d.sparkles.glow(toScreen(p.at), radius * refPx_, p.colour * level);
        else
            d.pools.glow(toScreen(p.at), radius * pxPerWu_, p.colour * level);
    }
    const float ringS = look_->boom.medium.ringS > 0.0f ? look_->boom.medium.ringS : 0.2f;
    const bool fxShown = shown(kMuteFx);
    for (const ShockRing& sr : shock_) {
        const float age = song - sr.start;
        if (age < 0.0f || age >= ringS || !visible(sr.at, sr.toWu) || !fxShown) continue;
        const float u = age / ringS;
        const float r = sr.toWu * (0.25f + 0.75f * std::sqrt(u));
        glyph(d.glyphs, gRing64_, false, sr.at, 0.0f, 0.0f, r, look_->lines.shockRing.intensity * (1.0f - u) * crowd,
              0.5f * look_->lines.shockRing.width, kGlyphRingW, palette_->hue[sr.hue], palette_->partner[sr.hue]);
    }
    for (u32 i = 0; i < emberCap_; ++i) {
        const Ember& m = embers_[i];
        const float age = song - m.born;
        if (age < 0.0f || age >= m.life) continue;
        const Vec2 p = m.p0 + m.v0 * ((1.0f - std::exp(-2.0f * age)) / 2.0f);
        if (!visible(p, 10.0f) || !fxShown) continue;
        d.sparkles.glow(toScreen(p), look_->soft.ember.radius[0] * refPx_,
                        palette_->partner[m.hue] * (look_->soft.ember.intensity[0] * (1.0f - age / m.life) * crowd));
    }
    for (u32 i = 0; i < shatterCap_; ++i) {
        const Shatter& s = shatter_[i];
        const float age = song - s.born;
        if (age < 0.0f || age >= look_->boom.shatterS || !fxShown) continue;
        const Vec2 c = s.centre + s.v * age;
        const float a = s.spin * age, cs = std::cos(a), sn = std::sin(a);
        const float fade = 1.0f - age / look_->boom.shatterS;
        const Vec3 col = mix(palette_->hue[s.hue], W1, 0.5f) * (look_->lines.shatter.intensity * fade * crowd);
        d.scene.line(toScreen(c + rotate(s.a, cs, sn)), toScreen(c + rotate(s.b, cs, sn)), 0.5f * look_->lines.shatter.width * refPx_,
                     col, col, 0);
    }
    params.wellCount = 0;
    for (u32 i = 0; i < wellCount; ++i) {
        const Vec2 w = wells[i];
        const Vec3 h = palette_->hue[i % 3];
        if (fxShown) {
            d.pools.glow(toScreen(w), look_->soft.wellPool.radius[0] * pxPerWu_, h * (look_->soft.wellPool.intensity[0] * crowd));
            d.sparkles.glow(toScreen(w), look_->soft.wellSeed.radius[0] * refPx_, W1 * look_->soft.wellSeed.intensity[0]);
            glyph(d.glyphs, gRing64_, false, w, -0.8f * t, 0.0f, 60.0f, look_->lines.wellRing.intensity * crowd,
                  0.5f * look_->lines.wellRing.width, kGlyphFlat, h, h);
        }
        if (params.wellCount < 4 && visible(w, 200.0f) && shown(kMuteDistortion)) {
            DistortionWell& dw = params.wells[params.wellCount++];
            const Vec2 sp = toScreen(w);
            dw.x = sp.x;
            dw.y = sp.y;
            dw.peakRadius = look_->distortion.well.peakAtWu * pxPerWu_;
            dw.goneRadius = look_->distortion.well.goneByWu * pxPerWu_;
            dw.amount = look_->distortion.well.refPx * refPx_;
        }
    }
    params.ringCount = 0;
    for (u32 i = 0; i < ringCap_; ++i) {
        const DistortRing& r = rings_[i];
        const float age = song - r.born;
        if (age < 0.0f || age >= r.life || params.ringCount >= 8 || !shown(kMuteDistortion)) continue;
        const float u = age / r.life;
        DistortionRing& o = params.rings[params.ringCount++];
        const Vec2 sp = toScreen(r.at);
        o.x = sp.x;
        o.y = sp.y;
        o.radius = r.toWu * u * pxPerWu_;
        o.width = r.widthRefPx * refPx_;
        o.amount = r.amountRefPx * refPx_ * (1.0f - u);
    }
    // The Drop's white front: a ring from the ship to 1400 wu in 0.35 s.
    if (endgame && fxShown) {
        const float age = song - dropT_;
        if (age >= 0.0f && age < 0.35f) {
            const float u = age / 0.35f;
            glyph(d.glyphs, gRing128_, false, shipAt(t), 0.0f, 0.0f, 1400.0f * u, 3.0f * (1.0f - u), 3.0f, kGlyphFlat, W1, W1);
        }
    }

    // ---- Composite controls (look.md §7).
    const float dropAge = song - dropT_;
    // Reduce-flashing mode removes the Drop burst and halves every other beat amount (look.md §7).
    const float dropBloom = (endgame && dropAge >= 0.0f && !reduceFlashing_)
                                ? look_->dropBurstBloom * std::exp(-dropAge * 1000.0f / look_->dropBurstDecayMs)
                                : 0.0f;
    const float pump = shown(kMutePump) ? kick_ * beatAmt : 0.0f;
    params.bloomPump = (1.0f + look_->bloomKick * pump) * (1.0f + dropBloom);
    params.exposurePump = 1.0f + look_->exposureKick * pump;
    params.vignette = shown(kMuteVignette);
    params.vignetteAmount = look_->vignetteAmount + (look_->vignetteBuild - look_->vignetteAmount) * buildAmount_;
    params.drain = 0.4f * buildAmount_;
    params.afterglow = shown(kMuteAfterglow);
    // look.md v2.2 §3, §7: the afterglow's strength follows the light budget.
    params.afterglowGain = look_->afterglowGain == Look::Gain::Swarm
                               ? swarm
                               : (look_->afterglowGain == Look::Gain::Crowd ? crowd : 1.0f);
    params.dtSeconds = lastDt_;
    params.aberration = shown(kMuteAberration);
    params.aberrationSnarePx = look_->aberrationSnareRefPx * beatAmt * refPx_ * snare_;
    params.world = {pxPerWu_, centre_.x + shake_.x - cam_.x * pxPerWu_, centre_.y + shake_.y - cam_.y * pxPerWu_};
    params.worldThreat = {pxPerWu_, centre_.x - cam_.x * pxPerWu_, centre_.y - cam_.y * pxPerWu_};
    params.accent = accent;
    params.enemySwell = 1.0f + look_->enemy.kickSwell * pump;
    params.particleTime = t;
    params.particleMaxLife = 2.0f;
    params.dimAt = S;
    params.dimRadius = 120.0f;
    params.dimFactor = 0.5f;
    // Afterglow re-aim: the previous frame's logical pixel p is at p x scale + shift now.
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
    stats_.glyphInstances = d.glyphs.count();
    // The next frame's light budget reads this frame's load (inside the camera rectangle).
    stats_.load = static_cast<float>(stats_.enemiesInView) + 0.25f * static_cast<float>(stats_.bulletsInView);
    (void)H;
}

}  // namespace rf
