// Unit tests for the simulation (M3): the spatial grid, the pools and their handles, the Chain and score
// rules, the lead rule on the sim's grid, the Nova's timing, the species table, a short determinism
// check and an allocation-free tick. Run by scripts/test.ps1 (with raveform_headless --sim determinism).
// Exit code: the number of failures (0 = pass).

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>

#include "core/alloc_counter.h"
#include "game/rng.h"
#include "sim/boss_prose.h"
#include "sim/grid.h"
#include "sim/sim_runner.h"
#include "sim/species.h"
#include "sim/tuning.h"
#include "sim/world.h"

using namespace rf;
using namespace rf::sim;

namespace {

int g_checks = 0, g_failures = 0;
const char* g_test = "";

#define CHECK(cond)                                                                      \
    do {                                                                                 \
        ++g_checks;                                                                      \
        if (!(cond)) {                                                                   \
            ++g_failures;                                                                \
            std::printf("  FAIL [%s] %s:%d: %s\n", g_test, __FILE__, __LINE__, #cond); \
        }                                                                                \
    } while (0)
#define CHECK_MSG(cond, ...)                                                             \
    do {                                                                                 \
        ++g_checks;                                                                      \
        if (!(cond)) {                                                                   \
            ++g_failures;                                                                \
            std::printf("  FAIL [%s] %s:%d: %s: ", g_test, __FILE__, __LINE__, #cond);  \
            std::printf(__VA_ARGS__);                                                    \
            std::printf("\n");                                                           \
        }                                                                                \
    } while (0)

SpeciesTable* g_species = nullptr;
constexpr float kBpm = 120.0f;
constexpr double kTicksPerSimTick = 120.0 * 96.0 / 60.0 / 60.0;  // 3.2 musical ticks per 1/60 s at 120 BPM

// A world with the spawner off and one player standing at the origin.
World* quietWorld() {
    auto* w = new (std::nothrow) World;
    w->init(g_species);
    w->spawner().mode = Spawner::Mode::Off;
    w->resetRun(7, 0.0, 1);
    return w;
}

// One tick with player 0 at `pos`, firing `fire` along +x.
void stepWorld(World& w, double& music, Vec2 pos, u8 fire = 0, bool nova = false) {
    TickInput in;
    in.music0 = music;
    in.music1 = music + kTicksPerSimTick;
    in.bpm = kBpm;
    in.players[0].present = true;
    in.players[0].pos = pos;
    in.players[0].aim = Vec2{1.0f, 0.0f};
    in.players[0].fire = fire;
    in.players[0].novaPressed = nova;
    w.tick(in);
    music = in.music1;
}

u32 countEvents(const World& w, EventType t) {
    u32 n = 0;
    for (u32 i = 0; i < w.eventCount(); ++i) n += w.events()[i].type == t ? 1u : 0u;
    return n;
}

// ---------------------------------------------------------------------------------------------- grid

void testGrid() {
    g_test = "grid";
    static float x[SpatialGrid::kMaxItems], y[SpatialGrid::kMaxItems];
    static u8 include[SpatialGrid::kMaxItems];
    static u8 seen[SpatialGrid::kMaxItems];
    auto* grid = new (std::nothrow) SpatialGrid;
    Rng rng(42);
    const u32 n = 3000;
    for (u32 i = 0; i < n; ++i) {
        // Mostly inside the arena, some on and beyond the walls (they land in the border cells).
        x[i] = rng.range(-kArenaHalfW - 100.0f, kArenaHalfW + 100.0f);
        y[i] = rng.range(-kArenaHalfH - 100.0f, kArenaHalfH + 100.0f);
        include[i] = rng.uniform() < 0.9f ? 1 : 0;
    }
    // Clusters, as a Shoal makes them.
    for (u32 i = 0; i < 200; ++i) {
        x[i] = 10.0f + rng.range(-30.0f, 30.0f);
        y[i] = -20.0f + rng.range(-30.0f, 30.0f);
    }
    grid->build(x, y, n, include);
    u32 expected = 0;
    for (u32 i = 0; i < n; ++i) expected += include[i];
    CHECK(grid->itemCount() == expected);
    // Every included item exactly once over the whole arena, in ascending order per cell.
    std::memset(seen, 0, sizeof seen);
    u32 total = 0;
    bool ascending = true;
    for (u32 cy = 0; cy < kGridRows; ++cy) {
        for (u32 cx = 0; cx < kGridCols; ++cx) {
            i32 last = -1;
            const float x0 = -kArenaHalfW + cx * kCellWu + 1.0f, y0 = -kArenaHalfH + cy * kCellWu + 1.0f;
            grid->query(x0, y0, x0, y0, [&](u32 i) {
                ++seen[i];
                ++total;
                ascending = ascending && static_cast<i32>(i) > last;
                last = static_cast<i32>(i);
            });
        }
    }
    CHECK(total == expected);
    CHECK(ascending);
    bool once = true;
    for (u32 i = 0; i < n; ++i) once = once && seen[i] == include[i];
    CHECK(once);
    // Random rectangle queries against brute force: nothing inside the rectangle is missed.
    u32 missed = 0, queries = 0;
    for (u32 q = 0; q < 2000; ++q) {
        const float cx = rng.range(-kArenaHalfW, kArenaHalfW), cy = rng.range(-kArenaHalfH, kArenaHalfH);
        const float r = rng.range(1.0f, 300.0f);
        std::memset(seen, 0, sizeof seen);
        grid->query(cx - r, cy - r, cx + r, cy + r, [&](u32 i) { seen[i] = 1; });
        for (u32 i = 0; i < n; ++i) {
            if (!include[i]) {
                if (seen[i]) ++missed;  // left out of the build, never reported
                continue;
            }
            const float px = clampf(x[i], -kArenaHalfW, kArenaHalfW - 0.01f), py = clampf(y[i], -kArenaHalfH, kArenaHalfH - 0.01f);
            if (std::fabs(px - cx) <= r && std::fabs(py - cy) <= r && !seen[i]) ++missed;
        }
        ++queries;
    }
    CHECK_MSG(missed == 0, "%u items missed over %u queries", missed, queries);
    // Cell arithmetic at the edges.
    CHECK(SpatialGrid::cellX(-kArenaHalfW) == 0);
    CHECK(SpatialGrid::cellX(kArenaHalfW) == kGridCols - 1);
    CHECK(SpatialGrid::cellY(-1e9f) == 0);
    CHECK(SpatialGrid::cellY(1e9f) == kGridRows - 1);
    CHECK(kGridCols * kCellWu >= 2.0f * kArenaHalfW && kGridRows * kCellWu >= 2.0f * kArenaHalfH);
    delete grid;
}

// ---------------------------------------------------------------------------------------------- pools

void testPools() {
    g_test = "pools";
    World* w = quietWorld();
    double music = 0.0;
    const u16 drifter = SpeciesTable::index(Family::Drifter, 1, kVariantA);
    u32 handles[64];
    for (u32 i = 0; i < 64; ++i) {
        const i32 k = w->spawnEnemy(drifter, Vec2{1000.0f + 20.0f * i, 500.0f}, 0.0f, 0.0);
        CHECK(k == static_cast<i32>(i));
        handles[i] = w->enemies().handle[k];
    }
    for (u32 i = 0; i < 64; ++i) CHECK(w->indexOf(handles[i]) == static_cast<i32>(i));
    // Kill every third; after the tick they are swap-removed and their handles are stale, the others
    // still find their own enemy (identified by spawn number).
    u32 serial[64];
    for (u32 i = 0; i < 64; ++i) serial[i] = w->enemies().serial[i];
    for (u32 i = 0; i < 64; i += 3) w->killEnemy(i, 0xFF);
    stepWorld(*w, music, Vec2{});
    CHECK(w->enemies().count == 64 - 22);
    u32 stale = 0, found = 0;
    for (u32 i = 0; i < 64; ++i) {
        const i32 k = w->indexOf(handles[i]);
        if (i % 3 == 0) {
            stale += k < 0 ? 1u : 0u;
        } else if (k >= 0 && w->enemies().serial[k] == serial[i]) {
            ++found;
        }
    }
    CHECK(stale == 22);
    CHECK(found == 42);
    // A freed slot comes back with a new generation: the old handle stays stale.
    const i32 again = w->spawnEnemy(drifter, Vec2{-1000.0f, 0.0f}, 0.0f, 0.0);
    CHECK(again >= 0);
    const u32 h = w->enemies().handle[again];
    bool reusedSlot = false;
    for (u32 i = 0; i < 64; i += 3) reusedSlot = reusedSlot || (handles[i] & 0xFFFFu) == (h & 0xFFFFu);
    CHECK(reusedSlot);
    for (u32 i = 0; i < 64; i += 3) CHECK(w->indexOf(handles[i]) < 0);
    // Full pool: spawning fails cleanly and is counted.
    const u32 before = w->counters().enemiesDropped;
    u32 made = 0;
    for (u32 i = 0; i < kMaxEnemies + 10; ++i)
        made += w->spawnEnemy(drifter, Vec2{0.0f, 2000.0f}, 0.0f, 0.0) >= 0 ? 1u : 0u;
    CHECK(w->enemies().count == kMaxEnemies);
    CHECK(made == kMaxEnemies - (64 - 22) - 1);
    CHECK(w->counters().enemiesDropped - before == 10 + (64 - 22) + 1);
    // Bullets: capacity, swap-remove after their life.
    World* b = quietWorld();
    double m2 = 0.0;
    u32 ok = 0;
    for (u32 i = 0; i < kMaxEnemyBullets + 5; ++i) ok += b->spawnBullet(Vec2{3000.0f, 0.0f}, Vec2{0.0f, 10.0f}, 0.0f) ? 1u : 0u;
    // (M5 part 7: the crowd's bullets stop at 95% of the pool, the rest kept for the fights' patterns)
    const u32 crowdMax = static_cast<u32>(std::ceil(tuning::kBulletPoolCrowd * static_cast<float>(kMaxEnemyBullets)));
    CHECK_MSG(ok == crowdMax, "%u bullets", ok);
    CHECK(b->counters().bulletsDropped == 0 && b->counters().bulletsHeld == kMaxEnemyBullets + 5 - crowdMax);
    for (u32 t = 0; t < 250; ++t) stepWorld(*b, m2, Vec2{});
    CHECK(b->bullets().count == 0);  // four seconds of life
    delete b;
    delete w;
}

// ---------------------------------------------------------------------------------------------- Chain and score

void testChainRules() {
    g_test = "chain";
    World* w = quietWorld();
    double music = 0.0;
    stepWorld(*w, music, Vec2{});
    // The Chain is a multiplier from x1 (GAME_DESIGN 5, v2.3). Shards on the ship: +1 each.
    CHECK(w->player(0).chain == 1);
    for (u32 i = 0; i < 5; ++i) w->spawnShard(Vec2{2.0f, 1.0f}, Vec2{}, 0);
    stepWorld(*w, music, Vec2{});
    CHECK(w->player(0).chain == 6);
    CHECK(w->player(0).bestChain == 6);
    CHECK(countEvents(*w, EventType::Pickup) == 5);
    // Score = value x Chain.
    const u16 hound = SpeciesTable::index(Family::Hound, 2, kVariantB);
    const float value = g_species->get(hound).score;
    i32 k = w->spawnEnemy(hound, Vec2{600.0f, 0.0f}, 0.0f, 0.0);
    w->killEnemy(static_cast<u32>(k), 0);
    CHECK(w->player(0).score == static_cast<u64>(std::llround(value * 6.0f)));
    // A fresh Chain scores x1, and a hit at x1 leaves it at x1.
    World* z = quietWorld();
    double mz = 0.0;
    stepWorld(*z, mz, Vec2{});
    k = z->spawnEnemy(hound, Vec2{600.0f, 0.0f}, 0.0f, 0.0);
    z->killEnemy(static_cast<u32>(k), 0);
    CHECK(z->player(0).score == static_cast<u64>(std::llround(value)));
    z->spawnBullet(Vec2{0.0f, 0.0f}, Vec2{}, 0.0f);
    stepWorld(*z, mz, Vec2{});
    CHECK(z->player(0).cells == kStartCells - 1);
    CHECK(z->player(0).chain == 1);
    delete z;
    // The kill dropped its Shards (2 for tier II), loose until the ship comes within the magnet.
    stepWorld(*w, music, Vec2{});
    CHECK(w->shards().count == g_species->get(hound).shards);
    // Nothing decays the Chain with time.
    for (u32 t = 0; t < 600; ++t) stepWorld(*w, music, Vec2{});
    CHECK(w->player(0).chain == 6);
    // Nine more: 15. A hit halves it (rounded down), costs a cell, grants invulnerability.
    for (u32 i = 0; i < 9; ++i) w->spawnShard(Vec2{0.0f, 0.0f}, Vec2{}, 0);
    stepWorld(*w, music, Vec2{});
    CHECK(w->player(0).chain == 15);
    w->spawnBullet(Vec2{0.0f, 0.0f}, Vec2{}, 0.0f);
    stepWorld(*w, music, Vec2{});
    CHECK(w->player(0).chain == 7);
    CHECK(w->player(0).bestChain == 15);
    CHECK(w->player(0).cells == kStartCells - 1);
    CHECK(w->player(0).invulnerable());
    CHECK(countEvents(*w, EventType::Hit) == 1);
    // Invulnerable for 2.5 s: a second bullet (passing through) does nothing.
    w->spawnBullet(Vec2{0.0f, 0.0f}, Vec2{0.0f, 400.0f}, 0.0f);
    stepWorld(*w, music, Vec2{});
    CHECK(w->player(0).chain == 7);
    // Magnet: a Shard 80 wu away homes in and is collected; one 200 wu away stays loose.
    for (u32 t = 0; t < 120; ++t) stepWorld(*w, music, Vec2{});
    const u32 chain0 = w->player(0).chain;
    w->spawnShard(Vec2{80.0f, 0.0f}, Vec2{}, 0);
    w->spawnShard(Vec2{-200.0f, 0.0f}, Vec2{}, 0);
    u32 before = 0;
    for (u32 i = 0; i < w->shards().count; ++i) before += w->shards().x[i] == -200.0f ? 1u : 0u;
    for (u32 t = 0; t < 30; ++t) stepWorld(*w, music, Vec2{});
    CHECK(w->player(0).chain >= chain0 + 1);
    bool farStays = false;
    for (u32 i = 0; i < w->shards().count; ++i) farStays = farStays || std::fabs(w->shards().x[i] + 200.0f) < 1.0f;
    CHECK(before == 1 && farStays);
    // Loose Shards fade after four bars (8 s at 120 BPM).
    for (u32 t = 0; t < 8 * 60 + 10; ++t) stepWorld(*w, music, Vec2{});
    CHECK(w->shards().count == 0);
    // Death: a hit with no cell left (and no Nova in stock: Auto Nova would fire one instead).
    PlayerState& p = w->playerMut(0);
    p.cells = 0;
    p.novas = 0;
    p.invulS = 0.0f;
    w->spawnBullet(Vec2{0.0f, 0.0f}, Vec2{}, 0.0f);
    stepWorld(*w, music, Vec2{});
    CHECK(!w->player(0).alive);
    CHECK(countEvents(*w, EventType::Death) == 1);
    delete w;
}

// ---------------------------------------------------------------------------------------------- lead rule, Nova

void testLeadRule() {
    g_test = "lead rule";
    // Fire held for one bar (2 s at 120 BPM = 120 ticks): a shot on each of the 16 sixteenths. (The clock
    // starts off the grid so that no step falls on a tick boundary, where float rounding decides.)
    World* w = quietWorld();
    double music = 1.0;
    u32 shots = 0;
    for (u32 t = 0; t < 120; ++t) {
        stepWorld(*w, music, Vec2{}, 1);
        shots += countEvents(*w, EventType::Shot);
    }
    CHECK_MSG(shots == 16, "%u shots in a bar", shots);
    // The assist's half-rate grid: every other step.
    shots = 0;
    for (u32 t = 0; t < 120; ++t) {
        stepWorld(*w, music, Vec2{}, 2);
        shots += countEvents(*w, EventType::Shot);
    }
    CHECK_MSG(shots == 8, "%u shots in a bar at half rate", shots);
    // Each shot is advanced by the time since its step: positions differ from the muzzle by speed x age.
    stepWorld(*w, music, Vec2{}, 0);
    bool advanced = true;
    for (u32 t = 0; t < 10; ++t) {
        const double before = music;
        stepWorld(*w, music, Vec2{}, 1);
        for (u32 i = 0; i < w->eventCount(); ++i) {
            const Event& e = w->events()[i];
            if (e.type != EventType::Shot) continue;
            const double step = std::floor(music / 24.0) * 24.0;
            const float age = static_cast<float>((music - step) / (96.0 * 2.0));
            advanced = advanced && step > before && std::fabs(e.age - age) < 1e-4f;
        }
    }
    CHECK(advanced);
    // Band shots replace the sim's own grid.
    TickInput in;
    in.music0 = music;
    in.music1 = music + kTicksPerSimTick;
    in.bpm = kBpm;
    in.players[0].present = true;
    in.players[0].fire = 1;
    in.players[0].aim = Vec2{0.0f, 1.0f};
    in.bandShots = true;
    in.shotCount = 3;
    w->tick(in);
    CHECK(countEvents(*w, EventType::Shot) == 3);
    delete w;
}

void testNova() {
    g_test = "nova";
    World* w = quietWorld();
    double music = 10.0;  // 86 ticks before beat 1
    stepWorld(*w, music, Vec2{});
    // Bullets and enemies around the ship.
    const u16 drifter = SpeciesTable::index(Family::Drifter, 1, kVariantA);
    for (u32 i = 0; i < 20; ++i) w->spawnEnemy(drifter, Vec2{300.0f + 15.0f * i, 100.0f}, 0.0f, 0.0);
    for (u32 i = 0; i < 40; ++i) w->spawnBullet(Vec2{-400.0f + 10.0f * i, -150.0f}, Vec2{0.0f, 0.1f}, 0.0f);
    w->spawnEnemy(drifter, Vec2{2500.0f, 0.0f}, 0.0f, 0.0);  // out of view: survives
    stepWorld(*w, music, Vec2{}, 0, true);
    CHECK(w->player(0).novaPending);
    CHECK(w->player(0).novas == kStartNovas - 1);
    CHECK(w->player(0).invulnerable());
    CHECK(w->player(0).novaBlastTick == 96.0);
    CHECK(countEvents(*w, EventType::NovaPress) == 1);
    // Nothing until the beat.
    while (music + kTicksPerSimTick < 96.0) {
        stepWorld(*w, music, Vec2{});
        CHECK(countEvents(*w, EventType::NovaBlast) == 0);
    }
    u32 blasts = 0, kills = 0;
    for (u32 t = 0; t < 40; ++t) {
        stepWorld(*w, music, Vec2{});
        blasts += countEvents(*w, EventType::NovaBlast);
        kills += countEvents(*w, EventType::Kill);
    }
    CHECK(blasts == 1);
    CHECK_MSG(kills == 20, "%u kills", kills);
    CHECK(w->bullets().count == 0);
    CHECK(w->enemies().count == 1);
    // Too close to a beat (within six ticks of it): the following beat.
    World* z = quietWorld();
    double mz = 84.0;  // the press lands at 90.4: beat 96 is under six ticks away
    stepWorld(*z, mz, Vec2{});
    stepWorld(*z, mz, Vec2{}, 0, true);
    CHECK(z->player(0).novaBlastTick == 192.0);
    delete z;
    delete w;
}

// ---------------------------------------------------------------------------------------------- species

void testSpecies() {
    g_test = "species";
    const SpeciesTable& s = *g_species;
    const i32 hb = s.find("hound_2_B");
    CHECK(hb >= 0);
    if (hb >= 0) {
        const Species& h = s.get(static_cast<u16>(hb));
        // species.json: family x tier x variant.
        const FamilyData& f = s.family(Family::Hound);
        CHECK(std::fabs(h.threat - f.threat * 1.8f * 1.5f) < 1e-4f);
        CHECK(std::fabs(h.score - f.score * 2.5f * 1.5f) < 1e-4f);
        CHECK(std::fabs(h.health - f.health * 2.0f) < 1e-4f);
        CHECK(h.shards == 2);
        CHECK(h.ranged());
        CHECK(h.killSize() == 0);
    }
    // M5 (species.json, was species_m5.json): the whole set list's roster, budgets and crowd for ten Tracks.
    CHECK_MSG(s.rosterCount == 106, "roster %u", s.rosterCount);
    CHECK(s.budgetTracks == 10 && s.crowd.targetTracks == 10);
    CHECK(s.budgetCycles[0] == 3 && s.budgetCycles[1] == 3);  // play test 2 (species.json): Track 1 has three cycles
    CHECK(s.get(SpeciesTable::index(Family::Ram, 3, kVariantA)).killSize() == 1);
    CHECK(s.get(SpeciesTable::index(Family::Ram, 5, kVariantA)).killSize() == 2);
    CHECK(s.family(Family::Sentry).patternArms == 3);
    CHECK(s.bulletSpeed > 0.0f);
}

// ---------------------------------------------------------------------------------------------- determinism, allocation

void testDeterminismShort() {
    g_test = "determinism";
    auto* a = new (std::nothrow) World;
    auto* b = new (std::nothrow) World;
    a->init(g_species);
    b->init(g_species);
    SimRunner ra, rb;
    ra.start(a, 11, Spawner::Mode::Slice, Bot::Policy::Play, kBpm);
    rb.start(b, 11, Spawner::Mode::Slice, Bot::Policy::Play, kBpm);
    bool same = true;
    for (u32 t = 0; t < 3600; ++t) {
        ra.step();
        rb.step();
        if (t % 300 == 299) same = same && a->hash() == b->hash();
    }
    CHECK(same);
    CHECK(a->counters().spawned > 10);
    CHECK(a->player(0).shotsFired > 100);
    delete a;
    delete b;
    // screens.md "Transitions (M6)", the drop-in's grace: nothing spawns in a run's first bar, and in its first two bars
    // nothing hostile warps in within 600 wu of the ship.
    {
        World* w = quietWorld();
        w->spawner().mode = Spawner::Mode::Slice;
        w->rules().godMode = true;
        double music = 0.0;
        const double start = w->director().startTick;
        u32 firstBar = 0, near = 0, seen = 0;
        while (w->music() < start + 2.0 * kTicksPerBar) {
            stepWorld(*w, music, Vec2{});  // (a still ship at the start)
            if (w->music() >= start + 2.0 * kTicksPerBar) break;  // (what the third bar's line orders is outside the grace)
            if (w->music() < start + kTicksPerBar) firstBar = w->counters().spawned;
            const Enemies& e = w->enemies();
            for (u32 i = 0; i < e.count; ++i) {
                if ((e.flags[i] & kFlagDead) || e.state[i] != kWarping) continue;
                ++seen;
                if (length(Vec2{e.x[i], e.y[i]} - w->player(0).pos) < tuning::kGraceClearWu - 1.0f) ++near;
            }
        }
        CHECK_MSG(firstBar == 0 && near == 0 && seen > 0, "%u spawned in the first bar, %u of %u arrivals within 600 wu", firstBar, near, seen);
        delete w;
    }
}

void testNoAllocations() {
    g_test = "allocations";
    auto* w = new (std::nothrow) World;
    w->init(g_species);
    SimRunner r;
    r.start(w, 3, Spawner::Mode::Stress, Bot::Policy::Stress, kBpm, true);
    for (u32 t = 0; t < 60; ++t) r.step();
    const u64 a0 = heapAllocCountThisThread();
    for (u32 t = 0; t < 1200; ++t) r.step();
    const u64 allocs = heapAllocCountThisThread() - a0;
    CHECK_MSG(allocs == 0, "%llu allocations in 1,200 stress ticks", static_cast<unsigned long long>(allocs));
    CHECK(w->enemies().count > 500);
    CHECK(w->finite());
    delete w;
}

// ---------------------------------------------------------------------------------------------- M3 stage B

void testDirectorCycle() {
    g_test = "director";
    DirectorState d;
    // Track 1: two Short cycles of 8/4/8/4 bars at 120 BPM (48 bars; play test 3: it had three).
    Phrase p = phraseAt(d, 0);
    CHECK(p.cycles == 2);
    CHECK(p.track == 1 && p.song == 0 && p.cycle == 1 && p.section == Section::Groove && p.sectionBars == 8);
    p = phraseAt(d, 8);
    CHECK(p.section == Section::Build && p.sectionBar == 0 && p.sectionBars == 4 && p.bpm == 120.0f);
    p = phraseAt(d, 11);  // the Build's last bar runs at 85%
    CHECK(p.section == Section::Build && p.buildLastBar() && std::fabs(p.bpm - 102.0f) < 1e-3f);
    // Surge: the Drop's first half, gear 1 then gear 2.
    CHECK(phraseAt(d, 12).section == Section::Drop && phraseAt(d, 12).surgeGear() == 1 && phraseAt(d, 13).surgeGear() == 1);
    CHECK(phraseAt(d, 14).surgeGear() == 2 && phraseAt(d, 15).surgeGear() == 2 && phraseAt(d, 16).surgeGear() == 0);
    CHECK(!phraseAt(d, 12).bossDrop);
    CHECK(phraseAt(d, 20).section == Section::Break && phraseAt(d, 23).section == Section::Break);
    CHECK(phraseAt(d, 24).section == Section::Groove && phraseAt(d, 24).cycle == 2);
    {
        // M7 (--drop T.C --drop-hold, the thermal check): the held cycle's Drop never ends.
        DirectorState h;
        h.holdDropCycle = 1;
        CHECK(phraseAt(h, 12).section == Section::Drop && phraseAt(h, 300).section == Section::Drop && phraseAt(h, 300).cycle == 1 &&
              !phraseAt(h, 300).bossDrop && phraseAt(h, 8).section == Section::Build);
    }
    // Track 1 has no mini-boss; its second (last) Drop is the boss's: held past its eight bars while the boss lives.
    p = phraseAt(d, 36);
    CHECK(p.cycle == 2 && p.section == Section::Drop && p.bossDrop && !p.miniDrop);
    p = phraseAt(d, 49);
    CHECK(p.section == Section::Drop && p.bossDrop && p.sectionBar == 13);
    // The boss died: the Track-change Break (8 bars, 120 -> 126 BPM, bar by bar), then Track 2.
    d.changeBar = 50;
    p = phraseAt(d, 50);
    CHECK(p.trackChange && p.section == Section::Break && p.track == 2 && p.sectionBars == 8 && p.bpm == 120.0f);
    CHECK(std::fabs(phraseAt(d, 57).bpm - 126.0f) < 1e-3f);
    // Every Track Short since play test 3: 8 / 4 / 8 / 4 bars, 24 bars a cycle; two cycles a Track since c2f113a (48 bars).
    p = phraseAt(d, 58);
    CHECK(!p.trackChange && p.track == 2 && p.song == 1 && p.cycle == 1 && p.section == Section::Groove && p.sectionBars == 8);
    CHECK(phraseAt(d, 58 + 8).section == Section::Build && phraseAt(d, 58 + 8).sectionBars == 4 &&
          phraseAt(d, 58 + 12).section == Section::Drop && phraseAt(d, 58 + 12).sectionBars == 8 &&
          phraseAt(d, 58 + 20).section == Section::Break && phraseAt(d, 58 + 20).sectionBars == 4);
    CHECK(phraseAt(d, 58 + 24).section == Section::Groove && phraseAt(d, 58 + 24).cycle == 2);
    // c2f113a: the mini-boss on the first cycle's Drop, the boss on the second's.
    CHECK(phraseAt(d, 58 + 24 + 12).bossDrop && phraseAt(d, 58 + 24 + 12).cycle == 2);
    CHECK(phraseAt(d, 58 + 12).section == Section::Drop && !phraseAt(d, 58 + 12).bossDrop && phraseAt(d, 58 + 12).miniDrop);
    // Track 3 is Reef (two Short cycles at 132 BPM); then the loop over all three, 4 BPM a Track faster.
    CHECK(bpmOfTrack(3, 3) == 132.0f && bpmOfTrack(4, 3) == 136.0f && bpmOfTrack(5, 3) == 140.0f && bpmOfTrack(40, 3) == 174.0f);
    CHECK(songOfTrack(3, 3) == 2 && cyclesOfTrack(3) == 2 && formOfTrack(3, 3) == 0 && cyclesOfTrack(1) == 2);
    CHECK(songOfTrack(4, 3) == 0 && cyclesOfTrack(4) == 2 && formOfTrack(4, 3) == 0 && songOfTrack(5, 3) == 1 && songOfTrack(6, 3) == 2);
    // Budgets (species.json firstSlice, threats only), rising past the data; section scales (GAME_DESIGN 8).
    const SpeciesTable& b = *g_species;
    CHECK(baseBudget(b, 1, 1, 3) == 2.0f && baseBudget(b, 1, 2, 3) == 3.5f && baseBudget(b, 1, 3, 3) == 6.0f);
    CHECK(baseBudget(b, 2, 3, 3) == 12.0f);
    CHECK(baseBudget(b, 3, 1, 3) == 14.0f && baseBudget(b, 3, 3, 3) == 18.0f);
    // Past a set list of three: Track 3's last plus a step a cycle (the first slice's rule); with ten, the data (M5).
    CHECK(baseBudget(b, 4, 1, 3) == 18.0f + tuning::kEndlessBudgetStep && baseBudget(b, 4, 1, 10) == 20.0f && baseBudget(b, 10, 3, 10) == 60.0f);
    Phrase s;
    s.section = Section::Build;
    s.sectionBars = 4;
    s.sectionBar = 0;
    CHECK(sectionScale(s) == 1.0f);
    s.sectionBar = 3;
    CHECK(std::fabs(sectionScale(s) - 1.6f) < 1e-5f);
    s.section = Section::Drop;
    s.sectionBars = 8;
    s.sectionBar = 0;
    CHECK(sectionScale(s) == 2.0f);
    s.sectionBar = 7;
    CHECK(std::fabs(sectionScale(s) - 1.2f) < 1e-5f);
    s.bossDrop = true;
    CHECK(std::fabs(sectionScale(s) - 0.3f) < 1e-5f);
    s.section = Section::Break;
    CHECK(sectionScale(s) == 0.0f);

    // In the world: the sections arrive in order on their bar lines, the first Break's Trio opens with the first
    // Break (bar 20, 40 s in), the Drop doubles score and magnet. (Play test 2: level-up Trios open in the fight too.)
    auto* w = new (std::nothrow) World;
    w->init(g_species);
    SimRunner r;
    r.start(w, 5, Spawner::Mode::Slice, Bot::Policy::Play, kBpm, true);
    u8 order[16];
    u32 n = 0, trioTick = 0;
    bool surgeRules = false;
    for (u32 t = 0; t < 60 * 50; ++t) {
        r.step();
        for (u32 k = 0; k < w->eventCount(); ++k) {
            const Event& ev = w->events()[k];
            if (ev.type == EventType::Section && n < 16) order[n++] = ev.a;
            if (ev.type == EventType::TrioOpen && ev.c == kTrioBreak && !trioTick) trioTick = t;
        }
        if (w->phrase().surge() && w->rules().scoreMultiplier == 2.0f && w->rules().magnetMultiplier == 2.0f) surgeRules = true;
    }
    CHECK(n >= 5 && order[0] == 0 && order[1] == 1 && order[2] == 2 && order[3] == 3 && order[4] == 0);
    // a667116 (the owner: "Remove The Break's Trio"): the first cycle's Break (40 s in) deals none; the boss's Trio comes later.
    CHECK_MSG(trioTick == 0, "a Break Trio at tick %u", trioTick);
    CHECK(surgeRules);
    delete w;
}

void testCrowd() {
    g_test = "crowd";
    // Targets (species.json firstSlice.crowd): by Track and cycle, then rising a step a Track up to the max.
    const SpeciesTable::Crowd& c = g_species->crowd;
    CHECK(crowdTarget(c, 1, 1, 3) == 20.0f && crowdTarget(c, 1, 2, 3) == 60.0f && crowdTarget(c, 1, 3, 3) == 120.0f);
    CHECK(crowdTarget(c, 2, 1, 3) == 150.0f && crowdTarget(c, 3, 3, 3) == 300.0f);
    CHECK(crowdTarget(c, 4, 1, 3) == 320.0f && crowdTarget(c, 30, 1, 3) == 400.0f);
    // M5: with ten Tracks in the set list their own targets (species.json Tracks 4 to 10), the max from Track 8.
    CHECK(crowdTarget(c, 4, 1, 10) == 310.0f && crowdTarget(c, 7, 3, 10) == 390.0f && crowdTarget(c, 8, 2, 10) == 400.0f && crowdTarget(c, 11, 1, 10) == 400.0f);
    Phrase p;
    p.section = Section::Groove;
    CHECK(crowdScale(c, p) == 1.0f);
    p.section = Section::Build;
    p.sectionBars = 4;
    p.sectionBar = 0;
    CHECK(crowdScale(c, p) == 1.0f);
    p.sectionBar = 3;
    CHECK(std::fabs(crowdScale(c, p) - 1.3f) < 1e-5f);
    p.section = Section::Drop;
    p.sectionBars = 8;
    p.sectionBar = 0;
    CHECK(std::fabs(crowdScale(c, p) - 1.6f) < 1e-5f);
    p.sectionBar = 7;
    CHECK(std::fabs(crowdScale(c, p) - 1.2f) < 1e-5f);
    p.bossDrop = true;
    CHECK(std::fabs(crowdScale(c, p) - 0.5f) < 1e-5f);
    p.section = Section::Break;
    p.bossDrop = false;
    CHECK(crowdScale(c, p) == 0.0f);

    // In the world: an idle, invulnerable ship in Track 1's first Groove; the director brings the crowd to
    // its target (20) over a few bars, the threats only from the budget.
    auto* w = new (std::nothrow) World;
    w->init(g_species);
    w->spawner().mode = Spawner::Mode::Slice;
    w->resetRun(9, 0.0, 1);
    w->rules().godMode = true;
    double music = 0.0;
    const double perBar = kTicksPerBar / kTicksPerSimTick;
    // M3 stage C2: the crowd arrives at the frame (species.json arrive), never within 260 wu of the ship and
    // never beyond the stray frame (twice the frame).
    u32 arrivals = 0, unfair = 0;
    for (u32 t = 0; t < static_cast<u32>(6.0 * perBar); ++t) {
        stepWorld(*w, music, Vec2{});
        const Enemies& e = w->enemies();
        for (u32 i = 0; i < e.count; ++i) {
            if ((e.flags[i] & kFlagDead) || e.state[i] != kWarping || g_species->get(e.species[i]).hue != 0) continue;
            ++arrivals;
            const float dx = std::fabs(e.x[i]), dy = std::fabs(e.y[i]);
            // (A formation scatters its pack up to 110 wu round its anchor.)
            if (dx * dx + dy * dy < 259.0f * 259.0f || dx > 2.0f * tuning::kFrameHalfWWu + 110.0f || dy > 2.0f * tuning::kFrameHalfHWu + 110.0f) ++unfair;
        }
    }
    CHECK_MSG(arrivals > 0 && unfair == 0, "%u of %u warping fodder samples too near or too far", unfair, arrivals);
    CHECK_MSG(w->crowdInFrame() >= 15 && w->crowdInFrame() <= 45, "in the frame %u (goal %.0f)", w->crowdInFrame(), w->crowdGoal());
    u32 fodder = 0, threats = 0;
    for (u32 i = 0; i < w->enemies().count; ++i) {
        if (w->enemies().flags[i] & kFlagDead) continue;
        (g_species->get(w->enemies().species[i]).hue == 0 ? fodder : threats) += 1;
    }
    CHECK_MSG(fodder >= 10 && threats >= 1, "fodder %u, threats %u", fodder, threats);
    // Stranded: the ship moves far away; the fodder left behind dissolves without Shards within a few bars.
    const u32 shards0 = w->shards().count;
    u32 dissolved = 0;
    for (u32 t = 0; t < static_cast<u32>(6.0 * perBar); ++t) {
        stepWorld(*w, music, Vec2{4200.0f, 2300.0f});
        for (u32 k = 0; k < w->eventCount(); ++k)
            if (w->events()[k].type == EventType::Kill && w->events()[k].c == 1) ++dissolved;
    }
    CHECK_MSG(dissolved >= fodder / 2, "dissolved %u of %u", dissolved, fodder);
    CHECK(w->shards().count <= shards0);
    delete w;
}

void testTrioRules() {
    g_test = "trio";
    Rng rng;
    rng.reseed(11);
    // a667116 (GAME_DESIGN 6.1, the owner): the first Trio of a run offers Split and Double-Time; the third orb is drawn as the
    // others are, never one of those two again.
    for (u32 run = 0; run < 200; ++run) {
        Loadout l;
        u8 since = 0;
        u8 o[kMaxOrbs];
        const u32 n = drawTrio(l, since, rng, o, true);
        CHECK_MSG(n == 3 && o[0] == kSplit && o[1] == kDoubleTime && o[2] != kSplit && o[2] != kDoubleTime && o[2] < kUpgradeCount,
                  "%u orbs: %u %u %u", n, o[0], o[1], o[2]);
    }
    // A fresh loadout: three different options, at least one weapon, a new Riff within three Trios.
    for (u32 run = 0; run < 400; ++run) {
        Loadout l;
        u8 since = 0;
        bool riffSeen = false;
        for (u32 trio = 0; trio < 3; ++trio) {
            u8 o[kMaxOrbs];
            const u32 n = drawTrio(l, since, rng, o);
            CHECK(n == 3);
            CHECK(o[0] != o[1] && o[1] != o[2] && o[0] != o[2]);
            // M5 (GAME_DESIGN 6.1): the first orb is firepower (a staple, a held weapon or rhythm Mod, a held Riff).
            CHECK((firepower(o[0]) && (l.mods[o[0]] > 0 || o[0] == kSplit || o[0] == kDoubleTime || o[0] == kOvercharge || o[0] == kOnBeatCrit)) ||
                  (isRiff(o[0]) && l.level(o[0]) > 0));
            for (u32 k = 0; k < 3; ++k) riffSeen = riffSeen || (isRiff(o[k]) && l.level(o[k]) == 0);
            // Take a Mod that is not a Riff when there is one (so the guarantee is what brings the Riff; M5: not Lucky
            // Break, which would make it four orbs).
            for (u32 k = 0; k < 3; ++k)
                if (!isRiff(o[k]) && o[k] != kLuckyBreak) {
                    ++l.mods[o[k]];
                    break;
                }
        }
        CHECK(riffSeen);
    }
    // Only non-weapon Mods left: they are offered, and (play test 2) the third orb offers a Boost.
    Loadout full;
    for (u32 u = 0; u < kModCount; ++u) full.mods[u] = upgradeInfo(static_cast<u8>(u)).cap;
    for (u8& r : full.riffs) r = kRiffMaxLevel;
    full.mods[kExtraCell] = 0;
    full.mods[kMagnet] = 0;
    full.fusions = 0xFF;  // M5: every Fusion taken (no gold orb); Lucky Break at its cap: five orbs
    u8 since = 0, o[kMaxOrbs];
    CHECK(drawTrio(full, since, rng, o) == 5);
    CHECK((o[0] == kExtraCell || o[0] == kMagnet) && (o[1] == kExtraCell || o[1] == kMagnet) && isBoostOrb(o[2]) && isBoostOrb(o[3]) &&
          isBoostOrb(o[4]));
    // Every Mod and Riff at its cap: three different Boosts.
    full.mods[kExtraCell] = upgradeInfo(kExtraCell).cap;
    full.mods[kMagnet] = upgradeInfo(kMagnet).cap;
    for (u32 k = 0; k < 50; ++k) {
        CHECK(drawTrio(full, since, rng, o) == 5);
        bool distinct = true;
        for (u32 a = 0; a < 5; ++a) {
            distinct = distinct && isBoostOrb(o[a]);
            for (u32 b = a + 1; b < 5; ++b) distinct = distinct && o[a] != o[b];
        }
        CHECK(distinct);
    }
    // M5 (GAME_DESIGN 6.5): both ingredients at their caps bring the Fusion as one more (gold) orb, once; taken, never again.
    {
        Loadout f;
        f.mods[kSplit] = upgradeInfo(kSplit).cap;
        f.mods[kWave] = upgradeInfo(kWave).cap;
        u8 s = 0, fo[kMaxOrbs];
        const u32 fn = drawTrio(f, s, rng, fo);
        CHECK(fn == 4 && fo[3] == kFusionOrb + kHelixArray);
        f.fusions = static_cast<u8>(1u << kHelixArray);
        CHECK(drawTrio(f, s, rng, fo) == 3);
        f.mods[kLuckyBreak] = 2;
        CHECK(drawTrio(f, s, rng, fo) == 5);
    }
    // A capped Mod and a maxed Riff are never offered.
    Loadout some;
    some.mods[kSplit] = upgradeInfo(kSplit).cap;
    some.riffs[riffIndex(kOrbitals)] = kRiffMaxLevel;
    for (u32 k = 0; k < 300; ++k) {
        u8 s2 = 0;
        const u32 n = drawTrio(some, s2, rng, o);
        for (u32 j = 0; j < n; ++j) CHECK(o[j] != kSplit && o[j] != kOrbitals);
    }
}

void testUpgradeStacking() {
    g_test = "upgrades";
    World* w = quietWorld();
    double music = 0.0;
    stepWorld(*w, music, Vec2{});
    // Play test 1 (GAME_DESIGN 5, 6.2): 4 cells and 3 Novas at the start (capacity 4), the Dash in two beats.
    CHECK(w->player(0).cells == 4 && w->player(0).cellCap == 4 && w->player(0).novas == 3 && w->player(0).novaCap == 4);
    CHECK(kDashRechargeBeats == 2.0);
    // Caps: Split stacks to 4, a lane a rank (2, 3, 4, 5 lanes; eb10554), Double-Time to 2 again; Extra Cell raises the capacity
    // and refills every cell, four times (play test 2: eight cells).
    for (u32 k = 0; k < 6; ++k) w->grantUpgrade(0, kSplit);
    CHECK(w->player(0).loadout.mods[kSplit] == 4 && splitLanes(1) == 2 && splitLanes(3) == 4 && splitLanes(4) == 5 &&
          upgradeInfo(kDoubleTime).cap == 2 && upgradeInfo(kFork).cap == 1);
    w->playerMut(0).cells = 1;
    w->grantUpgrade(0, kExtraCell);
    CHECK_MSG(w->player(0).cellCap == 5 && w->player(0).cells == 5, "cells %u of %u", w->player(0).cells, w->player(0).cellCap);
    for (u32 k = 0; k < 6; ++k) w->grantUpgrade(0, kExtraCell);
    CHECK(w->player(0).cellCap == 8 && w->player(0).cells == 8 && w->player(0).loadout.mods[kExtraCell] == 4 && kMaxCells == 8);
    CHECK(std::strcmp(upgradeWords(kExtraCell, 4), "+1 CELL, ALL REFILLED") == 0);
    // The numbers per stack and level.
    CHECK(pierceCount(1) == 2 && ricochetBounces(3) == 6 && onBeatCritMultiplier(1) == 3.0f && onBeatCritMultiplier(3) == 8.0f);
    CHECK(orbitalCount(1) == 3 && orbitalCount(5) == 11 && sentinelBurst(1) == 3 && sentinelBurst(4) == 5);
    CHECK(sentinelDrones(2) == 1 && sentinelDrones(3) == 2 && sentinelDrones(5) == 3);
    CHECK(std::strcmp(upgradeWords(kSplit, 1), "+1 SHOT LANE") == 0 && std::strcmp(upgradeWords(kSplit, 4), "+1 SHOT LANE") == 0 &&
          std::strcmp(upgradeWords(kMagnet, 2), "SHARD PULL X3") == 0);
    CHECK(std::strcmp(upgradeWords(kDoubleTime, 1), "FIRE RATE X1.5") == 0 && std::strcmp(upgradeWords(kDoubleTime, 2), "FIRE RATE X2") == 0);
    CHECK(std::strcmp(upgradeWords(kOnBeatCrit, 2), "BEAT SHOTS X5") == 0 && std::strcmp(upgradeWords(kOrbitals, 2), "LEVEL 2: FIVE SHARDS") == 0 &&
          std::strcmp(upgradeWords(kOrbitals, 1), "NEW RIFF: A GUARD RING") == 0 && std::strcmp(upgradeWords(kOrbitals, 3), "LEVEL 3: RELIGHTS FASTER") == 0);
    CHECK(std::strcmp(upgradeWords(kSentinel, 5), "LEVEL 5: THIRD DRONE") == 0 && std::strcmp(upgradeWords(kPulseRing, 5), "LEVEL 5: TWICE AS OFTEN") == 0);
    // Riffs level 1..5.
    for (u32 k = 0; k < 7; ++k) w->grantUpgrade(0, kPulseRing);
    CHECK(w->player(0).loadout.riffs[riffIndex(kPulseRing)] == kRiffMaxLevel);
    // Split at its cap: five lanes per step.
    u32 made = 0;
    for (u32 t = 0; t < 30 && !made; ++t) {
        stepWorld(*w, music, Vec2{}, 1);
        for (u32 k = 0; k < w->eventCount(); ++k)
            if (w->events()[k].type == EventType::Shot) made = w->events()[k].b;
    }
    CHECK_MSG(made == 5, "Split 4: %u lanes", made);
    World* p = quietWorld();
    double mp = 0.0;
    stepWorld(*p, mp, Vec2{});
    for (u32 k = 0; k < 4; ++k) p->grantUpgrade(0, kSplit);
    p->grantBoost(0, kPrism);
    u32 volley = 0;
    for (u32 t = 0; t < 30 && !volley; ++t) {
        stepWorld(*p, mp, Vec2{}, 1);
        for (u32 k = 0; k < p->eventCount(); ++k)
            if (p->events()[k].type == EventType::Shot) volley = p->events()[k].b;
    }
    CHECK_MSG(volley == 25, "Split 4 x Prism: %u shots", volley);
    // The lead grid: Double-Time moves it 1/16 -> triplets -> 1/32 (eb10554: two ranks again); Hyper halves it.
    CHECK(p->leadGrid(0) == 24.0);
    p->grantUpgrade(0, kDoubleTime);
    CHECK(p->leadGrid(0) == 16.0);
    p->grantUpgrade(0, kDoubleTime);
    CHECK(p->leadGrid(0) == 12.0);
    p->grantUpgrade(0, kDoubleTime);
    CHECK(p->leadGrid(0) == 12.0 && p->player(0).loadout.mods[kDoubleTime] == 2);
    p->grantBoost(0, kHyper);
    CHECK(p->leadGrid(0) == 6.0);
    // Two Boosts at once: a third replaces the oldest (Prism, taken first).
    p->grantBoost(0, kHalo);
    CHECK(!p->player(0).boost(kPrism) && p->player(0).boost(kHyper) && p->player(0).boost(kHalo));
    // Halo absorbs three hits, then it is gone and the next costs a cell.
    const u8 cells = p->player(0).cells;
    for (u32 hit = 0; hit < 4; ++hit) {
        for (u32 t = 0; t < 60; ++t) stepWorld(*p, mp, Vec2{});
        p->spawnBullet(Vec2{0.0f, 0.0f}, Vec2{}, 0.0f);
        stepWorld(*p, mp, Vec2{});
    }
    CHECK_MSG(p->player(0).cells == cells - 1, "cells %u -> %u", cells, p->player(0).cells);
    CHECK(!p->player(0).boost(kHalo));
    // Pierce (one stack passes two): one shot goes through the first of two Drifters in its lane and kills both.
    World* q = quietWorld();
    double mq = 0.0;
    stepWorld(*q, mq, Vec2{});
    q->grantUpgrade(0, kPierce);
    const u16 drifter = SpeciesTable::index(Family::Drifter, 1, kVariantA);
    q->spawnEnemy(drifter, Vec2{200.0f, 0.0f}, 0.0f, 0.0);
    q->spawnEnemy(drifter, Vec2{260.0f, 0.0f}, 0.0f, 0.0);
    u32 kills = 0, shots = 0;
    for (u32 t = 0; t < 40 && shots == 0; ++t) {
        stepWorld(*q, mq, Vec2{}, 1);
        for (u32 k = 0; k < q->eventCount(); ++k) shots += q->events()[k].type == EventType::Shot ? 1u : 0u;
    }
    for (u32 t = 0; t < 30; ++t) {
        stepWorld(*q, mq, Vec2{});
        kills += countEvents(*q, EventType::Kill);
    }
    CHECK_MSG(kills == 2, "pierce kills %u", kills);
    // Overcharge: +50% damage a stack; On-Beat Crit triples shots on the beat.
    World* o = quietWorld();
    double mo = 0.0;
    stepWorld(*o, mo, Vec2{});
    o->grantUpgrade(0, kOvercharge);
    o->grantUpgrade(0, kOnBeatCrit);
    bool sawCrit = false, sawPlain = false;
    for (u32 t = 0; t < 60; ++t) {
        stepWorld(*o, mo, Vec2{}, 1);
        for (u32 k = 0; k < o->shots().count; ++k) {
            const float dmg = o->shots().damage[k];
            if (std::fabs(dmg - 4.5f) < 1e-4f) sawCrit = true;
            if (std::fabs(dmg - 1.5f) < 1e-4f) sawPlain = true;
        }
    }
    CHECK(sawCrit && sawPlain);
    delete w;
    delete p;
    delete q;
    delete o;
}

void testAutoNova() {
    // Play test 1, second pass (GAME_DESIGN 5): a hit that would take the last Shield cell fires a Nova from the stock
    // instead and costs no cell (R6: invulnerable at once, the blast on the next beat); with no Nova in stock, or with
    // the switch off, the hit lands as before. Invulnerability after a hit is 2.5 s.
    g_test = "auto nova";
    CHECK(kInvulnerableS == 2.5f);
    const auto hitOnce = [&](World& w, double& music) {
        PlayerState& pl = w.playerMut(0);
        pl.invulS = pl.novaInvulS = 0.0f;
        w.spawnBullet(Vec2{}, Vec2{}, 0.0f);
        stepWorld(w, music, Vec2{});
    };
    {
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->playerMut(0).cells = 1;
        const u8 novas = w->player(0).novas;
        hitOnce(*w, music);
        CHECK_MSG(w->player(0).cells == 1 && w->player(0).novas == novas - 1 && w->player(0).novaPending, "cells %u, novas %u",
                  w->player(0).cells, w->player(0).novas);
        CHECK(countEvents(*w, EventType::NovaPress) == 1 && countEvents(*w, EventType::Hit) == 0);
        CHECK(w->player(0).invulnerable());
        delete w;
    }
    {
        // A hit that does not take the last cell is a hit.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->playerMut(0).cells = 3;
        hitOnce(*w, music);
        CHECK(w->player(0).cells == 2 && !w->player(0).novaPending && countEvents(*w, EventType::Hit) == 1);
        CHECK(std::fabs(w->player(0).invulS - kInvulnerableS) < 0.05f);
        delete w;
    }
    {
        // No Nova in stock: the last cell goes.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->playerMut(0).cells = 1;
        w->playerMut(0).novas = 0;
        hitOnce(*w, music);
        CHECK(w->player(0).cells == 0 && countEvents(*w, EventType::Hit) == 1);
        delete w;
    }
    {
        // Switched off: the last cell goes, the Novas stay.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->rules().autoNova = false;
        w->playerMut(0).cells = 1;
        const u8 novas = w->player(0).novas;
        hitOnce(*w, music);
        CHECK(w->player(0).cells == 0 && w->player(0).novas == novas && countEvents(*w, EventType::NovaPress) == 0);
        delete w;
    }
    // The first boss of a run has 0.7 of its health (Track 1's; M5: x the novice's factor); the cap of the living crowd is 900 solo.
    {
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->spawnBossNow(kTicksPerBar, Vec2{600.0f, 0.0f});
        const float k = 0.7f * tuning::kKaleidosHealthTune;
        CHECK(std::fabs(w->boss().partMax - k * g_species->boss.pairHealth) < 1e-3f);
        CHECK(std::fabs(w->boss().coreMax - k * g_species->boss.coreHealth) < 1e-3f);
        delete w;
    }
    CHECK(tuning::kDirectorAliveCap == 900);
}

void testPlayTest1() {
    // Play test 1 (2026-11-12): the Trio is held to be taken, a Break gives back a cell and a Nova, Ricochet goes on
    // to the nearest enemy, a beat shot bursts, Orbitals eat bullets, and the shot pool keeps its caps.
    g_test = "playtest1";
    const u8 forced[3] = {kSplit, kMagnet, kOrbitals};
    const double lock = tuning::kTrioLockBeats * kTicksPerBeat, hold = tuning::kTrioHoldBeats * kTicksPerBeat;
    {
        // Locked for two beats: sitting on an orb from the start takes nothing until two beats after the unlock.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->openTrioNow(0, music + 8.0 * kTicksPerBar, forced);
        const TrioState& tr = w->trio(0);
        const Vec2 orb = tr.orbPos[1];
        const double open = tr.openTick;
        u32 unlocks = 0;
        double takenAt = -1.0;
        u8 taken = kNoUpgrade;
        for (u32 t = 0; t < 400 && takenAt < 0.0; ++t) {
            stepWorld(*w, music, orb);
            unlocks += countEvents(*w, EventType::TrioUnlock);
            for (u32 k = 0; k < w->eventCount(); ++k)
                if (w->events()[k].type == EventType::TrioTake) {
                    takenAt = music;
                    taken = w->events()[k].b;
                }
        }
        CHECK(unlocks == 1);
        CHECK_MSG(takenAt >= open + lock + hold - 1e-6 && takenAt < open + lock + hold + 2.0 * kTicksPerSimTick, "taken at %.1f (open %.1f)",
                  takenAt, open);
        CHECK(taken == kMagnet && w->player(0).loadout.mods[kMagnet] == 1);
        delete w;
    }
    {
        // Flying through does nothing; leaving empties the fill at once; staying two beats takes it.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->openTrioNow(0, music + 8.0 * kTicksPerBar, forced);
        const TrioState& tr = w->trio(0);
        const Vec2 orb = tr.orbPos[0], away = tr.centre;
        while (music < tr.openTick + lock + 1.0) stepWorld(*w, music, away);
        for (u32 t = 0; t < 20; ++t) stepWorld(*w, music, orb);  // a third of a beat on it
        CHECK(tr.open && tr.holdOrb == 0 && tr.holdTicks > 0.0);
        stepWorld(*w, music, away);
        CHECK(tr.open && tr.holdOrb == -1 && tr.holdTicks == 0.0);
        bool took = false;
        for (u32 t = 0; t < 80 && !took; ++t) {
            stepWorld(*w, music, orb);
            took = countEvents(*w, EventType::TrioTake) > 0;
        }
        CHECK(took && w->player(0).loadout.mods[kSplit] == 1);
        delete w;
    }
    {
        // None taken: at the Break's end the orb nearest the ship flies to it.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->openTrioNow(0, music + 2.0 * kTicksPerBar, forced);
        const TrioState& tr = w->trio(0);
        const Vec2 nearRight = tr.orbPos[2] + Vec2{120.0f, 0.0f};  // outside its ring, nearest the right orb (Orbitals)
        u8 taken = kNoUpgrade;
        for (u32 t = 0; t < 600 && taken == kNoUpgrade; ++t) {
            stepWorld(*w, music, nearRight);
            for (u32 k = 0; k < w->eventCount(); ++k)
                if (w->events()[k].type == EventType::TrioTake) taken = w->events()[k].b;
        }
        CHECK_MSG(taken == kOrbitals, "taken %u", taken);
        delete w;
    }
    {
        // A Break gives back a cell and a Nova (up to the caps): the director's world, a hit and a Nova spent first.
        auto* w = new (std::nothrow) World;
        w->init(g_species);
        w->spawner().mode = Spawner::Mode::Off;
        w->resetRun(3, 0.0, 1);
        double music = 0.0;
        stepWorld(*w, music, Vec2{}, 0, true);  // a Nova
        w->playerMut(0).cells = 2;
        u32 refills = 0;
        while (music < 20.0 * kTicksPerBar + 2.0 * kTicksPerSimTick) {
            stepWorld(*w, music, Vec2{});
            refills += countEvents(*w, EventType::Refill);
        }
        CHECK_MSG(refills == 1 && w->player(0).cells == 3 && w->player(0).novas == 3, "refills %u, cells %u, novas %u", refills,
                  w->player(0).cells, w->player(0).novas);
        delete w;
    }
    {
        // Ricochet: a shot that kills nothing outright bounces from the enemy it hit on to the nearest other one.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->grantUpgrade(0, kRicochet);
        const u16 drifter = SpeciesTable::index(Family::Drifter, 1, kVariantA);
        const i32 a = w->spawnEnemy(drifter, Vec2{300.0f, 0.0f}, 0.0f, 0.0);
        const i32 b = w->spawnEnemy(drifter, Vec2{300.0f, 200.0f}, 0.0f, 0.0);
        CHECK(a >= 0 && b >= 0);
        w->setEnemyHp(static_cast<u32>(a), 100.0f);
        w->setEnemyHp(static_cast<u32>(b), 100.0f);
        u32 shots = 0;
        for (u32 t = 0; t < 40 && shots == 0; ++t) {
            stepWorld(*w, music, Vec2{}, 1);
            shots += countEvents(*w, EventType::Shot);
        }
        u32 impacts = 0;
        bool turned = false;
        for (u32 t = 0; t < 30; ++t) {
            stepWorld(*w, music, Vec2{});
            impacts += countEvents(*w, EventType::Impact);
            for (u32 k = 0; k < w->shots().count; ++k) turned = turned || w->shots().dy[k] > 0.5f;
        }
        CHECK_MSG(turned && impacts >= 2, "turned %d, impacts %u", turned ? 1 : 0, impacts);
        delete w;
    }
    {
        // On-Beat Crit's burst: the beat shot's hit hurts the enemies within 80 wu of it.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->grantUpgrade(0, kOnBeatCrit);
        // The targets arrive just before a beat, close, so the beat's shot meets them where they were put.
        while (std::fmod(music, kTicksPerBeat) < kTicksPerBeat - 8.0) stepWorld(*w, music, Vec2{});
        const u16 drifter = SpeciesTable::index(Family::Drifter, 1, kVariantA);
        const i32 a = w->spawnEnemy(drifter, Vec2{150.0f, 0.0f}, 0.0f, 0.0);
        const i32 b = w->spawnEnemy(drifter, Vec2{150.0f, 50.0f}, 0.0f, 0.0);
        w->setEnemyHp(static_cast<u32>(a), 1000.0f);
        w->setEnemyHp(static_cast<u32>(b), 1000.0f);
        u32 bursts = 0;
        for (u32 t = 0; t < 30 && !bursts; ++t) {
            stepWorld(*w, music, Vec2{}, 1);
            bursts += countEvents(*w, EventType::CritBurst);
        }
        CHECK_MSG(bursts > 0 && w->enemies().hp[b] < 1000.0f, "bursts %u, the neighbour's health %.1f", bursts, w->enemies().hp[b]);
        delete w;
    }
    {
        // Orbitals: the shards destroy the enemy bullets they touch.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->grantUpgrade(0, kOrbitals);
        for (u32 k = 0; k < 24; ++k) {
            const float a = kTau * static_cast<float>(k) / 24.0f;
            w->spawnBullet(dirOf(a) * tuning::kOrbitalRadiusWu, Vec2{}, 0.0f, 10.0f);
        }
        const u32 before = w->bullets().count;
        for (u32 t = 0; t < 120; ++t) stepWorld(*w, music, Vec2{});
        CHECK_MSG(w->bullets().count < before, "bullets %u -> %u", before, w->bullets().count);
        delete w;
    }
    {
        // The shot pool's caps (TECH_SPEC 7.1, 1,024 a player): four players, each with Split's five lanes, Double-Time 2, Pierce,
        // Ricochet, Overcharge, Crit, every Riff at level 5, Prism and Hyper, at 174 BPM in a crowd, for 20 seconds.
        auto* w = new (std::nothrow) World;
        w->init(g_species);
        w->spawner().mode = Spawner::Mode::Off;
        w->resetRun(9, 0.0, 4);
        for (u32 p = 0; p < 4; ++p) {
            for (u32 k = 0; k < 4; ++k) w->grantUpgrade(p, kSplit);
            for (u32 k = 0; k < 2; ++k) w->grantUpgrade(p, kDoubleTime);
            for (u32 k = 0; k < 3; ++k) w->grantUpgrade(p, kPierce);
            for (u32 k = 0; k < 3; ++k) w->grantUpgrade(p, kRicochet);
            for (u32 k = 0; k < 3; ++k) w->grantUpgrade(p, kOnBeatCrit);
            for (u32 k = 0; k < 5; ++k) {
                w->grantUpgrade(p, kOrbitals);
                w->grantUpgrade(p, kPulseRing);
                w->grantUpgrade(p, kSentinel);
            }
            w->grantBoost(p, kPrism);
            w->grantBoost(p, kHyper);
        }
        w->rules().godMode = true;
        const u16 drifter = SpeciesTable::index(Family::Drifter, 1, kVariantA);
        double music = 0.0;
        const double perTick = 174.0 * 96.0 / 60.0 / 60.0;
        u32 peak = 0, peakPlayer = 0;
        for (u32 t = 0; t < 60 * 20; ++t) {
            if (t % 30 == 0)
                for (u32 k = 0; k < 40 && w->enemies().count < 500; ++k) {
                    const float a = kTau * static_cast<float>(k) / 40.0f;
                    const i32 i = w->spawnEnemy(drifter, Vec2{std::cos(a) * 420.0f, std::sin(a) * 300.0f}, a, 0.0);
                    if (i >= 0) w->setEnemyHp(static_cast<u32>(i), 30.0f);
                }
            TickInput in;
            in.music0 = music;
            in.music1 = music + perTick;
            in.bpm = 174.0f;
            for (u32 p = 0; p < 4; ++p) {
                in.players[p].present = true;
                in.players[p].pos = Vec2{static_cast<float>(p) * 40.0f, 0.0f};
                in.players[p].aim = dirOf(static_cast<float>(t) * 0.01f + static_cast<float>(p) * 1.5f);
                in.players[p].fire = 1;
            }
            w->tick(in);
            music = in.music1;
            if (w->shots().count > peak) peak = w->shots().count;
            for (u32 p = 0; p < 4; ++p) peakPlayer = w->shotsLive(p) > peakPlayer ? w->shotsLive(p) : peakPlayer;
            if (t % 240 == 239)
                for (u32 p = 0; p < 4; ++p) {
                    w->grantBoost(p, kPrism);  // the same Boost again refreshes it: Prism and Hyper all the way through
                    w->grantBoost(p, kHyper);
                }
        }
        const WorldCounters& c = w->counters();
        CHECK_MSG(c.shotsDropped == 0 && c.shotsCapped == 0 && peak <= Shots::kCap && peakPlayer <= 1024, "shots peak %u (a player %u), dropped %u, capped %u",
                  peak, peakPlayer, c.shotsDropped, c.shotsCapped);
        std::printf("[sim tests] shot pool, four players at full strength with Prism and Hyper at 174 BPM: peak %u (a player %u), "
                    "dropped %u, held back by a player's cap %u\n",
                    peak, peakPlayer, c.shotsDropped, c.shotsCapped);
        delete w;
    }
}

void testBoss() {
    g_test = "boss";
    // c9218dc (GAME_DESIGN 7.6, a boss keeps pace): only aimed damage raises the pace; the same damage done through the assist's
    // fire (fire 2) leaves the novice's boss.
    for (u8 mode = 1; mode <= 2; ++mode) {
        World* pw = quietWorld();
        pw->rules().godMode = true;
        double pm = 0.0;
        stepWorld(*pw, pm, Vec2{});
        pw->spawnBossNow(kTicksPerBar, Vec2{550.0f, 0.0f});
        const BossState& ps = pw->boss();
        i64 lastBar = -1;
        while (pm < ps.fightTick + 9.0 * kTicksPerBar && ps.active && !ps.dying) {
            stepWorld(*pw, pm, Vec2{}, mode);
            const i64 fb = pm >= ps.fightTick ? static_cast<i64>(std::floor((pm - ps.fightTick) / kTicksPerBar)) : -1;
            if (fb < 0 || fb == lastBar) continue;
            lastBar = fb;
            for (u32 arm = 0; arm < 3; ++arm) {
                const Vec2 d = dirOf(pw->bossArmAngle(arm));
                pw->shootBoss(ps.pos + d * 400.0f, ps.pos + d * 180.0f, 20.0f);
            }
        }
        CHECK_MSG(ps.paceSet && (mode == 1 ? ps.pace > 1.5f : ps.pace == 1.0f), "fire %u: pace %.2f (set %d, measured %.0f)", mode, ps.pace,
                  ps.paceSet ? 1 : 0, ps.paceMeasured);
        delete pw;
    }
    World* w = quietWorld();
    double music = 0.0;
    stepWorld(*w, music, Vec2{});
    const BossData& b = g_species->boss;
    CHECK(b.pairHealth == 400.0f && b.coreHealth == 600.0f && b.phases[0].fan == 3 && b.phases[1].fan == 5);
    CHECK(b.phases[0].fireStepTicks == 48.0 && b.phases[2].fireStepTicks == 24.0 && b.phases[3].coreBurst == 16);
    // Enters on a downbeat; a bar later it fights. One pair rests each bar, A, B, C in turn.
    w->spawnBossNow(kTicksPerBar, Vec2{550.0f, 0.0f});
    const BossState& s = w->boss();
    CHECK(s.active && s.alive == 0x7 && s.fightTick == 2.0 * kTicksPerBar);
    CHECK(w->bossResting(0, 2.0 * kTicksPerBar + 10.0) && !w->bossResting(1, 2.0 * kTicksPerBar + 10.0));
    CHECK(w->bossResting(1, 3.0 * kTicksPerBar + 10.0) && w->bossResting(2, 4.0 * kTicksPerBar + 10.0));
    CHECK(w->bossFiringMask(2.0 * kTicksPerBar + 48.0) == 0x6);  // B and C fire while A rests
    CHECK(w->bossFiringMask(2.0 * kTicksPerBar + 24.0) == 0);     // only on eighths
    // It fires: 2 pairs x 2 tips x 3 bullets on every eighth.
    while (music < 2.0 * kTicksPerBar + 49.0) stepWorld(*w, music, Vec2{});
    CHECK_MSG(w->bullets().count >= 12, "bullets %u", w->bullets().count);
    // Damage: the resting pair x3, a firing pair x1/3; a lens plate stops shots; killing a pair turns the phase.
    {
        const double fight = 2.0 * kTicksPerBar;
        (void)fight;
        const auto along = [&](u32 arm, float from, float to) {
            const Vec2 d = dirOf(w->bossArmAngle(arm));
            return std::pair<Vec2, Vec2>{s.pos + d * from, s.pos + d * to};
        };
        // Arm 0 (pair A) rests in fight bar 0: x3.
        const float a0 = s.partHp[0];
        auto seg = along(0, 400.0f, 180.0f);
        CHECK(w->shootBoss(seg.first, seg.second, 1.0f) == 2);
        CHECK_MSG(std::fabs((a0 - s.partHp[0]) - 3.0f) < 1e-3f, "resting damage %.3f", a0 - s.partHp[0]);
        // Arm 1 (pair B) fires: x1/3.
        const float b0 = s.partHp[1];
        seg = along(1, 400.0f, 180.0f);
        CHECK(w->shootBoss(seg.first, seg.second, 1.0f) == 2);
        CHECK(std::fabs((b0 - s.partHp[1]) - 1.0f / 3.0f) < 1e-3f);
        // Between the arms, at the lens ring, on a live pair's plate: stopped, no damage.
        const Vec2 d0 = dirOf(w->bossArmAngle(0) + 0.2f);
        const float core0 = s.coreHp;
        CHECK(w->shootBoss(s.pos + d0 * 75.0f, s.pos + d0 * 60.0f, 1.0f) == 1);
        CHECK(s.coreHp == core0);
        // Kill pair A: the phase turns to Refraction, its plates are gone, the core opens from its side.
        CHECK(w->shootBoss(along(0, 400.0f, 180.0f).first, along(0, 400.0f, 180.0f).second, 1000.0f) == 2);
        CHECK(s.alive == 0x6 && s.phase == 1);
        CHECK(countEvents(*w, EventType::BossPair) == 1);
        const Vec2 dA = dirOf(w->bossArmAngle(0));
        CHECK(w->shootBoss(s.pos + dA * 75.0f, s.pos + dA * 30.0f, 1.0f) == 3);
        CHECK(s.coreHp < core0);
        // The core dies: the boss is over whatever arms remain, and the Track-change Break is scheduled.
        CHECK(w->shootBoss(s.pos + dA * 75.0f, s.pos + dA * 30.0f, 5000.0f) == 3);
        CHECK(s.dying && w->director().changeBar != kNoBar);
        CHECK(w->shards().count >= tuning::kBossShards);
        // Play test 2: and two Cells, whatever the health.
        CHECK(w->cellPickups().count == tuning::kBossCells && w->cellPickups().owner[0] == 0);
    }
    delete w;
}

void testPlayTest2() {
    // Play test 2 (2026-11-12, GAME_DESIGN 6.1): Shard levels (thresholds, banking, the level-up Trio and the Break's,
    // the shelter), Cells (the drop clock, at most two loose, taken, eight bars), Regen.
    g_test = "playtest2";
    using namespace tuning;
    // Level n takes base x ratio^(n - 1) Shards, rounded; the design's 25, 31, 37, 45, 55 with 25 and 1.22.
    CHECK(levelShards(1) == static_cast<u32>(kLevelShardsBase + 0.5f));
    for (u32 n = 1; n < 40; ++n) CHECK(levelShards(n + 1) >= levelShards(n));
    CHECK(std::fabs(static_cast<double>(levelShards(10)) - kLevelShardsBase * std::pow(static_cast<double>(kLevelShardsRatio), 9.0)) <= 0.51);
    if (kLevelShardsBase == 25.0f && kLevelShardsRatio == 1.22f)
        CHECK(levelShards(2) == 31 && levelShards(3) == 37 && levelShards(4) == 45 && levelShards(5) == 55);
    const auto heldDrop = [](World& w) {
        // The boss's held Drop from the first bar line on (no Break gives a cell back), without the boss.
        w.directorMut().startTick = -60.0 * kTicksPerBar;
        w.directorMut().bossSpawned = true;
    };
    {
        // A full meter is a level; its Trio opens round the ship on the next bar line, for four bars; levels filled while
        // it is open are banked and open one at a time, each on the bar line after the last one closed.
        World* w = quietWorld();
        w->spawner().trios = true;
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        const u32 need1 = levelShards(1);
        w->collectShardsNow(0, need1 - 1);
        CHECK(w->player(0).level == 0 && w->player(0).levelShards == need1 - 1 && countEvents(*w, EventType::LevelUp) == 0);
        w->collectShardsNow(0, 1);
        CHECK(w->player(0).level == 1 && w->player(0).levelsBanked == 1 && w->player(0).levelShards == 0);
        CHECK(countEvents(*w, EventType::LevelUp) == 1);
        const TrioState& tr = w->trio(0);
        CHECK(!tr.open);
        while (!tr.open && music < 2.0 * kTicksPerBar) stepWorld(*w, music, Vec2{});
        const double line = std::floor(tr.openTick / kTicksPerBar) * kTicksPerBar;
        CHECK(tr.open && tr.kind == kTrioLevel && tr.levelShown == 1 && w->player(0).levelsBanked == 0 && tr.orbs == 3);
        CHECK(tr.openTick - line < kTicksPerSimTick + 1e-6 && std::fabs(tr.closeTick - (line + kLevelTrioBars * kTicksPerBar)) < 1e-6);
        CHECK(length(tr.centre - w->player(0).pos) < 1.0f);
        w->collectShardsNow(0, levelShards(2) + levelShards(3));
        CHECK(w->player(0).level == 3 && w->player(0).levelsBanked == 2 && tr.open && tr.levelShown == 1);
        // Taken like the Break's: the lock, then two beats on an orb.
        const Vec2 orb = tr.orbPos[0];
        bool took = false;
        for (u32 t = 0; t < 600 && !took; ++t) {
            stepWorld(*w, music, orb);
            took = countEvents(*w, EventType::TrioTake) > 0;
        }
        CHECK(took && !tr.open);
        const double takenAt = music;
        while (!tr.open && music < takenAt + 2.0 * kTicksPerBar) stepWorld(*w, music, Vec2{});
        CHECK(tr.open && tr.kind == kTrioLevel && tr.levelShown == 2 && w->player(0).levelsBanked == 1);
        CHECK(tr.openTick > takenAt && tr.openTick - std::floor(tr.openTick / kTicksPerBar) * kTicksPerBar < kTicksPerSimTick + 1e-6);
        CHECK(w->player(0).levelTrios == 2);
        delete w;
    }
    {
        // a667116: a cycle's Break deals no Trio, so a level filled within four bars of one no longer waits for it: it opens on
        // the next bar line, and no Break Trio comes.
        World* w = quietWorld();
        w->spawner().trios = true;
        double music = 0.0;
        while (music < 17.0 * kTicksPerBar + 1.0) stepWorld(*w, music, Vec2{});
        w->collectShardsNow(0, levelShards(1));
        while (music < 18.0 * kTicksPerBar + 4.0) stepWorld(*w, music, Vec2{});
        CHECK(w->trio(0).open && w->trio(0).kind == kTrioLevel && w->player(0).levelsBanked == 0);
        bool breakTrio = false;
        while (music < 24.0 * kTicksPerBar) {
            stepWorld(*w, music, Vec2{});
            for (u32 k = 0; k < w->eventCount(); ++k)
                if (w->events()[k].type == EventType::TrioOpen && w->events()[k].c == kTrioBreak) breakTrio = true;
        }
        CHECK(!breakTrio);
        delete w;
    }
    {
        // A level-up Trio still open when a cycle's Break begins: nothing waits for it (a667116: that Break deals no Trio).
        World* w = quietWorld();
        w->spawner().trios = true;
        double music = 0.0;
        while (music < 18.0 * kTicksPerBar + 1.0) stepWorld(*w, music, Vec2{});
        w->openTrioNow(0, 22.0 * kTicksPerBar, nullptr, kTrioLevel);
        const TrioState& tr = w->trio(0);
        while (music < 20.0 * kTicksPerBar + 2.0) stepWorld(*w, music, Vec2{});
        CHECK(tr.open && tr.kind == kTrioLevel && tr.breakWaiting == 0);
        delete w;
    }
    {
        // The shelter: inside a level-up orb's thin ring the ship cannot be hurt; a body touching that ring is set off it;
        // outside the rings a bullet hits again.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->openTrioNow(0, music + 4.0 * kTicksPerBar, nullptr, kTrioLevel);
        const TrioState& tr = w->trio(0);
        const Vec2 orb = tr.orbPos[0];
        stepWorld(*w, music, orb);
        CHECK(w->sheltered(0));
        w->spawnBullet(orb, Vec2{}, 0.0f);
        stepWorld(*w, music, orb);
        CHECK(w->player(0).cells == 4 && w->player(0).hitsTaken == 0);
        const float ring = kTrioHoldRing * kTrioOrbWu;
        w->spawnEnemy(SpeciesTable::index(Family::Drifter, 1, kVariantA), orb + Vec2{20.0f, 30.0f}, 0.0f, 0.0);
        stepWorld(*w, music, orb);
        const Enemies& e = w->enemies();
        CHECK(e.count == 1);
        if (e.count == 1) CHECK_MSG(length(Vec2{e.x[0], e.y[0]} - orb) >= ring + e.radius[0] - 1.0f, "a body %.1f wu from the orb",
                                    length(Vec2{e.x[0], e.y[0]} - orb));
        CHECK(w->player(0).hitsTaken == 0);
        stepWorld(*w, music, tr.centre);
        CHECK(!w->sheltered(0));
        w->spawnBullet(tr.centre, Vec2{}, 0.0f);
        stepWorld(*w, music, tr.centre);
        CHECK(w->player(0).hitsTaken == 1);
        delete w;
    }
    {
        // Nothing spawns inside a level-up Trio's ring: the director's crowd, four bars with the ring round the ship.
        auto* w = new (std::nothrow) World;
        w->init(g_species);
        w->spawner().mode = Spawner::Mode::Slice;
        w->resetRun(13, 0.0, 1);
        w->rules().godMode = true;
        double music = 0.0;
        while (music < 24.0 * kTicksPerBar + 1.0) stepWorld(*w, music, Vec2{});
        w->openTrioNow(0, music + 4.0 * kTicksPerBar, nullptr, kTrioLevel);
        const Vec2 c = w->trio(0).centre;
        u32 spawns = 0, inside = 0;
        while (w->trio(0).open && w->trio(0).homing < 0) {
            stepWorld(*w, music, Vec2{});
            for (u32 k = 0; k < w->eventCount(); ++k) {
                const Event& ev = w->events()[k];
                if (ev.type != EventType::WarpStart) continue;
                ++spawns;
                if (length(Vec2{ev.x, ev.y} - c) < kTrioRingWu + 79.0f) ++inside;
            }
        }
        CHECK_MSG(spawns > 0 && inside == 0, "%u of %u spawns inside the ring", inside, spawns);
        delete w;
    }
    {
        // Cells: the drop clock rests at full health; with one cell missing a Cell is due after about 40 s, on the last cell
        // after 15 s; the next kill near the player drops it; at most two lie loose; taken on touch; gone after eight bars.
        auto* w = new (std::nothrow) World;
        w->init(g_species);
        w->spawner().mode = Spawner::Mode::Slice;
        w->resetRun(21, 0.0, 1);
        w->rules().godMode = true;
        heldDrop(*w);
        double music = 0.0;
        for (u32 t = 0; t < 60 * 30; ++t) stepWorld(*w, music, Vec2{});
        CHECK(w->player(0).cellDue == 0.0f && w->cellPickups().count == 0 && w->phrase().bossDrop);
        w->playerMut(0).cells = 3;
        for (u32 t = 0; t < 60 * 39; ++t) stepWorld(*w, music, Vec2{});
        CHECK_MSG(w->player(0).cellDue > 0.9f && w->player(0).cellDue < 1.0f, "due %.3f after 39 s", w->player(0).cellDue);
        for (u32 t = 0; t < 60 * 2; ++t) stepWorld(*w, music, Vec2{});
        CHECK(w->player(0).cellDue == 1.0f && w->cellPickups().count == 0);
        const u16 drifter = SpeciesTable::index(Family::Drifter, 1, kVariantA);
        i32 i = w->spawnEnemy(drifter, Vec2{400.0f, 0.0f}, 0.0f, 0.0);
        CHECK(i >= 0);
        if (i >= 0) w->killEnemy(static_cast<u32>(i), 0);
        CHECK(countEvents(*w, EventType::CellDrop) == 1 && w->cellPickups().count == 1 && w->cellPickups().owner[0] == 0 &&
              w->player(0).cellDue == 0.0f);
        w->playerMut(0).cells = 1;
        for (u32 t = 0; t < 60 * 14; ++t) stepWorld(*w, music, Vec2{-600.0f, 0.0f});
        CHECK_MSG(w->player(0).cellDue > 0.9f && w->player(0).cellDue < 1.0f, "due %.3f after 14 s on the last cell", w->player(0).cellDue);
        for (u32 t = 0; t < 60 * 3; ++t) stepWorld(*w, music, Vec2{-600.0f, 0.0f});
        CHECK(w->player(0).cellDue == 1.0f);
        CHECK(w->cellPickups().count == 0);  // the first Cell's eight bars (16 s) are over
        // Two loose already: the due Cell waits.
        w->dropCellNow(Vec2{900.0f, 300.0f}, 0);
        w->dropCellNow(Vec2{900.0f, -300.0f}, 0);
        i = w->spawnEnemy(drifter, Vec2{-200.0f, 0.0f}, 0.0f, 0.0);
        if (i >= 0) w->killEnemy(static_cast<u32>(i), 0);
        CHECK(countEvents(*w, EventType::CellDrop) == 2 && w->cellPickups().count == 2 && w->player(0).cellDue == 1.0f);
        // Taken on touch: a cell back.
        u32 takes = 0;
        for (u32 t = 0; t < 120 && !takes; ++t) {
            stepWorld(*w, music, Vec2{900.0f, 300.0f});
            for (u32 k = 0; k < w->eventCount(); ++k)
                if (w->events()[k].type == EventType::CellTake && w->events()[k].c == 0) ++takes;
        }
        CHECK(takes == 1 && w->player(0).cells == 2 && w->cellPickups().count == 1 && w->player(0).cellsTaken == 1);
        // At full health a Cell waits where it is (neither pulled nor taken).
        w->playerMut(0).cells = w->player(0).cellCap;
        for (u32 t = 0; t < 30; ++t) stepWorld(*w, music, Vec2{900.0f, -300.0f});
        CHECK(w->cellPickups().count == 1 && w->player(0).cells == w->player(0).cellCap);
        delete w;
    }
    {
        // The loop ramps (GAME_DESIGN 8, the lead's review of play test 2): nothing through Track 3, then a factor a Track,
        // compounding. An enemy spawned in Track 5 has kLoopHealthRamp^2 of its health; a Cell clock runs kLoopCellRamp^2
        // slower; the threat budget and the boss's health and bullets ramp the same way (their code reads loopRamp).
        CHECK(loopRamp(1.3f, 1, 3) == 1.0f && loopRamp(1.3f, 3, 3) == 1.0f && std::fabs(loopRamp(1.3f, 4, 3) - 1.3f) < 1e-6f &&
              std::fabs(loopRamp(1.3f, 6, 3) - 1.3f * 1.3f * 1.3f) < 1e-5f);
        CHECK(kLoopThreatRamp >= 1.15f && kLoopThreatRamp <= 1.5f && kLoopHealthRamp >= 1.15f && kLoopHealthRamp <= 1.5f &&
              kLoopCellRamp >= 1.15f && kLoopCellRamp <= 1.5f);
        auto* w = new (std::nothrow) World;
        w->init(g_species);
        w->spawner().mode = Spawner::Mode::Slice;
        w->setSetList(3);  // (a set list of three: Track 5 is the loop's second)
        w->resetRun(31, 0.0, 1);
        w->rules().godMode = true;
        w->directorMut().track = 5;
        w->directorMut().bossSpawned = true;
        double music = 0.0;
        while (music < 2.0 * kTicksPerBar) stepWorld(*w, music, Vec2{});
        CHECK(w->phrase().track == 5);
        const u16 drifter = SpeciesTable::index(Family::Drifter, 1, kVariantA);
        const i32 i = w->spawnEnemy(drifter, Vec2{600.0f, 0.0f}, 0.0f, 0.0);
        CHECK(i >= 0);
        if (i >= 0)
            CHECK_MSG(std::fabs(w->enemies().hp[i] - g_species->get(drifter).health * kLoopHealthRamp * kLoopHealthRamp) < 1e-4f, "Track 5 health %.4f",
                      w->enemies().hp[i]);
        // The halo burns down (`daec945`): no kill drops a Cell in the loop and Regen stops, so a missing cell stays missing.
        w->playerMut(0).cells = w->player(0).cellCap > 1 ? static_cast<u8>(w->player(0).cellCap - 1) : static_cast<u8>(1);
        for (u32 t = 0; t < 60 * 41; ++t) stepWorld(*w, music, Vec2{});
        CHECK_MSG(w->player(0).cellDue == 0.0f, "Track 5 Cell clock %.3f after 41 s", w->player(0).cellDue);
        w->grantUpgrade(0, kRegen);
        CHECK(w->regenPeriod(0) == 0);
        // The lead's decision (`2fba23c`): enemies and their bullets are faster (capped). M6 (A4): the crowd's at
        // kCrowdBulletSpeedScale of that.
        w->spawnBullet(Vec2{900.0f, 500.0f}, Vec2{100.0f, 0.0f}, 0.0f);
        const Bullets& bl = w->bullets();
        CHECK(bl.count > 0 &&
              std::fabs(bl.vx[bl.count - 1] - 100.0f * kCrowdBulletSpeedScale * std::fmin(kLoopBulletSpeedRamp * kLoopBulletSpeedRamp, kLoopBulletSpeedMax)) < 1e-3f);
        delete w;
    }
    {
        // In the loop the Breaks give back nothing; a Sentry's volley has more arms (here Track 7: x min(ramp^4, max)).
        auto* w = new (std::nothrow) World;
        w->init(g_species);
        w->spawner().mode = Spawner::Mode::Off;
        w->setSetList(3);  // (a set list of three: Track 7 is First Light again)
        w->resetRun(33, 0.0, 1);
        w->rules().godMode = true;
        w->directorMut().track = 7;
        double music = 0.0;
        w->playerMut(0).cells = 2;
        w->playerMut(0).novas = 1;
        u32 refills = 0, bars = 0;
        // Track 7 is First Light again (Short cycles): its first Break at bar 20.
        CHECK(songOfTrack(7, 3) == 0);
        while (music < 21.0 * kTicksPerBar) {
            stepWorld(*w, music, Vec2{});
            refills += countEvents(*w, EventType::Refill);
            bars = w->phrase().bar;
        }
        CHECK_MSG(refills == 0 && w->player(0).cells == 2 && w->player(0).novas == 1 && w->phrase().section == Section::Break, "refills %u by bar %u",
                  refills, bars);
        const i32 i = w->spawnEnemy(SpeciesTable::index(Family::Sentry, 1, kVariantB), Vec2{300.0f, 0.0f}, 0.0f, 0.0);
        CHECK(i >= 0);
        u32 most = 0;
        for (u32 t = 0; t < 600; ++t) {
            const u32 before = w->bullets().count;
            stepWorld(*w, music, Vec2{});
            if (countEvents(*w, EventType::EnemyShot) && w->bullets().count > before && w->bullets().count - before > most) most = w->bullets().count - before;
        }
        const float ramp = std::fmin(kLoopShooterRamp * kLoopShooterRamp * kLoopShooterRamp * kLoopShooterRamp, kLoopShooterMax);
        CHECK_MSG(most >= static_cast<u32>(3.0f * ramp) && most <= static_cast<u32>(3.0f * ramp) + 1, "a Track 7 volley of %u (x%.2f)", most, ramp);
        delete w;
    }
    {
        // The halo burns down (GAME_DESIGN 8, `daec945`): on the first downbeat of every loop Track every ship loses
        // kLoopHaloBurn cells of capacity, never below one, and the cells above it, with no invulnerability and the Chain
        // untouched; Extra Cell refills inside the burnt capacity; no kill drops a Cell, a boss still drops two.
        auto* w = new (std::nothrow) World;
        w->init(g_species);
        w->spawner().mode = Spawner::Mode::Off;
        w->resetRun(41, 0.0, 2);  // co-op's rule too: every ship burns alike
        w->setSetList(3);         // (a set list of three: the loop from Track 4, as the test is written; it grows as bosses are built)
        for (u32 k = 0; k < 4; ++k) w->grantUpgrade(0, kExtraCell);
        w->grantUpgrade(1, kExtraCell);
        w->grantUpgrade(1, kExtraCell);
        CHECK(w->player(0).cellCap == 8 && w->player(1).cellCap == 6);
        w->playerMut(0).chain = 40;
        w->playerMut(1).cells = 3;
        w->directorMut().track = 3;  // the first pass's last Track, its boss dead: the Track-change Break into Track 4
        w->directorMut().bossSpawned = true;
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->directorMut().changeBar = 2;
        u32 burns = 0, expectCap0 = 8;
        const u32 burnFrom = w->setList() + kLoopHaloBurnFrom;
        for (u32 track = 4; track <= 8; ++track) {
            const double line = w->tickOfBar(w->director().changeBar + kTrackChangeBars);
            while (music + kTicksPerSimTick < line - 0.5) {  // (stop short of the line: a step must not cross it)
                stepWorld(*w, music, Vec2{});
                burns += countEvents(*w, EventType::HaloBurn);
            }
            CHECK_MSG(w->player(0).cellCap == expectCap0, "before Track %u: capacity %u (want %u), phrase Track %u bar %u change bar %u", track,
                      w->player(0).cellCap, expectCap0, w->phrase().track, w->phrase().bar, w->director().changeBar);  // nothing before the first downbeat
            while (music < line + 2.0 * kTicksPerSimTick) {
                stepWorld(*w, music, Vec2{});
                burns += countEvents(*w, EventType::HaloBurn);
            }
            CHECK(w->phrase().track == track && !w->phrase().trackChange);
            if (track >= burnFrom) expectCap0 = expectCap0 > kLoopHaloBurn + 1 ? expectCap0 - kLoopHaloBurn : 1u;
            CHECK_MSG(w->player(0).cellCap == expectCap0 && w->player(0).cells <= expectCap0 && w->player(0).burnCap == expectCap0,
                      "Track %u: capacity %u (want %u)", track, w->player(0).cellCap, expectCap0);
            if (track == burnFrom) {
                CHECK(w->player(0).invulS <= 0.0f && w->player(0).chain == 40 && w->player(0).cells == expectCap0);
                CHECK(w->player(1).cellCap == (6u > kLoopHaloBurn + 1 ? 6u - kLoopHaloBurn : 1u) && w->player(1).cells <= w->player(1).cellCap);
                // Extra Cell refills inside the burnt capacity (player 1 has two picks left).
                w->playerMut(1).cells = 1;
                w->grantUpgrade(1, kExtraCell);
                CHECK(w->player(1).cellCap == w->player(1).burnCap && w->player(1).cells == w->player(1).cellCap);
            }
            w->directorMut().changeBar = w->runBar() + 2;  // the next Track's boss falls at once
        }
        CHECK(burns == 2u * (9u - burnFrom));  // two ships, a burn a loop Track
        CHECK(w->player(0).cellCap >= 1);
        // In the loop no kill drops a Cell; a boss's two still fall.
        w->playerMut(0).cells = 1;
        w->spawner().mode = Spawner::Mode::Slice;
        for (u32 t = 0; t < 60 * 30; ++t) stepWorld(*w, music, Vec2{});
        const i32 e = w->spawnEnemy(SpeciesTable::index(Family::Drifter, 1, kVariantA), Vec2{300.0f, 0.0f}, 0.0f, 0.0);
        if (e >= 0) w->killEnemy(static_cast<u32>(e), 0);
        u32 drops = 0;
        for (u32 i = 0; i < w->cellPickups().count; ++i) drops += w->cellPickups().owner[i] == 0 ? 1u : 0u;
        CHECK(w->player(0).cellDue == 0.0f && drops == 0);
        // A boss killed in the loop still drops its two Cells for every ship.
        w->rules().godMode = true;
        w->spawnBossNow(w->tickOfBar(w->runBar() + 1), Vec2{600.0f, 0.0f});
        w->killBossNextTick(0x7, true);
        stepWorld(*w, music, Vec2{});
        u32 bossCells[2] = {0, 0};
        for (u32 i = 0; i < w->cellPickups().count; ++i)
            if (!w->cellPickups().dead[i] && w->cellPickups().owner[i] < 2) ++bossCells[w->cellPickups().owner[i]];
        CHECK_MSG(w->phrase().track >= 8 && w->boss().dying && bossCells[0] == tuning::kBossCells && bossCells[1] == tuning::kBossCells,
                  "a Track %u boss: Cells %u and %u", w->phrase().track, bossCells[0], bossCells[1]);
        delete w;
    }
    {
        // Regen (Mod 18): a missing cell grows back every 16 bars, then 8, then 4; nothing while full.
        CHECK(regenEveryBars(1) == 16 && regenEveryBars(2) == 8 && regenEveryBars(3) == 4 && upgradeInfo(kRegen).cap == 3);
        CHECK(std::strcmp(upgradeWords(kRegen, 1), "CELL REGROWS: 16 BARS") == 0 && std::strcmp(upgradeWords(kRegen, 2), "CELL REGROWS: 8 BARS") == 0 &&
              std::strcmp(upgradeWords(kRegen, 3), "CELL REGROWS: 4 BARS") == 0);
        World* w = quietWorld();
        heldDrop(*w);
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        for (u32 stacks = 1; stacks <= 3; ++stacks) {
            w->grantUpgrade(0, kRegen);
            w->playerMut(0).cells = w->player(0).cellCap;
            const double full0 = music;
            u32 regrown = 0;
            while (music < full0 + 20.0 * kTicksPerBar) {
                stepWorld(*w, music, Vec2{});
                regrown += countEvents(*w, EventType::CellTake);
            }
            CHECK(regrown == 0);
            w->playerMut(0).cells = w->player(0).cellCap - 2;
            const double from = music;
            double at = -1.0;
            while (at < 0.0 && music < from + 40.0 * kTicksPerBar) {
                stepWorld(*w, music, Vec2{});
                for (u32 k = 0; k < w->eventCount(); ++k)
                    if (w->events()[k].type == EventType::CellTake && w->events()[k].c == 1) at = music;
            }
            const double bars = (at - from) / kTicksPerBar;
            const double every = static_cast<double>(regenEveryBars(stacks));
            CHECK_MSG(at > 0.0 && bars > every - 1.0 && bars <= every + 0.01, "Regen %u: a cell back after %.2f bars", stacks, bars);
            CHECK(w->player(0).cells == w->player(0).cellCap - 1);
        }
        delete w;
    }
}

// ---------------------------------------------------------------------------------------------- M5 part 1

// Steps to `until` (musical ticks), the ship at `ship`; returns the events of type `t` seen on the way.
u32 runTo(World& w, double& music, double until, Vec2 ship, EventType t = EventType::Restart) {
    u32 n = 0;
    while (music + kTicksPerSimTick <= until + 1e-9) {
        stepWorld(w, music, ship);
        n += countEvents(w, t);
    }
    return n;
}

u32 cuesIn(const World& w, BossCue c) {
    u32 n = 0;
    for (u32 i = 0; i < w.eventCount(); ++i) n += w.events()[i].type == EventType::BossCue && w.events()[i].a == static_cast<u8>(c) ? 1u : 0u;
    return n;
}

void testSetList() {
    g_test = "set list";
    // The lead's decision (GAME_DESIGN 4.2): a set list of N Tracks starts the loop at Track N + 1, whatever N is.
    for (u32 n : {1u, 2u, 3u, 5u, 10u}) {
        CHECK_MSG(!inLoop(n, n) && inLoop(n + 1, n), "N %u", n);
        CHECK_MSG(loopRamp(1.3f, n, n) == 1.0f && std::fabs(loopRamp(1.3f, n + 1, n) - 1.3f) < 1e-6f, "N %u", n);
        CHECK_MSG(songOfTrack(n + 1, n) == 0 && songOfTrack(n, n) == n - 1, "N %u", n);
        CHECK_MSG(std::fabs(bpmOfTrack(n + 1, n) - std::fmin(tuning::kTrackBpm[n - 1] + tuning::kEndlessBpmStep, tuning::kEndlessBpmCap)) < 1e-4f, "N %u", n);
    }
    // In a run: the first downbeat of Track N + 1 burns the halo (the loop's first Track), not before.
    for (u32 n : {2u, 3u}) {
        World* w = quietWorld();
        w->setSetList(n);
        w->resetRun(7, 0.0, 1);
        w->playerMut(0).cellCap = w->playerMut(0).cells = 8;
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->directorMut().track = static_cast<u8>(n);  // the set list's last Track, its boss dead: the Track-change Break
        w->directorMut().bossSpawned = true;
        w->directorMut().changeBar = 2;
        const u32 burns = runTo(*w, music, w->tickOfBar(2 + kTrackChangeBars) + 2.0 * kTicksPerSimTick, Vec2{}, EventType::HaloBurn);
        CHECK_MSG(w->phrase().track == n + 1 && inLoop(w->phrase().track, w->setList()) && burns == 1 && w->player(0).cellCap == 6,
                  "N %u: Track %u, burns %u, capacity %u", n, w->phrase().track, burns, w->player(0).cellCap);
        delete w;
    }
    {
        // A set list of four: Track 4 is still the first pass (no burn on its first downbeat).
        World* w = quietWorld();
        w->setSetList(4);
        w->resetRun(7, 0.0, 1);
        w->playerMut(0).cellCap = w->playerMut(0).cells = 8;
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->directorMut().track = 3;
        w->directorMut().bossSpawned = true;
        w->directorMut().changeBar = 2;
        const u32 burns = runTo(*w, music, w->tickOfBar(2 + kTrackChangeBars) + 2.0 * kTicksPerSimTick, Vec2{}, EventType::HaloBurn);
        CHECK(w->phrase().track == 4 && !inLoop(4, w->setList()) && burns == 0 && w->player(0).cellCap == 8);
        delete w;
    }
    // Each Track ends in its own boss, round again in the loop.
    CHECK(bossOfSong(0) == BossKind::Kaleidos && bossOfSong(1) == BossKind::Longwave && bossOfSong(2) == BossKind::Choir);
    CHECK(bossOfSong(songOfTrack(5, 3)) == BossKind::Longwave && bossOfSong(songOfTrack(6, 3)) == BossKind::Choir);
}

void testMiddleDrop() {
    g_test = "middle drop";
    // minibosses.md: from Track 2 on the first cycle's Drop (c2f113a: every Track is two cycles; it was the middle one's) brings a
    // mini-boss and holds while it lives, at most 8 bars past its 8 (play test 3: every Track Short; the hold keeps its bars);
    // the bars after it move by the hold.
    DirectorState d;
    d.songs = 3;
    d.track = 2;
    d.trackBar = 0;
    const u32 cycle = 8 + 4 + 8 + 4, dropAt = 8 + 4, natural = dropAt + 8;
    Phrase p = phraseAt(d, dropAt);
    CHECK(p.section == Section::Drop && p.miniDrop && !p.held && p.cycle == 1);
    d.midHolding = true;  // it lives: open-ended up to the cap
    p = phraseAt(d, natural + 7);
    CHECK(p.section == Section::Drop && p.held && p.miniDrop);
    p = phraseAt(d, natural + 8);
    CHECK(p.section == Section::Break && p.cycle == 1);
    d.midHolding = false;
    d.midHold = 3;  // it died in the third held bar
    p = phraseAt(d, natural + 2);
    CHECK(p.section == Section::Drop && p.held);
    p = phraseAt(d, natural + 3);
    CHECK(p.section == Section::Break && p.sectionBar == 0);
    p = phraseAt(d, cycle + 3);
    CHECK(p.cycle == 2 && p.section == Section::Groove && p.sectionBar == 0);
    d.midHold = 0;
    p = phraseAt(d, natural);
    CHECK(p.section == Section::Break);
    // Track 1 has none.
    d.track = 1;
    CHECK(!phraseAt(d, 8 + 8 + 4 + 2).miniDrop);
    // The pick: Track 2 from Alpha Hound and The Hive, Track 3 from Battering Ram and Carom King, both drawn over seeds, never
    // the one met in the Track before.
    World* w = quietWorld();
    u32 seen[kBossKinds] = {};
    for (u64 seed = 1; seed <= 24; ++seed) {
        w->resetRun(seed, 0.0, 1);
        ++seen[static_cast<u32>(w->pickMini(2))];
        ++seen[static_cast<u32>(w->pickMini(3))];
    }
    CHECK(seen[static_cast<u32>(BossKind::AlphaHound)] && seen[static_cast<u32>(BossKind::Hive)] && seen[static_cast<u32>(BossKind::BatteringRam)] &&
          seen[static_cast<u32>(BossKind::CaromKing)] && seen[static_cast<u32>(BossKind::Kaleidos)] == 0);
    CHECK(seen[static_cast<u32>(BossKind::AlphaHound)] + seen[static_cast<u32>(BossKind::Hive)] == 24);
    w->directorMut().lastMini = static_cast<u8>(BossKind::BlinkStar);
    CHECK(w->pickMini(5) == BossKind::SentryArray);  // (M5 part 4: Track 5 draws from Blink Star and Sentry Array)
    w->directorMut().lastMini = static_cast<u8>(BossKind::Hive);
    CHECK(w->pickMini(2) == BossKind::AlphaHound);
    w->directorMut().lastMini = static_cast<u8>(BossKind::CaromKing);
    CHECK(w->pickMini(4) != BossKind::CaromKing);
    delete w;
}

void testLongwave() {
    g_test = "longwave";
    const LongwaveData& d = g_species->longwave;
    CHECK(d.segments == 24 && d.segHealth == 60.0f && d.headHealth == 600.0f && d.wavelength == 8 && d.taper == 3);
    CHECK(d.phases[0].stepTicks == 48.0 && d.phases[1].stepTicks == 32.0 && d.phases[2].stepTicks == 24.0);
    CHECK(d.phases[0].perSide == 1 && d.phases[1].perSide == 3 && d.phases[2].perSide == 1 && d.phases[3].headBurst == 16 && d.whipCount == 32);
    CHECK(d.phaseOf(24) == 0 && d.phaseOf(17) == 0 && d.phaseOf(16) == 1 && d.phaseOf(9) == 1 && d.phaseOf(8) == 2 && d.phaseOf(1) == 2 && d.phaseOf(0) == 3);
    CHECK(d.lapLength > 6000.0f && d.lapLength < 9000.0f);
    for (double p : {0.3, 2.0, 7.5}) CHECK_MSG(std::fabs(d.paramOf(d.arcOf(p)) - p) < 1e-3, "p %.2f", p);
    World* w = quietWorld();
    w->rules().godMode = true;
    double music = 0.0;
    const Vec2 ship{0.0f, 1500.0f};
    stepWorld(*w, music, ship);
    w->directorMut().track = 2;  // its own Track (the health is the page's there)
    w->spawnBossNow(kTicksPerBar, Vec2{}, BossKind::Longwave);
    const BossState& s = w->boss();
    CHECK(s.kind == BossKind::Longwave && s.alive == 0xFFFFFFu && s.fightTick == 3.0 * kTicksPerBar && w->bossSegmentsAlive() == 24);
    CHECK(std::fabs(s.partMax - d.segHealth * tuning::kLongwaveHealthTune) < 1e-3f && std::fabs(s.coreMax - d.headHealth * tuning::kLongwaveHealthTune) < 1e-3f);
    runTo(*w, music, 3.0 * kTicksPerBar + kTicksPerBeat + 10.0, ship);  // beat 2 of the fight's first bar: the jaw is shut
    // Rule 2: three crests on the whole body (eight segments to a wave), each the place k = n + 2 (mod 8) for step n.
    double t = w->music();
    const i64 n = static_cast<i64>(std::floor(w->longwaveSteps(t)));
    u32 crests = 0, crest = 99, trough = 99;
    for (u32 k = 0; k < 24; ++k) {
        if (w->longwaveCrest(k, t) >= 0) {
            ++crests;
            if (crest == 99) crest = k;
            CHECK_MSG((static_cast<i64>(k) + 1 - n - 2) % 8 == 0, "crest %u at step %lld", k, static_cast<long long>(n));
        }
        if (trough == 99 && (((static_cast<i64>(k) + 1 - n - 6) % 8) + 8) % 8 == 0) trough = k;
    }
    CHECK_MSG(crests == 3, "%u crests", crests);
    // A shot into the crest counts (x 1); into a trough it sparks off the armour and does a quarter (GAME_DESIGN 7.6).
    {
        Vec2 tan;
        const Vec2 c = w->longwavePoint(t, static_cast<float>(crest + 1), 0.0f, &tan);
        const Vec2 nrm{-tan.y, tan.x};
        const float hp = s.partHp[crest];
        CHECK(w->shootBoss(c + nrm * 140.0f, c, 7.0f) == 2);
        CHECK_MSG(std::fabs(hp - s.partHp[crest] - 7.0f) < 1e-3f, "crest took %.2f", hp - s.partHp[crest]);
        const Vec2 q = w->longwavePoint(t, static_cast<float>(trough + 1), 0.0f, &tan);
        const float hq = s.partHp[trough];
        w->shootBoss(q - Vec2{-tan.y, tan.x} * 140.0f, q, 7.0f);
        CHECK_MSG(std::fabs(hq - s.partHp[trough] - 1.75f) < 1e-3f, "trough took %.2f", hq - s.partHp[trough]);
        CHECK(w->bossDamageScale(2, static_cast<u8>(trough), t, Vec2{1.0f, 0.0f}) == 0.25f);
        // (the armour's spark: an impact on part 1 with the armour's share)
        bool spark = false;
        for (u32 k = 0; k < w->eventCount(); ++k)
            spark = spark || (w->events()[k].type == EventType::Impact && w->events()[k].c == 1 && w->events()[k].f == 0.25f);
        CHECK(spark);
        // Calibration (the build the user played): armour at nothing.
        w->rules().armourNothing = true;
        const float hr = s.partHp[trough];
        w->shootBoss(q - Vec2{-tan.y, tan.x} * 140.0f, q, 7.0f);
        CHECK(s.partHp[trough] == hr);
        w->rules().armourNothing = false;
    }
    // Rule 5: the jaw opens for beat 1 only.
    CHECK(!w->longwaveJawOpen(t) && w->longwaveJawOpen(4.0 * kTicksPerBar + 10.0) && w->bossDamageScale(3, 0, t, Vec2{1.0f, 0.0f}) == 0.25f);
    // Rule 4: the troughs fire on every step: one bullet to each side in the Swell (three troughs, six bullets a step).
    {
        const u32 b0 = w->bullets().count;
        const double next = (std::floor(w->longwaveSteps(w->music())) + 1.0 - s.lw.s0) * s.lw.stepTicks + s.lw.sTick;
        runTo(*w, music, next + 0.5, ship);
        CHECK_MSG(w->bullets().count - b0 == 6, "%u bullets on a step", w->bullets().count - b0);
    }
    // Rule 6 and the phases: eight segments down, the Chop begins on the next beat line (the crest on 1/8 triplets), the
    // whip's ring of 32 on the downbeat after; a dead segment closes up (the next one takes its place on the path): a953828,
    // it slides up over a step (segmentCloseSteps) instead of snapping.
    w->killBossNextTick(0xFFu, false);
    stepWorld(*w, music, ship);
    const double died = w->music();
    CHECK(w->bossSegmentsAlive() == 16 && s.lw.phase == 0 && s.lw.pendingPhaseTick > w->music() - kTicksPerSimTick);
    CHECK_MSG(w->longwaveLag(8, died) > 7.0f && w->longwaveLag(8, died + 49.0) == 0.0f, "lag %.2f", w->longwaveLag(8, died));
    const double beat = s.lw.pendingPhaseTick;
    runTo(*w, music, std::fmax(beat + 1.0, died + 49.0), ship);
    CHECK(s.lw.phase == 1 && s.phase == 1 && s.lw.stepTicks == 32.0 && s.lw.whipTick == (std::floor(beat / kTicksPerBar) + 1.0) * kTicksPerBar);
    t = w->music();
    CHECK(length(s.body[1 + 8] - w->longwavePoint(t, 1.0f, 0.0f)) < 1.0f);
    {
        u32 whip = 0;
        while (music + kTicksPerSimTick <= s.lw.whipTick + kTicksPerSimTick + 1e-9) {
            stepWorld(*w, music, ship);
            whip += cuesIn(*w, BossCue::LwWhip);
        }
        CHECK(whip == 1);
    }
    // a953828 (the owner: it "jumps across the screen"): the Whitecap's lunge moves the head 500 wu over two beats and runs down
    // the body, place k making the head's move k x followBeatsPerSegment (a sixteenth) later; the path goes on from where the
    // head landed.
    {
        w->killBossNextTick(0xFF00u, false);  // eight more down: 8 left, the Whitecap
        stepWorld(*w, music, ship);
        const double until = w->music() + 8.0 * kTicksPerBar;
        while (s.lw.dashTick < 0.0 && w->music() < until) stepWorld(*w, music, ship);
        CHECK_MSG(s.lw.dashTick >= 0.0 && s.lw.phase == 2, "dash %.0f phase %u", s.lw.dashTick, s.lw.phase);
        const LongwaveData::Phase& P = d.phases[2];
        CHECK(P.lungeDashWu == 500.0f && P.lungeDashBeats == 2.0f && P.lungeFollowBeats == 0.25f && d.phases[0].lapBars == 24.0f &&
              d.phases[1].lapBars == 20.0f && d.phases[2].lapBars == 16.0f && d.closeSteps == 1.0f);
        const double t0 = s.lw.dashTick, follow = 0.25 * kTicksPerBeat;
        CHECK(length(s.lw.anchor - s.lw.lungeFrom) <= 500.0f + 1e-2f && length(s.lw.anchor - s.lw.lungeFrom) > 100.0f);
        // A beat in: the head is part way; the eighth place, two beats behind, has not started; two beats after the head's dash
        // ended the eighth place is where the head's anchor went.
        const Vec2 head = w->longwaveAnchor(t0 + kTicksPerBeat);
        CHECK(length(head - s.lw.lungeFrom) > 1.0f && length(head - s.lw.anchor) > 1.0f);
        CHECK(length(w->longwaveAnchor(t0 + kTicksPerBeat - 8.0 * follow) - s.lw.lungeFrom) < 1e-3f);
        CHECK(length(w->longwaveAnchor(t0 + 4.0 * kTicksPerBeat + 1.0 - 8.0 * follow) - s.lw.anchor) < 1e-3f);
    }
    // The bare head: every segment down, its open jaw takes x 3.
    w->killBossNextTick(0xFFFFFFu, false);
    stepWorld(*w, music, ship);
    runTo(*w, music, (std::floor(w->music() / kTicksPerBar) + 1.0) * kTicksPerBar + 5.0, ship);
    CHECK(s.lw.phase == 3 && w->bossSegmentsAlive() == 0);
    t = w->music();
    CHECK_MSG(w->longwaveJawOpen(t) && w->bossDamageScale(3, 0, t, Vec2{1.0f, 0.0f}) == d.dmgJawBare, "scale %.2f", w->bossDamageScale(3, 0, t, Vec2{1.0f, 0.0f}));
    delete w;
}

void testChoir() {
    g_test = "choir";
    const ChoirData& d = g_species->choir;
    CHECK(d.voiceHealth == 260.0f && d.midi[0] == 62 && d.midi[7] == 74 && d.unisonCall == 5 && d.canonAnswer == 3 && d.soloRing == 16);
    CHECK(d.ringRadius == 380.0f && d.ringFugue == 300.0f && d.dmgLast == 1.5f && d.orbitRadius == 280.0f);
    CHECK(World::choirPhaseOf(0xFF) == 0 && World::choirPhaseOf(0x3F) == 1 && World::choirPhaseOf(0x0F) == 2 && World::choirPhaseOf(0x03) == 3);
    World* w = quietWorld();
    w->rules().godMode = true;
    double music = 0.0;
    const Vec2 ship{0.0f, 0.0f};
    stepWorld(*w, music, ship);
    w->spawnBossNow(kTicksPerBar, ship, BossKind::Choir);
    const BossState& s = w->boss();
    CHECK(s.kind == BossKind::Choir && s.alive == 0xFFu && s.fightTick == 3.0 * kTicksPerBar);
    // Unison: the caller (Voice 0 in the first bar) on beats 1 and 3, the Voice opposite on 2 and 4; a fan of five each.
    const u32 b0 = w->bullets().count;
    runTo(*w, music, 3.0 * kTicksPerBar + 10.0, ship);
    double t = w->music();
    CHECK(w->choirSinging(0, t) && !w->choirSinging(4, t) && !w->choirSinging(1, t));
    CHECK_MSG(w->bullets().count - b0 == 5, "%u bullets", w->bullets().count - b0);
    // Rule 2: a singing Voice takes x 1; a silent one sparks and takes a quarter.
    {
        const Vec2 v0 = s.body[1], v2 = s.body[3];
        const float h0 = s.partHp[0], h2 = s.partHp[2];
        w->shootBoss(s.ch.centre + (v0 - s.ch.centre) * 0.5f, v0, 9.0f);
        w->shootBoss(s.ch.centre + (v2 - s.ch.centre) * 0.5f, v2, 9.0f);
        CHECK_MSG(std::fabs(h0 - s.partHp[0] - 9.0f) < 1e-3f && std::fabs(h2 - s.partHp[2] - 2.25f) < 1e-3f, "%.2f %.2f", h0 - s.partHp[0], h2 - s.partHp[2]);
    }
    runTo(*w, music, 3.0 * kTicksPerBar + kTicksPerBeat + 10.0, ship);
    t = w->music();
    CHECK(w->choirSinging(4, t));  // the answer
    // Rule 4: a dead Voice is a rest. Voice 1 calls the second bar: with it dead nothing sings on its beats, and the Round
    // turns the other way.
    const float dir = s.ch.ringDir;
    w->killBossNextTick(1u << 1, false);
    stepWorld(*w, music, ship);
    CHECK(!(s.alive & 2u) && s.ch.ringDir == -dir);
    {
        u32 voices = 0;
        while (music + kTicksPerSimTick <= 4.0 * kTicksPerBar + 20.0) {
            stepWorld(*w, music, ship);
            if (w->music() >= 4.0 * kTicksPerBar) voices += cuesIn(*w, BossCue::ChVoice);
        }
        CHECK_MSG(voices == 0 && s.ch.caller == 1, "voices %u caller %u", voices, s.ch.caller);
    }
    // The Canon (six left) from the next bar line; its cage every fourth bar: a bar of dashes, then two beats burning.
    w->killBossNextTick((1u << 3) | (1u << 5), false);
    stepWorld(*w, music, ship);
    runTo(*w, music, 5.0 * kTicksPerBar + 10.0, ship);
    CHECK(s.ch.phase == 1);
    const double phase0 = 5.0 * kTicksPerBar;
    CHECK(w->choirCage(phase0 + 3.0 * kTicksPerBar + 50.0) == 1 && w->choirCage(phase0 + 4.0 * kTicksPerBar + 50.0) == 2 &&
          w->choirCage(phase0 + 4.0 * kTicksPerBar + 2.5 * kTicksPerBeat) == 0 && w->choirCage(phase0 + 1.0 * kTicksPerBar + 50.0) == 0);
    // The last Voice takes x 1.5 while it sings (Solo: beats 2 and 4).
    w->killBossNextTick(0xFFu & ~1u, false);
    stepWorld(*w, music, ship);
    const double line = (std::floor(w->music() / kTicksPerBar) + 1.0) * kTicksPerBar;
    runTo(*w, music, line + kTicksPerBeat + 10.0, ship);
    t = w->music();
    CHECK_MSG(s.ch.phase == 3 && w->choirSinging(0, t) && w->bossDamageScale(2, 0, t, Vec2{1.0f, 0.0f}) == d.dmgLast, "phase %u singing %d",
              s.ch.phase, w->choirSinging(0, t) ? 1 : 0);
    // Its death is the fight's.
    w->killBossNextTick(1u, false);
    stepWorld(*w, music, ship);
    CHECK(s.dying);
    delete w;
}

// M5 part 4: Event Horizon (horizon.md): the pull, eating, the bar line's verdict, the lanes, the eruption, the wave, the
// jets, the contact throw.
u32 cuesUntil(World& w, double& music, double until, Vec2 ship, BossCue c) {
    u32 n = 0;
    while (music + kTicksPerSimTick <= until + 1e-9) {
        stepWorld(w, music, ship);
        n += cuesIn(w, c);
    }
    return n;
}

void testHorizon() {
    g_test = "horizon";
    const HorizonData& d = g_species->horizon;
    CHECK(d.horizonRadius == 120.0f && d.photonRadius == 136.0f && d.phases[0].lanes == 2 && d.phases[1].lanes == 3 && d.phases[2].lanes == 4);
    CHECK(d.phases[0].hunger == 5 && d.phases[1].hunger == 7 && d.phases[2].stream == 26 && d.phases[1].wave && !d.phases[0].wave);
    CHECK(d.phases[2].jets && !d.phases[1].jets && d.phases[2].drift == 90.0f && d.phases[1].pullScale == 1.25f);
    CHECK(d.pullShip == 260.0f && d.fed == 0.25f && d.starved == 1.0f && d.zoom == 0.8f && d.meterCells == 12);
    CHECK(bossOfSong(3) == BossKind::EventHorizon && bossHomeTrack(BossKind::EventHorizon) == 4 && fightBuilt(BossKind::EventHorizon));
    World* w = quietWorld();
    w->rules().godMode = true;
    double music = 0.0;
    const Vec2 ship{600.0f, 0.0f};
    stepWorld(*w, music, ship);
    w->spawnBossNow(kTicksPerBar, Vec2{}, BossKind::EventHorizon);
    const BossState& s = w->boss();
    CHECK(s.kind == BossKind::EventHorizon && s.fightTick == 2.0 * kTicksPerBar && s.hz.shield);
    runTo(*w, music, 2.0 * kTicksPerBar + 5.0, ship);
    CHECK(w->horizonOn());
    // The pull: 260 wu/s on the ship at the photon ring, falling off as (136 / distance)^1.2.
    CHECK(std::fabs(w->horizonPullAt(s.pos + Vec2{136.0f, 0.0f}, d.pullShip) - 260.0f) < 0.5f);
    CHECK(std::fabs(w->horizonPullAt(s.pos + Vec2{272.0f, 0.0f}, d.pullShip) - 260.0f * std::pow(0.5f, 1.2f)) < 0.5f);
    {
        const Vec2 pull = w->wellPull(ship);
        CHECK_MSG(pull.x < -10.0f && std::fabs(pull.y) < 1e-3f, "pull %.1f %.1f", pull.x, pull.y);
    }
    // Fed (the shield, from the start): x 0.15.
    CHECK(w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == d.fed);
    // A body that crosses the horizon is eaten: a cell on the meter and the count's note.
    w->spawnEnemy(SpeciesTable::index(Family::Drifter, 1, kVariantA), s.pos + Vec2{60.0f, 0.0f}, 0.0f, w->music());
    stepWorld(*w, music, ship);
    CHECK_MSG(s.hz.meal == 1 && cuesIn(*w, BossCue::HzEat) == 1, "meal %u", s.hz.meal);
    // The lanes: the bar's stream walks in (Accretion: two lanes, ten bodies a bar).
    u32 lane = 0;
    runTo(*w, music, 2.0 * kTicksPerBar + 0.95 * kTicksPerBar, ship);
    for (u32 i = 0; i < w->enemies().count; ++i) lane += w->enemies().state[i] == kLane && !(w->enemies().flags[i] & kFlagDead) ? 1u : 0u;
    CHECK_MSG(lane >= 8 && lane <= 10, "%u on the lanes", lane);
    // Starved (one body against a hunger of five): the gasp, and full damage for the bar.
    CHECK(cuesUntil(*w, music, 3.0 * kTicksPerBar + 10.0, ship, BossCue::HzStarved) == 1);
    CHECK_MSG(!s.hz.shield && w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == d.starved, "shield %d", s.hz.shield ? 1 : 0);
    // Fed: six bodies before the next bar line; the eruption (8 + 4 a body) leaves on two arms, one a sixteenth each.
    for (u32 k = 0; k < 6; ++k)
        w->spawnEnemy(SpeciesTable::index(Family::Drifter, 1, kVariantA), s.pos + dirOf(static_cast<float>(k)) * 50.0f, 0.0f, w->music());
    stepWorld(*w, music, ship);
    CHECK(s.hz.meal >= 6);
    CHECK(cuesUntil(*w, music, 4.0 * kTicksPerBar + 2.0, ship, BossCue::HzFed) == 1);
    CHECK_MSG(s.hz.shield && s.hz.eruptLeft >= 8 + 4 * 6 - 2, "erupt %u", s.hz.eruptLeft);
    {
        const u32 b0 = w->bullets().count;
        runTo(*w, music, 4.0 * kTicksPerBar + 2.0 * kTicksPerStep + 10.0, ship);
        CHECK_MSG(w->bullets().count - b0 >= 4, "%u bullets", w->bullets().count - b0);
    }
    // Touching the horizon: thrown 300 wu out (the controller takes the knock).
    {
        const u32 serial = w->player(0).knockSerial;
        stepWorld(*w, music, s.pos + Vec2{100.0f, 0.0f});
        const Vec2 k = w->player(0).knock;
        CHECK_MSG(w->player(0).knockSerial == serial + 1 && k.x > 250.0f, "knock %.1f %.1f", k.x, k.y);
    }
    // Tidal (two thirds of its health): the wave on beat 3 of every bar.
    w->bossHealthNow(0.6f);
    stepWorld(*w, music, ship);
    CHECK(s.hz.phase == 1);
    {
        const double line = (std::floor(w->music() / kTicksPerBar) + 1.0) * kTicksPerBar;
        runTo(*w, music, line + 5.0, ship);
        CHECK(cuesUntil(*w, music, line + 2.0 * kTicksPerBeat + 5.0, ship, BossCue::HzWave) == 1);
    }
    // Collapse: the jets burn on a fed bar line along the axis, which turns 45 degrees a bar; it drifts toward the players.
    w->bossHealthNow(0.3f);
    stepWorld(*w, music, ship);
    CHECK(s.hz.phase == 2);
    const Vec2 at0 = s.pos;
    for (u32 k = 0; k < 12; ++k)
        w->spawnEnemy(SpeciesTable::index(Family::Drifter, 1, kVariantA), s.pos + dirOf(static_cast<float>(k)) * 50.0f, 0.0f, w->music());
    stepWorld(*w, music, ship);
    {
        const double line = (std::floor(w->music() / kTicksPerBar) + 1.0) * kTicksPerBar;
        runTo(*w, music, line + 10.0, ship);
        CHECK(s.hz.shield && w->music() >= s.hz.jetFrom && w->music() < s.hz.jetUntil);
        Vec2 from;
        const Vec2 along = s.pos + dirOf(s.hz.axis) * 500.0f, aside = s.pos + dirOf(s.hz.axis + 1.0f) * 500.0f;
        CHECK(w->horizonJetHits(along, from) && !w->horizonJetHits(aside, from));
        runTo(*w, music, line + 2.0 * kTicksPerBeat + 10.0, ship);
        CHECK(!w->horizonJetHits(along, from));
    }
    CHECK(length(s.pos - at0) > 20.0f && (s.pos - at0).x > 0.0f);
    // Its death: 300 Shards a player.
    w->killBossNextTick(0, true);
    stepWorld(*w, music, ship);
    CHECK(s.dying);
    delete w;
}

// M5 part 4: Polyrhythm (polyrhythm.md): the pulses, the walls and their gaps, the One, the faces, a wall's hit, Hemiola's
// turn and burning link, the Solo, the death's late break.
const PolyRing* newestRing(const World& w, u8 rotor) {
    const PolyRing* best = nullptr;
    for (const PolyRing& r : w.boss().pr.rings)
        if (r.tick >= 0.0 && r.rotor == rotor && (!best || r.tick > best->tick)) best = &r;
    return best;
}

void testPolyrhythm() {
    g_test = "polyrhythm";
    const PolyData& d = g_species->poly;
    CHECK(d.health[0] == 1400.0f && d.health[1] == 1400.0f && d.sides[0] == 3 && d.sides[1] == 4 && d.pulses[0] == 3 && d.pulses[1] == 4);
    CHECK(d.arcDeg[0] == 80.0f && d.arcDeg[1] == 60.0f && d.turnDeg[0] == 40.0f && d.turnDeg[1] == 22.5f && d.span == 720.0f);
    CHECK(d.ringSpeed == 260.0f && d.wallLife == 2.6f && d.wallThickness == 10.0f && d.dmgSolo == 1.25f && d.zoom == 0.75f && d.linkTicks == 12);
    CHECK(bossOfSong(4) == BossKind::Polyrhythm && bossHomeTrack(BossKind::Polyrhythm) == 5 && fightBuilt(BossKind::Polyrhythm));
    World* w = quietWorld();
    w->rules().godMode = true;
    double music = 0.0;
    const Vec2 ship{0.0f, 420.0f};
    stepWorld(*w, music, ship);
    w->spawnBossNow(kTicksPerBar, Vec2{}, BossKind::Polyrhythm);
    const BossState& s = w->boss();
    CHECK(s.kind == BossKind::Polyrhythm && s.alive == 3u && s.fightTick == 3.0 * kTicksPerBar);
    // The One: both fire, a face of each looking at the other: Three's gap on the link (+x), Four's too (-x).
    u32 three = 0, four = 0, one = 0;
    while (music + kTicksPerSimTick <= 3.0 * kTicksPerBar + 5.0) {
        stepWorld(*w, music, ship);
        three += cuesIn(*w, BossCue::PrThree);
        four += cuesIn(*w, BossCue::PrFour);
        one += cuesIn(*w, BossCue::PrOne);
    }
    CHECK_MSG(three == 1 && four == 1 && one == 1, "%u %u %u", three, four, one);
    const PolyRing* r3 = newestRing(*w, 0);
    const PolyRing* r4 = newestRing(*w, 1);
    CHECK(r3 && r4 && r3->sides == 3 && r4->sides == 4);
    if (r3 && r4) {
        const float link = w->polyLinkAngle(r3->tick);
        CHECK_MSG(std::fabs(wrapAngle(r3->face0 - link)) < 1e-3f && std::fabs(wrapAngle(r4->face0 - link - kPi)) < 1e-3f, "%.3f %.3f %.3f", r3->face0, r4->face0, link);
        CHECK(!w->polyWallAt(*r3, link) && w->polyWallAt(*r3, link + kPi / 3.0f) && !w->polyWallAt(*r3, link + kPi / 3.0f + 0.75f));
        CHECK(!w->polyWallAt(*r4, link + kPi) && w->polyWallAt(*r4, link + kPi / 4.0f));
    }
    // A bar: Three three times, Four four times, the One once.
    three = four = one = 0;
    while (music + kTicksPerSimTick <= 4.0 * kTicksPerBar + 5.0) {
        stepWorld(*w, music, ship);
        three += cuesIn(*w, BossCue::PrThree);
        four += cuesIn(*w, BossCue::PrFour);
        one += cuesIn(*w, BossCue::PrOne);
    }
    CHECK_MSG(three == 3 && four == 4 && one == 1, "%u %u %u", three, four, one);
    // Between pulses Three turns 40 degrees (its gaps step round).
    CHECK(std::fabs(wrapAngle(w->polyFace(0, 4.0 * kTicksPerBar + kTicksPerBar / 3.0) - w->polyLinkAngle(4.0 * kTicksPerBar + kTicksPerBar / 3.0) - 40.0f * kTau / 360.0f)) < 1e-3f);
    // The faces take x 1; the corners and the body are armour (a quarter, GAME_DESIGN 7.6).
    CHECK(w->bossDamageScale(2, 0, w->music(), Vec2{1.0f, 0.0f}) == 1.0f && w->bossDamageScale(1, 0, w->music(), Vec2{1.0f, 0.0f}) == 0.25f);
    // A wall hits; it is erased where it hit.
    {
        w->rules().godMode = false;
        const PolyRing* r = nullptr;  // (an older ring, well clear of the rotor's body)
        for (const PolyRing& q : s.pr.rings)
            if (q.rotor == 1 && w->polyRingAlive(q, w->music()) && w->polyRingRadius(q, w->music()) > 250.0f && (!r || q.tick > r->tick)) r = &q;
        CHECK(r != nullptr);
        if (r) {
            const double next = w->music() + kTicksPerSimTick;
            const float a = r->face0 + kPi / 4.0f;
            const Vec2 on = r->at + dirOf(a) * w->polyRingRadius(*r, next);
            const u8 cells = w->player(0).cells;
            stepWorld(*w, music, on);
            CHECK_MSG(w->player(0).cells + 1 == cells || !w->player(0).alive, "cells %u -> %u", cells, w->player(0).cells);
            CHECK(!w->polyWallAt(*r, a) && w->polyWallAt(*r, a + 0.5f));
        }
        w->rules().godMode = true;
    }
    // Hemiola from the bar line after their joint health falls to 60%: the turn reverses every second bar, Four's walls fly
    // at 300, the link shows its dashes for a bar and burns on beat 4 of the next (M6, 1bcc897: one beat; it was 3 and 4).
    w->bossHealthNow(0.55f);
    stepWorld(*w, music, ship);
    const double h0 = (std::floor(w->music() / kTicksPerBar) + 1.0) * kTicksPerBar;
    CHECK(s.pr.phase == 1 && s.pr.hemiolaBar0 == static_cast<i64>(h0 / kTicksPerBar));
    CHECK(w->polyLinkBurn(h0 + 10.0) == 1 && w->polyLinkBurn(h0 + kTicksPerBar + 10.0) == 1 && w->polyLinkBurn(h0 + kTicksPerBar + 2.0 * kTicksPerBeat + 10.0) == 1 &&
          w->polyLinkBurn(h0 + kTicksPerBar + 3.0 * kTicksPerBeat + 10.0) == 2 && w->polyLinkBurn(h0 + 2.0 * kTicksPerBar - 10.0) == 2 &&
          w->polyLinkBurn(h0 + 2.0 * kTicksPerBar + 10.0) == 1);
    {
        const double mid = h0 + kTicksPerBar + kTicksPerBar / 3.0;  // the second bar: turning back
        CHECK(std::fabs(wrapAngle(w->polyFace(0, mid) - w->polyLinkAngle(mid) + 40.0f * kTau / 360.0f)) < 1e-3f);
    }
    runTo(*w, music, h0 + kTicksPerBar / 4.0 + 5.0, ship);
    {
        const PolyRing* r = newestRing(*w, 1);
        CHECK(r && r->speed == 300.0f);
    }
    // Solo: Four dead; Three plays both counts (its own ring and Four's, turning the other way, gaps together on the One)
    // and takes x 1.25.
    w->killBossNextTick(0x2u, false);
    stepWorld(*w, music, ship);
    CHECK(s.pr.phase == 2 && s.alive == 1u && s.pr.firstDead == 1);
    CHECK(w->bossDamageScale(2, 0, w->music(), Vec2{1.0f, 0.0f}) == d.dmgSolo);
    {
        const double line = (std::floor(w->music() / kTicksPerBar) + 1.0) * kTicksPerBar;
        runTo(*w, music, line + 5.0, ship);
        const PolyRing* own = newestRing(*w, 0);
        const PolyRing* other = newestRing(*w, 1);
        CHECK(own && other && own->tick == line && other->tick == line && other->sides == 4 && length(other->at - s.body[0]) < 30.0f);
        if (own && other) CHECK(std::fabs(wrapAngle(own->face0 - other->face0)) < 1e-3f);
    }
    // The death: the boss break and the Shards on the One that ends the Track.
    w->killBossNextTick(0x1u, false);
    stepWorld(*w, music, ship);
    CHECK(s.dying && s.payoutTick > w->music());
    const u32 shards0 = w->shards().count;
    runTo(*w, music, s.payoutTick + 5.0, ship);
    CHECK_MSG(w->shards().count >= shards0 + 240u, "%u -> %u", shards0, w->shards().count);  // polyrhythm.md: 240 Shards
    delete w;
}

// M5 part 4: Feedback (feedback.md): the tape, the Echo at the mirror point, its shots turned, the Head open only while
// the Echo is silent, Regeneration's second Echo, Howl's double speed and burning Loop, the death's late break.
u32 echoShots(const World& w) {
    u32 n = 0;
    for (u32 i = 0; i < w.bullets().count; ++i) n += !w.bullets().dead[i] && w.bullets().echo[i] ? 1u : 0u;
    return n;
}

void testFeedback() {
    g_test = "feedback";
    const FeedbackData& d = g_species->feedback;
    CHECK(d.health == 4800.0f && d.headRadius == 84.0f && d.loopRadius == 420.0f && d.loopSteps == 16 && d.shotSpeed == 400.0f && d.shotLife == 3.2f);
    CHECK(d.echoRadius == 16.0f && d.howlShrink == 15.0f && d.howlMin == 300.0f && d.zoom == 0.75f && d.blades == 6 && d.ribs == 16);
    CHECK(bossOfSong(5) == BossKind::Feedback && bossHomeTrack(BossKind::Feedback) == 6 && fightBuilt(BossKind::Feedback));
    World* w = quietWorld();
    w->rules().godMode = true;
    double music = 0.0;
    const Vec2 ship{300.0f, 0.0f};
    stepWorld(*w, music, ship);
    w->spawnBossNow(kTicksPerBar, Vec2{}, BossKind::Feedback);
    const BossState& s = w->boss();
    CHECK(s.kind == BossKind::Feedback && s.fightTick == 3.0 * kTicksPerBar && length(s.pos) < 1.0f);
    // Bar 3: the player fires for two beats (along +x, away from the Head), then rests.
    runTo(*w, music, 3.0 * kTicksPerBar, ship);
    while (music + kTicksPerSimTick <= 3.0 * kTicksPerBar + 2.0 * kTicksPerBeat) stepWorld(*w, music, ship, 1);
    runTo(*w, music, 4.0 * kTicksPerBar + 10.0, ship);
    // Bar 4: the Echo at the mirror point plays it back, the shots turned (along -x), and the Head is shut while it plays.
    CHECK_MSG((s.fb.active & 1u) && length(s.body[1] - Vec2{-300.0f, 0.0f}) < 20.0f, "active %u at %.0f %.0f", s.fb.active, s.body[1].x, s.body[1].y);
    runTo(*w, music, 4.0 * kTicksPerBar + kTicksPerBeat, ship);
    CHECK_MSG(echoShots(*w) >= 2, "%u echo shots", echoShots(*w));
    {
        bool turned = false;
        for (u32 i = 0; i < w->bullets().count; ++i)
            if (w->bullets().echo[i] && !w->bullets().dead[i]) turned = w->bullets().vx[i] < -300.0f && w->bullets().x[i] < 0.0f;
        CHECK(turned);
    }
    CHECK_MSG(s.fb.playing & 1u && w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == 0.25f, "playing %u open %.2f", s.fb.playing, s.fb.open);
    runTo(*w, music, 4.0 * kTicksPerBar + 2.0 * kTicksPerBeat + 3.0 * kTicksPerStep, ship);
    CHECK_MSG(!(s.fb.playing & 1u) && w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == 1.0f, "playing %u open %.2f", s.fb.playing, s.fb.open);
    // Its body hurts; player shots are not stopped by it (it is not a part).
    {
        Vec2 from;
        CHECK(w->feedbackHits(s.body[1], from) && !w->feedbackHits(Vec2{0.0f, 300.0f}, from));
        BossPart parts[kBossBodies + 4];
        CHECK(w->bossParts(parts, kBossBodies + 4) == 1 && parts[0].part == 3);
    }
    // Regeneration: a second Echo, two bars behind, where the player stood.
    w->bossHealthNow(0.5f);
    stepWorld(*w, music, ship);
    runTo(*w, music, 5.0 * kTicksPerBar + 10.0, ship);  // (the tape holds two bars from bar 5 on)
    CHECK_MSG(s.fb.phase == 1 && (s.fb.active & 3u) == 3u && length(s.body[2] - ship) < 20.0f, "phase %u active %u", s.fb.phase, s.fb.active);
    // Howl: the tape at double speed (two beats and four), the Loop tightening 15 wu a bar line, burning.
    w->bossHealthNow(0.25f);
    stepWorld(*w, music, ship);
    CHECK(s.fb.phase == 2 && w->feedbackDelay(0) == 2.0 * kTicksPerBeat && w->feedbackDelay(1) == 4.0 * kTicksPerBeat);
    {
        const double line = (std::floor(w->music() / kTicksPerBar) + 2.0) * kTicksPerBar;
        runTo(*w, music, line + 5.0, ship);
        CHECK_MSG(s.fb.loopR == d.loopRadius - 2.0f * d.howlShrink, "loop %.1f", s.fb.loopR);
        Vec2 from;
        CHECK(w->feedbackHits(Vec2{0.0f, s.fb.loopR}, from) && !w->feedbackHits(Vec2{0.0f, s.fb.loopR - 60.0f}, from));
    }
    // The death: the tape is cut; the break and the Shards come two bars on.
    w->killBossNextTick(0, true);
    stepWorld(*w, music, ship);
    CHECK(s.dying && s.payoutTick > w->music() + kTicksPerBar);
    const u32 shards0 = w->shards().count;
    runTo(*w, music, s.payoutTick + 5.0, ship);
    CHECK_MSG(w->shards().count >= shards0 + 320u, "%u -> %u", shards0, w->shards().count);  // feedback.md: 320 Shards
    delete w;
}

// M5 part 4: mini-bosses batch 2 (minibosses.md): the Moth Queen's court, the Brood Mother's nest, the Blink Star's star,
// the Sentry Array's clock.
u32 calledIn(const World& w, u8 state) {
    u32 n = 0;
    for (u32 i = 0; i < w.enemies().count; ++i)
        n += !(w.enemies().flags[i] & kFlagDead) && (w.enemies().flags[i] & kFlagCalled) && w.enemies().state[i] == state ? 1u : 0u;
    return n;
}

void testMinibosses2() {
    g_test = "minibosses batch 2";
    CHECK(fightBuilt(BossKind::MothQueen) && fightBuilt(BossKind::SentryArray) && fightBuilt(BossKind::MirrorMimic));
    CHECK(bossHomeTrack(BossKind::MothQueen) == 4 && bossHomeTrack(BossKind::BlinkStar) == 5);
    const Vec2 ship{0.0f, 0.0f};
    // The Moth Queen: the court of twelve warps in round the player and spirals in; a rosette of five on every eighth; on
    // bar four the court bursts (six bullets each) and she is open (x 2).
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, ship);
        w->spawnBossNow(kTicksPerBar, Vec2{500.0f, 0.0f}, BossKind::MothQueen);
        const BossState& s = w->boss();
        const double f0 = s.fightTick;
        runTo(*w, music, f0 + 2.0 * kTicksPerBeat + 10.0, ship);
        CHECK_MSG(calledIn(*w, kCourt) == 12, "%u court Moths", calledIn(*w, kCourt));
        CHECK(length(s.mini.ringAt - ship) < 5.0f && !w->mothOpen(w->music()));
        CHECK(w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == 1.0f);
        {
            const u32 b0 = w->bullets().count;
            runTo(*w, music, w->music() + kTicksPerBeat / 2.0, ship);
            CHECK_MSG(w->bullets().count - b0 == 5, "%u petals", w->bullets().count - b0);
        }
        runTo(*w, music, f0 + 3.0 * kTicksPerBar - 10.0, ship);
        {
            const u32 court = calledIn(*w, kCourt);
            const u32 b0 = w->bullets().count;
            runTo(*w, music, f0 + 3.0 * kTicksPerBar + 2.0, ship);
            CHECK_MSG(calledIn(*w, kCourt) == 0 && w->bullets().count >= b0 + 6u * court, "court %u bullets %u -> %u", court, b0, w->bullets().count);
        }
        CHECK(w->mothOpen(w->music()) && w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == 2.0f);
        delete w;
    }
    // The Brood Mother: six eggs on beat 1 (white, one shot), hatching on beat 3; while a broodling lives her dome is shut
    // (x 0.25); with none, x 2.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, ship);
        w->spawnBossNow(kTicksPerBar, Vec2{400.0f, 0.0f}, BossKind::BroodMother);
        const BossState& s = w->boss();
        const double f0 = s.fightTick;
        runTo(*w, music, f0 + 10.0, ship);
        CHECK_MSG(calledIn(*w, kEgg) == 6, "%u eggs", calledIn(*w, kEgg));
        CHECK(w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == 2.0f);  // (no broodling yet)
        runTo(*w, music, f0 + 2.0 * kTicksPerBeat + 10.0, ship);
        CHECK_MSG(calledIn(*w, kEgg) == 0 && calledIn(*w, kMoving) == 6, "%u eggs, %u young", calledIn(*w, kEgg), calledIn(*w, kMoving));
        CHECK(s.mini.shut && w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == 0.25f);
        for (u32 i = 0; i < w->enemies().count; ++i)
            if (w->enemies().flags[i] & kFlagCalled) w->setEnemyHp(i, 0.001f);
        for (u32 k = 0; k < 4; ++k) stepWorld(*w, music, ship, 1);
        delete w;
    }
    // The Blink Star: it appears at the first point, hops a point a beat in star order, fires all five edges on bar two's
    // beat 2 (16 wu wide), then is dazed at the first point (x 2).
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, ship);
        w->spawnBossNow(kTicksPerBar, Vec2{300.0f, 200.0f}, BossKind::BlinkStar);
        const BossState& s = w->boss();
        const double f0 = s.fightTick;
        runTo(*w, music, f0 + 10.0, ship);
        CHECK(length(s.mini.ringAt - ship) < 5.0f && length(s.body[0] - w->blinkPoint(0)) < 1.0f);
        runTo(*w, music, f0 + kTicksPerBeat + 10.0, ship);
        CHECK(length(s.body[0] - w->blinkPoint(2)) < 1.0f && w->blinkEdges(w->music()) == 1);
        runTo(*w, music, f0 + 5.0 * kTicksPerBeat + 10.0, ship);
        CHECK(w->blinkFiring(w->music()));
        {
            Vec2 from;
            const Vec2 mid = (w->blinkPoint(0) + w->blinkPoint(2)) * 0.5f;
            CHECK(w->blinkBeamHit(mid, from) && !w->blinkBeamHit(s.mini.ringAt, from));
        }
        runTo(*w, music, f0 + 6.0 * kTicksPerBeat + 10.0, ship);
        CHECK(w->blinkDazed(w->music()) && length(s.body[0] - w->blinkPoint(0)) < 1.0f && w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == 2.0f);
        delete w;
    }
    // The Sentry Array: eight turrets on a ring of 340 round the players' centre, fenced; the one under the hand is the only
    // one open (x 2); every other fires five inward on each beat; a death and they step round to stand evenly.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, ship);
        w->spawnBossNow(kTicksPerBar, Vec2{200.0f, 0.0f}, BossKind::SentryArray, Vec2{});
        const BossState& s = w->boss();
        const double f0 = s.fightTick;
        CHECK(s.alive == 0xFFu && length(s.body[1] - Vec2{0.0f, -340.0f}) < 1.0f);
        runTo(*w, music, f0 + 10.0, ship);
        const i32 hand = w->sentryHand();
        CHECK(hand >= 0);
        BossPart parts[kBossBodies + 4];
        const u32 n = w->bossParts(parts, kBossBodies + 4);
        u32 open = 0;
        for (u32 k = 0; k < n; ++k) open += parts[k].part == 2 ? 1u : 0u;
        CHECK(n == 8 && open == 1 && w->bossDamageScale(2, static_cast<u8>(hand), w->music(), Vec2{1.0f, 0.0f}) == 2.0f &&
              w->bossDamageScale(1, 0, w->music(), Vec2{1.0f, 0.0f}) == prose::kMiniArmour);
        {
            const u32 b0 = w->bullets().count;
            runTo(*w, music, f0 + kTicksPerBeat + 10.0, ship);
            CHECK_MSG(w->bullets().count - b0 == 35, "%u bullets", w->bullets().count - b0);
        }
        // The fence keeps a ship inside.
        {
            Vec2 to{400.0f, 0.0f};
            CHECK(w->penKeep(Vec2{100.0f, 0.0f}, to, 16.0f) && to.x < 340.0f);
        }
        w->killBossNextTick(1u << 3, false);
        stepWorld(*w, music, ship);
        CHECK(s.alive == (0xFFu & ~(1u << 3)));
        const double line = (std::floor(w->music() / kTicksPerBar) + 1.0) * kTicksPerBar;
        runTo(*w, music, line + kTicksPerBeat + 10.0, ship);
        {
            float gapMin = 10.0f, gapMax = 0.0f;
            for (u32 k = 0; k < 8; ++k) {
                if (!(s.alive & (1u << k))) continue;
                u32 j = (k + 1) % 8;
                while (!(s.alive & (1u << j))) j = (j + 1) % 8;
                const float g = wrapAngle(s.mini.turretAng[j] - s.mini.turretAng[k]);
                const float gp = g < 0.0f ? g + kTau : g;
                gapMin = std::fmin(gapMin, gp);
                gapMax = std::fmax(gapMax, gp);
            }
            CHECK_MSG(gapMax - gapMin < 0.02f, "gaps %.3f-%.3f", gapMin, gapMax);
        }
        delete w;
    }
}

