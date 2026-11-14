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
    // M4 stage B: the lobby draws no ship (the run has not begun; screens.md's lobby shows the ships in the seats).
    bool noShips = false;
    // M4 stage B (screens.md "Game over"): the co-op game over draws no beacon and no beacon flare.
    bool noBeacons = false;
    // M4 stage B (screens.md "Co-op (M4)"): the co-op setup screens: the lattice and the background only (what the run
    // left behind, its sparks and rings, is not drawn either; the world behind them stands still).
    bool backgroundOnly = false;
};

// M4 stage B (screens.md "Co-op (M4)", "In the run"): the HUD's blocks that edge chevrons keep 20 px clear of (screen
// pixels, top left origin): the score block, the phrase bar, the Nova stock, the pause ring.
struct HudKeepOut {
    static constexpr u32 kMax = 6;
    Vec2 a[kMax], b[kMax];
    u32 count = 0;
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
    // M4 stage B: the HUD's blocks this frame (edge chevrons keep clear of them).
    void setHudKeepOut(const HudKeepOut& k) { keepOut_ = k; }
    // M3 stage B: the upgrade icons and the font for the Trio's words.
    void setIcons(const IconSet* icons, const i32* upgradeIcons, const i32* boostIcons);
    void setFont(const VectorFont* font) { font_ = font; }
    float trioRest() const { return trioRest_; }
    void resetEffects();
    // The effects of one tick's events; `tickEnd` is the sim time at the end of that tick.
    void onEvents(const sim::World& w, double tickEnd);
    void render(FrameDraw& d, FrameParams& params, Background& bg, float W, float H, const ViewInput& in);
    const ViewStats& stats() const { return stats_; }
    // M4 stage B (the lead's question on frame_client): what pushed the lattice in the last second, by source, inside
    // this frame (impulses are what heats it), with the crowd gain and the kick ripples now. One line of text.
    enum ImpulseKind : u8 { kImpKill, kImpHit, kImpShipDown, kImpDeath, kImpNova, kImpDash, kImpDropWave, kImpBoss, kImpKinds };
    void latticeTrace(char* out, u32 capacity) const;

    // The camera of the last rendered frame (logical pixels).
    Vec2 toScreen(Vec2 w) const { return (w - cam_) * pxPerWu_ + centre_; }
    Vec2 toWorld(Vec2 p) const { return (p - centre_) * (1.0f / pxPerWu_) + cam_; }
    float pxPerWu() const { return pxPerWu_; }
    float chainPulse(double t) const;  // HUD: 1 on a pickup, decaying
    float kick() const { return kick_; }
    float beat() const { return beat_; }  // the clock's beat envelope (M5: "on the beat"; the kick is not every beat)
    float snare() const { return snare_; }

private:
    HudKeepOut keepOut_;
    struct Pool;
    struct Ember;
    struct Shatter;
    struct DistortRing;
    struct SparkReq {
        Vec2 at, dir;
        u16 count;
        u8 kind;  // 0 small burst, 1 large, 3 impact, 5 medium, 6 nova inhale, 7 nova front, 8 player death, 9 thrown sideways
        Vec3 a, b;
        float t0;  // birth time (view clock)
        float extra;  // inhale: seconds until the blast
        u8 prio = 0;  // play test 3 (GAME_DESIGN 7.6): a boss hit's sparks, never the ones dropped when the pool is full
    };

