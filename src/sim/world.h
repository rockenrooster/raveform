#pragma once
// The world (TECH_SPEC 7): every gameplay entity in fixed-capacity struct-of-arrays pools, advanced by a
// fixed 60 Hz tick that reads its inputs (player poses, heard musical time, lead-rule shots) and writes
// an event buffer (kills, hits, pickups, warp-ins, ...) that rendering, audio and score consume. The
// renderer reads the pools (previous and current positions) and interpolates; nothing here draws.
//
// Deterministic: no wall clock, stable iteration (dense arrays, swap-remove in index order), its own
// random streams (the spawner's PCG32, one xorshift state per enemy), and no allocation after init().
// hash() digests the whole state (TECH_SPEC 7.3).

#include "game/rng.h"
#include "sim/boss.h"
#include "sim/director.h"
#include "sim/grid.h"
#include "sim/sim_types.h"
#include "sim/species.h"
#include "sim/upgrades.h"

namespace rf::sim {

// ---------------------------------------------------------------------------------------------- input

// What a tick reads for one player (TECH_SPEC 4: the local player is an input to the sim, integrated by
// its controller every rendered frame; sim/player_controller.h).
struct PlayerPose {
    bool present = false;
    Vec2 pos, vel;
    Vec2 aim{1.0f, 0.0f};    // unit vector
    float heading = 0.0f;    // the hull's facing (radians)
    u8 fire = 0;             // 0 none, 1 every grid step, 2 every other step (auto-aim assist, GAME_DESIGN 2)
    bool dashing = false;    // invulnerable throughout (GAME_DESIGN 5)
    // Edges, latched by the controller until a tick has read them.
    bool novaPressed = false;
    bool dashStarted = false;
    bool perfectDash = false;
    Vec2 dashFrom, dashTo;
};

// ---------------------------------------------------------------------------------------------- co-op (M4)

// TECH_SPEC 8.1: every device simulates the whole world; the host's director spawns and its verdicts on
// enemy health stand; each player owns its own ship (what you see is what hits you). Solo is the M3 game
// unchanged: every player local, no network log, no network operations.
enum class NetRole : u8 { Solo, Host, Client };

// A spawn of the host's director, sent to the clients in a batch at least two beats before it lands.
struct NetSpawn {
    u32 serial = 0;            // the enemy's network id (the host's spawn number)
    u16 species = 0, group = 0;
    float x = 0.0f, y = 0.0f, heading = 0.0f;
    double land = 0.0;         // the beat it becomes solid (R4)
    u8 flags = 0;              // kFlagArriving for the crowd's fodder
};
// The host's verdict on an enemy (reliable).
enum NetDeathKind : u8 { kDeathKill = 0, kDeathDissolve = 1, kDeathShards = 2 };  // killed (Shards); gone (none); Break (Shards)
struct NetDeath {
    u32 serial = 0;
    u8 killer = 0xFF;
    u8 kind = kDeathKill;
};
// A client's hit on an enemy (TECH_SPEC 8.1: reliable, batched per tick).
enum NetClaimKind : u8 { kClaimHit = 0, kClaimKill = 1, kClaimDissolve = 2 };  // damage; a Nova's front (solid); a Nova's front (warping)
struct NetClaim {
    u32 serial = 0;
    float damage = 0.0f;
    u8 kind = kClaimHit;
};
struct NetBossClaim {
    u8 part = 0, pair = 0;
    float damage = 0.0f;  // final (the part's multiplier applied by the claimant)
};
// An enemy as the host has it (a correction, TECH_SPEC 8.1).
struct NetEnemyState {
    u32 serial = 0;
    float x = 0, y = 0, vx = 0, vy = 0, kx = 0, ky = 0, heading = 0, a = 0, b = 0, hp = 0;
    double until = 0.0;
    u32 rng = 0;
    u8 state = 0, flags = 0, target = 0xFF, far = 0;
};
// What a player's own device decided, applied to that player in the other worlds (reliable).
enum NetPlayerEventKind : u8 {
    kPeNova, kPeDash, kPeHit, kPeDown, kPeRevive, kPeTrioOpen, kPeTrioTake, kPeBoostTake, kPeHalo, kPeLeave, kPeCount
};
// One operation from the network, applied inside the next tick (so its events land in the tick's buffer
// and the view and the band see them like any other). `run`: the run it belongs to (World::runId());
// operations of a later run wait for the world to reach it, those of an earlier one are dropped.
struct NetOp {
    // Schedule: a run on bar `tick`, seed u | u2 << 32, a seats, its id in `species`. Census: the host's enemy
    // digest at this point of its stream (count u, digest serial | u2 << 32). Director: the host's director
    // state (tick = startTick, u = trackBar, u2 = changeBar, a = track, b = boss spawned, f = assist).
    enum Kind : u8 { Spawn, Death, Claim, BossClaim, BossEnter, BossPair, BossDeath, BoostDrop, Player, Schedule, Census, Director };
    Kind kind = Spawn;
    u8 player = 0;              // claimant, event owner
    u8 a = 0, b = 0, c = 0;     // death kind / killer, boss part / pair, boost type, player event kind, ...
    u8 opts[3] = {};            // a Trio's upgrades
    u16 run = 0;
    u16 species = 0, group = 0;
    u32 serial = 0, u = 0;      // enemy id; seed (low) / changeBar / ...
    u32 u2 = 0;                 // seed (high)
    float x = 0, y = 0, x2 = 0, y2 = 0, f = 0;
    double tick = 0.0;          // land, enter, bar, blast, close
};
// What a tick produced for the network (host: spawns and verdicts; client: claims). Drained after each
// tick by the session (net/session.h).
struct NetLog {
    // (Play test 1, second pass: a tick's deaths can be the whole crowd dissolving at a Break, 1,350 with four players.)
    static constexpr u32 kSpawns = 2048, kDeaths = kMaxEnemies, kClaims = 512, kBossClaims = 64, kBoss = 16, kBoosts = 8;
    NetSpawn spawns[kSpawns];
    u32 spawnIndex[kSpawns];  // dense index while the tick runs (its group and flags are read before compact)
    NetDeath deaths[kDeaths];
    NetClaim claims[kClaims];
    NetBossClaim bossClaims[kBossClaims];
    // The host's boss: 0 enters (x, y, tick = the entrance), 1 a pair dies (pair, killer), 2 dies (killer, changeBar).
    struct Boss {
        u8 kind = 0, pair = 0, killer = 0xFF;
        float x = 0.0f, y = 0.0f;
        double tick = 0.0;
        u32 changeBar = 0;
    } boss[kBoss];
    struct Boost {
        float x = 0.0f, y = 0.0f;
        u8 type = 0;
    } boosts[kBoosts];
    u32 spawnCount = 0, deathCount = 0, claimCount = 0, bossClaimCount = 0, bossCount = 0, boostCount = 0;
    u32 spawnFinished = 0;  // spawns whose group and flags are filled in (World::finishNetLog)
    u32 overflow = 0;
    void clear() { spawnCount = deathCount = claimCount = bossClaimCount = bossCount = boostCount = spawnFinished = 0; }
};

// A lead-rule step heard with fire held (audio/band.h LeadShot, TECH_SPEC 6.3).
struct ShotRequest {
    double tick = 0.0;
    float advanceS = 0.0f;  // how long the shot has already flown when this tick ends
    u8 player = 0;
};

struct TickInput {
    double music0 = 0.0, music1 = 0.0;  // musical position (96 PPQ) at the start and the end of the tick
    float bpm = 120.0f;
    float latencyTicks = 0.0f;          // the band's write head ahead of the heard position (Nova timing)
    PlayerPose players[kMaxPlayers];
    // Shots: from the band's lead rule when it plays (bandShots), else from the sim's own grid.
    bool bandShots = false;
    ShotRequest shots[32];
    u32 shotCount = 0;
    bool retry = false;  // "tap to retry": the run starts again on the next bar line
};

// ---------------------------------------------------------------------------------------------- events

enum class EventType : u8 {
    Kill,        // x, y; species; a = size (0/1/2); b = killer (0xFF: none); c = 1 dissolved warp-in; f = heading
    Impact,      // a shot hit without killing: x, y; x2, y2 = shot direction; species
    Shot,        // a player shot left the muzzle: x, y; x2, y2 = direction; a = player
    EnemyShot,   // a volley: x, y; species
    Telegraph,   // a shooter glints or a Ram shows its path: x, y; species; tick = when it resolves
    WarpStart,   // x, y; species; tick = the beat it becomes solid
    WarpLand,    // x, y; species
    Pickup,      // a Shard collected: x, y; a = player
    Hit,         // x, y; a = player; b = Shield cells left
    Death,       // x, y; a = player
    NovaPress,   // x, y; a = player; tick = the blast's beat
    NovaBlast,   // x, y; a = player
    Dash,        // x, y -> x2, y2; a = player; b = 1 Perfect Dash
    Restart,     // the run started again (on a bar line)
    // M3 stage B.
    Section,     // a section began: a = Section, b = its bars, c = Track; f = 1 the Track-change Break; tick = bar line
    TrioOpen,    // a = player, b = orbs; x, y = the ring's centre
    TrioTake,    // a = player, b = upgrade, c = its stacks or level now; x, y = the orb
    TrioPop,     // an orb not taken: a = player, b = upgrade; x, y
    BoostDrop,   // b = boost; x, y
    BoostTake,   // a = player, b = boost; x, y
    BoostEnd,    // a = player, b = boost
    HaloBlock,   // the Halo took a hit: a = player, b = hits left; x, y
    PulseRing,   // a = player, b = level, c = 1 the second ring; x, y
    SentinelShot,// a = player; x, y = the drone; x2, y2 = direction
    Spotlight,   // a debut: species; x, y = where its group lands; tick = the downbeat it lands on
    BossEnter,   // x, y; tick = the downbeat the fight begins (a bar after the entrance)
    BossPair,    // a pair shattered: a = pair (0 A, 1 B, 2 C), b = pairs left; x, y; f = its arm angle
    BossDeath,   // x, y
    BossBeam,    // a = pair, b = 0 telegraph, 1 fire; tick = when it fires
    BossBurst,   // the core fired a ring: b = count; x, y
    // M4 co-op (GAME_DESIGN 10). Death with a = 0xFF: every player is down at once (the run is over).
    Down,        // a hit at zero cells shattered the ship into a beacon: a = player; x, y = the beacon
    Revive,      // a = player; b = 1 by a teammate, 0 at the Break; x, y
    // Play test 1.
    CritBurst,   // a beat shot's hit burst (On-Beat Crit): a = player; x, y; f = its radius
    TrioUnlock,  // the Trio's orbs light (the Break's third beat): a = player; tick = that beat
    Refill,      // a Break gave back a cell and a Nova: a = player; b = cells now, c = Novas now
    // M4 stage B.
    Leave,       // a player's device left the run (its seat opened): a = player; x, y = where its ship was; f = heading
    // Play test 2 (GAME_DESIGN 6.1).
    LevelUp,     // a player's Shard meter filled: a = player; f = the level reached (its Trio opens on a coming bar line)
    CellDrop,    // a Cell (health pickup) fell: a = its owner; x, y
    CellTake,    // a cell came back: a = player, b = cells now, c = 1 grown back by Regen (0: a Cell taken); x, y
    HaloBurn,    // GAME_DESIGN 8, the loop: a = player, b = its capacity now, c = cells held before; x, y (the arcs shatter)
};
// Impact events on the boss carry this species, with c = 1 lens plate, 2 arm, 3 core. Play test 2: every Impact carries
// b = the shooter (0xFF: none); an enemy's Impact c = a Prism lane + 3 (1..5; 0: none).
constexpr u16 kBossSpecies = 0xFFFF;

struct Event {
    EventType type = EventType::Kill;
    u8 a = 0, b = 0, c = 0;
    u16 species = 0;
    float x = 0.0f, y = 0.0f, x2 = 0.0f, y2 = 0.0f;
    float f = 0.0f;
    float age = 0.0f;   // seconds before the end of the tick at which it happened
    double tick = 0.0;  // musical position, where the event names one
};

// ---------------------------------------------------------------------------------------------- pools

enum EnemyState : u8 {
    kWarping = 0,  // R4: two beats before it is solid; not drawn as a body, cannot hurt or be hurt
    kMoving,       // the family's normal motion
    kGlint,        // a shooter's telegraph (one beat)
    kFiring,       // a Sentry's pattern
    kTelegraph,    // a Ram's telegraph (one beat)
    kDash,         // a Ram's dash (one beat)
    kRest,         // a Ram's rest
};
enum EnemyFlag : u8 { kFlagDead = 1, kFlagEngaged = 2, kFlagArriving = 4, kFlagPredicted = 8 };  // arriving: crowd fodder on its way into the frame
// kFlagPredicted (co-op client, with kFlagDead): its own hit killed it and the host has not answered yet; it
// stays in the pool (gone for everything else) until the host confirms the death, or comes back after 300 ms.

struct Enemies {
    static constexpr u32 kCap = kMaxEnemies;
    u32 count = 0;
    float x[kCap], y[kCap], px[kCap], py[kCap];  // current and previous tick
    float ox[kCap], oy[kCap];                    // M4 client: the drawn offset left by a correction, decaying (TECH_SPEC 8.1)
    float pred[kCap];                            // M4 client: seconds a predicted death has waited for the host
    u8 target[kCap];                             // M4 co-op: the player it attends to (0xFF: the nearest, solo)
    float vx[kCap], vy[kCap];                    // the family's own motion
    float kx[kCap], ky[kCap];                    // knockback (panic pulse), decaying
    float heading[kCap], pheading[kCap];
    float hp[kCap], radius[kCap];
    float a[kCap], b[kCap];                      // per-family values (see world_enemies.cpp)
    double until[kCap];                          // musical tick at which the current state ends
    u16 species[kCap];
    u16 group[kCap];                             // a Shoal's school
    u8 state[kCap], flags[kCap];
    u8 far[kCap];                                // bars in a row outside every frame x the crowd's stray scale
    u32 handle[kCap];                            // generation << 16 | slot
    u32 serial[kCap];                            // spawn number (seeds the enemy's own random stream)
    u32 rng[kCap];
    // Handles (TECH_SPEC 7.1: generation-checked): slot -> dense index, generation per slot.
    u32 denseOf[kCap];
    u16 gen[kCap];
    u32 freeSlots[kCap];
    u32 freeCount = 0;
};

struct Bullets {
    static constexpr u32 kCap = kMaxEnemyBullets;
    u32 count = 0;
    float x[kCap], y[kCap], px[kCap], py[kCap], vx[kCap], vy[kCap];
    float life[kCap];  // seconds left
    u8 dead[kCap];
};

enum ShotKind : u8 { kShotPrimary = 0, kShotSentinel, kShotMini };

struct Shots {
    static constexpr u32 kCap = kMaxShots;  // 1,024 per player in one pool (M4)
    u32 count = 0;
    float x[kCap], y[kCap], px[kCap], py[kCap], dx[kCap], dy[kCap];
    float speed[kCap], life[kCap], damage[kCap], radius[kCap];
    u32 lastHit[kCap];               // the enemy handle it last went through (Pierce)
    u8 owner[kCap], dead[kCap];
    u8 pierce[kCap], bounces[kCap];  // Pierce and Ricochet left
    u8 kind[kCap], flags[kCap];      // ShotKind; ShotFlag
};
// kShotRefracted (play test 1): a Prism shot refracts on its first hit only.
enum ShotFlag : u8 { kShotCrit = 1, kShotPrism = 2, kShotLaneMask = 7u << 2, kShotRefracted = 32 };
// M3 stage C2: a Prism shot's lane in its fan (-2..2, GAME_DESIGN 7: the rainbow), kept in flags bits 2-4 (0: none);
// its mini-refractions carry it on.
inline u8 shotLaneFlags(i32 lane) { return static_cast<u8>((lane + 3) << 2); }
inline bool shotHasLane(u8 flags) { return (flags & kShotLaneMask) != 0; }
inline i32 shotLane(u8 flags) { return static_cast<i32>((flags & kShotLaneMask) >> 2) - 3; }

// Boosts lying in the arena (GAME_DESIGN 6.4), until collected or eight bars old. Loot is instanced
// (GAME_DESIGN 10): in co-op each drop is one orb per player, only its owner can take it.
struct BoostPickups {
    static constexpr u32 kCap = 32;
    u32 count = 0;
    float x[kCap], y[kCap];
    double born[kCap];
    u8 type[kCap], dead[kCap], owner[kCap];
};

// A Pulse Ring Riff's expanding ring.
struct PulseWave {
    bool active = false;
    Vec2 centre;
    float age = 0.0f;     // seconds (may start negative: the second ring waits)
    float reach = 0.0f;   // final radius
    float damage = 0.0f;
    float lastR = 0.0f;   // radius at the end of the last tick
    u8 level = 1;
};
constexpr u32 kPulseWaves = 12;  // play test 1: two rings every eighth at level 5 (every sixteenth under Hyper)

struct ActiveBoost {
    u8 type = kNoBoost;
    double from = 0.0, until = 0.0;  // musical ticks
    u8 hits = 0;                     // Halo: hits it can still absorb
};

// The Trio (GAME_DESIGN 6.1, screens.md): three orbs on a ring around where the ship was when the Break
// began; the ring is the clock. Index 3 is kept for the Fusion's gold fourth orb (M5). Play test 2: a level-up Trio
// opens round the ship on a bar line in the fight, for four bars; its orbs are shelters (a ship inside an orb's thin
// ring cannot be hurt, bodies touching it are shoved off), and the Break's Trio waits for it to close.
enum TrioKind : u8 { kTrioBreak = 0, kTrioLevel = 1 };
struct TrioState {
    bool open = false;
    u8 kind = kTrioBreak;
    u16 levelShown = 0;    // a level-up Trio's level (LEVEL n at its centre for the first bar)
    u8 breakWaiting = 0;   // Break Trios (1, or 2 after a boss) waiting for the open level-up Trio to close
    u8 orbs = 0;
    u8 upgrade[4] = {kNoUpgrade, kNoUpgrade, kNoUpgrade, kNoUpgrade};
    u8 level[4] = {};      // the stacks or Riff level the pick gives
    Vec2 centre;
    Vec2 orbPos[4];
    double openTick = 0.0, closeTick = 0.0;  // the clock (musical ticks)
    // Play test 1: locked for the Break's first two beats (until openTick + kTrioLockBeats); then the orb the ship
    // stays inside fills over kTrioHoldBeats (musical ticks so far; leaving empties it at once).
    i8 holdOrb = -1;
    double holdTicks = 0.0;
    i8 homing = -1;        // the orb flying to the ship after the clock ran out (the nearest)
    u8 queued = 0;         // further Trios in this Break (the boss's bonus Trio)
    u32 opened = 0;        // Trios opened this run (the first shows the hint)
    u8 sinceRiff = 0;      // Trios in a row without a new Riff
    u8 lastTaken = kNoUpgrade;
    double lastTakenTick = -1e9;
    Vec2 lastTakenAt;
};

struct Shards {
    static constexpr u32 kCap = kMaxShards;
    u32 count = 0;
    float x[kCap], y[kCap], px[kCap], py[kCap], vx[kCap], vy[kCap];
    double born[kCap];  // musical tick
    u8 owner[kCap];     // loot is instanced per player (GAME_DESIGN 10)
    u8 homing[kCap], dead[kCap];
};

// Cells lying in the arena (play test 2, GAME_DESIGN 6.1): health pickups, instanced like Shards (each device holds its
// own players' only), pulled by the magnet while their player is hurt, gone after eight bars.
struct CellPickups {
    static constexpr u32 kCap = 32;
    u32 count = 0;
    float x[kCap], y[kCap], px[kCap], py[kCap], vx[kCap], vy[kCap];
    double born[kCap];  // musical tick
    u8 owner[kCap], homing[kCap], dead[kCap];
};

struct PlayerState {
    bool present = false, alive = false;
    Vec2 pos, prevPos, vel;
    Vec2 aim{1.0f, 0.0f};
    float heading = 0.0f, prevHeading = 0.0f;
    u8 cells = kStartCells, cellCap = kStartCells;
    u8 novas = kStartNovas, novaCap = kNovaCapacity;
    u8 fire = 0;
    bool dashing = false;
    float invulS = 0.0f;       // after a hit (1.5 s)
    float novaInvulS = 0.0f;   // from a Nova press until shortly after its blast
    bool novaPending = false;
    double novaBlastTick = 0.0;
    float novaFrontAge = -1.0f;  // seconds since the blast while its front still clears (< 0: none)
    Vec2 novaAt;
    u32 chain = 1, bestChain = 1;  // GAME_DESIGN 5 (v2.3): a multiplier from x1, never below it
    u64 score = 0;
    u32 kills = 0, hitsTaken = 0, pickups = 0, shotsFired = 0, novasUsed = 0, dashes = 0, perfectDashes = 0;
    // M3 stage B: upgrades and Boosts.
    Loadout loadout;
    ActiveBoost boosts[kBoostSlots];
    Vec2 sentinel[kMaxDrones], prevSentinel[kMaxDrones];  // the Sentinel drones (play test 1: up to three)
    PulseWave pulses[kPulseWaves];
    u32 boostsTaken = 0;
    // M4 co-op (GAME_DESIGN 10): down, a beacon, revive.
    bool down = false;         // shattered into a beacon (alive is false); the run goes on
    Vec2 beacon;
    double reviveTicks = 0.0;  // musical ticks a teammate has stood in the beacon's ring
    u32 downs = 0, revives = 0;
    // Play test 2 (GAME_DESIGN 6.1): Shard levels; Cells and Regen. Kept on the player's own device (Shards and Cells
    // are its own; the others hear of its Trios and cells).
    u32 levelShards = 0;      // Shards toward the next level
    u16 level = 0;            // levels reached this run
    u8 levelsBanked = 0;      // levels whose Trio has not opened yet
    float cellDue = 0.0f;     // toward the next Cell (1: the next kill near the player drops one)
    u8 regenBars = 0;         // bars the missing cell has been growing back (Regen)
    u8 burnCap = kMaxCells;   // GAME_DESIGN 8: the most cells the halo can hold once the loop has burnt it down
    u32 cellsTaken = 0, cellsRegrown = 0, levelTrios = 0;
    bool invulnerable() const { return invulS > 0.0f || novaInvulS > 0.0f || dashing; }
    bool boost(u8 b) const {
        for (const ActiveBoost& a : boosts)
            if (a.type == b) return true;
        return false;
    }
};

struct Rules {
    bool godMode = false;           // hits are detected but cost nothing (bench_stress)
    bool novas = true;
    bool autoNova = true;           // GAME_DESIGN 5 (play test 1, second pass): a hit that would take the last cell fires a
                                    // Nova from the stock instead (this device's players; the CONTROLS switch)
    float scoreMultiplier = 1.0f;   // Surge x2 (GAME_DESIGN 4.1, stage B)
    float magnetMultiplier = 1.0f;  // Surge x2
};

struct WorldCounters {
    u32 enemiesDropped = 0, bulletsDropped = 0, shotsDropped = 0, shardsDropped = 0, eventsDropped = 0;
    u32 kills = 0, deaths = 0, spawned = 0, restarts = 0;
    u32 shotsCapped = 0;  // play test 1: shots a player's hard cap held back (the soft cap's rules should keep it 0)
    // M4 co-op.
    u32 netOpsDropped = 0, netLogDropped = 0;  // pool overflows of the network's queues
    u32 predicted = 0, confirmed = 0, undone = 0, lateSpawns = 0, unknownIds = 0, downs = 0, revives = 0;
    u32 censusChecks = 0, censusMismatches = 0;
    i32 censusLastDiff = 0;  // this world's count minus the host's at the last mismatch
};

// ---------------------------------------------------------------------------------------------- spawner

// What spawns enemies: the director (sim/director.h, GAME_DESIGN 8: the phrase cycle, the threat budget by
// Track, cycle and section, formations, debuts, the Drop's mega-wave, the boss) or, for bench_stress, a
// fixed crowd of kStressEnemies around the ship. Off: nothing spawns (pictures, tests); the phrase still
// runs.
struct Spawner {
    enum class Mode : u8 { Off, Slice, Stress };
    Mode mode = Mode::Slice;
    double startTick = 0.0;  // the bar the run began on
    u32 nextSchool = 1;
    u32 lastBar = 0;
    u32 sentries = 0;  // stress: Sentries alive
    bool trios = false;  // Off and Stress runs: whether Breaks still open Trios (the director's always do)
};

class World {
public:
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // `species` must outlive the world. No allocation afterwards.
    void init(const SpeciesTable* species);
    // A fresh run on the bar line `startTick`: empty pools, players at the start, the spawner reset.
    void resetRun(u64 seed, double startTick, u32 players = 1, u8 presentMask = 0xF);
    void tick(const TickInput& in);
    // RETRY (GAME_DESIGN 4.3): the run starts again on the first bar line at least half a bar away (so the
    // band, told now, can start its Groove on the same line).
    void requestRestart();
    bool restartPending() const { return restartPending_; }
    double restartTick() const { return restartAt_; }

