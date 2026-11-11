#pragma once
// Scripted stages for milestone M1 (design/scenes.md §2-§4): bench_mid and bench_endgame. There is no
// simulation yet: every formation is a function of the song clock, anchored to the ship, so the number
// of things in view holds while the world moves underneath. Kills, sparks, Shards, rings, pools,
// distortion and the Drop / boss / Build events follow design/look.md §5 and §7. Everything random
// comes from one generator seeded with 1. All storage is allocated by init(); the frame allocates
// nothing.

#include "content/glyphs.h"
#include "content/look.h"
#include "content/palette.h"
#include "game/lattice.h"
#include "game/rng.h"
#include "game/tiers.h"
#include "gfx/renderer.h"

namespace rf {

enum class StageKind : u8 { Mid, Endgame };

struct StageStats {
    u32 enemies = 0;        // every enemy slot alive (the endgame keeps the pool at its cap)
    u32 enemiesInView = 0;
    u32 bullets = 0;        // enemy bullets alive
    u32 bulletsInView = 0;
    u32 shards = 0;
    u32 shots = 0;
    u32 sparksBorn = 0;     // this frame
    u32 sparksDropped = 0;  // ring full (this frame)
    u32 glyphInstances = 0;
    u32 latticeSegments = 0;
    u32 kills = 0;          // since the start
    u32 drops = 0;
    u32 bossBreaks = 0;
    // Light budget (look.md v2.1 §3): load = enemies in view + bullets in view / 4 (inside the camera
    // rectangle), crowd gain followed with a time constant, swarm gain = crowd gain ^ 1.5.
    float load = 0.0f;
    float crowdGain = 1.0f;
    float swarmGain = 1.0f;
};

class Stage {
public:
    Stage() = default;
    Stage(const Stage&) = delete;
    Stage& operator=(const Stage&) = delete;
    ~Stage();

    bool init(StageKind kind, const Look& look, const Palettes& palettes, const Palette& palette, const GlyphSet& glyphs,
              const TierSettings& tier);
    void setPalette(const Palette& palette) { palette_ = &palette; }
    // Outside the measured window: may rebuild the lattice for the tier's spacing.
    bool setTier(const TierSettings& tier);
    void setDensity(float governorDensity) { governorDensity_ = governorDensity; }
    // Analysis pictures (--mute, core/args.h MuteBits): kinds of light that are not drawn.
    void setMute(u32 muteBits) { mute_ = muteBits; }

    void advance(float dt);
    // Emits one frame: primitives into `draw`, composite controls into `params` (look and bloom are
    // left to the caller), the background into `bg`. Logical size in pixels.
    void render(FrameDraw& draw, FrameParams& params, Background& bg, float widthPx, float heightPx);

    float time() const { return songT_; }
    const StageStats& stats() const { return stats_; }

private:
    struct Enemy;
    struct Bullet;
    struct Shot;
    struct Shard;
    struct Ember;
    struct Shatter;
    struct Pool;
    struct DistortRing;

    // Clock and envelopes (design/scenes.md §2, look.md §7).
    float beatLen() const { return 60.0f / 132.0f; }
    float barLen() const { return 4.0f * beatLen(); }
    void envelopes();
    Vec2 shipAt(float t) const;
    Vec2 shipVelocity(float t) const;

    // Formations.
    void buildMid();
    void buildEndgame();
    u32 addEnemy(u8 family, u8 tier, u8 hue, u16 group, u16 index);
    void place(Enemy& e, float t, Vec2 ship) const;
    void randomiseSlot(Enemy& e);
    float spearBlend(float t) const;
    Vec2 shoalCurve(u32 stream, float s, float t, Vec2* tangent) const;

    // Events.
    void onSixteenth(u32 step);
    void kill(Enemy& e, u32 size);  // 0 small, 1 medium, 2 large
    void sparks(Vec2 at, u32 count, float speedLo, float speedHi, float lifeLo, float lifeHi, Vec3 hue, Vec3 partner,
                float halfWidth);
    void spawnBullet(Vec2 at, Vec2 velocity, float life);
    void spawnShot(Vec2 from, Vec2 dir, float speed, float intensity, float width);
    void dropBurst();
    void bossBreak();
    void addDistortion(Vec2 at, float toWu, float lifeS, float amountRefPx, float widthRefPx);
    // fade: 0 linear from i0 to i1, 1 as the square of the time left (to nothing). sparkle: radius in ref
    // px, drawn at full resolution (else a pool, radius in wu). dimmed: scaled by the crowd gain.
    void addPool(Vec2 at, float r0, float r1, Vec3 colour, float i0, float i1, float life, bool kill, u8 fade = 0,
                 bool sparkle = false, bool dimmed = true);
    void addShockRings(Vec2 at, u32 count, float toWu, u8 hue);

