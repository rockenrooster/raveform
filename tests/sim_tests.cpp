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

// ---------------------------------------------------------------------------------------------- M3 stage B

void testDirectorCycle() {
    g_test = "director";
    DirectorState d;
    // Track 1: four Short cycles of 8/4/8/4 bars at 120 BPM.
    Phrase p = phraseAt(d, 0);
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
    // The fourth Drop is the boss's: held past its eight bars while the boss lives.
    p = phraseAt(d, 84);
    CHECK(p.cycle == 4 && p.section == Section::Drop && p.bossDrop);
    p = phraseAt(d, 97);
    CHECK(p.section == Section::Drop && p.bossDrop && p.sectionBar == 13);
    // The boss died: the Track-change Break (8 bars, 120 -> 126 BPM, bar by bar), then Track 2.
    d.changeBar = 98;
    p = phraseAt(d, 98);
    CHECK(p.trackChange && p.section == Section::Break && p.track == 2 && p.sectionBars == 8 && p.bpm == 120.0f);
    CHECK(std::fabs(phraseAt(d, 105).bpm - 126.0f) < 1e-3f);
    p = phraseAt(d, 106);
    CHECK(!p.trackChange && p.track == 2 && p.song == 1 && p.cycle == 1 && p.section == Section::Groove && p.sectionBars == 16);
    CHECK(phraseAt(d, 106 + 16).section == Section::Build && phraseAt(d, 106 + 24).section == Section::Drop &&
          phraseAt(d, 106 + 24).sectionBars == 16 && phraseAt(d, 106 + 40).section == Section::Break);
    CHECK(phraseAt(d, 106 + 96 + 24).bossDrop && phraseAt(d, 106 + 96 + 24).cycle == 3);
    // Track 3 is Reef (three Standard cycles at 132 BPM); then the loop over all three, 4 BPM a Track faster.
    CHECK(bpmOfTrack(3) == 132.0f && bpmOfTrack(4) == 136.0f && bpmOfTrack(5) == 140.0f && bpmOfTrack(40) == 174.0f);
    CHECK(songOfTrack(3) == 2 && cyclesOfTrack(3) == 3 && formOfTrack(3) == 1);
    CHECK(songOfTrack(4) == 0 && cyclesOfTrack(4) == 4 && formOfTrack(4) == 0 && songOfTrack(5) == 1 && songOfTrack(6) == 2);
    // Budgets (species.json firstSlice, threats only), rising past the data; section scales (GAME_DESIGN 8).
    const float(*b)[8] = g_species->budget;
    CHECK(baseBudget(b, g_species->budgetCycles, 1, 1) == 2.0f && baseBudget(b, g_species->budgetCycles, 1, 4) == 6.0f);
    CHECK(baseBudget(b, g_species->budgetCycles, 2, 3) == 12.0f);
    CHECK(baseBudget(b, g_species->budgetCycles, 3, 1) == 14.0f && baseBudget(b, g_species->budgetCycles, 3, 3) == 18.0f);
    CHECK(baseBudget(b, g_species->budgetCycles, 4, 1) == 20.0f);
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

    // In the world: the sections arrive in order on their bar lines, the first Trio opens with the first
    // Break (bar 20, 40 s in), the Drop doubles score and magnet.
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
            if (ev.type == EventType::TrioOpen && !trioTick) trioTick = t;
        }
        if (w->phrase().surge() && w->rules().scoreMultiplier == 2.0f && w->rules().magnetMultiplier == 2.0f) surgeRules = true;
    }
    CHECK(n >= 5 && order[0] == 0 && order[1] == 1 && order[2] == 2 && order[3] == 3 && order[4] == 0);
    CHECK_MSG(trioTick > 39 * 60 && trioTick < 41 * 60, "first Trio at tick %u", trioTick);
    CHECK(surgeRules);
    delete w;
}