    Rules& rules() { return rules_; }
    Spawner& spawner() { return spawner_; }
    const Spawner& spawner() const { return spawner_; }
    const SpeciesTable& species() const { return *species_; }

    const Enemies& enemies() const { return enemies_; }
    const Bullets& bullets() const { return bullets_; }
    const Shots& shots() const { return shots_; }
    const Shards& shards() const { return shards_; }
    const PlayerState& player(u32 i) const { return players_[i]; }
    PlayerState& playerMut(u32 i) { return players_[i]; }
    u32 playerCount() const { return playerCount_; }
    // M4 client: how long a predicted kill waits for the host's verdict (the session follows the round trip).
    void setPredictUndoS(float s) { predictUndoS_ = s; }
    float predictUndoS() const { return predictUndoS_; }
    const SpatialGrid& grid() const { return grid_; }
    // Play test 1: a player's live shots as the last tick ended (the shot pool's per-player caps, tuning.h).
    u32 shotsLive(u32 p) const { return p < kMaxPlayers ? shotsLive_[p] : 0u; }
    const Event* events() const { return events_; }
    u32 eventCount() const { return eventCount_; }
    const WorldCounters& counters() const { return counters_; }

    u64 tickCount() const { return tick_; }
    double music() const { return m1_; }
    float bpm() const { return bpm_; }
    float ticksPerSecond() const { return bpm_ * static_cast<float>(kTicksPerBeat) / 60.0f; }
    u64 seed() const { return seed_; }
    // The hit's dip and the Build's last bar (GAME_DESIGN 4.1: the game runs at 85% speed) slow the world.
    float timeScale() const { return allDownDipS_ > 0.0f ? 0.5f : (dipS_ > 0.0f || phrase_.buildLastBar() ? 0.85f : 1.0f); }
    // Threat of the solid enemies inside the nominal view around player 0 (band intensity).
    float threatInView() const { return threatInView_; }
    // The crowd (GAME_DESIGN 8, M3 stage C2): the enemies counted in the neediest player's frame (grown by
    // count.frameScale, with those still warping in) at the last bar line, and the target it closed on then.
    u32 crowdInFrame() const { return crowdInFrame_; }
    float crowdGoal() const { return crowdGoal_; }