// M5 (look.md v2.5 "Shots that leave the straight line", GAME_DESIGN 6.2, 6.5, 6.6): a stream of shots reads as a line.
float sidewaysWorst(const World& w) {
    // The largest sideways move of a Wave shot this tick against its forward move.
    float worst = 0.0f;
    const Shots& s = w.shots();
    for (u32 i = 0; i < s.count; ++i) {
        if (s.dead[i] || !(s.mods[i] & kShotWave)) continue;
        const Vec2 d{s.dx[i], s.dy[i]}, m{s.x[i] - s.px[i], s.y[i] - s.py[i]};
        const float fwd = dot(m, d), side = std::fabs(m.x * d.y - m.y * d.x);
        if (fwd > 1.0f) worst = std::fmax(worst, side / fwd);
    }
    return worst;
}

// M5 part 6: Cadence (design/bosses/cadence.md): the bar's chord open and silent, the other three firing on one and three.
void testCadence() {
    g_test = "cadence";
    const CadenceData& d = g_species->cadence;
    CHECK(d.health == 5200.0f && d.pillarRadius == 90.0f && d.squareSide == 700.0f && d.fanBullets == 5 && d.bulletSpeed == 180.0f && d.rewardShards == 300);
    CHECK(bossOfSong(6) == BossKind::Cadence && bossHomeTrack(BossKind::Cadence) == 7 && fightBuilt(BossKind::Cadence));
    World* w = quietWorld();
    w->rules().godMode = true;
    double music = 0.0;
    const Vec2 ship{0.0f, 0.0f};
    stepWorld(*w, music, ship);
    w->spawnBossNow(kTicksPerBar, Vec2{}, BossKind::Cadence);
    const BossState& s = w->boss();
    CHECK(s.kind == BossKind::Cadence && s.fightTick == 2.0 * kTicksPerBar);
    // The square: 700 wu a side, I at the top left.
    const Vec2 p0 = w->cadencePillar(0, music), p1 = w->cadencePillar(1, music);
    CHECK(std::fabs(length(p1 - p0) - 700.0f) < 1.0f && p0.x < s.cd.centre.x && p0.y < s.cd.centre.y);
    // Fans on beat 1 from the three that do not sound (5 x the bullet density each, at least 2).
    const u32 b0 = w->bullets().count;
    runTo(*w, music, 2.0 * kTicksPerBar + 10.0, ship);
    const u32 chord = w->cadenceChord(w->music());
    CHECK_MSG(w->bullets().count - b0 >= 6 && w->bullets().count - b0 <= 15, "%u bullets", w->bullets().count - b0);
    // The open pillar: x 2 on its One, x 1 after it; the others a quarter.
    CHECK(w->cadenceScale(chord, w->music()) == 2.0f && w->cadenceScale((chord + 1) % 4, w->music()) == 0.25f);
    runTo(*w, music, 2.0 * kTicksPerBar + kTicksPerBeat + 10.0, ship);
    CHECK(w->cadenceScale(chord, w->music()) == 1.0f);
    // The next bar: the next chord.
    runTo(*w, music, 3.0 * kTicksPerBar + 10.0, ship);
    CHECK(w->cadenceChord(w->music()) == (chord + 1) % 4);
    // Inversion (below a third): the sounding pillar closed, the others open.
    w->bossHealthNow(0.3f);
    runTo(*w, music, 4.0 * kTicksPerBar + 10.0, ship);
    const u32 c2 = w->cadenceChord(w->music());
    CHECK(s.cd.phase == 2 && w->cadenceScale(c2, w->music()) == 0.25f && w->cadenceScale((c2 + 1) % 4, w->music()) == 1.0f);
    delete w;
}