void testCrowd() {
    g_test = "crowd";
    // Targets (species.json firstSlice.crowd): by Track and cycle, then rising a step a Track up to the max.
    const SpeciesTable::Crowd& c = g_species->crowd;
    CHECK(crowdTarget(c, 1, 1) == 20.0f && crowdTarget(c, 1, 4) == 120.0f);
    CHECK(crowdTarget(c, 2, 1) == 150.0f && crowdTarget(c, 3, 3) == 300.0f);
    CHECK(crowdTarget(c, 4, 1) == 320.0f && crowdTarget(c, 30, 1) == 400.0f);
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
    for (u32 t = 0; t < static_cast<u32>(6.0 * perBar); ++t) stepWorld(*w, music, Vec2{});
    CHECK_MSG(w->crowdNear() >= 15 && w->crowdNear() <= 45, "near %u (goal %.0f)", w->crowdNear(), w->crowdGoal());
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
    // A fresh loadout: three different options, at least one weapon, a new Riff within three Trios.
    for (u32 run = 0; run < 400; ++run) {
        Loadout l;
        u8 since = 0;
        bool riffSeen = false;
        for (u32 trio = 0; trio < 3; ++trio) {
            u8 o[3];
            const u32 n = drawTrio(l, since, rng, o);
            CHECK(n == 3);
            CHECK(o[0] != o[1] && o[1] != o[2] && o[0] != o[2]);
            CHECK(upgradeInfo(o[0]).weapon || upgradeInfo(o[1]).weapon || upgradeInfo(o[2]).weapon);
            for (u32 k = 0; k < 3; ++k) riffSeen = riffSeen || (isRiff(o[k]) && l.level(o[k]) == 0);
            // Take a Mod that is not a Riff when there is one (so the guarantee is what brings the Riff).
            for (u32 k = 0; k < 3; ++k)
                if (!isRiff(o[k])) {
                    ++l.mods[o[k]];
                    break;
                }
        }
        CHECK(riffSeen);
    }
    // Only non-weapon Mods left: they are offered (fewer than three), nothing else.
    Loadout full;
    for (u32 u = 0; u < kModCount; ++u) full.mods[u] = upgradeInfo(static_cast<u8>(u)).cap;
    for (u8& r : full.riffs) r = kRiffMaxLevel;
    full.mods[kExtraCell] = 0;
    full.mods[kMagnet] = 0;
    u8 since = 0, o[3];
    CHECK(drawTrio(full, since, rng, o) == 2);
    CHECK((o[0] == kExtraCell || o[0] == kMagnet) && (o[1] == kExtraCell || o[1] == kMagnet));
    full.mods[kExtraCell] = upgradeInfo(kExtraCell).cap;
    full.mods[kMagnet] = upgradeInfo(kMagnet).cap;
    CHECK(drawTrio(full, since, rng, o) == 0);
    // A capped Mod and a maxed Riff are never offered.
    Loadout some;
    some.mods[kSplit] = 4;
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
    // Caps (GAME_DESIGN 6.2): Split stacks to 4; Extra Cell raises the capacity and refills, up to 6 cells.
    for (u32 k = 0; k < 6; ++k) w->grantUpgrade(0, kSplit);
    CHECK(w->player(0).loadout.mods[kSplit] == 4);
    w->playerMut(0).cells = 1;
    w->grantUpgrade(0, kExtraCell);
    CHECK(w->player(0).cellCap == 4 && w->player(0).cells == 2);
    for (u32 k = 0; k < 4; ++k) w->grantUpgrade(0, kExtraCell);
    CHECK(w->player(0).cellCap == 6 && w->player(0).loadout.mods[kExtraCell] == 3);
    // Riffs level 1..5.
    for (u32 k = 0; k < 7; ++k) w->grantUpgrade(0, kPulseRing);
    CHECK(w->player(0).loadout.riffs[riffIndex(kPulseRing)] == kRiffMaxLevel);
    // Split 4: five lanes per step; with Prism, five each, capped at kMaxShotsPerStep.
    const u32 before = w->shots().count;
    stepWorld(*w, music, Vec2{}, 1);
    u32 made = 0;
    for (u32 k = 0; k < w->eventCount(); ++k)
        if (w->events()[k].type == EventType::Shot) made += w->events()[k].b;
    CHECK(made == 0 || made % 5 == 0);
    (void)before;
    World* p = quietWorld();
    double mp = 0.0;
    stepWorld(*p, mp, Vec2{});
    p->grantUpgrade(0, kSplit);
    p->grantUpgrade(0, kSplit);
    p->grantBoost(0, kPrism);
    u32 volley = 0;
    for (u32 t = 0; t < 30 && !volley; ++t) {
        stepWorld(*p, mp, Vec2{}, 1);
        for (u32 k = 0; k < p->eventCount(); ++k)
            if (p->events()[k].type == EventType::Shot) volley = p->events()[k].b;
    }
    CHECK_MSG(volley == 15, "Split 2 x Prism: %u shots", volley);
    // The lead grid: Double-Time moves it 1/16 -> triplets -> 1/32; Hyper halves it.
    CHECK(p->leadGrid(0) == 24.0);
    p->grantUpgrade(0, kDoubleTime);
    CHECK(p->leadGrid(0) == 16.0);
    p->grantUpgrade(0, kDoubleTime);
    CHECK(p->leadGrid(0) == 12.0);
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
    // Pierce 1: one shot goes through the first of two Drifters in its lane and kills both.
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
    // Overcharge: +25% damage a stack; On-Beat Crit doubles shots on the beat.
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
            if (std::fabs(dmg - 2.5f) < 1e-4f) sawCrit = true;
            if (std::fabs(dmg - 1.25f) < 1e-4f) sawPlain = true;
        }
    }
    CHECK(sawCrit && sawPlain);
    delete w;
    delete p;
    delete q;
    delete o;
}

void testBoss() {
    g_test = "boss";
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
        const float a0 = s.pairHp[0];
        auto seg = along(0, 400.0f, 180.0f);
        CHECK(w->shootBoss(seg.first, seg.second, 1.0f) == 2);
        CHECK_MSG(std::fabs((a0 - s.pairHp[0]) - 3.0f) < 1e-3f, "resting damage %.3f", a0 - s.pairHp[0]);
        // Arm 1 (pair B) fires: x1/3.
        const float b0 = s.pairHp[1];
        seg = along(1, 400.0f, 180.0f);
        CHECK(w->shootBoss(seg.first, seg.second, 1.0f) == 2);
        CHECK(std::fabs((b0 - s.pairHp[1]) - 1.0f / 3.0f) < 1e-3f);
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
    }
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
    testDirectorCycle();
    testCrowd();
    testTrioRules();
    testUpgradeStacking();
    testBoss();
    std::printf("[sim tests] %d checks, %d failures\n", g_checks, g_failures);
    delete g_species;
    return g_failures;
}