    // M3 stage B: the director, upgrades, Boosts, the boss.
    const DirectorState& director() const { return director_; }
    DirectorState& directorMut() { return director_; }
    const Phrase& phrase() const { return phrase_; }  // the bar being played
    u32 runBar() const { return phrase_.bar; }
    Phrase phraseOfBar(u32 bar) const { return phraseAt(director_, bar); }
    u32 barOf(double tick) const;  // run bar of a musical tick
    double tickOfBar(u32 bar) const { return director_.startTick + static_cast<double>(bar) * kTicksPerBar; }
    float tempoAt(double tick) const { return sim::tempoAt(director_, tick); }
    const TrioState& trio(u32 p) const { return trio_[p]; }
    const BoostPickups& boostPickups() const { return boostPickups_; }
    const CellPickups& cellPickups() const { return cellPickups_; }
    // Play test 2: a ship inside one of its own level-up Trio's orb rings (the shelter: it cannot be hurt).
    bool sheltered(u32 p) const;
    // A player's Regen period in bars now (0: no Regen); the loop stretches it (GAME_DESIGN 8).
    u32 regenPeriod(u32 p) const;
    // Pictures and tests: a Cell for a player; Shards collected as if picked up (the level meter).
    void dropCellNow(Vec2 pos, u8 owner) { spawnCell(pos, Vec2{}, owner); }
    void collectShardsNow(u32 p, u32 n) {
        for (u32 k = 0; k < n; ++k) collectShard(p);
    }
    const BossState& boss() const { return boss_; }
    const BossData& bossData() const { return species_->boss; }
    // The lead grid (musical ticks per shot) a player fires on now: 1/16, moved along by Double-Time and
    // the Surge's gear (capped at 1/32), halved by Hyper (GAME_DESIGN 4.1, 6.2, 6.4).
    double leadGrid(u32 p) const;
    // Pictures and tests: give a player an upgrade or a Boost, open a Trio, bring the boss in now.
    void grantUpgrade(u32 p, u8 upgrade) { applyUpgrade(p, upgrade); }
    void grantBoost(u32 p, u8 boost) { activateBoost(p, boost); }
    void openTrioNow(u32 p, double closeTick, const u8* forced = nullptr, u8 kind = kTrioBreak);
    void spawnBossNow(double enterTick, Vec2 pos);
    void dropBoost(Vec2 pos, u8 type);
    // Boss geometry for the view, the band and tests.
    float bossArmAngle(u32 arm) const;                // arm k (0..5) now
    bool bossResting(u32 pair, double tick) const;    // pair p rests (white-hot, triple damage)
    u8 bossFiringMask(double stepTick) const;         // pairs whose tips fire on the step at `stepTick`
    u8 bossBeamState(u32 pair, double tick) const;    // 0 none, 1 telegraph, 2 firing
    u32 bossFightBar(double tick) const;
    // Tests: a shot's path from a to b against the boss, with its damage; returns the part it hit (0 none,
    // 1 lens plate, 2 arm, 3 core).
    u8 shootBoss(Vec2 a, Vec2 b, float damage);
    // Pictures: kill these pairs (bits 0-2) and, with core, the core, inside the next tick (so its events are
    // seen like a kill in play).
    void killBossNextTick(u8 pairs, bool core) {
        scriptKillPairs_ = pairs;
        scriptKillCore_ = core;
    }

