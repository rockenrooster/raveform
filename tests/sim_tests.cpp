// Unit tests for the simulation (M3): the spatial grid, the pools and their handles, the Chain and score
// rules, the lead rule on the sim's grid, the Nova's timing, the species table, a short determinism
// check and an allocation-free tick. Run by scripts/test.ps1 (with raveform_headless --sim determinism).
// Exit code: the number of failures (0 = pass).

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "core/alloc_counter.h"
#include "game/rng.h"
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
    CHECK(ok == kMaxEnemyBullets);
    CHECK(b->counters().bulletsDropped == 5);
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
    // Shards on the ship: +1 Chain each.
    for (u32 i = 0; i < 5; ++i) w->spawnShard(Vec2{2.0f, 1.0f}, Vec2{}, 0);
    stepWorld(*w, music, Vec2{});
    CHECK(w->player(0).chain == 5);
    CHECK(w->player(0).bestChain == 5);
    CHECK(countEvents(*w, EventType::Pickup) == 5);
    // Score = value x Chain.
    const u16 hound = SpeciesTable::index(Family::Hound, 2, kVariantB);
    const float value = g_species->get(hound).score;
    i32 k = w->spawnEnemy(hound, Vec2{600.0f, 0.0f}, 0.0f, 0.0);
    w->killEnemy(static_cast<u32>(k), 0);
    CHECK(w->player(0).score == static_cast<u64>(std::llround(value * 5.0f)));
    // A Chain of 0 still scores x1.
    World* z = quietWorld();
    double mz = 0.0;
    stepWorld(*z, mz, Vec2{});
    k = z->spawnEnemy(hound, Vec2{600.0f, 0.0f}, 0.0f, 0.0);
    z->killEnemy(static_cast<u32>(k), 0);
    CHECK(z->player(0).score == static_cast<u64>(std::llround(value)));
    delete z;
    // The kill dropped its Shards (2 for tier II), loose until the ship comes within the magnet.
    stepWorld(*w, music, Vec2{});
    CHECK(w->shards().count == g_species->get(hound).shards);
    // Nothing decays the Chain with time.
    for (u32 t = 0; t < 600; ++t) stepWorld(*w, music, Vec2{});
    CHECK(w->player(0).chain == 5);
    // Nine more: 14. A hit halves it (rounded down), costs a cell, grants invulnerability.
    for (u32 i = 0; i < 9; ++i) w->spawnShard(Vec2{0.0f, 0.0f}, Vec2{}, 0);
    stepWorld(*w, music, Vec2{});
    CHECK(w->player(0).chain == 14);
    w->spawnBullet(Vec2{0.0f, 0.0f}, Vec2{}, 0.0f);
    stepWorld(*w, music, Vec2{});
    CHECK(w->player(0).chain == 7);
    CHECK(w->player(0).bestChain == 14);
    CHECK(w->player(0).cells == kStartCells - 1);
    CHECK(w->player(0).invulnerable());
    CHECK(countEvents(*w, EventType::Hit) == 1);
    // Invulnerable for 1.5 s: a second bullet (passing through) does nothing.
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
    // Death: a hit with no cell left.
    PlayerState& p = w->playerMut(0);
    p.cells = 0;
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
    CHECK(s.rosterCount == 12);
    CHECK(s.budgetCycles[0] == 4 && s.budgetCycles[1] == 3);
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

}  // namespace

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
    std::printf("[sim tests] %d checks, %d failures\n", g_checks, g_failures);
    delete g_species;
    return g_failures;
}
