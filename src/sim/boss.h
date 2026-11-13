#pragma once
// The bosses and the mini-bosses (GAME_DESIGN 7.5, 7.6; design/bosses/*.md). Their numbers and shapes are
// embedded from design/bosses/*.json and parsed here, never retyped; the few numbers the pages give only in
// prose are quoted in sim/boss_prose.h with the page they come from. Everything a fight does is a function
// of song time and of which parts are alive; health, and the times parts died, are its only shared state
// (co-op: the host's). The world runs the fights (sim/world_boss*.cpp); the view draws them from the same
// data.
//
// M3: Kaleidos (Track 1). M5 part 1: Longwave (Track 2), The Choir (Track 3) and mini-bosses batch 1 (Alpha
// Hound, The Hive, Battering Ram, Carom King) with the Pen. In the loop the bosses come round in order.

#include "core/types.h"
#include "sim/sim_types.h"

namespace rf::sim {

// Which fight. The value is on the wire (co-op BossEnter) and in the world's hash.
enum class BossKind : u8 {
    Kaleidos = 0, Longwave, Choir,                       // bosses (one a Track, in set-list order)
    AlphaHound, Hive, BatteringRam, CaromKing,           // mini-bosses, batch 1
    MothQueen, BroodMother, BlinkStar, SentryArray,      // batch 2 (data only until M5 part 4)
    Count
};
constexpr u32 kBossKinds = static_cast<u32>(BossKind::Count);
inline bool isMiniBoss(BossKind k) { return k >= BossKind::AlphaHound; }
const char* bossName(BossKind k);  // the name card: "KALEIDOS", "LONGWAVE", "THE CHOIR", "ALPHA HOUND", ...
const char* bossId(BossKind k);    // "kaleidos", "longwave", "choir", "alpha_hound", ... (arguments, pictures, logs)
bool bossFromId(const char* id, BossKind& out);
// The bosses built so far (M5 part 1: three), in set-list order: Track t's boss in the first pass, and the loop's
// boss for Track t is the one its song's Track had.
constexpr u32 kBossesBuilt = 3;
inline BossKind bossOfSong(u32 song) { return static_cast<BossKind>(song % kBossesBuilt); }

// The fights' sounds (the pages' "Sound" sections), as the simulation calls them: a BossCue event names one, with the
// musical tick it belongs to, the Voice or wave place (b) and a pitch (f: semitones above the chord root, or a MIDI note
// where the page names notes). The game plays each through the band (game/play.cpp bossCue).
enum class BossCue : u8 {
    // Longwave: the run (a crest step), a trough's tick, the jaw's horn (beat 1), the lunge, the whip.
    LwRun, LwTrough, LwJaw, LwLunge, LwWhip,
    // The Choir: a Voice sings (f = MIDI), a Voice dies (its note falls a fourth), the cage's tick and burn, the held note
    // (b = 0 it begins, 1 it rises as the beam fires).
    ChVoice, ChDeath, ChCageTick, ChCageBurn, ChHeld,
    // Alpha Hound: the howl, the pack's charge (a snare roll into beat 3), the lunge (the sub drop).
    AlHowl, AlCharge, AlLunge,
    // The Hive: a change of shape (a reverse cymbal into the bar line), the Ring's hats, the Spear's toms, the Wall's sweep.
    HvShape, HvRing, HvSpear, HvWall,
    // Battering Ram: the lock (a tom roll), the charge, the wall (the gong), stunned (the bed low-passed, b = 1 on, 0 off).
    RmLock, RmCharge, RmWall, RmStun,
    // Carom King: a bounce (a rim shot, f = the wall's chord note: 0 root, 4 third, 7 fifth, 10 seventh; b = 1 a Prince).
    CkBounce,
    // Every mini-boss: it arrives (its name card), it dies, it warps out.
    MiniEnter, MiniDeath, MiniGone,
    Count
};

// ---------------------------------------------------------------------------------------------- Kaleidos

struct BossData {
    // Shapes (boss space, world units; arm-local segments [x0, y0, x1, y1, layer], x outward).
    struct Facet {
        float armRadius = 0, length = 0, halfWidth = 0;
        float seg[8][5] = {};
        u32 segs = 0;
    };
    Facet facets[4];
    Facet tip;
    float lensRadius = 66, lensHalfAngleDeg = 26, lensInner = 59, lensWidthPx = 3;
    float coreRingRadius[3] = {}, coreRingPhaseDeg[3] = {};
    u32 coreRingSides[3] = {};
    u32 irisShutters = 6;
    float irisClosed = 4, irisOpen = 13;
    float healthRingRadius = 52;
    float reflectScale[2] = {}, reflectTurnDeg[2] = {}, reflectIntensity[2] = {};
    float accentL = 0.83f, accentHue = 76.0f, accentChroma = 0.96f;
    // Numbers.
    float radius = 270, contactCore = 46;
    float pairHealth = 400, coreHealth = 600;
    float dmgFiring = 1.0f / 3.0f, dmgResting = 3, dmgLens = 0, dmgCoreExposed = 1, dmgCoreDownbeat = 3;
    float driftSpeed = 30, keepNear = 450, keepFar = 650;
    float bulletSpeed = 200, bulletLife = 5;
    struct Phase {
        u32 pairsAlive = 3;
        float turnBars = 8;
        float direction = 1;
        double fireStepTicks = 0;  // 0: the tips do not fire
        u32 fan = 0;
        float spreadDeg = 0;
        u32 coreBurst = 0;         // ring size (Spiral: on beat 1; Naked core: every beat)
        float beamTelegraphBeats = 0, beamFireBeats = 0, beamWidth = 0;
    };
    Phase phases[4];
    float bpm = 120;