    // Drawing helpers (logical px; the scene view shakes, the threat view does not).
    Vec2 toScreen(Vec2 w) const { return (w - cam_) * pxPerWu_ + centre_ + shake_; }
    Vec2 toThreat(Vec2 w) const { return (w - cam_) * pxPerWu_ + centre_; }
    bool visible(Vec2 w, float marginWu) const;
    void glyph(GlyphBatch& b, int glyph, bool hullOnly, Vec2 pos, float rot0, float rot1, float scaleWu, float intensity,
               float halfWidthRef, GlyphMode mode, Vec3 a, Vec3 c);
    void drawEnemy(FrameDraw& d, const Enemy& e);
    void flare(CapsuleBatch& out, Vec2 at, float angle, float lengthWu, u32 arms, Vec3 colour, float halfWidthRef);
    void lethalLine(FrameDraw& d, Vec2 a, Vec2 b);

    const Look* look_ = nullptr;
    const Palettes* palettes_ = nullptr;
    const Palette* palette_ = nullptr;
    const GlyphSet* glyphs_ = nullptr;
    TierSettings tier_;
    StageKind kind_ = StageKind::Mid;
    Rng rng_{1};
    float governorDensity_ = 1.0f;
    u32 mute_ = 0;
    bool shown(u32 muteBit) const { return (mute_ & muteBit) == 0; }

    float songT_ = 0.0f;   // the song clock (envelopes, events)
    float simT_ = 0.0f;    // stops during hit-stop (formations, bullets, sparks)
    float lastDt_ = 1.0f / 60.0f;
    u32 nextSixteenth_ = 0;
    float hitStopUntil_ = -1.0f;
    float kick_ = 0.0f, snare_ = 0.0f, hat_ = 0.0f, bass_ = 0.0f;
    float lastKickT_ = -10.0f, prevKickT_ = -10.0f, lastHatT_ = -10.0f;

    // Camera.
    Vec2 cam_;
    float zoom_ = 1.0f;
    float pxPerWu_ = 1.5f;
    Vec2 centre_;
    Vec2 shake_;
    float shakeAmp_ = 0.0f;  // ref px
    float refPx_ = 1.0f;
    float viewW_ = 1600.0f, viewH_ = 720.0f;  // world units at the current zoom
    bool hasPrevView_ = false;
    float prevScale_ = 1.0f;
    Vec2 prevOffset_;

    // Events.
    float dropT_ = -10.0f, bossT_ = -10.0f;
    bool accentSwapped_ = false;
    float buildAmount_ = 0.0f;  // 0..1 across the Build bar

    // Pools of things (allocated by init).
    Enemy* enemies_ = nullptr;
    u32 enemyCount_ = 0, enemyCap_ = 0;
    u32 farCount_ = 0;  // endgame: slots alive outside the view
    Vec2* farPos_ = nullptr;
    float* farParam_ = nullptr;  // radius, angle, angular speed per far slot (3 floats)
    Bullet* bullets_ = nullptr;
    u32 bulletCap_ = 0, bulletHead_ = 0;
    Shot* shots_ = nullptr;
    u32 shotCap_ = 0, shotHead_ = 0;
    Shard* shards_ = nullptr;
    u32 shardCap_ = 0, shardHead_ = 0;
    Ember* embers_ = nullptr;
    u32 emberCap_ = 0, emberHead_ = 0;
    Shatter* shatter_ = nullptr;
    u32 shatterCap_ = 0, shatterHead_ = 0;
    Pool* pools_ = nullptr;
    u32 poolCap_ = 0, poolHead_ = 0;
    DistortRing* rings_ = nullptr;
    u32 ringCap_ = 0, ringHead_ = 0;
    // Shock rings (look.md v2.1 §5, §6): medium and large kills, at most maxShockRings alive; a newer one
    // replaces the oldest.
    struct ShockRing {
        Vec2 at;
        float start = -100.0f;
        float toWu = 0.0f;
        u8 hue = 0;
    };
    static constexpr u32 kMaxShockRings = 16;
    ShockRing shock_[kMaxShockRings];
    float crowdGain_ = 1.0f;  // look.md v2.1 §3, followed
    float swarmGain_ = 1.0f;
    // Pending sparks (spawned when the frame's ring is available).
    struct SparkReq {
        Vec2 at;
        u16 count;
        u8 hue;
        u8 kind;  // 0 burst, 1 large (two bands), 2 boss, 3 impact, 4 confetti
        Vec2 dir;
    };
    SparkReq* sparkReqs_ = nullptr;
    u32 sparkReqCount_ = 0, sparkReqCap_ = 0;

    // Background.
    struct Star {
        Vec2 p;
        float radius, intensity;
        u8 colour;
    };
    Star* stars_ = nullptr;
    u32 starCount_ = 0;
    struct Cloud {
        Vec2 p;
        float radius, intensity;
        u8 colour;
    } clouds_[8];
    u32 cloudCount_ = 0;
    Vec2 cloudTile_, starTile_;

    // Glyph ids.
    int gShip_ = -1, gHalo3_ = -1, gShard_ = -1, gBullet_ = -1, gMarkBHeading_ = -1, gMarkBSpin_ = -1, gMarkC_ = -1;
    int gRing32_ = -1, gRing64_ = -1, gRing128_ = -1, gCarom3_ = -1;

    Lattice lattice_;
    StageStats stats_;
};

}  // namespace rf