// M5 part 6: Arpeggiator (design/bosses/arpeggiator.md), and the entrance rule (nobody meets a boss on their last cell).
void testArpeggiator() {
    g_test = "arpeggiator";
    const ArpData& d = g_species->arp;
    CHECK(d.health == 6400.0f && d.coreRadius == 70.0f && d.laneWidth == 200.0f && d.laneLength == 900.0f && d.blockWidth == 90.0f &&
          d.blockSpeed == 420.0f && d.rewardShards == 340);
    CHECK(bossOfSong(7) == BossKind::Arpeggiator && bossHomeTrack(BossKind::Arpeggiator) == 8 && fightBuilt(BossKind::Arpeggiator));
    const Vec2 ship{0.0f, 0.0f};
    // A mini-boss leaves the cells alone.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, ship);
        w->playerMut(0).cells = 1;
        w->spawnBossNow(kTicksPerBar, Vec2{600.0f, 0.0f}, BossKind::AlphaHound);
        CHECK(w->player(0).cells == 1);
        delete w;
    }
    // M6 (GAME_DESIGN 5, 1bcc897): a boss pays for its phases: a Cell a player at each phase change, at the boss, drifting to
    // the player while it is hurt.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, ship);
        w->spawnBossNow(kTicksPerBar, Vec2{600.0f, 0.0f}, BossKind::Kaleidos);
        while (music < 3.0 * kTicksPerBar) stepWorld(*w, music, ship);
        w->playerMut(0).cells = 2;
        const u32 before = w->cellPickups().count;
        CHECK(w->boss().phase == 0 && w->boss().phasePaid == 0);
        w->killBossNextTick(1u, false);
        stepWorld(*w, music, ship);
        stepWorld(*w, music, ship);
        const CellPickups& c = w->cellPickups();
        CHECK_MSG(w->boss().phase == 1 && w->boss().phasePaid == 1 && c.count == before + 1, "phase %u paid %u cells %u", w->boss().phase,
                  w->boss().phasePaid, c.count);
        if (c.count == before + 1) {
            const u32 i = c.count - 1;
            CHECK(c.owner[i] == 0 && c.homing[i] == 2 && c.vx[i] < -0.9f * tuning::kBossPhaseCellDriftWu);  // toward the ship at (0, 0)
        }
        // A second pair: the second phase's Cell.
        w->killBossNextTick(2u, false);
        stepWorld(*w, music, ship);
        stepWorld(*w, music, ship);
        CHECK(w->boss().phase == 2 && w->boss().phasePaid == 2);
        delete w;
    }
    World* w = quietWorld();
    w->rules().godMode = true;
    double music = 0.0;
    stepWorld(*w, music, ship);
    // The entrance rule: one cell grows back to three as the boss arrives (an arc each, as Regen's; M6, 1bcc897: was two).
    w->playerMut(0).cells = 1;
    w->spawnBossNow(kTicksPerBar, Vec2{}, BossKind::Arpeggiator);
    CHECK_MSG(w->player(0).cells == 3 && countEvents(*w, EventType::CellTake) == 2, "cells %u", w->player(0).cells);
    const BossState& s = w->boss();
    CHECK(s.kind == BossKind::Arpeggiator && s.fightTick == 2.0 * kTicksPerBar);
    // The machine stands above the players, 380 wu out; its four lanes 200 wu apart run down toward them.
    const Vec2 m0 = w->arpMachine(0);
    CHECK_MSG(std::fabs(m0.x) < 1.0f && std::fabs(m0.y + 380.0f) < 1.0f, "machine at %.1f %.1f", m0.x, m0.y);
    Vec2 f1, d1, f4, d4;
    CHECK(w->arpLane(0, 1, f1, d1) && w->arpLane(0, 4, f4, d4) && std::fabs(length(f4 - f1) - 600.0f) < 1.0f && d1.y > 0.99f);
    CHECK(!w->arpLane(1, 1, f1, d1));
    // Quarter notes: four blocks a bar, in G's lanes (1 3 3 1 on steps 0 4 8 12... as kApLanesG).
    const auto blocksIn = [&](double until, u32* lanes) {
        u32 n = 0;
        while (music + kTicksPerSimTick <= until + 1e-9) {
            stepWorld(*w, music, ship);
            for (u32 i = 0; i < w->eventCount(); ++i) {
                const Event& e = w->events()[i];
                if (e.type == EventType::BossCue && e.a == static_cast<u8>(BossCue::ApBlock)) {
                    if (lanes && n < 16) lanes[n] = e.b + 1u;
                    ++n;
                }
            }
        }
        return n;
    };
    runTo(*w, music, 2.0 * kTicksPerBar - 2.0, ship);
    u32 lanes[16] = {};
    const u32 q = blocksIn(3.0 * kTicksPerBar - 2.0, lanes);
    CHECK_MSG(q == 4 && lanes[0] == prose::kApLanesG[0] && lanes[1] == prose::kApLanesG[4] && lanes[2] == prose::kApLanesG[8] && lanes[3] == prose::kApLanesG[12],
              "%u blocks, lanes %u %u %u %u", q, lanes[0], lanes[1], lanes[2], lanes[3]);
    // The core: x 2 on the One, x 1 after it; the console a quarter.
    runTo(*w, music, 3.0 * kTicksPerBar + 10.0, ship);
    CHECK(w->arpScale(3, 0, w->music()) == 2.0f && w->arpScale(1, 0, w->music()) == 0.25f);
    runTo(*w, music, 3.0 * kTicksPerBar + kTicksPerBeat + 10.0, ship);
    CHECK(w->arpScale(3, 0, w->music()) == 1.0f);
    // A block hurts where it falls; a Nova clears the lanes.
    bool hit = false;
    const float tps = w->ticksPerSecond();
    for (const ArpState::Block& b : s.ap.blocks) {
        if (b.tick < 0.0) continue;
        const float dist = static_cast<float>(w->music() - b.tick) / tps * d.blockSpeed;
        Vec2 from;
        if (dist > 0.0f && dist < d.laneLength) hit = hit || w->arpBlockHit(b.from + b.dir * dist, from);
    }
    CHECK(hit);
    stepWorld(*w, music, ship, 0, true);
    runTo(*w, music, 4.0 * kTicksPerBar - 30.0, ship);
    u32 left = 0;
    for (const ArpState::Block& b : s.ap.blocks) left += b.tick >= 0.0 && b.tick < 4.0 * kTicksPerBar - 2.0 * kTicksPerBeat ? 1u : 0u;
    CHECK_MSG(left == 0, "%u blocks left after the Nova", left);
    // Eighth notes below two thirds: eight a bar; every fourth bar it moves to the next side.
    w->bossHealthNow(0.6f);
    runTo(*w, music, 5.0 * kTicksPerBar - 2.0, ship);
    const u32 e8 = blocksIn(6.0 * kTicksPerBar - 2.0, nullptr);
    CHECK_MSG(s.ap.phase == 1 && e8 == 8, "phase %u, %u blocks", s.ap.phase, e8);
    runTo(*w, music, 10.0 * kTicksPerBar + 2.0, ship);
    CHECK_MSG(s.ap.side[0] == 1, "side %u", s.ap.side[0]);
    // Two hands below a third: the second machine at right angles; the stab's six on top of the quarter notes.
    w->bossHealthNow(0.3f);
    runTo(*w, music, 11.0 * kTicksPerBar - 2.0, ship);
    const u32 two = blocksIn(12.0 * kTicksPerBar - 2.0, nullptr);
    CHECK_MSG(s.ap.two && s.ap.side[1] == ((s.ap.side[0] + 1u) & 3u) && two == 10, "two %d, %u blocks", s.ap.two ? 1 : 0, two);
    delete w;
}