    bool load();  // false (logged) when the data the fight needs is missing
};

// A part-local line [x0, y0, x1, y1, layer] (0 hull, 1 inner mark, 2 core line).
struct BossLine {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    u8 layer = 0;
};

// ---------------------------------------------------------------------------------------------- Longwave

struct LongwaveData {
    static constexpr u32 kMaxLines = 16;
    // Shapes (part-local, x forward along the body).
    BossLine segLines[kMaxLines];
    u32 segLineCount = 0;
    float finClosed = 8, finCrest = 30;
    BossLine jawClosed[kMaxLines], jawOpen[kMaxLines];
    u32 jawCount = 0;
    BossLine crown[4];
    u32 crownCount = 0;
    float eyes[2][2] = {};
    float seedX = 34, seedRadius = 9;
    float tailBlades[3][4] = {};
    float spineWidthPx = 3.4f, spineCoreWidthPx = 1.2f;
    // Numbers.
    u32 segments = 24;
    float spacing = 100, segRadius = 34, headRadius = 70;
    float segHealth = 60, headHealth = 600;
    float dmgCrest = 1, dmgElsewhere = 0, dmgJaw = 1, dmgJawBare = 3, dmgJawClosed = 0;
    float amplitude = 150;
    u32 wavelength = 8, taper = 3;
    float pathHalfW = 1500, pathHalfH = 520;
    float zoom = 0.75f;
    float bulletSpeed = 220, bulletLife = 5;
    struct Phase {
        u32 segHi = 24, segLo = 17;     // segments alive while this phase lasts
        double stepTicks = 48;          // the crest's step (1/8, 1/8 triplet, 1/16)
        float lapBars = 16;             // the head's lap of the path
        u32 perSide = 1;                // a trough's bullets to each side
        float lungeTelegraphBeats = 0, lungeDashWu = 0, lungeDashBeats = 0;
        u32 headBurst = 0;              // the bare head's rings
    };
    Phase phases[4];                    // Swell, Chop, Whitecap, Bare head
    u32 whipCount = 32;
    float echoBehind[2] = {0.125f, 0.25f}, echoIntensity[2] = {0.22f, 0.10f};
    float accentL = 0.70f, accentHue = 258.0f, accentChroma = 0.96f;
    float bpm = 126;
    // The figure of eight's arc length, sampled over one lap (p in [0, 2 pi]): arc[i] at p = 2 pi i / kArc.
    static constexpr u32 kArc = 1024;
    float arc[kArc + 1] = {};
    float lapLength = 0;

