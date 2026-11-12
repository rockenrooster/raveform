#pragma once
// Draws the simulation (M3) with the look of design/look.md v2.2: the renderer reads the world's pools
// (previous and newest tick) and interpolates; the local ship is drawn where its controller holds it now
// (TECH_SPEC 4). Everything here is cosmetic and outside the simulation: the camera, the lattice, the
// light budget, the kill recipes, rings, pools, embers, shatter, the Nova's inhale and front, Dash ghosts,
// telegraphs, edge indicators and the beat pumps. Its random numbers come from its own generator, so it
// never touches the simulation's determinism. All storage is allocated by init(); a frame allocates
// nothing.

#include "content/glyphs.h"
#include "content/look.h"
#include "content/palette.h"
#include "game/lattice.h"
#include "game/rng.h"
#include "game/tiers.h"
#include "gfx/renderer.h"
#include "sim/world.h"

namespace rf {

struct BeatView;
class IconSet;
class VectorFont;

struct ViewInput {
    const sim::World* world = nullptr;
    float alpha = 1.0f;        // between the previous tick (0) and the newest (1)
    double time = 0.0;         // the rendered sim time (seconds): effects and sparks run on it
    float dt = 1.0f / 60.0f;   // frame time
    double music = 0.0;        // the rendered musical position (ticks, 96 per beat)
    float bpm = 120.0f;
    const BeatView* beat = nullptr;  // heard envelopes (null: derived from the musical position)
    // The local ship as its controller holds it now.
    Vec2 shipPos, shipVel, shipAim{1.0f, 0.0f};
    float shipHeading = 0.0f;
    bool shipFiring = false;
    // M4 pictures (--net-view-seat): the camera on this point (no look-ahead) instead of the local ship.
    bool camFixed = false;
    Vec2 camPos;
};

struct ViewStats {
    u32 enemiesInView = 0, bulletsInView = 0, glyphs = 0, sparksBorn = 0, sparksDropped = 0, indicators = 0;
    float load = 0.0f, crowdGain = 1.0f, swarmGain = 1.0f, zoom = 1.0f;
};

class PlayView {
public:
    PlayView() = default;
    PlayView(const PlayView&) = delete;
    PlayView& operator=(const PlayView&) = delete;
    ~PlayView();

    bool init(const Look& look, const Palettes& palettes, const Palette& palette, const GlyphSet& glyphs,
              const TierSettings& tier);
    bool setTier(const TierSettings& tier);
    void setDensity(float governorDensity) { density_ = governorDensity; }
    void setReduceFlashing(bool on) { reduceFlashing_ = on; }
    void setPalette(const Palette& p) { palette_ = &p; }
    void setMute(u32 bits) { mute_ = bits; }
    // M3 stage B: the upgrade icons and the font for the Trio's words.
    void setIcons(const IconSet* icons, const i32* upgradeIcons, const i32* boostIcons);
    void setFont(const VectorFont* font) { font_ = font; }
    float trioRest() const { return trioRest_; }
    void resetEffects();
    // The effects of one tick's events; `tickEnd` is the sim time at the end of that tick.
    void onEvents(const sim::World& w, double tickEnd);
    void render(FrameDraw& d, FrameParams& params, Background& bg, float W, float H, const ViewInput& in);
    const ViewStats& stats() const { return stats_; }

    // The camera of the last rendered frame (logical pixels).
    Vec2 toScreen(Vec2 w) const { return (w - cam_) * pxPerWu_ + centre_; }
    Vec2 toWorld(Vec2 p) const { return (p - centre_) * (1.0f / pxPerWu_) + cam_; }
    float pxPerWu() const { return pxPerWu_; }
    float chainPulse(double t) const;  // HUD: 1 on a pickup, decaying
    float kick() const { return kick_; }
    float snare() const { return snare_; }

private:
    struct Pool;
    struct Ember;
    struct Shatter;
    struct DistortRing;
    struct SparkReq {
        Vec2 at, dir;
        u16 count;
        u8 kind;  // 0 small burst, 1 large, 3 impact, 5 medium, 6 nova inhale, 7 nova front, 8 player death
        Vec3 a, b;
        float t0;  // birth time (view clock)
        float extra;  // inhale: seconds until the blast
    };