// M5 part 6: Fata Morgana (design/bosses/morgana.md): the storm on the kick, the drift and the rest, the mirage, the leaps.
void testMorgana() {
    g_test = "morgana";
    const MorganaData& d = g_species->morgana;
    CHECK(d.health == 7600.0f && d.oasisRadius == 260.0f && d.bodyRadius == 76.0f && d.fanBullets == 5 && d.bulletSpeed == 200.0f && d.rewardShards == 380);
    CHECK(bossOfSong(8) == BossKind::Morgana && bossHomeTrack(BossKind::Morgana) == 9 && fightBuilt(BossKind::Morgana));
    World* w = quietWorld();
    w->rules().godMode = true;
    double music = 0.0;
    Vec2 ship{0.0f, 0.0f};
    stepWorld(*w, music, ship);
    w->spawnBossNow(kTicksPerBar, Vec2{}, BossKind::Morgana);
    const BossState& s = w->boss();
    CHECK(s.kind == BossKind::Morgana && s.fightTick == 2.0 * kTicksPerBar);
    // The oasis starts where the players are; she circles 70 wu outside its edge.
    runTo(*w, music, 2.0 * kTicksPerBar + 10.0, ship);
    const Vec2 o = w->morganaOasis(w->music());
    CHECK_MSG(length(o) < 40.0f && w->morganaCalm(Vec2{}, w->music()) && !w->morganaCalm(Vec2{500.0f, 0.0f}, w->music()), "oasis at %.1f %.1f", o.x, o.y);
    CHECK_MSG(std::fabs(length(w->morganaBody(w->music()) - o) - 330.0f) < 1.0f, "body %.1f out", length(w->morganaBody(w->music()) - o));
    // It drifts at 110 wu/s along its figure.
    const Vec2 o1 = w->morganaOasis(w->music() + kTicksPerBeat);
    const float v = length(o1 - o) / (static_cast<float>(kTicksPerBeat) / w->ticksPerSecond());
    CHECK_MSG(w->morganaResting(w->music()) || std::fabs(v - 110.0f) < 6.0f, "drift %.1f wu/s", v);
    // The storm hurts on the kick: a ship outside the calm loses a cell (god mode off just over one kick, far from her fans).
    runTo(*w, music, 3.0 * kTicksPerBar + 5.0 * kTicksPerStep, ship);
    ship = Vec2{1500.0f, 900.0f};
    w->rules().godMode = false;
    const u8 cells = w->player(0).cells;
    runTo(*w, music, 3.0 * kTicksPerBar + 7.0 * kTicksPerStep, ship);
    CHECK_MSG(w->player(0).cells + 1 == cells, "cells %u -> %u", cells, w->player(0).cells);
    w->rules().godMode = true;
    ship = Vec2{};
    // A rest bar (the last of four): she crosses the water, x 2 while over it.
    double rest = -1.0;
    for (double t = w->music(); t < w->music() + 5.0 * kTicksPerBar && rest < 0.0; t += kTicksPerBar / 8.0)
        if (w->morganaResting(t)) rest = std::floor(t / kTicksPerBar) * kTicksPerBar;
    CHECK(rest > 0.0);
    CHECK(w->morganaScale(rest + 0.5 * kTicksPerBar) == 2.0f && w->morganaScale(rest + 0.02 * kTicksPerBar) == 1.0f && w->morganaScale(rest + 1.2 * kTicksPerBar) == 1.0f);
    CHECK(length(w->morganaBody(rest + 0.5 * kTicksPerBar) - w->morganaOasis(rest + 0.5 * kTicksPerBar)) < 5.0f);
    // The Mirage (below two thirds): a second oasis 500 wu away at the next fourth bar line, out on the snares, gone after two bars.
    w->bossHealthNow(0.6f);
    double mt = -1.0;
    for (u32 k = 0; k < 6 * 120 && mt < 0.0; ++k) {
        stepWorld(*w, music, w->morganaOasis(music));
        mt = s.mg.mirageTick;
    }
    Vec2 mp;
    bool lit = false;
    CHECK(mt > 0.0 && w->morganaMirage(mt + 10.0, mp, lit) && lit && std::fabs(length(mp - w->morganaOasis(mt + 10.0)) - 500.0f) < 1.0f);
    CHECK(w->morganaMirage(mt + 4.5 * kTicksPerStep, mp, lit) && !lit && w->morganaMirage(mt + 6.5 * kTicksPerStep, mp, lit) && lit);
    CHECK(!w->morganaMirage(mt + 2.0 * kTicksPerBar + 1.0, mp, lit));
    // The Crossing (below a third): every second bar a leap of 600 wu; the old one shrinks while a corridor joins them; x 2 a beat.
    w->bossHealthNow(0.3f);
    for (u32 k = 0; k < 6 * 120 && s.mg.leapTick < 0.0; ++k) stepWorld(*w, music, w->morganaOasis(music));
    const MorganaState& g = s.mg;
    CHECK(g.leapTick > 0.0 && std::fabs(length(g.at - g.from) - 600.0f) < 1.0f);
    const double lt = g.leapTick;
    CHECK(w->morganaScale(lt + 10.0) == 2.0f && w->morganaScale(lt + kTicksPerBeat + 10.0) == 1.0f);
    CHECK(w->morganaCalm((g.at + g.from) * 0.5f, lt + 0.5 * kTicksPerBar) && !w->morganaCalm((g.at + g.from) * 0.5f, lt + 1.01 * kTicksPerBar));
    CHECK(g.nextTick == lt + 2.0 * kTicksPerBar);
    delete w;
}