    bool load();
    // The path at parameter p (unbounded; one lap is 2 pi), around the anchor's origin, and its unit tangent.
    Vec2 pathAt(double p) const;
    Vec2 tangentAt(double p) const;
    double arcOf(double p) const;   // arc length from p = 0 (laps included)
    double paramOf(double s) const; // the inverse
    u32 phaseOf(u32 segmentsAlive) const;
};

// ---------------------------------------------------------------------------------------------- The Choir

struct ChoirData {
    static constexpr u32 kVoices = 8;
    static constexpr u32 kMaxPts = 16;
    // Shapes (Voice-local, +x toward the ring's centre).
    float hood[kMaxPts][2] = {};
    u32 hoodCount = 0;
    float ribs[3][2][2] = {};
    float mouthClosed[2][2][2] = {}, mouthOpen[2][2][2] = {};
    float faceX = 5.2f, faceRadius = 20.8f;
    u32 faceSides = 10;
    float coreX = 5.2f, coreRadius = 9.1f;
    float haloRadius = 98.8f, haloFromDeg = 112, haloToDeg = 248;
    float waveRadii[3] = {96, 135, 174};
    float waveHalfDeg = 26;
    // Numbers.
    float voiceRadius = 66, voiceHealth = 260;
    u8 midi[kVoices] = {62, 64, 65, 67, 69, 71, 72, 74};
    float dmgSinging = 1, dmgSilent = 0, dmgLast = 1.5f;
    // numbers.contact.hoodHurts (2026-11-13): a hood hurts by touch only while it sings (its mouth open), always, or never.
    enum class HoodHurts : u8 { Singing, Always, Never };
    HoodHurts hoodHurts = HoodHurts::Singing;
    float ringRadius = 380, ringFugue = 300, shrinkBars = 2;
    float zoom = 0.75f;
    float bulletSpeed = 220, bulletLife = 5;
    u32 unisonCall = 5, unisonAnswer = 5, canonCall = 5, canonAnswer = 3, fugueStream = 1, soloRing = 16;
    float orbitRadius = 280, orbitLapBars = 4;
    float heldTelegraphBars = 1, heldSweepDeg = 120, heldSweepBeats = 2;
    float cageTelegraphBars = 1, cageBurnBeats = 2;
    float neighbourWidthPx = 1.6f, neighbourShimmer = 5, callWidthPx = 2.6f, callAmplitude = 44, callDecayBeats = 1;
    u32 callAntinodes = 3;
    float pearlL = 0.93f, pearlHue = 300, pearlChroma = 0.22f;
    float bpm = 132;

    bool load();
};

// ---------------------------------------------------------------------------------------------- mini-bosses

struct MiniData {
    static constexpr u32 kMax = 12;
    struct Entry {
        BossKind kind = BossKind::AlphaHound;
        char id[16] = {};
        char name[24] = {};          // as the name card shows it (upper case)
        char family[12] = {};        // the family's glyph name ("hound", ...)
        char glyph[16] = {};         // "hound_5"
        float radius = 64, healthFactor = 1;
        bool pen = false;
    };
    Entry entries[kMax];
    u32 count = 0;
    // The rules common to every mini-boss.
    float healthByTrack[8] = {};     // [2..6] (GAME_DESIGN 7.5: from Track 2 on)
    u32 healthTracks = 0;            // the last Track with a number
    float loopFactor = 1.25f;
    float crowdScale = 0.75f;
    u32 rewardOrbs = 1, rewardShards = 60;
    float penSide = 1400, penRiseBeats = 1;

