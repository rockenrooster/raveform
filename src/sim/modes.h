#pragma once
// M8 (docs/MODES_PLAN.md 2, design/modes/): the run's setup (which mode, which Track, together or each for themselves) and the
// modes' numbers, read from design/modes/modes.json at start-up (never retyped). THE SET is the setup's default: every run that
// does not ask for another mode is the set, bit for bit (MODES_PLAN briefing 4).

#include "core/math.h"
#include "core/types.h"

namespace rf::sim {

enum class Mode : u8 { Set, OneTrack, Headliners, FullBlast, Tracers, Songline, Jam, Count };
constexpr u32 kModeCount = static_cast<u32>(Mode::Count);
constexpr u32 kModeTracks = 12;  // the set's ten and the two bonus Tracks (11 Mirrorball, 12 Toybox)

struct RunSetup {
    Mode mode = Mode::Set;
    u8 track = 1;         // 1..12 for a mode that takes one; else 1
    bool versus = false;  // each player for themselves (design/modes/README.md 8)
    bool help = true;     // versus: the underdog's help (V7), on unless the host switches it off
    // A moment for a networked benchmark (docs/MODES_PLAN.md 7: `--mode <id>:<track>:<moment>:hold` with `--host --loopback-bots`):
    // a timed form starts at its section moment - 1 (the ones before it skipped), held there with momentHold; JAM: 1 its DROP from the
    // first bar, 2 every layer on. Every device takes it from the run's setup, so the room's worlds agree. 0 in play.
    u8 moment = 0;
    bool momentHold = false;
    bool operator==(const RunSetup& o) const {
        return mode == o.mode && track == o.track && versus == o.versus && help == o.help && moment == o.moment && momentHold == o.momentHold;
    }
    bool operator!=(const RunSetup& o) const { return !(*this == o); }
};

const char* modeId(Mode m);                   // "set", "one", "headliners", "blast", "tracers", "songline", "jam"
bool modeFromId(const char* id, Mode& out);
// "<id>[:<track>]" (arguments): false on an unknown mode or a Track the mode does not take.
bool parseModeArg(const char* text, RunSetup& out, char* moment = nullptr, u32 momentCap = 0);

// A form's section (modes.json "forms"): its kind and length in bars (0: held, as a boss's Drop or JAM's Groove).
enum class FormKind : u8 { Groove, Build, Drop, Break, BossDrop, TrackChange };
struct FormSection {
    FormKind kind = FormKind::Groove;
    u16 bars = 0;
};
struct Form {
    static constexpr u32 kMax = 12;
    FormSection s[kMax];
    u32 count = 0;
    u32 bars() const;  // the form's whole length (a held section counts 0)
};

// Where a run's bar sits in a mode's form (docs/MODES_PLAN.md 2.2): its section, the bar in it, the section's length, the bars left
// to the form's end. A held section (0 bars: JAM's Groove, a boss's Drop) holds every bar from its start; a bar past the form's end
// is `ended`, at the last section's last bar.
struct FormPlace {
    u32 section = 0;
    FormKind kind = FormKind::Groove;
    u32 sectionBar = 0, sectionBars = 0;  // (sectionBars 0: held)
    u32 barsLeft = 0;                     // to the form's end (0 when held or ended)
    bool held = false, ended = false;
};
FormPlace formPlace(const Form& f, u32 bar);

struct ModesData {
    struct Info {
        char id[12] = {}, name[24] = {}, caption[40] = {}, nameCard[32] = {};
        bool takesTrack = false, stage = false;
        u8 trackLo = 1, trackHi = 10, home = 1;
        u16 timedBars = 0;
        bool trackColours = false;  // ONE TRACK: the chosen Track's accent to its first enemy hue
        float from[2] = {}, to[2] = {};  // OKLCH [L, hue] at full chroma
        char counts[8] = {};  // versus: score, acts, tiles, gates, pops
    } mode[kModeCount];
    struct Bonus {
        u8 track = 0, crowdOf = 1;
        u16 bpm = 0;
        char name[16] = {}, palette[16] = {}, homeOf[12] = {};
    } bonus[2];
    struct Forms {
        Form headlinersFirstAct, headlinersAct, blast, timed48, jam, jamDrop, popOff;
    } forms;
    struct Stage {
        float widthWu = 0, heightWu = 0, fitMarginWu = 0, minZoom = 0, tileWu = 0, spawnClearWu = 0;
        u32 tilesX = 0, tilesY = 0, rigFixtures = 0;
        float rigSpacingWu = 0, rigYFromTopWu = 0, rigLevel = 0;
    } stage;
    struct Kit {
        u8 atTrackStart[10] = {}, atBoss[10] = {};
        bool measured = false;
    } kit;
    struct OneTrack {
        u8 outroBars = 4;
    } oneTrack;
    struct Blast {  // full_blast.md (modes.json fullBlast)
        u8 riffs = 4, riffLevel = 5;
        struct Wave {
            char name[16] = {};
            float crowd[2] = {1.0f, 1.0f};
            u8 elitesAtStart = 0, eliteEvery = 0, boostEvery = 4;
            bool mini = false;
        } wave[4];
    } blast;
    struct LoadIn {  // README.md 5
        u32 bars = 2, perSixteenth = 1, perSixteenthPast32 = 2, bedLayerEvery = 4;
        float ringWu = 300.0f, spiralTurns = 0.25f, iconScale = 0.5f, graceWu = 600.0f, closeRingBeats = 0.5f;
        float closeRingWu[2] = {90.0f, 400.0f};
    } loadIn;
    struct Versus {
        float shoveWu = 0, shoveSeconds = 0, underdogShare = 0;
        float crownWidthWu = 0, crownAboveWu = 0, crownLevel = 0;
        u32 crownSparks = 0;
    } versus;
    // SONGLINE (songline.md; modes.json songline): the Spark, its line, the gates, the streak, the crowd along the line, the camera,
    // the finale, co-op's strings. Sections index Groove, Build, Drop, Break.
    struct Songline {
        float speedOfShip[4] = {0.8f, 0.9f, 0.95f, 0.58f};
        float startAheadWu = 300.0f;
        float curveAmp[2] = {0.0018f, 0.0011f}, curvePeriodWu[2] = {1400.0f, 3700.0f};
        float sectionScale[4] = {1.0f, 0.3f, 1.0f, 0.6f};
        float wallLookAheadWu = 600.0f, wallNearWu = 700.0f, wallTurn = 0.003f, maxCurvature = 0.004f;
        float comeBackWu = 1400.0f, comeBackTurn = 0.002f, comeBackSpeed = 0.8f, comeBackUntilWu = 600.0f;
        u8 every[4] = {2, 2, 1, 4};  // a gate every so many sixteenths, by section
        float radiusWu = 34.0f, breakRadiusWu = 60.0f, weaveWuPerSemitone = 7.0f, weaveMaxWu = 56.0f;
        float lifeBars = 2.0f, fadeBeats = 1.0f;
        u32 maxAlive = 256;
        float score = 10.0f, breakScore = 30.0f, inTimeWu = 80.0f, inTimeScale = 2.0f;
        u32 goldEveryBars = 8;
        float goldRadiusWu = 60.0f, goldScore = 100.0f;
        u16 tiers[4] = {16, 32, 64, 128};
        float crowdScale = 0.7f, threatScale = 0.7f, aheadWu[2] = {600.0f, 1200.0f};
        float leanShare = 0.3f, leanMaxWu = 320.0f, zoom = 0.85f, withinWu = 1800.0f;
        u32 finaleBars = 2, finaleGates = 32, finaleRings = 8, finaleSparks = 300;
        float coopOffsets[3][4] = {{-50.0f, 50.0f}, {-100.0f, 0.0f, 100.0f}, {-150.0f, -50.0f, 50.0f, 150.0f}};  // 2, 3, 4 players
        u32 braidEveryBars = 4;
        u8 harmonySteps[4] = {0, 2, 4, 7};
    } songline;
    // Each Track's tune as SONGLINE reads it from its music file: the progression's chords' arpeggios a sixteenth a note, the scale
    // from the key's tonic (the gates' notes and their places; co-op's harmonies). 1-12.
    struct Tune {
        u8 chords = 0;
        u8 arp[8][16] = {};  // midi, by the progression's place
        i8 scale[12] = {};
        u8 scaleCount = 0, tonic = 0;
    } tunes[kModeTracks];
    struct Jam {  // jam.md (modes.json jam): the room's 25 pads, the shelves, the dancers, POP-OFF
        static constexpr u32 kPads = 25, kMixer = 8, kTrackPads = 12;
        // (the pads in order: 0-7 the mixer, 8 DROP, 9 TOYS, 10 BOOSTS, 11 EMPTY, 12 SOUND, 13-24 the Tracks 1-12)
        static constexpr u8 kPadDrop = 8, kPadToys = 9, kPadBoosts = 10, kPadEmpty = 11, kPadSound = 12, kPadTrack0 = 13;
        struct Pad {
            Vec2 at;
            float radiusWu = 30.0f, holdBeats = 0.0f;  // holdBeats 0: a fly-over (the mixer)
        } pad[kPads];
        char layer[kMixer][12] = {};
        u8 onAtStart = 1;  // a bit a layer
        u32 dropRestBars = 8, dropBuildBars = 4, dropBars = 8;
        u32 orbs = 6, closesAfterBars = 2, boostBars = 8, riffSlots = 4;
        float ringWu = 170.0f, takeBeats = 1.0f, staysWithinWu = 300.0f;
        u8 toy[7][6] = {}, toyShelves = 0;
        u8 boostShelf[3][6] = {}, boostShelfSize[3] = {}, boostShelves = 0;
        u32 perLayer = 20, crowdMax = 160, eliteEveryBars = 8, eliteFromLayers = 5;
        float dropScale = 1.6f;
        u32 glints = 24;
        float glintWu = 26.0f, turnBars = 4.0f, glintLevel = 0.22f;
        float beamCorePx = 2.0f, beamGlowWu = 6.0f, beamGlow = 0.2f, beamLevel = 0.6f;
        u32 picks = 5, pickBars = 2, popBuildBars = 4, popDropBars = 16, popCrowd = 200, eliteWorth = 5;
    } jam;
    struct Tracers {  // tracers.md (modes.json tracers)
        float lifeBeats = 8.0f, lifeBeatsPerLevel = 1.0f, lifeBeatsMax = 16.0f, minSpeedWu = 120.0f, pointEveryWu = 16.0f;
        u32 maxPoints = 160, skipNewest = 6;
        float snapWu = 36.0f, hintWu = 120.0f, width = 2.2f, level = 2.6f, nodePx = 9.0f, expiringBeats = 2.0f;
        u32 minTiles = 4, maxVertices = 48, maxAlive = 8;
        u8 bonusFrom[5] = {1, 5, 10, 20, 40}, bonus[5] = {1, 2, 3, 5, 8};
        float heavyShare = 0.25f, duetBonus = 2.0f;
        float fullHouseShare = 0.9f, housePayout = 5000.0f, endPerTile = 10.0f;
        float litLevel = 0.55f, restLevel = 0.3f, edgeWidth = 1.5f, edgeLevel = 1.4f, edgeGlowWu = 10.0f, edgeGlow = 0.2f;
        float houseCrowdStep = 0.15f, crowdScale = 0.5f, crowdMax = 200.0f, threatScale = 0.5f;
        u32 eliteEveryBars = 8, boostEveryBars = 6;
        u8 boosts[12] = {};  // the mode's orbs: a Boost (sim/upgrades.h), or kGelOrb | a gel
        u8 boostCount = 0;
        float longScale = 2.0f, gelBars[2] = {4.0f, 4.0f}, echoBeats = 1.0f, echoScale = 1.5f;
        float knockShare = 0.25f, spinOutWu = 300.0f, dazzledBeats = 2.0f;
        u8 bonusFor(u32 caught) const {  // the ring's bonus for what it caught (rule 8)
            u8 b = bonus[0];
            for (u32 k = 0; k < 5; ++k)
                if (caught >= bonusFrom[k]) b = bonus[k];
            return b;
        }
    } tracers;