// M5 part 6: Kaleidos, Reprise (design/bosses/reprise.md): the Reprise's fans and armour, the Medley's solos and shares, Last
// Light's arpeggio (First Light's, from design/music/track01.json).
void testReprise() {
    g_test = "reprise";
    const RepriseData& d = g_species->reprise;
    CHECK(d.health == 9000.0f && d.coreRadius == 80.0f && d.armLength == 300.0f && d.rewardShards == 500);
    CHECK(d.arpMidi[0][0] == 69 && d.arpSemis[0][3] == 12 && d.arpSemis[0][12] == 10 && d.arpMidi[3][0] == 62 && d.arpSemis[3][12] == 14);
    CHECK(bossOfSong(9) == BossKind::Reprise && bossHomeTrack(BossKind::Reprise) == 10 && fightBuilt(BossKind::Reprise) && kBossesBuilt == 10);
    World* w = quietWorld();
    w->rules().godMode = true;
    double music = 0.0;
    const Vec2 ship{0.0f, 0.0f};
    stepWorld(*w, music, ship);
    w->spawnBossNow(kTicksPerBar, Vec2{500.0f, 0.0f}, BossKind::Reprise);
    const BossState& s = w->boss();
    CHECK(s.kind == BossKind::Reprise && s.fightTick == 2.0 * kTicksPerBar && s.alive == 0x3Fu);
    // The Reprise: fans of five on the two-step's hits from the four arms whose pair is not resting; the core x 2 on the One.
    runTo(*w, music, 2.0 * kTicksPerBar - 2.0, ship);
    const u32 b0 = w->bullets().count;
    runTo(*w, music, 2.0 * kTicksPerBar + 4.0, ship);
    CHECK_MSG(w->bullets().count - b0 == 20u, "%u bullets", w->bullets().count - b0);
    CHECK(w->repriseScale(3, 0, w->music()) == 2.0f && w->repriseScale(2, 1, w->music()) == 0.25f);
    runTo(*w, music, 2.0 * kTicksPerBar + kTicksPerBeat + 4.0, ship);
    CHECK(w->repriseScale(3, 0, w->music()) == 1.0f);
    // The Medley (below 70%): each arm a sixth of the phase's health; the solos two bars each, from Kaleidos's arm; the soloist
    // alone open.
    w->bossHealthNow(0.6f);
    runTo(*w, music, 3.0 * kTicksPerBar + 4.0, ship);
    CHECK_MSG(s.rp.phase == 1 && std::fabs(s.partHp[0] - 0.4f / 6.0f * s.coreMax) < 1.0f, "phase %u, arm %.1f", s.rp.phase, s.partHp[0]);
    CHECK(s.rp.solo == 0 && w->repriseScale(2, 0, w->music()) == 1.0f && w->repriseScale(2, 2, w->music()) == 0.25f && w->repriseScale(3, 0, w->music()) == 0.25f);
    runTo(*w, music, 5.0 * kTicksPerBar + 4.0, ship);
    CHECK(s.rp.solo == 1 && w->repriseWaveOut(w->music()));
    // Longwave's arm let go: its wave's crests open, the rest a quarter.
    u32 crests = 0;
    for (u32 j = 0; j < 8; ++j) crests += w->repriseCrest(j, w->music()) ? 1u : 0u;
    CHECK(crests >= 1 && crests <= 4);
    // Shooting the soloist off: its share goes, the core's health with it; its solo is skipped from then on.
    w->killBossNextTick(0x2u, false);
    stepWorld(*w, music, ship);
    CHECK(!(s.alive & 0x2u) && std::fabs(s.coreHp - (0.2f + 5.0f * 0.4f / 6.0f) * s.coreMax) < 2.0f);
    runTo(*w, music, 6.0 * kTicksPerBar + 4.0, ship);
    CHECK_MSG(s.rp.solo == 2, "solo %u", s.rp.solo);
    // Last Light: every arm off; the core fires First Light's arpeggio, a bullet a sixteenth.
    w->killBossNextTick(0x3Fu, false);
    stepWorld(*w, music, ship);
    stepWorld(*w, music, ship);
    CHECK(s.rp.phase == 2 && s.rp.lastTick == 7.0 * kTicksPerBar);
    runTo(*w, music, 7.0 * kTicksPerBar - 2.0, ship);
    u32 notes = 0;
    while (music + kTicksPerSimTick <= 8.0 * kTicksPerBar - 2.0) {
        stepWorld(*w, music, ship);
        for (u32 i = 0; i < w->eventCount(); ++i)
            notes += w->events()[i].type == EventType::BossCue && w->events()[i].a == static_cast<u8>(BossCue::RpArp) ? 1u : 0u;
    }
    CHECK_MSG(notes == 16u, "%u notes in a bar", notes);
    delete w;
}