    bool load();
    const Entry* find(BossKind k) const;
};

// ---------------------------------------------------------------------------------------------- state

constexpr u32 kBossParts = 32;   // parts with their own health (Kaleidos's pairs, Longwave's segments, Voices, Princes)
constexpr u32 kBossBodies = 34;  // positions kept for the view and the hits: the parts, plus the head / Queen / body

// Longwave (longwave.md): the head swims a figure of eight around `anchor`; the wave runs along the body.
struct LongwaveState {
    Vec2 anchor;                 // the path's centre (moved by the lunges)
    double p0 = 0.0, pTick = 0.0;  // the head's path parameter at pTick ...
    double pRate = 0.0;          // ... and its rate (radians a musical tick)
    double s0 = 0.0, sTick = 0.0, stepTicks = 48.0;  // grid steps of the wave since the fight began
    u8 phase = 0;                // the phase whose rates run (changes on a beat line after the count crosses)
    double pendingPhaseTick = -1.0;
    double whipTick = -1.0;      // the downbeat after a phase change: the tail cracks
    // The lunge (Whitecap, bare head): its line and its dash.
    bool lungeArmed = false;
    Vec2 lungeDir{1.0f, 0.0f};
    double lungeTelegraph = -1.0, lungeTick = -1.0;  // the line shows from lungeTelegraph; the dash starts at lungeTick
    double dashTick = -1.0;      // the last dash began here (the anchor slides from lungeFrom to anchor over its beat)
    Vec2 lungeFrom;              // the anchor when the dash began
    double jawUntil = -1.0;      // the jaw stays open after a lunge until then
    Vec2 enterFrom;              // where the head broke in (the nearest wall)
};

// The Choir (choir.md): eight Voices on the Round around `centre`.
struct ChoirState {
    Vec2 centre;
    float ring0 = 0.0f;          // the Round's angle at ringTick
    double ringTick = 0.0;
    float ringDir = 1.0f;        // +1 clockwise
    float radius = 380.0f;       // now (shrinks into the Fugue)
    double shrinkTick = -1.0;
    u8 caller = 0;               // Unison / Canon: the bar's caller
    u8 phase = 0;                // the phase of the bar being played (set on its bar line)
    u32 bar = 0xFFFFFFFFu;       // that bar
    u8 groupA = 0, groupB = 0;   // Fugue: the two callers (mask) and the others
    double singUntil[ChoirData::kVoices] = {};
    double heldTelegraph = -1.0; // Solo: the held note's line shows from here; the beam fires a bar later
    float heldAim[ChoirData::kVoices] = {};
    float fugueAim = 0.0f;       // the bar's sweep centre (toward the nearest player at its bar line)
};

// A mini-boss (minibosses.md): its timers, and what its called bodies follow (sim/world_miniboss.cpp).
struct MiniState {
    u8 state = 0;                // per mini-boss (world_miniboss.cpp)
    double stateUntil = -1.0;    // the current state ends here
    double cueTick = -1.0;       // the next scheduled moment (a lock, a charge)
    Vec2 lockAt, lockDir{1.0f, 0.0f};
    double winded = -1.0;        // its x 2 window runs until here (winded, stunned, the Queen's tell, after a bounce)
    bool half = false;           // past half health (the host's step)
    u32 called = 0;              // bodies it has called (diagnostics)
    Vec2 face{1.0f, 0.0f};       // where it faces (the Ram's horns)
    // Battering Ram: the charge's wake, a second charge after the skid (half health).
    bool wake = false, again = false;
    Vec2 wakeFrom, wakeTo;
    double wakeUntil = -1.0;
    // Carom King (index 0) and its Princes (1-3): straight legs from wall to wall, each ending on a beat (half beat).
    Vec2 legFrom[4], legTo[4];
    double legStart[4] = {}, legEnd[4] = {-1.0, -1.0, -1.0, -1.0};
    float speedFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    double hot[4] = {-1.0, -1.0, -1.0, -1.0};  // gold (x 2) for a beat after a bounce
    u8 splits = 0;               // Princes shed (at 75%, 50%, 25%)
    double bulletsUntil[12] = {};  // its bounce volleys still flying (the cap of 60)
    // The Hive: one or two hives (after the half), each its own shape round its own anchor.
    u8 hives = 1;
    u8 shape[2] = {0, 1};        // 0 Ring, 1 Spear, 2 Wall
    double shapeTick = 0.0;      // the current shapes began (a bar line)
    Vec2 shapeAt[2], shapeDir[2];
    float gap[2] = {};           // the Ring's gap angle at shapeTick; the Wall's first gap offset
    u16 nextSlot = 0;            // the next dart's slot (its place in its hive's shape)
    u16 slots[2] = {};           // slots in each hive
    u8 legWall[4] = {};          // Carom King: the wall each leg ends on (0 left, 1 top, 2 right, 3 bottom)
};

// The Pen (minibosses.md): a square of white light walls round the players' centre while a fight needs it.
struct PenState {
    bool up = false;
    Vec2 centre;
    float half = 700.0f;
    double riseTick = 0.0, fallTick = -1.0;
};

// The fight's state. Kaleidos: phase = 3 - pairs alive (0 Bloom, 1 Refraction, 2 Spiral, 3 Naked core).
struct BossState {
    bool active = false;       // entered and not dead
    bool dying = false;        // the death plays out (no hits, no fire)
    BossKind kind = BossKind::Kaleidos;
    u32 alive = 0;             // parts alive (bit k: part k): Kaleidos's pairs A, B, C, Longwave's segments, the Voices, Princes
    u8 phase = 0;
    double enterTick = 0;      // the Drop's downbeat: the entrance begins
    double fightTick = 0;      // when it fires and can be hurt
    double phaseTick = 0;      // when the current phase's turning began (a pair death stops it a beat)
    float angle0 = 0;          // wheel angle at phaseTick (radians)
    double deathTick = 0;
    double deathBars = 1.0;    // how long the death plays out
    float partHp[kBossParts] = {}, coreHp = 0;
    float partMax = 0, coreMax = 0;
    double partDeath[kBossParts] = {};  // when each part died (song ticks)
    Vec2 pos, prevPos;         // Kaleidos: the wheel; Longwave: the head; Choir: the Round's centre; a mini-boss: its body
    float angle = 0, prevAngle = 0;
    u32 phaseBar0 = 0;         // fight bar on which the current phase began (Naked core: "from the ninth bar")
    u8 lastBeamBar = 0xFF;
    u32 hits = 0;              // shots that landed (diagnostics)
    u32 hitsTaken = 0;         // hits the players took from it (diagnostics)
    u32 hitsBody = 0, hitsBeam = 0;  // ... of which from its bodies, and from its beams / cage / wake (the rest: bullets)
    // Bodies (M5): positions and headings of the head / Queen / King (index 0) and the parts (1 + k), this tick and the
    // last (the view interpolates). Longwave: 1 + segment index; Choir: 1 + Voice; Carom King: 1 + Prince.
    Vec2 body[kBossBodies], bodyPrev[kBossBodies];
    float bodyAng[kBossBodies] = {}, bodyPrevAng[kBossBodies] = {};
    LongwaveState lw;
    ChoirState ch;
    MiniState mini;
    float scale = 1.0f;        // health scale it entered with (players, Track, loop)
    double geomTick[8] = {-1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0};  // the moment of each BossGeom decision applied
};

// A count scaled by a lever (play test 1), rounded, never below `least` (an exact count at scale 1).
u32 bossScaledCount(u32 n, float scale, u32 least);
// A host's verdict that is not a part's death travels as a "part" from here (World::bossStep), and a decision that reads
// where the players stand (a shape's anchor, a lock, a lunge's line) from kBossGeomBase (World::bossGeom), so every
// device's fight follows the host's.
constexpr u8 kBossStepBase = 0x40;
constexpr u8 kBossGeomBase = 0x50;
enum BossGeom : u8 { kGeomHive0 = 0, kGeomHive1, kGeomSpear0, kGeomSpear1, kGeomAlphaLock, kGeomRamLock, kGeomLunge, kGeomCount };

// A part of a boss as the hits see it (Longwave, the Choir and the mini-bosses; Kaleidos keeps its arms).
struct BossPart {
    Vec2 pos;
    float radius = 0.0f;
    u8 part = 0;    // as an Impact's c: 1 armour (no damage), 2 a part (index), 3 the core
    u8 index = 0;
};

}  // namespace rf::sim