    // Effects.
    void killRecipe(Vec2 at, u8 size, u8 hue, float heading, int glyph, float radius, float t, i32 owner = -1);
    void addPool(Vec2 at, float r0, float r1, Vec3 colour, float i0, float i1, float life, bool kill, u8 fade, bool sparkle,
                 bool dimmed, float t);
    void addShockRings(Vec2 at, u32 count, float toWu, Vec3 a, Vec3 b, float t, float lifeS = 0.0f);
    void addDistortion(Vec2 at, float toWu, float lifeS, float amountRefPx, float widthRefPx, float t);
    void sparkRequest(const SparkReq& r);
    void shatterGlyph(int glyph, Vec2 at, float heading, float scale, Vec3 colour, float speedLo, float speedHi, float t);
    // The halo's arcs [from, to) of an n-arc halo fly apart (a lost cell, the loop's burn; look.md 5): the halo glyphs are all
    // ornament (layer 1), which shatterGlyph's hull pass skips.
    void shatterArcs(Vec2 at, u32 slots, u32 from, u32 to, Vec3 colour, float t);
    // Drawing helpers (world -> logical px; the scene view shakes, the threat view does not).
    Vec2 toScene(Vec2 w) const { return (w - cam_) * pxPerWu_ + centre_ + shake_; }
    bool visible(Vec2 w, float marginWu) const;
    void glyph(GlyphBatch& b, int g, bool hullOnly, Vec2 pos, float rot0, float rot1, float scaleWu, float intensity,
               float halfWidthRef, GlyphMode mode, Vec3 a, Vec3 c);
    void flare(CapsuleBatch& out, Vec2 at, float angle, float lengthWu, u32 arms, Vec3 colour, float halfWidthRef);
    // (widthWu > 0: the dashes that warn of a beam, as wide as it kills: look.md v2.5)
    void telegraphLine(FrameDraw& d, Vec2 a, Vec2 b, float sceneLight, float widthWu = 0.0f);
    // look.md v2.5, a lethal beam is a laser too: the priority capsule exactly as wide as it kills, a 2 px white-pink line on
    // its axis, and round it a danger glow (0.25 on the axis, sigma twice the kill width) that hurts nobody.
    void lethalBeam(FrameDraw& d, Vec2 a, Vec2 b, float killWu, float level = 1.0f);
    // M5 part 5 (play_view_arena.cpp): the arena's structures and events, and the elites' extras (look.md 3, 4).
    void drawArena(FrameDraw& d, const ViewInput& in);
    void drawCadence(FrameDraw& d, const ViewInput& in);  // M5 part 6 (play_view_m6.cpp)
    void drawArpeggiator(FrameDraw& d, const ViewInput& in);
    void drawMorgana(FrameDraw& d, const ViewInput& in);
    void drawReprise(FrameDraw& d, const ViewInput& in);
    void drawElite(FrameDraw& d, const ViewInput& in, u32 i, Vec2 pos, float rot0, float rot1, float scale, Vec3 hue, int g, float level);
    void eventArena(const sim::World& w, const sim::Event& e, float t);
    void arenaShape(FrameDraw& d, const sim::ArenaGlyph& g, Vec2 pos, float rot, float scale, Vec3 c0, Vec3 c1, Vec3 c2, float widthRef, u8 layers);
    struct Strike {
        Vec2 a, b;
        float born = -100.0f;
    };
    static constexpr u32 kStrikes = 16;
    Strike strikes_[kStrikes];
    u32 strikeHead_ = 0;
    Vec2 lastStrike_;
    float lastStrikeT_ = -100.0f;
    float eclipseK_ = 1.0f;  // the Eclipse's scene light (1 outside one)
    void envelopes(const ViewInput& in);
    bool shown(u32 bit) const { return ((mute_ | frameMute_) & bit) == 0; }
    u32 frameMute_ = 0;  // this frame's extra mute (ViewInput::backgroundOnly)
    // M3 stage B (play_view_b.cpp).
    void sectionLook(const ViewInput& in);
    void eventB(const sim::World& w, const sim::Event& e, float t);
    void drawStageB(FrameDraw& d, const ViewInput& in, float W, float H);
    void drawBoss(FrameDraw& d, const ViewInput& in);
    // M5 part 1 (play_view_m5.cpp): Longwave, the Choir, the mini-bosses and the Pen, and their events.
    void drawFight(FrameDraw& d, const ViewInput& in);
    void drawLongwave(FrameDraw& d, const ViewInput& in);
    void drawChoir(FrameDraw& d, const ViewInput& in);
    void drawMini(FrameDraw& d, const ViewInput& in);
    void drawPen(FrameDraw& d, const ViewInput& in);
    // M5 part 4 (play_view_m5b.cpp): Event Horizon, Polyrhythm, Feedback and mini-bosses batch 2.
    void drawHorizon(FrameDraw& d, const ViewInput& in);
    void drawPoly(FrameDraw& d, const ViewInput& in);
    void drawFeedback(FrameDraw& d, const ViewInput& in);
    const Palette* fbHueFor_ = nullptr;
    Vec3 fbHue_;                   // the Head's electric blue
    void fightBreak(Vec2 at, float t);  // a boss break played late (Event Horizon's flash, Polyrhythm's break on the One)
    void polyWalls(FrameDraw& d, const ViewInput& in);
    const Palette* polyHueFor_ = nullptr;
    Vec3 polyHue_[2];              // Three (the palette's cyan, or its nearest), Four (its amber)
    double polySpark_ = -1.0;      // the last twelfth the dying playhead fired its sparks on
    double polyBurst_ = -1.0;      // the first dead rotor's burst (on the One after it died)
    double hzBeat_ = -1.0;      // the last beat the lattice was pulled into the Well
    float hzEatAt_ = -1e9f;     // the last body eaten (view time): its cell flashes white
    void eventFight(const sim::World& w, const sim::Event& e, float t);
    void fightChevron(FrameDraw& d, const ViewInput& in, Vec2 at, Vec3 colour);
    // Play test 3 (GAME_DESIGN 7.6, "Every hit on a boss is seen and heard"): a weak point's hit flashes its part white for
    // 60 ms and throws six sparks in its hue back along the shot; armour three grey sparks sideways and no flash.
    void bossHit(const sim::World& w, const sim::Event& e, float t, Vec3 hue);
    void drawBossHitFlash(FrameDraw& d, const ViewInput& in);
    struct HitFlash {
        u8 part = 0, index = 0;
        float t = -1e9f;
    };
    static constexpr u32 kHitFlashes = 8;
    HitFlash hitFlash_[kHitFlashes];
    u32 hitFlashNext_ = 0;
    // Play test 3 (GAME_DESIGN 5, screens.md): EXTRA LIFE as world text at the ship for a bar (view time it began).
    float extraLifeT_[sim::kMaxPlayers] = {-1e9f, -1e9f, -1e9f, -1e9f};
    // Play test 3 (GAME_DESIGN 6.3 row 1): the Orbitals' ring breaking (its shards fly) and relighting.
    void ringBreak(const sim::World& w, u32 p, Vec2 at, float t);
    // Lines a fight has shown (a pack's charge, the Hive's spear), each until its moment.
    struct FightLine {
        Vec2 from, dir;
        double until = -1.0;
        float length = 0.0f;
    };
    static constexpr u32 kFightLines = 16;
    FightLine fightLines_[kFightLines];
    u32 fightLineNext_ = 0;
    // The Choir's eight hues (choir.md: 45 degrees apart from the Track's first enemy hue, clear of the danger band), for the
    // palette they were made from; the death's shown parts (Longwave's segments unravelled, the Choir's hoods shattered).
    Vec3 choirHue_[8];
    const Palette* choirHueFor_ = nullptr;
    u32 deathShown_ = 0;
    double deathSeenTick_ = -1.0;
    // Edge indicator (look.md §6, M3 stage C): a chevron inside the screen edge pointing at world, on a dark
    // disc, in the threat layer; oss: the larger double chevron, scaled by swell.
    // tip: 1 a small ring at its tip (a downed teammate, M4), 2 a plus (a Cell while its player is on the last cell, play
    // test 2).
    void edgeChevron(FrameDraw& d, Vec2 world, Vec3 colour, bool boss, float swell, u8 tip = 0);
    // Where an edge chevron's disc of radius r stands for the direction `dsc` from the screen's centre: on the edge
    // box, slid along the edge to the nearest place 20 px clear of the HUD's blocks (screens.md).
    Vec2 edgePlace(Vec2 dsc, float hx, float hy, float r) const;
    // Play test 2 (look.md 5, "The build on the shots"): the shots, their heads by the build (play_view_shots.cpp); the
    // colours it needs (Prism's lanes from the local player's colour, Overcharge's heads), computed once.
    void drawShots(FrameDraw& d, const ViewInput& in, float a);
    // M5 part 3 (play_view_upgrades.cpp): the new upgrades on screen, and their events; lightning's jumps.
    void drawUpgrades(FrameDraw& d, const ViewInput& in);
    void eventUpgrade(const sim::World& w, const sim::Event& e, float t);
    struct Bolt {
        Vec2 a, b;
        float born = -100.0f;
        u8 owner = 0, style = 0;
    };
    static constexpr u32 kBolts = 96;
    Bolt bolts_[kBolts];
    u32 boltHead_ = 0;
    void buildColours(Vec3 player);
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
    int gShip_ = -1, gHalo_[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1}, gShard_ = -1, gBullet_ = -1;
    int gHaloPart_[9][9] = {};  // [k][n]: the first k arcs of an n-arc halo (play test 2)
    int gMarkBHeading_ = -1, gMarkBSpin_ = -1, gMarkC_ = -1, gRing32_ = -1, gRing64_ = -1, gRing128_ = -1;
    int speciesGlyph_[sim::SpeciesTable::kCount];
    int wyrmHeadGlyph_[sim::SpeciesTable::kCount] = {};  // M5 part 4
    void drawHazards(FrameDraw& d, const ViewInput& in);  // M5 part 4: Tether beams, trails, Wells (play_view_upgrades.cpp)
    void drawBeam(FrameDraw& d, Vec2 from, Vec2 dir, float len, float w, float corePx, Vec3 noseCol, Vec3 farCol, Vec3 glowCol,
                  float coreLevel, float glowLevel);  // look.md v2.5: a beam is a laser, one thin line and its glow
    bool speciesGlyphsReady_ = false;