// M5 part 6: mini-bosses batch 3 (minibosses.md "Batch 3") and the picks of Tracks 6 to 10.
void testMinis3() {
    g_test = "minibosses batch 3";
    const Vec2 ship{0.0f, 0.0f};
    const auto fight = [&](BossKind k, double& music) {
        World* w = quietWorld();
        w->rules().godMode = true;
        stepWorld(*w, music, ship);
        w->spawnBossNow(kTicksPerBar, Vec2{500.0f, 0.0f}, k, Vec2{});
        return w;
    };
    // Mirror Mimic: through the mirror point (where the players' centre was), x 2 while it copies a Dash a beat late.
    {
        double music = 0.0;
        World* w = fight(BossKind::MirrorMimic, music);
        const Vec2 p{200.0f, 100.0f};
        runTo(*w, music, 3.0 * kTicksPerBar, p);
        const Vec2 m = w->mimicImage(0);
        CHECK_MSG(std::fabs(m.x + 200.0f) < 1.0f && std::fabs(m.y + 100.0f) < 1.0f, "mimic at %.1f %.1f", m.x, m.y);
        CHECK(w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == 1.0f);
        delete w;
    }
    // Twin Tether: two nodes 520 wu apart, the rope between them lethal; each half the health.
    {
        double music = 0.0;
        World* w = fight(BossKind::TwinTether, music);
        runTo(*w, music, 3.0 * kTicksPerBar, ship);
        const BossState& s = w->boss();
        CHECK(std::fabs(length(s.body[1] - s.body[2]) - 520.0f) < 1.0f && std::fabs(s.partHp[0] - 0.5f * s.coreMax) < 1.0f);
        Vec2 a, b;
        CHECK(w->tetherBeam(a, b) && !w->tetherSlack(w->music()));
        delete w;
    }
    // Weaver Prime: a cage round where the player stood at its first knot, laps of 300, 250, ...; x 2 on the knot.
    {
        double music = 0.0;
        World* w = fight(BossKind::WeaverPrime, music);
        const Vec2 p{100.0f, 0.0f};
        runTo(*w, music, 2.0 * kTicksPerBar + 10.0, p);
        const MiniState& m = w->boss().mini;
        CHECK_MSG(m.cageTick == 2.0 * kTicksPerBar && length(m.cage - p) < 1.0f, "cage at %.1f (%.1f %.1f)", m.cageTick, m.cage.x, m.cage.y);
        CHECK(w->weaverKnot(0, w->music()) && w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == 2.0f);
        runTo(*w, music, 2.0 * kTicksPerBar + 2.5 * kTicksPerBeat, p);
        CHECK(std::fabs(length(w->weaverPos(0, w->music()) - m.cage) - 300.0f) < 1.0f && !w->weaverKnot(0, w->music()));
        Vec2 c;
        float r = 0.0f, a0 = 0.0f, dir = 0.0f, u0 = 0.0f, u1 = 0.0f;
        CHECK(w->weaverLap(0, 0, w->music(), c, r, a0, dir, u0, u1) && r == 300.0f && u1 > 0.4f && u1 < 0.6f);
        runTo(*w, music, 3.0 * kTicksPerBar + 2.5 * kTicksPerBeat, p);
        CHECK(std::fabs(length(w->weaverPos(0, w->music()) - m.cage) - 250.0f) < 1.0f);
        delete w;
    }
    // Deep Well: it swallows (a quarter) and fills; overfed in a bar it chokes for the next (x 2), else it gives a ring back.
    {
        double music = 0.0;
        World* w = fight(BossKind::DeepWell, music);
        runTo(*w, music, 2.0 * kTicksPerBar + 10.0, ship);
        CHECK(w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == 0.25f && w->wellStomach() > 0.0f);
        const Vec2 at = w->boss().body[0];
        const u32 b0 = w->bullets().count;
        const float st = w->wellStomach();
        w->shootBoss(at - Vec2{200.0f, 0.0f}, at, 0.9f * st);
        CHECK(w->boss().mini.fill > 0.89f * st && w->boss().mini.swallowed == 1);
        runTo(*w, music, 3.0 * kTicksPerBar + 4.0, ship);
        const u32 want = static_cast<u32>(0.9f * st / 3.0f);
        CHECK_MSG(w->bullets().count + 1u >= b0 + want && !w->wellChoked(w->music()), "%u bullets back (%u)", w->bullets().count - b0, want);
        w->shootBoss(at - Vec2{200.0f, 0.0f}, w->boss().body[0], st * 1.2f);
        runTo(*w, music, 4.0 * kTicksPerBar + 4.0, ship);
        CHECK(w->wellChoked(w->music()) && w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == 2.0f);
        delete w;
    }
    // The picks: Track 6 Mirror Mimic; Track 7 Twin Tether or Weaver Prime; Track 8 Deep Well or the other.
    {
        World* w = quietWorld();
        CHECK(w->pickMini(6) == BossKind::MirrorMimic);
        const BossKind t7 = w->pickMini(7);
        CHECK(t7 == BossKind::TwinTether || t7 == BossKind::WeaverPrime);
        w->directorMut().minisMet = static_cast<u16>(1u << (static_cast<u32>(t7) - static_cast<u32>(BossKind::AlphaHound)));
        w->directorMut().lastMini = static_cast<u8>(t7);
        const BossKind t8 = w->pickMini(8);
        CHECK(t8 == BossKind::DeepWell || (t8 != t7 && (t8 == BossKind::TwinTether || t8 == BossKind::WeaverPrime)));
        delete w;
    }
}

// M5 part 5: elites and their traits (design/traits.md), the arena's structures and events (design/arena.md).
void testArena() {
    g_test = "arena";
    const TraitData& td = g_species->traits;
    const ArenaData& ad = g_species->arena;
    CHECK(td.scale == 1.3f && td.health == 6.0f && td.score == 10.0f && td.threat == 4.0f && td.shards == 20 && td.alive[0] == 3 && td.alive[1] == 5);
    CHECK(td.fromTrack[kArmored] == 3 && td.fromCycle[kGiant] == 2 && td.fromCycle[kArmored] == 1 && td.fromTrack[kVolatile] == 4 && td.fromTrack[kPhasing] == 7 && td.twinHealth == 1.6f && td.volatileRing == 160.0f);
    CHECK(ad.radius[static_cast<u32>(StructKind::Vault)] == 60.0f && ad.radius[static_cast<u32>(StructKind::Lens)] == 160.0f &&
          ad.eventBars[static_cast<u32>(EventKind::Eclipse)] == 8);
    CHECK(ad.structCount[1] == 0 && ad.structCount[2] >= 1 && ad.structs[2][0] == static_cast<u8>(StructKind::Vault) && ad.eventCount[1] == 1 &&
          ad.events[1][0] == static_cast<u8>(EventKind::Comet) && ad.structCount[4] == 5 && ad.glyph[ArenaData::gVault].count == 18);
    const Vec2 ship{0.0f, 0.0f};
    const u16 drifter = SpeciesTable::index(Family::Drifter, 3, kVariantA);
    const Species& ds = g_species->get(drifter);
    // An elite: x 1.3 its size, x 6 its health (Armored x 2 again).
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, ship);
        const i32 i = w->spawnEliteNow(drifter, Vec2{600.0f, 0.0f}, traitBit(kArmored));
        CHECK(i >= 0);
        const Enemies& e = w->enemies();
        CHECK(std::fabs(e.radius[i] - ds.radius * 1.3f) < 1e-3f && std::fabs(e.hp[i] - ds.health * 12.0f) < 1e-2f && (e.traits[i] & kEliteBit));
        CHECK(w->elitesAlive() == 1);
        // Its death: a large kill, 20 Shards and a Cell for the player, x 10 its score.
        const u64 score0 = w->player(0).score;
        w->hitEnemyNow(static_cast<u32>(i), 1e6f, 0);
        bool large = false;
        for (u32 k = 0; k < w->eventCount(); ++k) large = large || (w->events()[k].type == EventType::Kill && w->events()[k].a == 2);
        CHECK(large && w->shards().count == 20 && w->cellPickups().count == 1);
        CHECK_MSG(w->player(0).score - score0 == static_cast<u64>(std::llround(ds.score * 10.0f)), "score %llu", static_cast<unsigned long long>(w->player(0).score - score0));
        delete w;
    }
    // Twin: two bodies, one health bar; they die together.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, ship);
        const i32 i = w->spawnEliteNow(drifter, Vec2{600.0f, 0.0f}, traitBit(kTwin));
        const Enemies& e = w->enemies();
        CHECK(e.count == 2 && e.twin[i] != 0);
        const i32 j = w->enemyIndex(e.twin[i]);
        CHECK(j >= 0 && std::fabs(e.hp[i] - ds.health * 6.0f * 1.6f) < 1e-2f);
        w->hitEnemyNow(static_cast<u32>(i), 10.0f, 0);
        CHECK(std::fabs(e.hp[j] - e.hp[i]) < 1e-3f);
        w->hitEnemyNow(static_cast<u32>(i), 1e6f, 0);
        CHECK((e.flags[i] & kFlagDead) && (e.flags[j] & kFlagDead));
        delete w;
    }
    // Splitting: two copies, 0.6 of its size and 30% of its health, no trait.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, ship);
        const i32 i = w->spawnEliteNow(drifter, Vec2{600.0f, 0.0f}, traitBit(kSplitting));
        const Enemies& e = w->enemies();
        const float r = e.radius[i], hmax = e.hpMax[i];
        w->hitEnemyNow(static_cast<u32>(i), 1e6f, 0);
        u32 copies = 0;
        for (u32 k = 0; k < e.count; ++k)
            if (!(e.flags[k] & kFlagDead) && !e.traits[k] && std::fabs(e.radius[k] - r * 0.6f) < 1e-3f && std::fabs(e.hp[k] - hmax * 0.3f) < 1e-2f) ++copies;
        CHECK_MSG(copies == 2, "%u copies", copies);
        delete w;
    }
    // Volatile: its ring bursts two beats on, 30 x the Track's number to every enemy inside; Regenerating heals after two quiet
    // beats; Shielded sheds a shot from its front.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, ship);
        const i32 v = w->spawnEliteNow(drifter, Vec2{600.0f, 0.0f}, traitBit(kVolatile));
        const i32 near = w->spawnEnemy(drifter, Vec2{650.0f, 40.0f}, 0.0f, music);
        const Enemies& e = w->enemies();
        const u32 nearSerial = e.serial[near];
        w->setEnemyHp(static_cast<u32>(near), 100.0f);
        w->hitEnemyNow(static_cast<u32>(v), 1e6f, 0);
        CHECK(w->bombs()[0].burst > music);
        runTo(*w, music, w->bombs()[0].burst + 10.0, ship);
        i32 nk = -1;
        for (u32 k = 0; k < e.count; ++k)
            if (e.serial[k] == nearSerial) nk = static_cast<i32>(k);
        CHECK_MSG(nk >= 0 && std::fabs(e.hp[nk] - 70.0f) < 1e-2f, "hp %.1f", nk >= 0 ? e.hp[nk] : -1.0f);
        const i32 g = w->spawnEliteNow(drifter, Vec2{-600.0f, 0.0f}, traitBit(kRegenerating) | traitBit(kShielded));
        const u32 gs = e.serial[g];
        w->hitEnemyNow(static_cast<u32>(g), 50.0f, 0, Vec2{-1.0f, 0.0f});
        float hp0 = 0.0f, hpMax = 0.0f;
        for (u32 k = 0; k < e.count; ++k)
            if (e.serial[k] == gs) {
                hp0 = e.hp[k];
                hpMax = e.hpMax[k];
            }
        runTo(*w, music, music + 4.0 * kTicksPerBeat, ship);
        for (u32 k = 0; k < e.count; ++k)
            if (e.serial[k] == gs) {
                CHECK_MSG(e.hp[k] > hp0 && e.hp[k] <= hpMax, "regen %.1f -> %.1f of %.1f", hp0, e.hp[k], hpMax);
                // (its shield turns to the ship: a shot from the ship meets its front)
                const float before = e.hp[k];
                w->hitEnemyNow(k, 30.0f, 0, normalizeOr(Vec2{e.x[k], e.y[k]} - ship, Vec2{-1.0f, 0.0f}));
                CHECK(e.hp[k] == before);
            }
        delete w;
    }
    // A Vault: shots break it for a Boost and 40 Shards; a Lens splits a shot in three; a Current carries; a Bumper kicks.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, ship);
        const i32 s = w->placeStructureNow(StructKind::Vault, Vec2{300.0f, 0.0f}, 0.0f);
        CHECK(s >= 0 && std::fabs(w->arenaBodies()[s].hp - 300.0f * 0.33f) < 1e-2f);  // Track 1's trackScale is 300
        u32 fired = 0;
        w->arenaBodyMut(static_cast<u32>(s)).hp = 20.0f;  // (the base stream does about 8 a second)
        for (u32 k = 0; k < 600 && w->arenaBodies()[s].alive; ++k) {
            stepWorld(*w, music, ship, 1);
            fired = fired > w->shots().count ? fired : w->shots().count;
        }
        CHECK_MSG(!w->arenaBodies()[s].alive, "vault hp %.1f, shots at most %u", w->arenaBodies()[s].hp, fired);
        CHECK(!w->arenaBodies()[s].alive && w->boostPickups().count == 1 && w->shards().count >= 40);
        const i32 l = w->placeStructureNow(StructKind::Lens, Vec2{200.0f, 0.0f}, 0.5f * kPi);
        u32 most = 0, splits = 0;
        for (u32 k = 0; k < 90; ++k) {
            stepWorld(*w, music, ship, 1);
            if (w->shots().count > most) most = w->shots().count;
            splits += cuesIn(*w, BossCue::LeSplit);
        }
        CHECK_MSG(l >= 0 && splits >= 1 && most >= 3, "%u splits, %u shots at most", splits, most);
        const i32 c = w->placeStructureNow(StructKind::Current, Vec2{0.0f, 1000.0f}, 0.0f);
        CHECK(c >= 0 && std::fabs(w->currentFlow(Vec2{100.0f, 1050.0f}).x - 260.0f) < 1e-3f && w->currentFlow(Vec2{100.0f, 1300.0f}).x == 0.0f);
        const i32 b = w->placeStructureNow(StructKind::Bumper, Vec2{-1000.0f, 0.0f}, 0.0f);
        Vec2 p{-960.0f, 0.0f}, vel{-200.0f, 0.0f};
        CHECK(b >= 0 && w->bumperBounce(p, vel, 12.0f) && vel.x == 600.0f);
        delete w;
    }
    // Events: the Comet crosses at 700 wu/s once announced; Meteor Rain's rings land two beats after they show.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, ship);
        w->startEventNow(EventKind::Comet);
        runTo(*w, music, music + 3.0 * kTicksPerBeat, ship);  // (its chevron shows two beats ahead)
        i32 comet = -1;
        for (u32 k = 0; k < World::kArenaBodies; ++k)
            if (w->arenaBodies()[k].alive && w->arenaBodies()[k].kind == kBodyComet) comet = static_cast<i32>(k);
        CHECK(comet >= 0 && std::fabs(length(w->arenaBodies()[comet].vel) - 700.0f) < 1e-2f);
        delete w;
        w = quietWorld();
        w->rules().godMode = true;
        music = 0.0;
        stepWorld(*w, music, ship);
        w->startEventNow(EventKind::MeteorRain);
        u32 warn = 0, land = 0;
        while (music < 4.0 * kTicksPerBar) {
            stepWorld(*w, music, ship);
            warn += cuesIn(*w, BossCue::MeWarn);
            land += cuesIn(*w, BossCue::MeLand);
        }
        CHECK_MSG(warn >= 30 && land >= 30, "%u warnings, %u landings", warn, land);
        delete w;
    }
}