    // ---- M4 co-op (TECH_SPEC 8.1, GAME_DESIGN 10; world_net.cpp) ----------------------------------------
    // The world's role and the players this device owns: their hits, Shards, Trios, Boost orbs, revives and
    // the damage their shots and Riffs do. Solo: every player is local (the M3 game, unchanged).
    void setNet(NetRole role, u8 localMask);
    NetRole netRole() const { return role_; }
    bool isLocal(u32 p) const { return ((localMask_ >> p) & 1u) != 0; }
    u32 viewPlayer() const { return viewPlayer_; }  // the first local player: the camera's ship, the band's intensity
    u16 runId() const { return runId_; }
    // A run on the bar line `bar` (musical ticks) with this seed, these seats and this id: every device resets
    // at the same musical position (the co-op start; a RETRY when every player is down).
    void scheduleRun(double bar, u64 seed, u32 players, u16 runId, u8 presentMask = 0xF);
    u8 restartMask() const { return restartMask_; }
    u64 restartSeed() const { return restartSeed_; }
    u32 restartPlayers() const { return restartPlayers_; }
    u16 restartRunId() const { return nextRunId_; }
    // An operation from the network for the next tick (false: the queue is full, counted).
    bool netPush(const NetOp& op);
    u32 netPending() const { return netOpCount_; }
    // What the ticks since the last clear produced for the network (the session drains and clears it).
    NetLog& netLog() { return netLog_; }
    // Host: an enemy as it is now. Client: the host's state sampled at musical tick `hostMusic`, applied (the
    // simulation takes it, the drawn position keeps its place through a decaying offset; above 96 wu it
    // snaps). Returns the distance between the two before the correction (-1: not here, or gone).
    bool netSample(u32 index, NetEnemyState& out) const;
    float netCorrect(const NetEnemyState& s, double hostMusic);
    // The director's state as the host has it (the phrase, the Track, the boss's place in the run).
    struct DirectorSync {
        double startTick = 0.0;
        u32 trackBar = 0, changeBar = kNoBar;
        u8 track = 1;
        bool bossSpawned = false;
        float assist = 1.0f;
        bool operator==(const DirectorSync& o) const {
            return startTick == o.startTick && trackBar == o.trackBar && changeBar == o.changeBar && track == o.track &&
                   bossSpawned == o.bossSpawned && assist == o.assist;
        }
    };
    DirectorSync directorSync() const;
    void netDirector(const DirectorSync& d);  // client
    // Client: the boss as the host has it (health never above what this device has already taken off).
    void netBossState(const float pairHp[3], float coreHp, Vec2 pos);
    // A player whose device left: out of the run (enemies forget it, its frame stops counting).
    void netSetPresent(u32 p, bool present);
    // Every enemy alive (and every predicted death the host has not answered) as an order-free digest: equal on
    // host and client when their enemy sets agree (the session's census).
    u64 netCensus(u32& count) const;
    i32 indexOfSerial(u32 serial) const;