    const Info& of(Mode m) const { return mode[static_cast<u32>(m) < kModeCount ? static_cast<u32>(m) : 0]; }
    bool takes(Mode m, u32 track) const;  // the mode plays that Track
    bool load();
};
u64 modesDataDigest();  // design/modes/modes.json and the bonus Tracks (co-op peers must match)

// TRACERS's gels (tracers.md rule 15): two Boost-like orbs of the mode's own, held for four bars.
constexpr u8 kGelLong = 0, kGelEcho = 1, kGels = 2;
constexpr u8 kGelOrb = 0x80;  // ModesData::Tracers::boosts: kGelOrb | the gel

// TRACERS's state in the world (tracers.md "Caps and the wire"): each player's tracer, the rings alive, the floor.
struct TracerLine {
    static constexpr u32 kMax = 160;  // points a player
    Vec2 pt[kMax];
    double born[kMax] = {};
    u8 node[kMax] = {};  // 1: a sixteenth fell while this point was laid (a bead and a note of the lead)
    u16 head = 0, count = 0;
    bool laying = false;
    double fizzleTick = -1e18;  // the last dry tick (one a beat at most)
    Vec2 at(u32 k) const { return pt[(head + k) % kMax]; }
    double bornAt(u32 k) const { return born[(head + k) % kMax]; }
    bool nodeAt(u32 k) const { return node[(head + k) % kMax] != 0; }
};
struct TracerRing {
    static constexpr u32 kCorners = 48, kCaught = 224;
    Vec2 pt[kCorners];
    u8 n = 0;
    bool active = false, popped = false, echo = false;  // echo: ECHO's second pop of a ring
    u8 owner = 0, owner2 = 0xFF;                        // owner2: a duet's partner (0xFF: none)
    u8 bonus = 1;
    u16 caught = 0, tiles = 0;
    double lockTick = 0.0, popTick = 0.0;
    Vec2 centre;
    float radius = 0.0f;  // the farthest corner from the centre (the pop's pool, the words)
    u32 handle[kCaught] = {};
};
// SONGLINE's state in the world (songline.md "Caps and the wire"): the Spark with its line a bar ahead (a point a sixteenth), the
// gates it has laid, each player's streak.
struct SongSpark {
    Vec2 pos, prevPos;
    float heading = 0.0f;
    u32 step0 = 0;           // the sixteenth (since the run's first bar line) of ahead[0]
    Vec2 ahead[17];          // the line: where the Spark is on each of the next sixteen sixteenths (ahead[0] the last passed)
    float aheadHeading[17] = {};
    double aheadFlown[17] = {};
    float aheadWall[17] = {};  // the wall rule's turn held at each point (+1 left, -1 right; 0: clear of the walls)
    float aheadHome[17] = {};  // rule 4's way round held while the lost ship is behind the line (+1 left, -1 right; 0: none)
    float phase[2] = {};     // the two waves' phases (the run's seed)
    bool comeBack = false;   // rule 4: bending back to the nearest ship
    Vec2 comeTo;
    bool spiral = false;     // the finale's last two bars
    float spiralSign = 0.0f; // its turn (+1 left, -1 right), toward the arena's centre as it begins (0: not yet)
};
struct SongGate {
    Vec2 pos;
    double born = -1.0;      // < 0: free
    u32 step = 0;            // the sixteenth it was laid on
    u8 note = 0;             // its note (midi, the string's own: co-op's harmonies)
    u8 kind = 0;             // 0 a gate, 1 the Break's, 2 gold, 3 the finale's
    u8 string = 0;           // its player's (co-op), 0xFF everyone's (solo's one, versus's one)
    u8 threaded = 0, inTime = 0;  // a bit a player
    u8 owner = 0xFF;         // versus: who took it
    u8 boost = 0xFF;         // gold: its Boost
    bool missed = false;     // (faded unthreaded: drawn grey)
    // M8 net (songline.md "Caps and the wire"): versus: the owner's claim (its tick), whether the host has decided it and whether
    // it was in time; co-op: the step's chord paid.
    bool decided = false, ownerInTime = false, chord = false;
    double takeTick = 0.0;
};
// TRACERS on the wire (tracers.md "Caps and the wire"): a ring from its owner's device (kind 0: its corners, its pop's eighth, its
// owners; ECHO's second pop flagged), the host's house (kind 1: the chase's bar line in `tick`); the host's floor.
struct NetRing {
    u16 run = 0;
    u8 kind = 0, owner = 0, owner2 = 0xFF, echo = 0, n = 0;
    double tick = 0.0;
    Vec2 pt[48];
};
struct TracerFloor;
struct NetFloor {
    static constexpr u32 kTiles = 43 * 21;
    u16 run = 0;
    u16 house = 1, houses = 0;
    u16 rings[4] = {};  // the rings the host has locked of each seat (a snapshot older than a seat's own last ring is not taken)
    u8 owner[kTiles] = {};
};
struct TracerFloor {
    static constexpr u32 kTilesX = 43, kTilesY = 21, kTiles = kTilesX * kTilesY;
    static constexpr u8 kDark = 0xFF, kHouse = 0xFE;  // an owner: dark; lit by the house's chase
    u8 owner[kTiles] = {};
    u16 order[kTiles] = {};  // the order it was lit in (the knock-out's "oldest")
    u16 serial = 0, lit = 0;
    u16 house = 1, houses = 0;
    double chaseTick = -1.0, popTick = -1.0, darkTick = -1.0;  // FULL HOUSE: the chase, the crowd's pop and the pay, the dark
    Vec2 lastRing;  // the last ring's centre (the chase's spiral starts there)
};

}  // namespace rf::sim