void testShotStream() {
    g_test = "shot stream";
    const Vec2 ship{0.0f, 0.0f};
    // Wave: every shot on one curve from the muzzle (y = A sin(2 pi age / beat)), sideways never above 0.6 of forward.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        w->grantUpgrade(0, kWave);
        w->grantUpgrade(0, kWave);
        double music = 0.0;
        stepWorld(*w, music, ship);
        float worst = 0.0f, offCurve = 0.0f;
        const float wv = kTau / (60.0f / kBpm);
        u32 seen = 0;
        for (u32 t = 0; t < 120; ++t) {
            stepWorld(*w, music, ship, 1);
            worst = std::fmax(worst, sidewaysWorst(*w));
            const Shots& s = w->shots();
            for (u32 i = 0; i < s.count; ++i) {
                if (s.dead[i] || !(s.mods[i] & kShotWave)) continue;
                const float want = 40.0f * std::sin(wv * (s.life0[i] - s.life[i]));
                offCurve = std::fmax(offCurve, std::fmin(std::fabs(s.y[i] - want), std::fabs(s.y[i] + want)));
                ++seen;
            }
        }
        CHECK_MSG(seen > 100 && offCurve < 0.5f && worst <= 0.6f, "%u shots, %.2f wu off the curve, sideways %.2f of forward", seen, offCurve, worst);
        delete w;
    }
    // Helix Array: the Split lanes ride strands along the aim (no fan), mirrored pairs at 80 and 53 wu (Split at its cap, five
    // lanes), the middle lane straight; one wave to two beats.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        for (u32 k = 0; k < 4; ++k) w->grantUpgrade(0, kSplit);
        w->grantUpgrade(0, kWave);
        w->grantUpgrade(0, kWave);
        w->playerMut(0).loadout.fusions = static_cast<u8>(1u << kHelixArray);
        double music = 0.0;
        stepWorld(*w, music, ship);
        u32 outer = 0, mid = 0, inner = 0, straight = 0, neg = 0;
        bool fan = false;
        float worst = 0.0f;
        for (u32 t = 0; t < 60; ++t) {
            const u32 before = w->shots().count;
            stepWorld(*w, music, ship, 1);
            worst = std::fmax(worst, sidewaysWorst(*w));
            if (t > 5 || w->shots().count == before) continue;
        }
        const Shots& s = w->shots();
        for (u32 i = 0; i < s.count; ++i) {
            if (s.dead[i]) continue;
            fan = fan || std::fabs(s.dy[i]) > 1e-4f;
            const u8 m = s.mods[i];
            if (!(m & kShotWave)) ++straight;
            else if ((m & kShotHelix) && !(m & kShotHelixIn)) ++outer;
            else if ((m & kShotHelix) && (m & kShotHelixIn)) ++mid;
            else if (m & kShotHelixIn) ++inner;
            if (m & kShotWaveNeg) ++neg;
        }
        CHECK_MSG(!fan && outer > 0 && outer == mid && inner == 0 && straight * 2 == outer && neg * 2 == outer + mid && worst <= 0.6f,
                  "fan %d outer %u mid %u inner %u straight %u neg %u sideways %.2f", fan ? 1 : 0, outer, mid, inner, straight, neg, worst);
        delete w;
    }
    // Split's fan (eb10554): 2, 3, 4, 5 lanes kSplitFanDeg apart about the aim; an even count straddles the aim line (no lane
    // down it). Heavy Step's orb and Accent's lance take the middle lane's place; an even fan has none, so every lane fires
    // beside them.
    for (u32 st = 1; st <= 4; ++st) {
        World* w = quietWorld();
        w->rules().godMode = true;
        for (u32 k = 0; k < st; ++k) w->grantUpgrade(0, kSplit);
        double music = 0.0;
        stepWorld(*w, music, ship);
        u32 made = 0;
        for (u32 t = 0; t < 30 && !made; ++t) {
            stepWorld(*w, music, ship, 1);
            for (u32 k = 0; k < w->eventCount(); ++k)
                if (w->events()[k].type == EventType::Shot) made = w->events()[k].b;
        }
        const u32 lanes = st + 1u;
        float angles[8];
        u32 n = 0;
        const Shots& s = w->shots();
        for (u32 i = 0; i < s.count && n < 8; ++i)
            if (!s.dead[i] && s.kind[i] == kShotPrimary) angles[n++] = std::atan2(s.dy[i], s.dx[i]) * 360.0f / kTau;
        bool spaced = n == lanes, onAxis = false;
        for (u32 a = 0; a < n; ++a) {
            const float want = (static_cast<float>(a) - 0.5f * static_cast<float>(lanes - 1u)) * tuning::kSplitFanDeg;
            bool found = false;
            for (u32 b = 0; b < n; ++b) found = found || std::fabs(angles[b] - want) < 0.01f;
            spaced = spaced && found;
            onAxis = onAxis || std::fabs(angles[a]) < 0.01f;
        }
        CHECK_MSG(made == lanes && spaced && onAxis == ((lanes & 1u) == 1u), "Split %u: %u made, %u shots, on the axis %d", st, made, n, onAxis ? 1 : 0);
        delete w;
    }
    {
        // Heavy Step with four lanes: the fourth volley is the four lanes and an orb down the aim line.
        World* w = quietWorld();
        w->rules().godMode = true;
        for (u32 k = 0; k < 3; ++k) w->grantUpgrade(0, kSplit);
        w->grantUpgrade(0, kHeavyStep);
        double music = 0.0;
        stepWorld(*w, music, ship);
        u32 volleys = 0, orbMade = 0;
        for (u32 t = 0; t < 120 && volleys < 4; ++t) {
            stepWorld(*w, music, ship, 1);
            for (u32 k = 0; k < w->eventCount(); ++k)
                if (w->events()[k].type == EventType::Shot && ++volleys == 4) orbMade = w->events()[k].b;
        }
        u32 orbs = 0;
        bool orbOnAxis = false;
        const Shots& s = w->shots();
        for (u32 i = 0; i < s.count; ++i)
            if (!s.dead[i] && s.kind[i] == kShotOrb) {
                ++orbs;
                orbOnAxis = std::fabs(s.dy[i]) < 1e-4f;
            }
        CHECK_MSG(orbMade == 5 && orbs == 1 && orbOnAxis, "Heavy Step, four lanes: %u made, %u orbs", orbMade, orbs);
        delete w;
    }
    // Thinning never flickers: it starts above the soft cap and ends below three quarters of it, only on a beat; while it
    // lasts every lane fires and the lanes beyond the middle three fly half as far (eb10554: four lanes, an even fan: its
    // outer pair).
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        for (u32 k = 0; k < 3; ++k) w->grantUpgrade(0, kSplit);
        for (u32 k = 0; k < 3; ++k) w->grantUpgrade(0, kDoubleTime);
        double music = 0.0;
        stepWorld(*w, music, ship);
        u32 changesOffBeat = 0, thinnedTicks = 0, halfOuter = 0, fullOuter = 0, most = 0;
        bool was = false;
        for (u32 t = 0; t < 600; ++t) {
            if (t % 60 == 0) {
                w->grantBoost(0, kPrism);
                w->grantBoost(0, kHyper);
            }
            const double m0 = music;
            {
                // (Split's four lanes x Prism at 174 BPM, the set list's last tempo, with Double-Time 2: over the soft cap)
                TickInput in;
                in.music0 = music;
                in.music1 = music + 174.0 * 96.0 / 60.0 / 60.0;
                in.bpm = 174.0f;
                in.players[0].present = true;
                in.players[0].pos = ship;
                in.players[0].aim = Vec2{1.0f, 0.0f};
                in.players[0].fire = t < 360 ? 1 : 0;
                w->tick(in);
                music = in.music1;
            }
            most = w->shotsLive(0) > most ? w->shotsLive(0) : most;
            const bool now = w->thinning(0);
            if (now != was && std::floor(m0 / kTicksPerBeat) == std::floor(music / kTicksPerBeat)) ++changesOffBeat;
            was = now;
            if (!now) continue;
            ++thinnedTicks;
            const Shots& s = w->shots();
            for (u32 i = 0; i < s.count; ++i) {
                if (s.dead[i] || s.kind[i] != kShotPrimary || s.life0[i] - s.life[i] > 0.02f) continue;
                if (std::fabs(s.life0[i] - 0.5f * tuning::kShotLifeS) < 1e-4f) ++halfOuter;
                else ++fullOuter;
            }
        }
        CHECK_MSG(thinnedTicks > 0 && changesOffBeat == 0 && halfOuter > 0 && fullOuter > 0 && !w->thinning(0),
                  "thinned %u ticks, %u changes off a beat, %u half-reach and %u full shots, %u live at most", thinnedTicks, changesOffBeat, halfOuter,
                  fullOuter, most);
        delete w;
    }
}

void testMinibosses() {
    g_test = "minibosses";
    const MiniData& md = g_species->minis;
    CHECK(md.healthByTrack[2] == 600.0f && md.healthByTrack[3] == 1100.0f && md.healthTracks >= 6 && md.loopFactor == 1.25f);
    CHECK(md.crowdScale == 0.75f && md.rewardOrbs == 1 && md.rewardShards == 60 && md.penSide == 1400.0f);
    CHECK(md.find(BossKind::BatteringRam)->pen && md.find(BossKind::CaromKing)->pen && !md.find(BossKind::AlphaHound)->pen && !md.find(BossKind::Hive)->pen);
    CHECK(md.find(BossKind::Hive)->healthFactor == 0.8f && md.find(BossKind::BatteringRam)->healthFactor == 1.2f);
    // The Pen: rises with the fight round the players' centre; its walls stop ships and bodies both ways; fodder caught
    // outside dissolves; what spawns while it stands lands inside.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        const i32 out = w->spawnEnemy(SpeciesTable::index(Family::Drifter, 1, kVariantA), Vec2{1200.0f, 0.0f}, 0.0f, 0.0);
        const i32 in = w->spawnEnemy(SpeciesTable::index(Family::Drifter, 1, kVariantA), Vec2{400.0f, 300.0f}, 0.0f, 0.0);
        CHECK(out >= 0 && in >= 0);
        w->spawnBossNow(kTicksPerBar, Vec2{300.0f, 0.0f}, BossKind::BatteringRam, Vec2{});
        const PenState& pen = w->pen();
        CHECK(pen.up && std::fabs(pen.half - 700.0f) < 1e-3f && (w->enemies().flags[out] & kFlagDead) && !(w->enemies().flags[in] & kFlagDead));
        Vec2 to{900.0f, 0.0f};
        CHECK(w->penKeep(Vec2{0.0f, 0.0f}, to, 14.0f) == 1 && std::fabs(to.x - 686.0f) < 1e-3f);
        to = Vec2{650.0f, 0.0f};
        CHECK(w->penKeep(Vec2{1000.0f, 0.0f}, to, 14.0f) == 1 && std::fabs(to.x - 714.0f) < 1e-3f);
        const i32 late = w->spawnEnemy(SpeciesTable::index(Family::Drifter, 1, kVariantA), Vec2{3000.0f, 0.0f}, 0.0f, 0.0);
        CHECK(late >= 0 && w->penInside(Vec2{w->enemies().x[late], w->enemies().y[late]}, 0.0f));
        // Battering Ram: its horns shed every shot; it locks on beat 3, charges on the next downbeat, and a wall within 300 wu
        // behind the lock stuns it (x 2 from every side) with a ring of 16 from the wall.
        const Vec2 ship{-560.0f, 0.0f};
        const BossState& s = w->boss();
        u32 walls = 0, bullets = 0;
        double stunned = -1.0;
        while (music < s.fightTick + 4.0 * kTicksPerBar) {
            const u32 b0 = w->bullets().count;
            stepWorld(*w, music, ship);
            if (cuesIn(*w, BossCue::RmWall)) {
                ++walls;
                bullets += w->bullets().count - b0;
            }
            if (s.mini.state == 4 && stunned < 0.0) stunned = w->music();
        }
        CHECK_MSG(walls >= 1 && bullets >= 16 && stunned > 0.0, "walls %u bullets %u", walls, bullets);
        const Vec2 face = s.mini.face;
        CHECK(w->bossDamageScale(3, 0, w->music(), face * -1.0f) == (s.mini.state == 4 ? 2.0f : prose::kMiniArmour));
        CHECK(w->bossDamageScale(3, 0, w->music(), face) == (s.mini.state == 4 ? 2.0f : 1.0f));
        // Its death: the Pen falls, a Boost orb and 60 Shards for the player, the middle Drop's hold decided.
        w->killBossNextTick(0, true);
        stepWorld(*w, music, ship);
        CHECK_MSG(!w->pen().up && w->boostPickups().count + w->player(0).boostsTaken == 1 && !w->director().midHolding && s.dying, "pen %d orbs %u holding %d dying %d active %d",
                  w->pen().up ? 1 : 0, w->boostPickups().count, w->director().midHolding ? 1 : 0, s.dying ? 1 : 0, s.active ? 1 : 0);
        delete w;
    }
    // Alpha Hound: never more than 30 called Hounds alive; winded (x 2) for two beats after its lunge.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->spawnBossNow(kTicksPerBar, Vec2{420.0f, 0.0f}, BossKind::AlphaHound);
        const BossState& s = w->boss();
        u32 most = 0, howls = 0;
        bool winded = false;
        while (music < s.fightTick + 12.0 * kTicksPerBar) {
            stepWorld(*w, music, Vec2{});
            howls += cuesIn(*w, BossCue::AlHowl);
            u32 n = 0;
            for (u32 i = 0; i < w->enemies().count; ++i)
                n += !(w->enemies().flags[i] & kFlagDead) && (w->enemies().flags[i] & kFlagCalled) ? 1u : 0u;
            if (n > most) most = n;
            if (s.mini.state == 3) winded = winded || w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == 2.0f;
        }
        CHECK_MSG(most <= 30 && most >= 6 && howls >= 10 && winded, "most %u howls %u", most, howls);
        delete w;
    }
    // The Hive: at most 160 darts; the Queen takes x 2 on the tell's beat only.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->spawnBossNow(kTicksPerBar, Vec2{500.0f, 0.0f}, BossKind::Hive);
        const BossState& s = w->boss();
        u32 most = 0, doubled = 0;
        while (music < s.fightTick + 10.0 * kTicksPerBar) {
            stepWorld(*w, music, Vec2{});
            u32 n = 0;
            for (u32 i = 0; i < w->enemies().count; ++i)
                n += !(w->enemies().flags[i] & kFlagDead) && (w->enemies().flags[i] & kFlagCalled) ? 1u : 0u;
            if (n > most) most = n;
            doubled += w->bossDamageScale(3, 0, w->music(), Vec2{1.0f, 0.0f}) == 2.0f ? 1u : 0u;
        }
        const double perBeat = kTicksPerBeat / kTicksPerSimTick;
        CHECK_MSG(most == 160 && doubled > 0 && static_cast<double>(doubled) < 6.0 * perBeat, "darts %u, ticks at x 2 %u", most, doubled);
        // Half health: two hives, one with a white decoy.
        w->bossHealthNow(0.45f);
        stepWorld(*w, music, Vec2{});
        CHECK(s.mini.half && s.mini.hives == 2);
        delete w;
    }
    // Carom King: every bounce on a beat (a Prince's on a half beat), at most 60 of its bullets, three Princes at 75, 50, 25%.
    {
        World* w = quietWorld();
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->spawnBossNow(kTicksPerBar, Vec2{200.0f, 100.0f}, BossKind::CaromKing, Vec2{});
        const BossState& s = w->boss();
        u32 offBeat = 0, bounces = 0, mostBullets = 0;
        bool hot = false;
        while (music < s.fightTick + 8.0 * kTicksPerBar) {
            stepWorld(*w, music, Vec2{});
            for (u32 i = 0; i < w->eventCount(); ++i) {
                const Event& e = w->events()[i];
                if (e.type != EventType::BossCue || e.a != static_cast<u8>(BossCue::CkBounce)) continue;
                ++bounces;
                const double grid = e.b ? kTicksPerBeat / 2.0 : kTicksPerBeat;
                if (std::fabs(e.tick - std::floor(e.tick / grid + 0.5) * grid) > 1e-6) ++offBeat;
                hot = hot || w->bossDamageScale(3, 0, e.tick + 10.0, Vec2{1.0f, 0.0f}) == 2.0f;
            }
            if (w->bullets().count > mostBullets) mostBullets = w->bullets().count;
            if (music > s.fightTick + 2.0 * kTicksPerBar && s.mini.splits == 0) w->bossHealthNow(0.7f);
            if (music > s.fightTick + 4.0 * kTicksPerBar && s.mini.splits == 1) w->bossHealthNow(0.2f);
        }
        CHECK_MSG(bounces >= 8 && offBeat == 0 && mostBullets <= 60 && hot && s.mini.splits == 3 && (s.alive & 7u) == 7u,
                  "bounces %u off the beat %u bullets %u splits %u", bounces, offBeat, mostBullets, s.mini.splits);
        // A dead Prince breaks into two ordinary Caroms.
        const u32 e0 = w->enemies().count;
        w->killBossNextTick(1u, false);
        stepWorld(*w, music, Vec2{});
        CHECK(w->enemies().count == e0 + 2 && !(s.alive & 1u));
        delete w;
    }
    // In a run: the first Drop of Track 2 (c2f113a; it was the middle one's) brings one, in place of the mega-wave, and its end
    // moves the Break; it drops the Track's Extra Life as it dies (GAME_DESIGN 5, c2f113a).
    {
        World* w = new (std::nothrow) World;
        w->init(g_species);
        w->resetRun(3, 0.0, 1);
        w->rules().godMode = true;
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->directorMut().track = 2;
        w->directorMut().trackBar = 0;
        const u32 dropAt = 12;  // (play test 3: every Track Short, 8 / 4 / 8 / 4; c2f113a: the first cycle's Drop)
        runTo(*w, music, w->tickOfBar(dropAt) + 2.0 * kTicksPerSimTick, Vec2{});
        const BossState& s = w->boss();
        CHECK_MSG(s.active && isMiniBoss(s.kind) && w->director().midHolding && w->phrase().miniDrop, "kind %u", static_cast<u32>(s.kind));
        CHECK(s.kind == BossKind::AlphaHound || s.kind == BossKind::Hive);
        runTo(*w, music, w->tickOfBar(dropAt + 10) + 10.0, Vec2{});
        CHECK(w->phrase().held && w->phrase().section == Section::Drop);
        w->killBossNextTick(0, true);
        stepWorld(*w, music, Vec2{});
        const u32 end = w->runBar() + 1u + (w->music() - w->tickOfBar(w->runBar()) > 0.5 * kTicksPerBar ? 1u : 0u);
        CHECK_MSG(w->director().midHold == end - (dropAt + 8) && !w->director().midHolding, "hold %u end %u", w->director().midHold, end);
        {
            u32 lives = 0;
            const BoostPickups& bp = w->boostPickups();
            for (u32 i = 0; i < bp.count; ++i) lives += !bp.dead[i] && bp.type[i] == kPickupExtraLife ? 1u : 0u;
            CHECK_MSG(lives == 1 && w->director().extraLifeTrack == 2, "Extra Life orbs %u", lives);
        }
        runTo(*w, music, w->tickOfBar(end) + 5.0, Vec2{});
        CHECK(w->phrase().section == Section::Break && w->phrase().cycle == 1);
        delete w;
    }
}

}  // namespace

// ---------------------------------------------------------------------------------------------- M5 part 2: families

// Steps until `until` with the ship at `ship`, firing along +x when `fire`.
void runShip(World& w, double& music, double until, Vec2 ship, u8 fire = 0) {
    while (music + kTicksPerSimTick <= until + 1e-9) stepWorld(w, music, ship, fire);
}
i32 spawnAt(World& w, Family f, u32 tier, u32 variant, Vec2 at, double land) {
    return w.spawnEnemy(SpeciesTable::index(f, tier, variant), at, 0.0f, land);
}

void testFamiliesM5() {
    g_test = "families (M5 part 2)";
    const SpeciesTable& s = *g_species;
    // The table: sixteen families, 240 species and the broodling; every roster entry is a family x tier x variant row.
    CHECK(kFamilyCount == 16 && SpeciesTable::kFamilySpecies == 240 && SpeciesTable::kCount == 241);
    CHECK(std::strcmp(s.get(static_cast<u16>(SpeciesTable::kBroodling)).id, "broodling") == 0 &&
          s.get(static_cast<u16>(SpeciesTable::kBroodling)).hue == 0 && s.get(static_cast<u16>(SpeciesTable::kBroodling)).radius == 8.0f);
    CHECK(s.find("moth_2_B") == SpeciesTable::index(Family::Moth, 2, kVariantB) && s.find("mimic_5_C") == 239);
    CHECK(s.family(Family::Moth).orbitRadius == 360.0f && s.family(Family::Blink).hops == 4 && s.family(Family::Brood).maxMinisEach == 6);
    CHECK(s.family(Family::Hound).v.shieldArcDeg == 120.0f && s.family(Family::Moth).v.petals == 2 && s.family(Family::Brood).v.swarmOnDeath == 8);
    {
        // Only families with behaviour are picked; the first pass's Track 3 picks Track 3's debuts in their cycles.
        u32 unbuilt = 0, t3 = 0;
        for (u32 k = 0; k < s.rosterCount; ++k) {
            const Species& sp = s.get(s.roster[k].species);
            if (!familyBuilt(sp.family) && s.roster[k].track <= 3) ++unbuilt;
            if (s.roster[k].track == 3) ++t3;
        }
        CHECK(unbuilt == 0 && t3 == 15);
    }
    const double beat = kTicksPerBeat;
    {
        // Moth: its place steps inward a quarter of closePerBar on every beat, down to minRadius.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        const i32 i = spawnAt(*w, Family::Moth, 1, kVariantA, Vec2{300.0f, 0.0f}, music + 1.0);
        runShip(*w, music, 4.0 * beat, Vec2{});
        const Enemies& e = w->enemies();
        const float r0 = e.b[i];
        const double m0 = music;
        runShip(*w, music, 8.0 * beat, Vec2{});
        const float beats = static_cast<float>(std::floor(music / beat) - std::floor(m0 / beat));
        CHECK_MSG(std::fabs((r0 - e.b[i]) - 0.25f * s.family(Family::Moth).closePerBar * beats) < 1.0f, "radius %.1f -> %.1f in %.0f beats", r0, e.b[i], beats);
        runShip(*w, music, 60.0 * beat, Vec2{});
        CHECK(e.b[i] == s.family(Family::Moth).minRadius && length(Vec2{e.x[i], e.y[i]}) < s.family(Family::Moth).minRadius + 40.0f);
        delete w;
    }
    {
        // Blink: it lands on a point of a star of hopWu round the player, hops on four beats (every second point), then
        // tells its strike for a beat, strikes, rests, and starts a new star.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        const i32 i = spawnAt(*w, Family::Blink, 2, kVariantB, Vec2{0.0f, 300.0f}, music + 1.0);
        const Enemies& e = w->enemies();
        const float hop = s.family(Family::Blink).hopWu;
        runShip(*w, music, 0.6 * beat, Vec2{});
        CHECK(e.state[i] == kHop && std::fabs(length(Vec2{e.x[i], e.y[i]}) - hop) < 1.0f);
        u32 hops = 0, bullets = 0;
        u8 last = e.step[i];
        while (music < 8.0 * beat && e.state[i] == kHop) {
            const u32 b0 = w->bullets().count;
            stepWorld(*w, music, Vec2{});
            if (e.step[i] != last) {
                ++hops;
                last = e.step[i];
                CHECK(std::fabs(length(Vec2{e.x[i], e.y[i]}) - hop) < 1.0f);
                bullets += w->bullets().count - b0;
            }
        }
        CHECK_MSG(hops == 4 && e.state[i] == kTelegraph, "hops %u state %u", hops, e.state[i]);
        CHECK_MSG(bullets == 4 * s.family(Family::Blink).v.pathBullets, "path bullets %u", bullets);
        runShip(*w, music, music + 1.05 * beat, Vec2{});
        CHECK(e.state[i] == kDash);
        runShip(*w, music, music + 1.0 * beat, Vec2{});
        CHECK(e.state[i] == kRest);
        runShip(*w, music, music + 2.1 * beat, Vec2{});
        CHECK(e.state[i] == kHop && e.step[i] == 0);
        delete w;
    }
    {
        // Brood B hatches its clutch on the bar line, never more than maxMinisEach of its own; Brood C bursts into its swarm.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->rules().godMode = true;
        const i32 b = spawnAt(*w, Family::Brood, 1, kVariantB, Vec2{500.0f, 0.0f}, music + 1.0);
        runShip(*w, music, 2.5 * kTicksPerBar, Vec2{});  // clutches on the bar lines 1 and 2
        u32 minis = 0;
        const Enemies& e = w->enemies();
        for (u32 k = 0; k < e.count; ++k)
            if (!(e.flags[k] & kFlagDead) && e.species[k] == SpeciesTable::kBroodling) ++minis;
        (void)b;
        CHECK_MSG(minis >= 2 && minis <= s.family(Family::Brood).maxMinisEach, "minis %u", minis);
        delete w;
        w = quietWorld();
        music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->rules().godMode = true;
        const i32 c = spawnAt(*w, Family::Brood, 2, kVariantC, Vec2{300.0f, 0.0f}, music + 1.0);
        runShip(*w, music, beat, Vec2{});
        w->setEnemyHp(static_cast<u32>(c), 0.001f);
        // The ship's stream kills it; its swarm is there the same tick.
        const u16 broodC = SpeciesTable::index(Family::Brood, 2, kVariantC);
        bool killed = false;
        for (u32 t = 0; t < 240 && !killed; ++t) {
            stepWorld(*w, music, Vec2{}, 1);
            for (u32 k = 0; k < w->eventCount(); ++k) killed = killed || (w->events()[k].type == EventType::Kill && w->events()[k].species == broodC);
        }
        (void)c;
        const Enemies& e2 = w->enemies();
        u32 swarm = 0;
        for (u32 k = 0; k < e2.count; ++k)
            if (!(e2.flags[k] & kFlagDead) && e2.species[k] == SpeciesTable::kBroodling) ++swarm;
        CHECK_MSG(killed && swarm == s.family(Family::Brood).v.swarmOnDeath, "killed %d swarm %u", killed ? 1 : 0, swarm);
        delete w;
    }
    {
        // Hound C: its front arc sheds a shot, its back takes one.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        const i32 h = spawnAt(*w, Family::Hound, 2, kVariantC, Vec2{400.0f, 0.0f}, music + 1.0);
        runShip(*w, music, 0.5 * beat, Vec2{});
        w->setEnemyB(static_cast<u32>(h), kPi);  // the shield faces the ship (at the origin)
        CHECK(w->shieldWouldStop(static_cast<u32>(h), Vec2{1.0f, 0.0f}) && !w->shieldWouldStop(static_cast<u32>(h), Vec2{-1.0f, 0.0f}));
        delete w;
    }
    {
        // Drifter B: a beat's tell, then one aimed shot.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        const i32 d = spawnAt(*w, Family::Drifter, 1, kVariantB, Vec2{350.0f, 0.0f}, music + 1.0);
        u32 tells = 0, shots = 0;
        while (music < 5.0 * kTicksPerBar) {
            stepWorld(*w, music, Vec2{});
            tells += countEvents(*w, EventType::Telegraph);
            shots += countEvents(*w, EventType::EnemyShot);
        }
        (void)d;
        CHECK_MSG(tells >= 1 && shots >= 1 && shots <= tells, "tells %u shots %u", tells, shots);
        delete w;
    }
    {
        // Flanker: it comes round and circles inside its orbit band.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        const i32 f = spawnAt(*w, Family::Flanker, 1, kVariantA, Vec2{900.0f, 0.0f}, music + 1.0);
        runShip(*w, music, 4.0 * kTicksPerBar, Vec2{});
        const Enemies& e = w->enemies();
        CHECK_MSG(length(Vec2{e.x[f], e.y[f]}) < s.family(Family::Flanker).orbitRadius * tuning::kFlankerOrbitBand + 40.0f, "at %.0f",
                  length(Vec2{e.x[f], e.y[f]}));
        delete w;
    }
    {
        // Skitter: in the ship's line of fire it steps aside.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        const i32 k = spawnAt(*w, Family::Skitter, 1, kVariantA, Vec2{500.0f, 0.0f}, music + 1.0);
        bool dodged = false;
        for (u32 t = 0; t < 60 && !dodged; ++t) {
            stepWorld(*w, music, Vec2{}, 1);
            dodged = w->enemies().state[k] == kDodge;
        }
        CHECK(dodged);
        delete w;
    }
}

// ---------------------------------------------------------------------------------------------- M5 part 4: families