    // Handles (TECH_SPEC 7.1): -1 when the handle is stale.
    i32 indexOf(u32 handle) const;

    // Spawning (spawner, picture scripts, tests). `landTick` is the beat on which it becomes solid
    // (R4); landTick <= music() makes it solid at once. Returns the dense index or -1 when full.
    i32 spawnEnemy(u16 species, Vec2 pos, float heading, double landTick);
    bool spawnBullet(Vec2 pos, Vec2 vel, float advanceS, float lifeS = 0.0f);  // 0: an enemy bullet's life
    bool spawnShard(Vec2 pos, Vec2 vel, u8 owner);
    void setEnemyHp(u32 i, float hp) { enemies_.hp[i] = hp; }
    void killEnemy(u32 i, u8 killer);

    u64 hash() const;
    bool finite() const;  // no NaN or infinity in any position or velocity

private:
    // world.cpp
    void applyPlayers(const TickInput& in);
    bool pressNova(u32 p);  // R6: invulnerable at once, the blast on the next beat (false: none to spend)
    void spawnShots(const TickInput& in);
    void fireShot(u32 p, float advanceS, double stepTick);
    void updateNovas();
    void moveProjectiles(float dt);
    void collide();
    void hitPlayer(u32 p, Vec2 from);
    void compact();
    void emit(const Event& e);
    void removeEnemyAt(u32 i);
    void dropShards(u32 i, u8 owner);
    // world_enemies.cpp
    void landEnemy(u32 i);  // becomes solid: the family's motion starts
    void updateEnemies(float dt);
    void flock(u32 i, const Species& s, float dt);
    i32 nearestPlayer(float x, float y) const;
    bool engagedBy(float x, float y) const;
    float tickAgeOf(double tick) const;  // seconds from `tick` to the end of this tick (>= 0)
    // world_spawner.cpp
    void runSpawner();
    void runStress();
    void runDirector();
    void directorBar(double barTick, u32 bar);
    void directorBeat(double beatTick);
    void spendBudget(double landTick);
    void spendCrowd(double landTick, const Phrase& ph);
    void spawnCrowd(u16 species, u32 count, double landTick, Vec2 frameCentre, u32 framePlayer = 0);
    i32 spawnEnemyAs(u16 species, Vec2 pos, float heading, double landTick, u32 serial);  // serial 0: the next one
    u32 crowdCount(Vec2 frameCentre) const;  // the crowd counted for a frame centred there
    void replaceCrowd(double beatTick);
    void strayFodder();
    void megaWave(double landTick);
    void spotlight(double landTick, const Phrase& next);
    void dissolveFodder(float progress);
    i32 pickSpecies(int kind);  // 0 any, 1 fodder (the crowd), 2 threats (hunters and specials)
    void spawnFormation(u16 species, u32 count, Vec2 anchor, double landTick);
    void spawnShape(u16 species, u32 count, double landTick);
    Vec2 spawnAnchor(Vec2 player);
    // world_upgrades.cpp
    void openTrio(u32 p, double closeTick, u8 kind = kTrioBreak, u16 level = 0);
    void updateTrios();
    // Play test 2 (world_upgrades.cpp): Shard levels and their Trios, the orbs' shelter, Cells, Regen.
    void collectShard(u32 p);
    void openLevelTrios(u32 bar);
    void shelterTrios();
    void cellClock();               // every tick: the Cells' drop clock per player
    void regenBar();                // every bar line: Regen
    void burnHalos();               // a loop Track's first downbeat: the halos burn down (GAME_DESIGN 8)
    bool dropCellFor(u32 p, Vec2 at, u32& rngState);  // a death near p: its Cell, when one is due
    void spawnCell(Vec2 at, Vec2 vel, u8 owner);
    void updateCells(float dt);
    u32 cellsLoose(u32 p) const;
    void takeOrb(u32 p, u32 k);
    void applyUpgrade(u32 p, u8 upgrade);
    void updateRiffs(float dt);
    void updateBoosts();
    void activateBoost(u32 p, u8 type);
    bool haloBlocks(u32 p, Vec2 from);
    void hitEnemy(u32 i, float dmg, u8 owner, Vec2 at, Vec2 dir);
    void spawnShotRaw(u32 p, Vec2 pos, Vec2 dir, float advanceS, float damage, u8 kind, u8 flags, float life, u8 pierce,
                      u8 bounces);
    // Play test 1: a beat shot's burst (On-Beat Crit) around its hit; the nearest other enemy a Ricochet goes on to.
    void critBurst(u32 hit, u8 owner, Vec2 at, float damage);
    i32 ricochetTarget(u32 hit, Vec2 at) const;
    void orbitalBullets(const PlayerState& pl, u32 n);
    // world_boss.cpp
    void updateBoss(float dt);
    void bossFire(double stepTick);
    bool bossHitTest(Vec2 a, Vec2 b, float r, u8& part, u8& pair, float& t) const;
    void damageBoss(u8 part, u8 pair, float dmg, u8 owner, Vec2 at, Vec2 dir);
    void damageBossFinal(u8 part, u8 pair, float dmg, u8 owner, Vec2 at, Vec2 dir, float scale);  // the multiplier applied
    void bossContact();
    void killBossPair(u8 pair, u8 owner);
    void killBoss(u8 owner);
    // world_net.cpp (M4)
    bool coop() const { return playerCount_ > 1; }
    u32 presentCount() const;
    i32 firstAlive() const;
    u32 aliveCap() const;          // the director's cap on live bodies (grows with the seats)
    float coopBudgetScale() const;  // GAME_DESIGN 8: x (1 + 0.6 x (players - 1))
    i32 targetOf(u32 i);            // the player enemy i attends to (solo: the nearest)
    Vec2 formationCentre(u32* player = nullptr);  // the player the next formation goes to (solo: player 0)
    void applyNetOps();
    void applyNetOp(const NetOp& op);
    void applyPlayerOp(const NetOp& op);
    void finishKill(u32 i, u8 killer, u8 kind);  // client: the host's verdict
    void predictKill(u32 i, u8 killer, bool dissolve);
    void updatePredictions();
    void logSpawn(u32 i, double land);
    void logDeath(u32 i, u8 killer, u8 kind);
    void logClaim(u32 i, float damage, u8 kind);
    void finishNetLog();  // before compact(): the spawns' group and flags
    void updateDowned();
    void downPlayer(u32 p);
    void revivePlayer(u32 p, bool byTeammate);
    void checkAllDown();
    void spendCrowdCoop(double landTick, float target, u32 lost, u32 killed);
    void idInsert(u32 serial, u32 slot);
    void idErase(u32 serial);
    i32 idSlot(u32 serial) const;

