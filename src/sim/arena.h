#pragma once
// M5 part 5: the arena's structures and events (design/arena.md, arena.json) and the elites with their twelve traits
// (design/traits.md, traits.json; GAME_DESIGN 7.3, 9). The data is embedded at build time and read once; the pages'
// prose numbers are in sim/boss_prose.h ("arena.md").

#include "core/types.h"

namespace rf::sim {

enum class StructKind : u8 { Vault, Pylon, Rift, Bumper, Current, Lens, Count };
constexpr u32 kStructKinds = static_cast<u32>(StructKind::Count);
enum class EventKind : u8 { VaultDrop, RiftSurge, EliteHunt, Comet, Eclipse, MeteorRain, Convoy, StormCell, Count };
constexpr u32 kEventKinds = static_cast<u32>(EventKind::Count);
constexpr u8 kNoEvent = 0xFF;

// The twelve traits (traits.json order), as bits of Enemies::traits; kEliteBit marks an elite (with or without traits:
// Splitting's copies are not elites).
enum TraitId : u8 {
    kArmored, kSwift, kGiant, kSplitting, kVolatile, kRegenerating, kShielded, kTwin, kMagnetic, kCommander, kCloaked, kPhasing,
    kTraitCount
};
constexpr u16 kEliteBit = 0x8000u;
constexpr u16 traitBit(u32 t) { return static_cast<u16>(1u << t); }
inline bool hasTrait(u16 traits, u32 t) { return (traits & traitBit(t)) != 0; }

const char* structId(StructKind k);  // "vault", ...
const char* eventId(EventKind k);    // "vault_drop", ...
const char* eventName(EventKind k);  // "VAULT DROP", ... (the world text that announces it)
const char* traitId(u32 t);          // "armored", ...
bool structFromId(const char* id, StructKind& out);
bool eventFromId(const char* id, EventKind& out);

// A shape from arena.json "glyphs" (unit segments x radius; layers 0 hull, 1 ornament, 2 white mark).
struct ArenaGlyph {
    static constexpr u32 kMax = 64;
    float seg[kMax][4] = {};
    u8 layer[kMax] = {};
    u32 count = 0;
    float radius = 0.0f;
};

struct ArenaData {
    static constexpr u32 kTracks = 10;
    float radius[kStructKinds] = {};  // structures[].radius (the Lens: half its length)
    u32 eventBars[kEventKinds] = {};  // events[].bars (0: as long as it takes)
    // byTrack: which structures stand in a Track (with repeats: "Pylon x 2") and which events it draws from.
    u8 structs[kTracks + 1][8] = {};
    u8 structCount[kTracks + 1] = {};
    u8 events[kTracks + 1][4] = {};
    u8 eventCount[kTracks + 1] = {};
    // glyphs: vault, pylon, rift, bumper, lens (the structures) and comet, carrier (the events' bodies).
    enum GlyphId : u8 { gVault, gPylon, gRift, gBumper, gLens, gComet, gCarrier, gCount };
    ArenaGlyph glyph[gCount];
    bool load();
};

struct TraitData {
    // elite
    float scale = 1.3f, health = 6.0f, score = 10.0f, threat = 4.0f;
    u32 shards = 20;
    u32 alive[2] = {3, 5};    // at once: Tracks 2-6, 7-10
    u32 carried[2] = {1, 2};  // traits carried: Tracks 2-6, 7-10
    u8 fromTrack[kTraitCount] = {};
    u8 fromCycle[kTraitCount] = {};  // (d59062f) not drawn before this cycle of its first Track (absent: 1)
    // each trait's numbers
    float armoredHealth = 2.0f, armoredHull = 0.5f;
    float swiftSpeed = 1.6f, swiftTurn = 1.3f;
    float giantScale = 1.8f, giantHealth = 2.5f;
    u32 splitCopies = 2;
    float splitScale = 0.6f, splitHealth = 0.3f;
    float volatileRing = 160.0f, volatileDamage = 30.0f;
    u32 volatileFuse = 2;
    u32 regenQuiet = 2;
    float regenHeal = 0.08f;
    float shieldArc = 120.0f, shieldTurn = 1.2f;
    float twinHealth = 1.6f, twinApart[2] = {200.0f, 300.0f};
    float magnetReach = 300.0f, magnetRelease = 2.0f;
    u32 magnetMax = 40;
    float commanderReach = 400.0f, commanderSpeed = 1.3f;
    u32 commanderStun = 1;
    float cloaked = 0.08f, phasingSolid = 0.5f;
    bool load();
};

u64 arenaDataDigest();  // design/arena.json and traits.json (M4: peers must match)

}  // namespace rf::sim