void testFamiliesM5b() {
    g_test = "families (M5 part 4)";
    const SpeciesTable& s = *g_species;
    const double beat = kTicksPerBeat, bar = kTicksPerBar;
    CHECK(familyBuilt(Family::Mimic) && s.family(Family::Wyrm).segmentsMax == 12 && s.family(Family::Tether).pairDistance == 160.0f &&
          s.family(Family::Well).pullRadius == 200.0f && s.family(Family::Mimic).beatStepWu == 80.0f && s.family(Family::Tether).v.triad);
    {
        // Wyrm: a chain, each segment spacing behind the one before; C: cut in two, the rear half grows a head in two beats.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->spawnCreatureNow(SpeciesTable::index(Family::Wyrm, 1, kVariantC), Vec2{600.0f, 0.0f}, kPi, music);
        runShip(*w, music, music + 2.0 * beat, Vec2{-800.0f, 0.0f});
        const Enemies& e = w->enemies();
        u32 n = 0, heads = 0;
        float worst = 0.0f;
        i32 mid = -1;
        for (u32 i = 0; i < e.count; ++i) {
            if (e.flags[i] & kFlagDead) continue;
            ++n;
            heads += e.a[i] > 0.5f ? 1u : 0u;
            if (e.step[i] == 3) mid = static_cast<i32>(i);
            for (u32 j = 0; j < e.count; ++j)
                if (!(e.flags[j] & kFlagDead) && e.serial[j] + 1u == e.serial[i])
                    worst = std::fmax(worst, std::fabs(length(Vec2{e.x[i] - e.x[j], e.y[i] - e.y[j]}) - s.family(Family::Wyrm).segmentSpacing));
        }
        CHECK_MSG(n >= 6 && n <= 12 && heads == 1 && worst < 3.0f, "segments %u, heads %u, spacing off by %.1f", n, heads, worst);
        w->killEnemy(static_cast<u32>(mid), 0);
        runShip(*w, music, music + 3.0 * beat, Vec2{-800.0f, 0.0f});
        heads = 0;
        for (u32 i = 0; i < e.count; ++i) heads += (!(e.flags[i] & kFlagDead) && e.a[i] > 0.5f) ? 1u : 0u;
        CHECK_MSG(heads == 2, "heads after the cut %u", heads);
        delete w;
    }
    {
        // Tether: a pair joined by a lethal beam; one killed, the beam goes.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->spawnCreatureNow(SpeciesTable::index(Family::Tether, 1, kVariantA), Vec2{0.0f, -300.0f}, 0.0f, music);
        runShip(*w, music, music + beat, Vec2{0.0f, 300.0f});
        Vec2 a[4], b[4];
        CHECK(w->tetherBeams(a, b, 4) == 1 && std::fabs(length(a[0] - b[0]) - s.family(Family::Tether).pairDistance) < 4.0f);
        // The ship crossing the beam is hit.
        const Vec2 mid = (a[0] + b[0]) * 0.5f;
        const u32 cells = w->player(0).cells;
        stepWorld(*w, music, mid);
        CHECK(w->player(0).cells == cells - 1);
        const Enemies& e = w->enemies();
        for (u32 i = 0; i < e.count; ++i)
            if (!(e.flags[i] & kFlagDead)) {
                w->killEnemy(i, 0);
                break;
            }
        stepWorld(*w, music, Vec2{0.0f, 900.0f});
        CHECK(w->tetherBeams(a, b, 4) == 0);
        delete w;
    }
    {
        // Weaver: its trail is lethal for two bars; Well: it pulls a ship (none beyond its radius) and B returns what it
        // swallowed as a ring on beat 1; Mimic: it steps on beats to where the player would be turned half round.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->spawnEnemy(SpeciesTable::index(Family::Weaver, 1, kVariantA), Vec2{-600.0f, 400.0f}, 0.0f, music);
        runShip(*w, music, music + bar, Vec2{2000.0f, -1500.0f});
        u32 points = 0;
        for (u32 k = 0; k < World::kTrails; ++k) points += w->trails()[k].n;
        CHECK_MSG(points >= 15, "trail points %u", points);
        const i32 wl = w->spawnEnemy(SpeciesTable::index(Family::Well, 1, kVariantB), Vec2{1000.0f, 0.0f}, 0.0f, music);
        stepWorld(*w, music, Vec2{2000.0f, -1500.0f});
        CHECK(length(w->wellPull(Vec2{1100.0f, 0.0f})) > 1.0f && length(w->wellPull(Vec2{1300.0f, 0.0f})) == 0.0f);
        (void)wl;
        u32 bullets0 = w->bullets().count;
        // Shots into it from beside it, then the bar line.
        for (u32 k = 0; k < 30; ++k) stepWorld(*w, music, Vec2{850.0f, 0.0f}, 1);
        runShip(*w, music, (std::floor(music / bar) + 1.0) * bar + 2.0, Vec2{2000.0f, -1500.0f});
        CHECK_MSG(w->bullets().count >= bullets0 + 12, "bullets %u -> %u", bullets0, w->bullets().count);
        delete w;
    }
    {
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        const i32 m = w->spawnEnemy(SpeciesTable::index(Family::Mimic, 1, kVariantA), Vec2{400.0f, 0.0f}, 0.0f, music);
        runShip(*w, music, music + 2.0 * beat, Vec2{});
        // The centre is (200, 0): with the ship at (-100, 0) its place is (500, 0).
        runShip(*w, music, music + 8.0 * beat, Vec2{-100.0f, 0.0f});
        const Enemies& e = w->enemies();
        CHECK_MSG(std::fabs(e.x[m] - 500.0f) < 2.0f && std::fabs(e.y[m]) < 2.0f, "mimic at (%.0f, %.0f)", e.x[m], e.y[m]);
        delete w;
    }
    {
        // The slice's families' variants of Tracks 4 to 6: Drifter C splits a tier down; Moth C bursts as its spiral closes;
        // Sentry C's dome (with a neighbour near) stops shots.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        const i32 dr = w->spawnEnemy(SpeciesTable::index(Family::Drifter, 3, kVariantC), Vec2{900.0f, 0.0f}, 0.0f, music);
        stepWorld(*w, music, Vec2{});
        w->killEnemy(static_cast<u32>(dr), 0);
        stepWorld(*w, music, Vec2{});
        u32 children = 0;
        for (u32 i = 0; i < w->enemies().count; ++i)
            children += (!(w->enemies().flags[i] & kFlagDead) && w->enemies().species[i] == SpeciesTable::index(Family::Drifter, 2, kVariantA)) ? 1u : 0u;
        CHECK_MSG(children == s.family(Family::Drifter).v.splitInto, "children %u", children);
        const i32 mo = w->spawnEnemy(SpeciesTable::index(Family::Moth, 1, kVariantC), Vec2{0.0f, -400.0f}, 0.0f, music);
        const u32 b0 = w->bullets().count;
        bool burst = false;
        for (u32 k = 0; k < 60 * 60 && !burst; ++k) {
            stepWorld(*w, music, Vec2{});
            burst = (w->enemies().flags[mo] & kFlagDead) != 0;
        }
        CHECK_MSG(burst && w->bullets().count >= b0 + s.family(Family::Moth).v.burstBullets - 2, "moth burst %d, bullets %u", burst ? 1 : 0, w->bullets().count);
        delete w;
    }
    {
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        const i32 se = w->spawnEnemy(SpeciesTable::index(Family::Sentry, 1, kVariantC), Vec2{500.0f, 0.0f}, 0.0f, music);
        w->setEnemyHp(static_cast<u32>(se), 1000.0f);
        const i32 nb = w->spawnEnemy(SpeciesTable::index(Family::Sentry, 1, kVariantA), Vec2{560.0f, 120.0f}, 0.0f, music);
        w->setEnemyHp(static_cast<u32>(nb), 1000.0f);
        runShip(*w, music, music + 2.0 * beat, Vec2{});
        for (u32 k = 0; k < 40; ++k) stepWorld(*w, music, Vec2{}, 1);
        CHECK_MSG(w->enemies().hp[se] == 1000.0f, "the dome let %.0f damage through", 1000.0f - w->enemies().hp[se]);
        delete w;
    }
}

// ---------------------------------------------------------------------------------------------- M5 part 3: upgrades

u32 countFx(const World& w, u8 fx) {
    u32 n = 0;
    for (u32 i = 0; i < w.eventCount(); ++i) n += (w.events()[i].type == EventType::Upgrade && w.events()[i].b == fx) ? 1u : 0u;
    return n;
}
u32 countShots(const World& w, u8 kind, u8 mod = 0) {
    u32 n = 0;
    for (u32 i = 0; i < w.shots().count; ++i)
        if (!w.shots().dead[i] && w.shots().kind[i] == kind && (!mod || (w.shots().mods[i] & mod))) ++n;
    return n;
}

void testUpgradesM5() {
    g_test = "upgrades (M5 part 3)";
    const double beat = kTicksPerBeat, bar = kTicksPerBar;
    const u16 target = SpeciesTable::index(Family::Drifter, 1, kVariantA);
    {
        // Nova Stock: +1 capacity and a Nova; Encore: a Boost one bar longer; Collector: 250 Shards spawn a Boost.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->grantUpgrade(0, kNovaStock);
        CHECK(w->player(0).novaCap == kNovaCapacity + 1 && w->player(0).novas == kStartNovas + 1);
        w->grantUpgrade(0, kEncore);
        w->grantBoost(0, kHyper);
        const ActiveBoost& b = w->player(0).boosts[0];
        CHECK(b.type == kHyper && std::fabs((b.until - b.from) - 5.0 * bar) < 1e-6);
        w->grantUpgrade(0, kCollector);
        w->collectShardsNow(0, 249);
        const u32 before = w->player(0).boostsTaken;
        w->collectShardsNow(0, 1);
        CHECK(w->player(0).boostsTaken == before + 1);
        delete w;
    }
    {
        // Accent: the volley on beat 1 is a lance along the aim: an enemy 600 wu away dies at once (3 x the shot).
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->grantUpgrade(0, kAccent);
        runShip(*w, music, bar - 40.0, Vec2{});
        const i32 i = w->spawnEnemy(target, Vec2{600.0f, 0.0f}, 0.0f, music);
        w->setEnemyHp(static_cast<u32>(i), 2.5f);
        runShip(*w, music, bar - 2.0, Vec2{});
        u32 lances = 0;
        bool dead = false;
        while (music < bar + 4.0) {
            stepWorld(*w, music, Vec2{}, 1);
            lances += countFx(*w, kFxLance);
            dead = dead || countEvents(*w, EventType::Kill) > 0;
        }
        CHECK_MSG(lances == 1 && dead, "lances %u, killed %d", lances, dead ? 1 : 0);
        delete w;
    }
    {
        // Heavy Step: every fourth volley's middle lane is a slow orb; Echo: each volley again a beat later, as ghosts.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->grantUpgrade(0, kHeavyStep);
        w->grantUpgrade(0, kEcho);
        const u32 v0 = w->player(0).volleys;
        u32 orbs = 0;
        while (w->player(0).volleys < v0 + 8) {
            stepWorld(*w, music, Vec2{}, 1);
            orbs = countShots(*w, kShotOrb) > orbs ? countShots(*w, kShotOrb) : orbs;
        }
        CHECK_MSG(orbs == 2, "orbs %u in 8 volleys", orbs);
        u32 echoes = 0, ghosts = 0;
        const double stop = music;
        while (music < stop + 1.5 * beat) {
            stepWorld(*w, music, Vec2{}, 0);
            echoes += countFx(*w, kFxEcho);
            ghosts = countShots(*w, kShotPrimary, kShotGhost) > ghosts ? countShots(*w, kShotPrimary, kShotGhost) : ghosts;
        }
        CHECK_MSG(echoes >= 4 && ghosts >= 1, "echo volleys %u, ghosts %u", echoes, ghosts);
        delete w;
    }
    {
        // Fork: the first hit splits a shot into two; Burn: the hit sets it alight, and the fire hurts on every beat.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->grantUpgrade(0, kFork);
        w->grantUpgrade(0, kBurn);
        const i32 i = w->spawnEnemy(target, Vec2{200.0f, 0.0f}, 0.0f, music);
        w->setEnemyHp(static_cast<u32>(i), 1000.0f);
        u32 forks = 0;
        for (u32 k = 0; k < 40 && !w->burning(static_cast<u32>(i)); ++k) {
            stepWorld(*w, music, Vec2{-200.0f, 0.0f}, 1);
            forks += countFx(*w, kFxFork);
        }
        CHECK(forks >= 1 && w->burning(static_cast<u32>(i)));
        runShip(*w, music, music + 8.0, Vec2{-200.0f, 0.0f});
        const float hp0 = w->enemies().hp[i];
        runShip(*w, music, std::ceil(music / beat) * beat + 1.0, Vec2{-2000.0f, 0.0f});
        CHECK_MSG(w->enemies().hp[i] <= hp0 - tuning::kBurnDamage + 1e-3f, "hp %.2f -> %.2f", hp0, w->enemies().hp[i]);
        delete w;
    }
    {
        // Freeze Frame: the frame's bodies stop and die in one hit; Time Stretch: the world at half speed.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        const i32 i = w->spawnEnemy(target, Vec2{300.0f, 100.0f}, 0.0f, music);
        w->setEnemyHp(static_cast<u32>(i), 1000.0f);
        runShip(*w, music, music + beat, Vec2{});
        w->grantBoost(0, kFreezeFrame);
        stepWorld(*w, music, Vec2{});
        const Vec2 at{w->enemies().x[i], w->enemies().y[i]};
        runShip(*w, music, music + beat, Vec2{});
        CHECK(w->frozen(static_cast<u32>(i)) && w->enemies().x[i] == at.x && w->enemies().y[i] == at.y);
        bool killed = false;
        for (u32 k = 0; k < 60 && !killed; ++k) {
            stepWorld(*w, music, Vec2{0.0f, 100.0f}, 1);
            killed = countEvents(*w, EventType::Kill) > 0;
        }
        CHECK(killed);
        w->grantBoost(0, kTimeStretch);
        stepWorld(*w, music, Vec2{});
        CHECK(w->worldStretch() == tuning::kTimeStretchSpeed);
        delete w;
    }
    {
        // Black Hole: the next Nova is a singularity that pulls for three beats, then blows on a beat.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        const i32 i = w->spawnEnemy(target, Vec2{500.0f, 0.0f}, 0.0f, music);
        w->setEnemyHp(static_cast<u32>(i), 1e6f);
        runShip(*w, music, music + beat, Vec2{});
        w->grantBoost(0, kBlackHole);
        const double press = music;
        stepWorld(*w, music, Vec2{}, 0, true);
        const PlayerState& pl = w->player(0);
        CHECK(pl.holeFrom >= 0.0 && pl.novaBlastTick >= press + 3.0 * beat - 1e-6 && pl.novaBlastTick < press + 4.0 * beat + 1e-6 && !pl.boost(kBlackHole));
        const float d0 = length(Vec2{w->enemies().x[i], w->enemies().y[i]});
        runShip(*w, music, music + beat, Vec2{});
        CHECK_MSG(length(Vec2{w->enemies().x[i], w->enemies().y[i]}) < d0 - 100.0f, "pulled from %.0f to %.0f", d0,
                  length(Vec2{w->enemies().x[i], w->enemies().y[i]}));
        u32 blows = 0;
        while (music < press + 5.0 * beat) {
            stepWorld(*w, music, Vec2{});
            blows += countFx(*w, kFxHoleBlow);
        }
        CHECK(blows == 1 && pl.holeFrom < 0.0);
        delete w;
    }
    {
        // Minelayer: a cluster on every bar line (two at level 2), all blowing on the next one; Crescendo: +5% a clean bar.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->grantUpgrade(0, kMinelayer);
        w->grantUpgrade(0, kMinelayer);
        w->grantUpgrade(0, kCrescendo);
        u32 drops = 0, blows = 0;
        while (music < 2.0 * bar + 2.0) {
            stepWorld(*w, music, Vec2{});
            drops += countFx(*w, kFxMineDrop);
            blows += countFx(*w, kFxMineBlow);
        }
        CHECK_MSG(drops == 4 && blows == 2, "drops %u, blows %u", drops, blows);
        CHECK(std::fabs(w->crescendo(0) - (1.0f + 2.0f * tuning::kCrescendoStep)) < 1e-4f);
        w->hitNextTick(0);
        stepWorld(*w, music, Vec2{});
        CHECK(w->crescendo(0) == 1.0f);
        delete w;
    }
    {
        // The band's clock (audio, riffs.*): Singularity pulls in the bars whose song index + 1 is a multiple of four and
        // blows on the next One; the Sweeper's arm stands at twelve o'clock on the One of every second song bar; Minelayer
        // never blows in the first bar after it is taken. With the song two bars ahead of the run too.
        for (i64 shift : {0, 2, 5}) {
            World* w = quietWorld();
            w->setSongBarShift(shift);
            double music = 0.0;
            stepWorld(*w, music, Vec2{});
            for (u32 k = 0; k < 3; ++k) {
                w->grantUpgrade(0, kSingularity);
                w->grantUpgrade(0, kSweeper);
            }
            runShip(*w, music, 0.5 * bar, Vec2{});
            w->grantUpgrade(0, kMinelayer);  // mid-bar 0
            u32 wellsBad = 0, wells = 0, blowsBad = 0, sweepBad = 0;
            i64 firstMineBlowBar = -1;
            while (music < 12.0 * bar) {
                const double m0 = music;
                stepWorld(*w, music, Vec2{});
                const i64 b = static_cast<i64>(std::floor(music / bar));
                const i64 song = b + shift;
                for (u32 k = 0; k < w->eventCount(); ++k) {
                    const Event& ev = w->events()[k];
                    if (ev.type != EventType::Upgrade) continue;
                    if (ev.b == kFxWell) {
                        ++wells;
                        const double inBar = std::fmod(music, bar);
                        if ((song + 1) % 4 != 0 || inBar < beat || inBar > beat + 4.0) ++wellsBad;
                    }
                    if (ev.b == kFxWellBlow && (std::fmod(music, bar) > 4.0 || song % 4 != 0)) ++blowsBad;
                    if (ev.b == kFxMineBlow && firstMineBlowBar < 0) firstMineBlowBar = b;
                }
                if (std::floor(m0 / bar) != std::floor(music / bar) && song % 2 == 0) {
                    // just after the One of an even song bar: the arm is at twelve o'clock (y down: -pi/2)
                    const float a = w->sweeperAngle(0);
                    if (std::fabs(wrapAngle(a + 0.5f * kPi)) > 0.1f) ++sweepBad;
                }
            }
            CHECK_MSG(wells >= 2 && wellsBad == 0 && blowsBad == 0 && sweepBad == 0 && firstMineBlowBar == 2,
                      "shift %lld: wells %u (%u off the band), blows off %u, sweeper off %u, first mine blast in bar %lld", static_cast<long long>(shift),
                      wells, wellsBad, blowsBad, sweepBad, static_cast<long long>(firstMineBlowBar));
            delete w;
        }
    }
    {
        // How a Trio is dealt (GAME_DESIGN 6.1): the weights; the first orb from the held firepower.
        Loadout l;
        CHECK(dealWeight(l, kAccent) == tuning::kDealModNew && dealWeight(l, kLancer) == tuning::kDealRiffNew);
        l.mods[kSplit] = 1;
        CHECK(dealWeight(l, kSplit) == tuning::kDealModHeld && dealWeight(l, kWave) == tuning::kDealModNew * tuning::kDealFusionFactor);
        l.riffs[riffIndex(kOrbitals)] = 2;
        CHECK(dealWeight(l, kOrbitals) == tuning::kDealRiffLevel && dealWeight(l, kShatter) == tuning::kDealModNew * tuning::kDealFusionFactor);
        Loadout f;  // the staples at their caps, Accent held: the first orb is Accent, always
        f.mods[kSplit] = upgradeInfo(kSplit).cap;
        f.mods[kDoubleTime] = upgradeInfo(kDoubleTime).cap;
        f.mods[kOvercharge] = upgradeInfo(kOvercharge).cap;
        f.mods[kOnBeatCrit] = upgradeInfo(kOnBeatCrit).cap;
        f.mods[kAccent] = 1;
        f.mods[kExtraCell] = 1;  // (held, not firepower)
        Rng rng;
        rng.reseed(5);
        u32 accentFirst = 0, held = 0;
        for (u32 k = 0; k < 300; ++k) {
            u8 s = 0, o[kMaxOrbs];
            const u32 n = drawTrio(f, s, rng, o);
            accentFirst += (n >= 1 && o[0] == kAccent) ? 1u : 0u;
            for (u32 j = 1; j < n; ++j) held += (o[j] == kExtraCell) ? 1u : 0u;
        }
        CHECK_MSG(accentFirst == 300 && held > 20, "Accent first %u of 300, Extra Cell offered %u", accentFirst, held);
    }
    {
        // The caps hold (GAME_DESIGN 6.6, TECH_SPEC 7.1): Split 5, Double-Time 1, Echo 2, Fork 1, Quartet's three wingmen,
        // Rear Guard 2, Chorus and Fractal into a wall of enemies for eight bars: no shot over the player's 1,024.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        for (u8 u : {kSplit, kSplit, kSplit, kDoubleTime, kDoubleTime, kEcho, kEcho, kFork, kFork, kRearGuard, kRearGuard, kShatter, kShatter, kShatter})
            w->grantUpgrade(0, u);
        for (u32 k = 0; k < kRiffMaxLevel; ++k) {
            w->grantUpgrade(0, kMirror);
            w->grantUpgrade(0, kSwarm);
            w->grantUpgrade(0, kFlak);
        }
        w->playerMut(0).loadout.fusions = static_cast<u8>(1u << kQuartet);
        w->grantBoost(0, kChorus);
        w->grantBoost(0, kFractal);
        for (u32 k = 0; k < 120; ++k) {
            const i32 i = w->spawnEnemy(target, Vec2{300.0f + 10.0f * static_cast<float>(k % 12), -300.0f + 50.0f * static_cast<float>(k / 12)}, 0.0f, music);
            if (i >= 0) w->setEnemyHp(static_cast<u32>(i), 1e6f);
        }
        u32 most = 0;
        while (music < 8.0 * bar) {
            stepWorld(*w, music, Vec2{}, 1);
            most = w->shotsLive(0) > most ? w->shotsLive(0) : most;
        }
        CHECK_MSG(w->counters().shotsCapped == 0 && w->counters().shotsDropped == 0 && most <= 1024, "most %u, capped %u, dropped %u", most,
                  w->counters().shotsCapped, w->counters().shotsDropped);
        delete w;
    }
}

// ---------------------------------------------------------------------------------------------- play test 3

void testPlayTest3() {
    g_test = "play test 3";
    const double bar = kTicksPerBar;
    const u16 drifter = SpeciesTable::index(Family::Drifter, 1, kVariantA);
    const auto hitOnce = [&](World& w, double& music) {
        PlayerState& pl = w.playerMut(0);
        pl.invulS = pl.novaInvulS = 0.0f;
        w.spawnBullet(w.player(0).pos, Vec2{}, 0.0f);
        stepWorld(w, music, Vec2{});
    };
    {
        // GAME_DESIGN 6.3 row 1: the Orbitals' guard ring takes a hit instead of a cell (the panic pulse, 1 s invulnerable), its
        // shards relight one by one and it guards again 8 bars after it broke (4 at level 5).
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->grantUpgrade(0, kOrbitals);
        CHECK(w->ringGuards(0) && w->orbitalsLit(0, music) == 3);
        w->playerMut(0).cells = 3;
        hitOnce(*w, music);
        bool ringHit = false;
        for (u32 k = 0; k < w->eventCount(); ++k)
            if (w->events()[k].type == EventType::Hit) ringHit = w->events()[k].c == 1;
        CHECK_MSG(w->player(0).cells == 3 && ringHit && !w->ringGuards(0) && w->orbitalsLit(0, music) == 0, "cells %u, lit %u", w->player(0).cells,
                  w->orbitalsLit(0, music));
        CHECK(std::fabs(w->player(0).invulS - tuning::kRingInvulS) < 0.05f);
        const double broke = w->player(0).ringBroke;
        CHECK(w->orbitalsLit(0, broke + 4.0 * bar + 1.0) == 1 && w->orbitalsLit(0, broke + 8.0 * bar - 1.0) == 2);
        // The next hit while it is dark takes a cell.
        hitOnce(*w, music);
        CHECK(w->player(0).cells == 2);
        runShip(*w, music, broke + 8.0 * bar + 4.0, Vec2{});
        CHECK_MSG(w->ringGuards(0) && w->orbitalsLit(0, music) == 3, "guards %d, lit %u", w->ringGuards(0) ? 1 : 0, w->orbitalsLit(0, music));
        for (u32 k = 0; k < 4; ++k) w->grantUpgrade(0, kOrbitals);
        hitOnce(*w, music);
        const double broke5 = w->player(0).ringBroke;
        CHECK_MSG(broke5 > 0.0 && w->orbitalsLit(0, broke5 + 4.0 * bar + 1.0) == 11 && w->orbitalsLit(0, broke5 + 2.0 * bar) == 5,
                  "level %u, broke %.0f, lit %u / %u, guards %d", w->player(0).loadout.riffs[riffIndex(kOrbitals)], broke5, w->orbitalsLit(0, broke5 + 2.0 * bar),
                  w->orbitalsLit(0, broke5 + 4.0 * bar), w->ringGuards(0) ? 1 : 0);
        delete w;
    }
    {
        // GAME_DESIGN 5: the Extra Life. Taken (one held), spent on the hit that would end the run once Auto Nova has none to
        // fire: two cells, a free Nova (no stock), a bar of invulnerability, half the Chain.
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->dropExtraLifeNow(Vec2{}, 0);
        stepWorld(*w, music, Vec2{});
        CHECK(w->player(0).extraLives == 1 && countEvents(*w, EventType::ExtraLife) == 1);
        w->playerMut(0).cells = 0;
        w->playerMut(0).novas = 0;
        w->playerMut(0).chain = 8;
        hitOnce(*w, music);
        CHECK_MSG(w->player(0).alive && w->player(0).extraLives == 0 && w->player(0).cells == tuning::kExtraLifeCells && w->player(0).novaPending &&
                      w->player(0).novas == 0 && w->player(0).chain == 4,
                  "alive %d, held %u, cells %u, nova %d, chain %u", w->player(0).alive ? 1 : 0, w->player(0).extraLives, w->player(0).cells,
                  w->player(0).novaPending ? 1 : 0, w->player(0).chain);
        CHECK(w->player(0).invulS > 1.9f);  // a bar at 120 BPM: 2 s
        // None held: the run ends.
        runShip(*w, music, music + 4.0 * bar, Vec2{});
        w->playerMut(0).cells = 0;
        w->playerMut(0).novas = 0;
        hitOnce(*w, music);
        CHECK(!w->player(0).alive);
        delete w;
    }
    {
        // GAME_DESIGN 6.2 rows 13 and 15 (half, one step): Arc 1 arcs to one more within 160 wu and that kill arcs no further;
        // Shatter 1 throws two pieces.
        for (u32 pass = 0; pass < 2; ++pass) {
            World* w = quietWorld();
            double music = 0.0;
            stepWorld(*w, music, Vec2{});
            w->grantUpgrade(0, pass == 0 ? kArc : kShatter);
            i32 ids[3];
            for (u32 k = 0; k < 3; ++k) {
                ids[k] = w->spawnEnemy(drifter, Vec2{300.0f, 120.0f * static_cast<float>(k)}, 0.0f, music);
                w->setEnemyHp(static_cast<u32>(ids[k]), 0.5f);
            }
            const u32 h2 = w->enemies().handle[ids[2]];
            u32 arcs = 0, pieces = 0;
            for (u32 t = 0; t < 30; ++t) {
                stepWorld(*w, music, Vec2{}, t < 10 ? 1 : 0);
                arcs += countFx(*w, kFxArc);
                for (u32 k = 0; k < w->eventCount(); ++k)
                    if (w->events()[k].type == EventType::Upgrade && w->events()[k].b == kFxShatter && pieces == 0) pieces = w->events()[k].c;
            }
            if (pass == 0)
                CHECK_MSG(arcs == 1 && w->enemyIndex(h2) >= 0, "Arc: %u arcs, the third %s", arcs, w->enemyIndex(h2) >= 0 ? "lives" : "died");
            else
                CHECK_MSG(pieces == 2, "Shatter 1: %u pieces", pieces);
            delete w;
        }
        CHECK(tuning::kArcReachWu == 160.0f && tuning::kBurnSpreadWu == 45.0f && tuning::kFreezeChainMax == 12 && tuning::kSupernovaShare == 0.5f);
    }
    {
        // GAME_DESIGN 2: in a fight the assist aims at the boss (Kaleidos: a resting arm or the core).
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->spawnBossNow(music, Vec2{500.0f, 0.0f});
        Vec2 at{};
        CHECK(!w->assistBossAim(Vec2{}, music, at));  // (its entrance: no fight yet)
        runShip(*w, music, w->boss().fightTick + 8.0, Vec2{});
        CHECK_MSG(w->assistBossAim(Vec2{}, music, at) && length(at - w->boss().pos) < tuning::kBossArmOuter + 1.0f, "at %.0f, %.0f", at.x, at.y);
        delete w;
    }
    {
        // GAME_DESIGN 2 (27077b6, the owner): the assist fires at half rate (fire 2: every second grid step); a thumb on the aim
        // stick fires on every step (fire 1).
        World* w = quietWorld();
        double music = 0.0;
        stepWorld(*w, music, Vec2{});
        w->spawnEnemy(SpeciesTable::index(Family::Drifter, 1, kVariantA), Vec2{300.0f, 0.0f}, 0.0f, 0.0);
        PlayerController c;
        c.reset(Vec2{});
        StickInput in;
        in.assist = true;
        c.update(in, kDt, music, 120.0f, *w, 0);
        CHECK_MSG(c.pose().fire == 2, "assist fire %u", c.pose().fire);
        in.aim = Vec2{1.0f, 0.0f};
        in.aimActive = true;
        c.update(in, kDt, music, 120.0f, *w, 0);
        CHECK_MSG(c.pose().fire == 1, "aimed fire %u", c.pose().fire);
        delete w;
    }
}

int main() {
    g_species = new (std::nothrow) SpeciesTable;
    if (!g_species || !g_species->load()) {
        std::printf("species.json did not load\n");
        return 1;
    }
    testGrid();
    testPools();
    testChainRules();
    testLeadRule();
    testNova();
    testSpecies();
    testDeterminismShort();
    testNoAllocations();
    testDirectorCycle();
    testCrowd();
    testTrioRules();
    testUpgradeStacking();
    testPlayTest1();
    testAutoNova();
    testBoss();
    testPlayTest2();
    testSetList();
    testMiddleDrop();
    testLongwave();
    testChoir();
    testHorizon();
    testPolyrhythm();
    testFeedback();
    testMinibosses2();
    testShotStream();
    testArena();
    testCadence();
    testArpeggiator();
    testMorgana();
    testReprise();
    testMinis3();
    testMinibosses();
    testFamiliesM5();
    testUpgradesM5();
    testPlayTest3();
    testFamiliesM5b();
    std::printf("[sim tests] %d checks, %d failures\n", g_checks, g_failures);
    delete g_species;
    return g_failures;
}