    // Effects.
    void killRecipe(Vec2 at, u8 size, u8 hue, float heading, int glyph, float radius, float t, i32 owner = -1);
    void addPool(Vec2 at, float r0, float r1, Vec3 colour, float i0, float i1, float life, bool kill, u8 fade, bool sparkle,
                 bool dimmed, float t);
    void addShockRings(Vec2 at, u32 count, float toWu, Vec3 a, Vec3 b, float t, float lifeS = 0.0f);
    void addDistortion(Vec2 at, float toWu, float lifeS, float amountRefPx, float widthRefPx, float t);
    void sparkRequest(const SparkReq& r);
    void shatterGlyph(int glyph, Vec2 at, float heading, float scale, Vec3 colour, float speedLo, float speedHi, float t);
    // Drawing helpers (world -> logical px; the scene view shakes, the threat view does not).
    Vec2 toScene(Vec2 w) const { return (w - cam_) * pxPerWu_ + centre_ + shake_; }
    bool visible(Vec2 w, float marginWu) const;
    void glyph(GlyphBatch& b, int g, bool hullOnly, Vec2 pos, float rot0, float rot1, float scaleWu, float intensity,
               float halfWidthRef, GlyphMode mode, Vec3 a, Vec3 c);
    void flare(CapsuleBatch& out, Vec2 at, float angle, float lengthWu, u32 arms, Vec3 colour, float halfWidthRef);
    void telegraphLine(FrameDraw& d, Vec2 a, Vec2 b, float sceneLight);
    void envelopes(const ViewInput& in);
    bool shown(u32 bit) const { return (mute_ & bit) == 0; }
    // M3 stage B (play_view_b.cpp).
    void sectionLook(const ViewInput& in);
    void eventB(const sim::World& w, const sim::Event& e, float t);
    void drawStageB(FrameDraw& d, const ViewInput& in, float W, float H);
    void drawBoss(FrameDraw& d, const ViewInput& in);
    // Edge indicator (look.md §6, M3 stage C): a chevron inside the screen edge pointing at world, on a dark
    // disc, in the threat layer; oss: the larger double chevron, scaled by swell.
    void edgeChevron(FrameDraw& d, Vec2 world, Vec3 colour, bool boss, float swell, bool ringTip = false);
    float beatAmount() const;

    const Look* look_ = nullptr;
    const Palettes* palettes_ = nullptr;
    const Palette* palette_ = nullptr;
    const GlyphSet* glyphs_ = nullptr;
    TierSettings tier_;
    float density_ = 1.0f;
    bool reduceFlashing_ = false;
    u32 mute_ = 0;
    Rng rng_{99};
    ViewStats stats_;

    // Glyphs.
    int gShip_ = -1, gHalo_[7] = {-1, -1, -1, -1, -1, -1, -1}, gShard_ = -1, gBullet_ = -1;
    int gMarkBHeading_ = -1, gMarkBSpin_ = -1, gMarkC_ = -1, gRing32_ = -1, gRing64_ = -1, gRing128_ = -1;
    int speciesGlyph_[sim::SpeciesTable::kCount];
    bool speciesGlyphsReady_ = false;

    // Camera.
    Vec2 cam_, lookAhead_, bossLean_, centre_, shake_;
    Vec3 laneColour_[5];       // Prism's rainbow, lanes -2..2 (computed once from the player's colour)
    bool laneReady_ = false;
    float zoom_ = 1.0f, zoomBase_ = 1.0f, pxPerWu_ = 1.5f, refPx_ = 1.0f, viewW_ = 1600.0f, viewH_ = 720.0f;
    float shakeAmp_ = 0.0f;
    bool hasPrevView_ = false;
    float prevScale_ = 1.0f;
    Vec2 prevOffset_;
    float crowdGain_ = 1.0f, swarmGain_ = 1.0f;

    // Beat envelopes (look.md §7).
    float kick_ = 0.0f, snare_ = 0.0f, hat_ = 0.0f, bass_ = 0.0f;
    double lastKickT_ = -10.0, prevKickT_ = -10.0, lastHatT_ = -10.0;  // view clock

