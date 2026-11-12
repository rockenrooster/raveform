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
#include "sim/grid.h"
#include "sim/sim_types.h"
#include "sim/species.h"

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
};

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
enum EnemyFlag : u8 { kFlagDead = 1, kFlagEngaged = 2 };

struct Enemies {
    static constexpr u32 kCap = kMaxEnemies;
    u32 count = 0;
    float x[kCap], y[kCap], px[kCap], py[kCap];  // current and previous tick
    float vx[kCap], vy[kCap];                    // the family's own motion
    float kx[kCap], ky[kCap];                    // knockback (panic pulse), decaying
    float heading[kCap], pheading[kCap];
    float hp[kCap], radius[kCap];
    float a[kCap], b[kCap];                      // per-family values (see world_enemies.cpp)
    double until[kCap];                          // musical tick at which the current state ends
    u16 species[kCap];
    u16 group[kCap];                             // a Shoal's school
    u8 state[kCap], flags[kCap];
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

struct Shots {
    static constexpr u32 kCap = kMaxShots;  // one player in M3; co-op adds a pool per player
    u32 count = 0;
    float x[kCap], y[kCap], px[kCap], py[kCap], dx[kCap], dy[kCap];
    float speed[kCap], life[kCap], damage[kCap];
    u8 owner[kCap], dead[kCap];
};

struct Shards {
    static constexpr u32 kCap = kMaxShards;
    u32 count = 0;
    float x[kCap], y[kCap], px[kCap], py[kCap], vx[kCap], vy[kCap];
    double born[kCap];  // musical tick
    u8 owner[kCap];     // loot is instanced per player (GAME_DESIGN 10)
    u8 homing[kCap], dead[kCap];
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
    u32 chain = 0, bestChain = 0;
    u64 score = 0;
    u32 kills = 0, hitsTaken = 0, pickups = 0, shotsFired = 0, novasUsed = 0, dashes = 0, perfectDashes = 0;
    bool invulnerable() const { return invulS > 0.0f || novaInvulS > 0.0f || dashing; }
};

struct Rules {
    bool godMode = false;           // hits are detected but cost nothing (bench_stress)
    bool novas = true;
    float scoreMultiplier = 1.0f;   // Surge x2 (GAME_DESIGN 4.1, stage B)
    float magnetMultiplier = 1.0f;  // Surge x2
};

struct WorldCounters {
    u32 enemiesDropped = 0, bulletsDropped = 0, shotsDropped = 0, shardsDropped = 0, eventsDropped = 0;
    u32 kills = 0, deaths = 0, spawned = 0, restarts = 0;
};

// ---------------------------------------------------------------------------------------------- spawner

// The simple spawner of M3 stage A (not the director, which replaces it): on every bar line it spends
// the first-slice threat budget (species.json firstSlice.threatBudgetPerBar, Groove x 1.0) on formations
// from the roster, with debuts by elapsed bars (Track 1: four Short cycles of 24 bars, Track 2: three
// Standard cycles of 48). Stress mode holds kStressEnemies around the ship instead.
struct Spawner {
    enum class Mode : u8 { Off, Slice, Stress };
    Mode mode = Mode::Slice;
    double startTick = 0.0;  // the bar the run began on
    float bank = 0.0f;
    i32 pending = -1;  // the next formation's species, waiting until the bank buys it
    u32 nextSchool = 1;
    u32 lastBar = 0;
    u8 track = 1, cycle = 1;
    float budgetPerBar = 0.0f;
    u32 sentries = 0;  // stress: Sentries alive
};

class World {
public:
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // `species` must outlive the world. No allocation afterwards.
    void init(const SpeciesTable* species);
    // A fresh run on the bar line `startTick`: empty pools, players at the start, the spawner reset.
    void resetRun(u64 seed, double startTick, u32 players = 1);
    void tick(const TickInput& in);
    // "Tap to retry" (GAME_DESIGN 4.3, stage A): the run starts again on the next bar line.
    void requestRestart() { restartPending_ = true; }
    bool restartPending() const { return restartPending_; }

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
    const SpatialGrid& grid() const { return grid_; }
    const Event* events() const { return events_; }
    u32 eventCount() const { return eventCount_; }
    const WorldCounters& counters() const { return counters_; }

    u64 tickCount() const { return tick_; }
    double music() const { return m1_; }
    float bpm() const { return bpm_; }
    float ticksPerSecond() const { return bpm_ * static_cast<float>(kTicksPerBeat) / 60.0f; }
    u64 seed() const { return seed_; }
    float timeScale() const { return dipS_ > 0.0f ? 0.85f : 1.0f; }
    // Threat of the solid enemies inside the nominal view around player 0 (band intensity, stage A).
    float threatInView() const { return threatInView_; }

    // Handles (TECH_SPEC 7.1): -1 when the handle is stale.
    i32 indexOf(u32 handle) const;

    // Spawning (spawner, picture scripts, tests). `landTick` is the beat on which it becomes solid
    // (R4); landTick <= music() makes it solid at once. Returns the dense index or -1 when full.
    i32 spawnEnemy(u16 species, Vec2 pos, float heading, double landTick);
    bool spawnBullet(Vec2 pos, Vec2 vel, float advanceS);
    bool spawnShard(Vec2 pos, Vec2 vel, u8 owner);
    void setEnemyHp(u32 i, float hp) { enemies_.hp[i] = hp; }
    void killEnemy(u32 i, u8 killer);

    u64 hash() const;
    bool finite() const;  // no NaN or infinity in any position or velocity

private:
    // world.cpp
    void applyPlayers(const TickInput& in);
    void spawnShots(const TickInput& in);
    void fireShot(u32 p, float advanceS);
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
    void spawnFormation(u16 species, u32 count, Vec2 anchor, double landTick);
    Vec2 spawnAnchor(Vec2 player);

    const SpeciesTable* species_ = nullptr;
    Rules rules_;
    Spawner spawner_;
    Rng rng_{1};  // the spawner's stream
    u64 seed_ = 1;
    u64 tick_ = 0;
    double m0_ = 0.0, m1_ = 0.0;
    float bpm_ = 120.0f;
    float latencyTicks_ = 0.0f;
    float dipS_ = 0.0f;
    float threatInView_ = 0.0f;
    bool restartPending_ = false;
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
};

}  // namespace rf::sim