    const SpeciesTable* species_ = nullptr;
    Rules rules_;
    Spawner spawner_;
    DirectorState director_;
    Phrase phrase_;
    TrioState trio_[kMaxPlayers];
    BoostPickups boostPickups_;
    CellPickups cellPickups_;
    BossState boss_;
    u8 hitShotFlags_ = 0;  // play test 2: the flags of the shot whose hit hitEnemy() is applying (its Impact's Prism lane)
    // The loop's ramps this tick (GAME_DESIGN 8; 1 in the first pass): enemy speed, enemy bullet speed, shooters' counts.
    float loopSpeed_ = 1.0f, loopBulletSpeed_ = 1.0f, loopShooters_ = 1.0f;
    u32 shooterCount(u32 base, u32& rngState) const;  // a volley's bullets with the loop's ramp, inside the pool
    Rng rng_{1};         // the director's stream
    Rng upgradeRng_{2};  // Trios and Boost types (their own stream, so a pick never moves a spawn)
    u64 seed_ = 1;
    u64 tick_ = 0;
    u64 runTicks_ = 0;  // ticks since this run began (M4: the same on every device)
    double m0_ = 0.0, m1_ = 0.0;
    float bpm_ = 120.0f;
    float latencyTicks_ = 0.0f;
    float dipS_ = 0.0f;
    float allDownDipS_ = 0.0f;  // M4: all down, time at 50% for one beat (look.md 6)
    float predictUndoS_ = 0.30f;  // M4 client: a predicted death unconfirmed this long is undone (tuning::kPredictUndoS at least)
    float threatInView_ = 0.0f;
    u32 shotsLive_[kMaxPlayers] = {};  // play test 1: each player's live shots (counted in compact(), added on spawn)
    u32 crowdInFrame_ = 0;
    float crowdGoal_ = 0.0f;
    bool restartPending_ = false;
    double restartAt_ = 0.0;
    u8 scriptKillPairs_ = 0;
    bool scriptKillCore_ = false;
    bool restarted_ = false;  // this tick began a new run
    u32 serial_ = 0;
    u32 playerCount_ = 1;
    PlayerState players_[kMaxPlayers];
    WorldCounters counters_;