    // Camera.
    Vec2 cam_, lookAhead_, bossLean_, centre_, shake_;
    Vec3 laneColour_[5];       // Prism's rainbow, lanes -2..2 (computed once from the player's colour)
    bool laneReady_ = false;
    Vec3 overHead_[6];         // play test 2: the shot head by Overcharge stacks 0..5 (look.md 5)
    Vec3 buildGold_;           // ... the gold of stack 2 (stack 3's rim, the crit star's gold)
    bool buildReady_ = false;
    u32 shotSparkTurn_[sim::kMaxPlayers] = {};  // Overcharge 3: a spark off every second shot
    float cellBackT_ = -100.0f;  // play test 2: the local player's cell came back (view clock) ...
    u8 cellBackTo_ = 0;          // ... to this many
    float zoom_ = 1.0f, zoomBase_ = 1.0f, pxPerWu_ = 1.5f, refPx_ = 1.0f, viewW_ = 1600.0f, viewH_ = 720.0f;
    float shakeAmp_ = 0.0f;
    bool hasPrevView_ = false;
    float prevScale_ = 1.0f;
    Vec2 prevOffset_;
    float crowdGain_ = 1.0f, swarmGain_ = 1.0f;
    // The lattice's impulses of the last moments (the trace above): when, where, how hard, from what.
    struct ImpulseNote {
        float t = -100.0f, amp = 0.0f, radius = 0.0f;
        Vec2 at;
        u8 kind = 0;
    };
    static constexpr u32 kImpulseNotes = 512;
    ImpulseNote impulses_[kImpulseNotes];
    u32 impulseHead_ = 0;
    float lastViewT_ = 0.0f, lastBeatAmt_ = 0.0f;
    void push(u8 kind, Vec2 at, float amp, float radius, float t);

    // Beat envelopes (look.md §7).
    float kick_ = 0.0f, snare_ = 0.0f, hat_ = 0.0f, bass_ = 0.0f, beat_ = 0.0f;
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
    i32 fusionIcon_[sim::kFusionCount] = {};
    // A Trio orb's icon: an upgrade's, a Boost's or (M5) a Fusion's.
    i32 orbIcon(u8 u) const {
        return sim::isBoostOrb(u) ? boostIcon_[sim::boostOfOrb(u)] : (sim::isFusionOrb(u) ? fusionIcon_[sim::fusionOfOrb(u)] : (u < sim::kUpgradeCount ? upgradeIcon_[u] : -1));
    }
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