    // Effect pools (allocated by init).
    Pool* pools_ = nullptr;
    u32 poolCap_ = 0, poolHead_ = 0;
    Ember* embers_ = nullptr;
    u32 emberCap_ = 0, emberHead_ = 0;
    Shatter* shatter_ = nullptr;
    u32 shatterCap_ = 0, shatterHead_ = 0;
    DistortRing* rings_ = nullptr;
    u32 ringCap_ = 0, ringHead_ = 0;
    struct ShockRing {
        Vec2 at;
        float start = -100.0f, toWu = 0.0f, life = 0.2f;
        Vec3 a, b;
    };
    static constexpr u32 kMaxShockRings = 16;
    ShockRing shock_[kMaxShockRings];
    SparkReq* sparkReqs_ = nullptr;
    u32 sparkReqCount_ = 0, sparkReqCap_ = 0;
    // The ship's trail: recent positions on the view clock.
    static constexpr u32 kTrail = 48;
    Vec2 trailPos_[kTrail];
    float trailT_[kTrail] = {};
    u32 trailHead_ = 0, trailCount_ = 0;
    // Dash ghosts and the Perfect Dash ring (M4: each player's own, in its colours).
    struct Dash {
        Vec2 from, to;
        float t = -100.0f;
        float heading = 0.0f;
        bool perfect = false;
    } dash_[sim::kMaxPlayers];
    // The Nova: inhale from press to blast, then the front (each player's own).
    float novaPressT_[sim::kMaxPlayers] = {-100.0f, -100.0f, -100.0f, -100.0f};
    float novaBlastT_[sim::kMaxPlayers] = {-100.0f, -100.0f, -100.0f, -100.0f};
    float novaFrontT_[sim::kMaxPlayers] = {-100.0f, -100.0f, -100.0f, -100.0f};
    Vec2 novaAt_[sim::kMaxPlayers];
    // M4 co-op (look.md 6): teammates' trails, the revive's close, all down.
    Vec2 mateTrailPos_[sim::kMaxPlayers][kTrail];
    float mateTrailT_[sim::kMaxPlayers][kTrail] = {};
    u32 mateTrailHead_[sim::kMaxPlayers] = {}, mateTrailCount_[sim::kMaxPlayers] = {};
    float reviveT_[sim::kMaxPlayers] = {-100.0f, -100.0f, -100.0f, -100.0f};  // the ring's close (view clock)
    bool reviveFill_[sim::kMaxPlayers] = {};  // closed by a teammate (else at a Break: no fill)
    float allDownT_ = -100.0f, allDownRest_ = 0.0f;
    // M4 stage B: a player that left fades where its ship was.
    float leaveT_[sim::kMaxPlayers] = {-100.0f, -100.0f, -100.0f, -100.0f};
    Vec2 leavePos_[sim::kMaxPlayers];
    float leaveHeading_[sim::kMaxPlayers] = {};
    void drawCoop(FrameDraw& d, const ViewInput& in);
    float novaPump_ = 0.0f;  // bloom +60% for 120 ms (look.md §7)
    float hitT_ = -100.0f;   // the last hit taken (panic ring, cell shatter)
    float pickupT_ = -100.0f;
    float lastWarpLandT_ = -100.0f;
    double now_ = 0.0;

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

    Lattice lattice_;

    // M3 stage B.
    const IconSet* icons_ = nullptr;
    const VectorFont* font_ = nullptr;
    i32 upgradeIcon_[sim::kUpgradeCount] = {};
    i32 boostIcon_[sim::kBoostCount] = {};
    float enemyLight_ = 1.0f, trioRest_ = 0.0f, drain_ = 0.0f, sectionZoom_ = 1.0f, exposureSection_ = 1.0f;
    struct Tumble {
        Vec2 p, v;
        float ang = 0.0f, spin = 0.0f, born = -100.0f;
        u8 facet = 0, pair = 0;
    };
    static constexpr u32 kTumbles = 24;
    Tumble tumble_[kTumbles];
    u32 tumbleCount_ = 0;
};

}  // namespace rf