    Enemies enemies_;
    Bullets bullets_;
    Shots shots_;
    Shards shards_;
    SpatialGrid grid_;
    u8 solid_[kMaxEnemies] = {};

    static constexpr u32 kMaxEvents = 8192;
    Event events_[kMaxEvents];
    u32 eventCount_ = 0;

    // M4 co-op.
    NetRole role_ = NetRole::Solo;
    u8 localMask_ = 0xF;
    u32 viewPlayer_ = 0;
    u16 runId_ = 0, nextRunId_ = 0;
    u8 restartMask_ = 0xF;  // M4: the seats present in the next run
    u64 restartSeed_ = 0;
    u32 restartPlayers_ = 1;
    u32 formationTurn_ = 0;
    bool applyingClaim_ = false;  // host: a client's hit is applied (its own copy of the shot showed the impact)
    NetLog netLog_;
    static constexpr u32 kNetOps = 4096;
    NetOp netOps_[kNetOps];
    u32 netOpCount_ = 0;
    // Serial -> slot (co-op): open addressing with linear probing; key 0 is empty (serials start at 1).
    static constexpr u32 kIdSlots = 8192;
    u32 idKey_[kIdSlots] = {};
    u16 idSlot_[kIdSlots] = {};
};

}  // namespace rf::sim
