// The simulation without a window (TECH_SPEC 7.3, 10): part of raveform_headless.
//
//   raveform_headless --sim determinism [--minutes 2] [--seed 1]
//       Two live bot runs with the same seed, and a replay of the first one's recorded inputs into a fresh
//       world: the state hashes must agree at every checkpoint (every 10 s) and at the end; a run with
//       another seed must differ. Exit 0 = deterministic.
//   raveform_headless --sim soak [--minutes 10] [--seed 1] [--policy play|reckless]
//       The bot plays the stage A game (the simple spawner, Track 1's tempo), retrying after each death.
//       Checks every tick: no NaN or infinity, no pool overflow; reports kills, deaths, peak entity counts
//       and the tick's time (P50 / P95 / max). Exit 0 = clean.
//   raveform_headless --sim stress [--seconds 120] [--seed 1]
//       bench_stress without a window: about 700 enemies and 1,500 enemy bullets around the bot's ship.
// Each prints a summary and one line tagged RFSIM (JSON).

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "audio/music_data.h"
#include "core/alloc_counter.h"
#include "core/log.h"
#include "sim/sim_runner.h"
#include "sim/species.h"

using namespace rf;
using namespace rf::sim;

namespace {

double nowMs() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct Pct {
    float p50 = 0, p95 = 0, max = 0, mean = 0;
};
Pct percentiles(float* v, u32 n) {
    Pct r;
    if (!n) return r;
    double sum = 0.0;
    for (u32 i = 0; i < n; ++i) sum += v[i];
    std::sort(v, v + n);
    const auto at = [&](double q) {
        u32 k = static_cast<u32>(q * n + 0.999999);
        if (k < 1) k = 1;
        if (k > n) k = n;
        return v[k - 1];
    };
    r.p50 = at(0.50);
    r.p95 = at(0.95);
    r.max = v[n - 1];
    r.mean = static_cast<float>(sum / n);
    return r;
}

float trackBpm() {
    // Track 1's tempo from the music data (the stage A game plays Track 1).
    auto* md = new (std::nothrow) audio::MusicData;
    float bpm = 120.0f;
    if (md && md->load() && md->trackCount > 0) bpm = md->tracks[0].bpm;
    delete md;
    return bpm;
}

struct Peaks {
    u32 enemies = 0, bullets = 0, shots = 0, shards = 0, events = 0;
    void take(const World& w) {
        enemies = std::max(enemies, w.enemies().count);
        bullets = std::max(bullets, w.bullets().count);
        shots = std::max(shots, w.shots().count);
        shards = std::max(shards, w.shards().count);
        events = std::max(events, w.eventCount());
    }
};

int runDeterminism(const SpeciesTable& species, float minutes, u64 seed, float bpm, Bot::Policy policy) {
    const u32 ticks = static_cast<u32>(minutes * 3600.0f);
    const u32 every = 600;  // checkpoints every 10 s
    const u32 checks = ticks / every + 1;
    auto* rec = new (std::nothrow) TickInput[ticks];
    auto* hashA = new (std::nothrow) u64[checks];
    auto* hashB = new (std::nothrow) u64[checks];
    auto* hashC = new (std::nothrow) u64[checks];
    auto* world = new (std::nothrow) World;
    if (!rec || !hashA || !hashB || !hashC || !world) {
        std::printf("[sim] out of memory\n");
        return 2;
    }
    SimRunner runner;
    u32 deathsA = 0;
    u64 killsA = 0;
    // A: live, recorded.
    world->init(&species);
    runner.start(world, seed, Spawner::Mode::Slice, policy, bpm);
    for (u32 t = 0; t < ticks; ++t) {
        rec[t] = runner.step();
        if ((t + 1) % every == 0) hashA[(t + 1) / every - 1] = world->hash();
    }
    const u64 finalA = world->hash();
    deathsA = runner.deaths();
    killsA = world->counters().kills;
    const u32 restartsA = world->counters().restarts;
    // B: live again, same seed.
    world->init(&species);
    runner.start(world, seed, Spawner::Mode::Slice, policy, bpm);
    for (u32 t = 0; t < ticks; ++t) {
        runner.step();
        if ((t + 1) % every == 0) hashB[(t + 1) / every - 1] = world->hash();
    }
    const u64 finalB = world->hash();
    // C: A's recorded inputs replayed into a fresh world.
    world->init(&species);
    runner.start(world, seed, Spawner::Mode::Slice, policy, bpm);
    for (u32 t = 0; t < ticks; ++t) {
        runner.replay(rec[t]);
        if ((t + 1) % every == 0) hashC[(t + 1) / every - 1] = world->hash();
    }
    const u64 finalC = world->hash();
    // D: another seed must give another run.
    world->init(&species);
    runner.start(world, seed + 1, Spawner::Mode::Slice, policy, bpm);
    for (u32 t = 0; t < ticks; ++t) runner.step();
    const u64 finalD = world->hash();

    u32 firstDiffB = 0, firstDiffC = 0;
    for (u32 k = 0; k + 1 < checks && k < ticks / every; ++k) {
        if (!firstDiffB && hashA[k] != hashB[k]) firstDiffB = (k + 1) * every;
        if (!firstDiffC && hashA[k] != hashC[k]) firstDiffC = (k + 1) * every;
    }
    const bool ok = finalA == finalB && finalA == finalC && finalA != finalD && !firstDiffB && !firstDiffC;
    const char* pname = policy == Bot::Policy::Reckless ? "reckless" : "play";
    std::printf("[sim] determinism (%s bot): %u ticks (%.1f min), seed %llu, %llu kills, %u deaths and %u restarts in run A\n",
                pname, ticks, minutes, static_cast<unsigned long long>(seed), static_cast<unsigned long long>(killsA), deathsA,
                restartsA);
    std::printf("[sim]   live A %016llx, live B %016llx, replay of A %016llx, seed+1 %016llx\n",
                static_cast<unsigned long long>(finalA), static_cast<unsigned long long>(finalB),
                static_cast<unsigned long long>(finalC), static_cast<unsigned long long>(finalD));
    if (firstDiffB) std::printf("[sim]   live runs diverge by tick %u\n", firstDiffB);
    if (firstDiffC) std::printf("[sim]   the replay diverges by tick %u\n", firstDiffC);
    std::printf("[sim] determinism %s\n", ok ? "PASS" : "FAIL");
    logTagged("RFSIM", "{\"test\":\"determinism\",\"bot\":\"%s\",\"ticks\":%u,\"seed\":%llu,\"hash\":\"%016llx\",\"replay_hash\":\"%016llx\","
                       "\"second_run_hash\":\"%016llx\",\"other_seed_hash\":\"%016llx\",\"kills\":%llu,\"deaths\":%u,\"pass\":%s}",
              pname, ticks, static_cast<unsigned long long>(seed), static_cast<unsigned long long>(finalA),
              static_cast<unsigned long long>(finalC), static_cast<unsigned long long>(finalB),
              static_cast<unsigned long long>(finalD), static_cast<unsigned long long>(killsA), deathsA, ok ? "true" : "false");
    delete world;
    delete[] rec;
    delete[] hashA;
    delete[] hashB;
    delete[] hashC;
    return ok ? 0 : 1;
}

int runSoak(const SpeciesTable& species, float seconds, u64 seed, float bpm, bool stress, Bot::Policy policy) {
    const u32 ticks = static_cast<u32>(seconds * 60.0f);
    auto* tickMs = static_cast<float*>(std::malloc(sizeof(float) * (ticks ? ticks : 1)));
    auto* world = new (std::nothrow) World;
    if (!tickMs || !world) {
        std::printf("[sim] out of memory\n");
        return 2;
    }
    world->init(&species);
    SimRunner runner;
    runner.start(world, seed, stress ? Spawner::Mode::Stress : Spawner::Mode::Slice,
                 stress ? Bot::Policy::Stress : policy, bpm, stress);
    Peaks peak;
    u32 nanTick = 0;
    double sumEnemies = 0.0, sumBullets = 0.0, sumInView = 0.0;
    u64 bestScore = 0;
    u32 bestChain = 0;
    float longestLifeS = 0.0f, lifeS = 0.0f;
    const u64 allocs0 = heapAllocCountThisThread();
    const double wall0 = nowMs();
    for (u32 t = 0; t < ticks; ++t) {
        const double t0 = nowMs();
        runner.step();
        tickMs[t] = static_cast<float>(nowMs() - t0);
        const World& w = *world;
        peak.take(w);
        sumEnemies += w.enemies().count;
        sumBullets += w.bullets().count;
        sumInView += w.threatInView();
        if (!nanTick && !w.finite()) nanTick = t + 1;
        const PlayerState& p = w.player(0);
        if (p.score > bestScore) bestScore = p.score;
        if (p.bestChain > bestChain) bestChain = p.bestChain;
        lifeS = p.alive ? lifeS + kDt : 0.0f;
        if (lifeS > longestLifeS) longestLifeS = lifeS;
    }
    const double wallMs = nowMs() - wall0;
    const u64 allocs = heapAllocCountThisThread() - allocs0;
    const WorldCounters& c = world->counters();
    const u32 overflow = c.enemiesDropped + c.bulletsDropped + c.shotsDropped + c.shardsDropped + c.eventsDropped;
    const Pct tp = percentiles(tickMs, ticks);
    const bool ok = nanTick == 0 && overflow == 0 && allocs == 0;
    const char* name = stress ? "stress" : "soak";
    std::printf("[sim] %s: %u ticks (%.1f simulated minutes) in %.1f s of wall time, seed %llu\n", name, ticks, seconds / 60.0f,
                wallMs * 1e-3, static_cast<unsigned long long>(seed));
    std::printf("[sim]   kills %u, deaths %u, restarts %u, spawned %u; best score %llu, best Chain %u, longest life %.1f s\n",
                c.kills, runner.deaths(), c.restarts, c.spawned, static_cast<unsigned long long>(bestScore), bestChain,
                longestLifeS);
    std::printf("[sim]   peak: enemies %u, enemy bullets %u, shots %u, Shards %u, events/tick %u; mean enemies %.0f, bullets %.0f\n",
                peak.enemies, peak.bullets, peak.shots, peak.shards, peak.events, sumEnemies / ticks, sumBullets / ticks);
    std::printf("[sim]   tick ms: P50 %.3f, P95 %.3f, max %.3f (mean %.3f)\n", tp.p50, tp.p95, tp.max, tp.mean);
    std::printf("[sim]   overflow: enemies %u, bullets %u, shots %u, Shards %u, events %u; NaN %s; heap allocations in the "
                "loop %llu\n",
                c.enemiesDropped, c.bulletsDropped, c.shotsDropped, c.shardsDropped, c.eventsDropped,
                nanTick ? "YES" : "none", static_cast<unsigned long long>(allocs));
    std::printf("[sim] %s %s\n", name, ok ? "PASS" : "FAIL");
    logTagged("RFSIM",
              "{\"test\":\"%s\",\"ticks\":%u,\"seed\":%llu,\"kills\":%u,\"deaths\":%u,\"restarts\":%u,\"spawned\":%u,"
              "\"best_score\":%llu,\"best_chain\":%u,\"peak\":{\"enemies\":%u,\"bullets\":%u,\"shots\":%u,\"shards\":%u},"
              "\"mean\":{\"enemies\":%.1f,\"bullets\":%.1f,\"threat_in_view\":%.2f},\"tick_ms\":[%.4f,%.4f,%.4f],"
              "\"overflow\":%u,\"nan_tick\":%u,\"heap_allocs\":%llu,\"pass\":%s}",
              name, ticks, static_cast<unsigned long long>(seed), c.kills, runner.deaths(), c.restarts, c.spawned,
              static_cast<unsigned long long>(bestScore), bestChain, peak.enemies, peak.bullets, peak.shots, peak.shards,
              sumEnemies / ticks, sumBullets / ticks, sumInView / ticks, tp.p50, tp.p95, tp.max, overflow, nanTick,
              static_cast<unsigned long long>(allocs), ok ? "true" : "false");
    delete world;
    std::free(tickMs);
    return ok ? 0 : 1;
}

}  // namespace

int simHeadlessMain(int argc, char** argv) {
    const char* mode = argc > 2 ? argv[2] : "";
    float minutes = -1.0f, seconds = -1.0f;
    u64 seed = 1;
    Bot::Policy policy = Bot::Policy::Play;
    for (int i = 3; i < argc; ++i) {
        const char* a = argv[i];
        const char* next = i + 1 < argc ? argv[i + 1] : nullptr;
        if (std::strcmp(a, "--minutes") == 0 && next) {
            minutes = static_cast<float>(std::atof(next));
            ++i;
        } else if (std::strcmp(a, "--seconds") == 0 && next) {
            seconds = static_cast<float>(std::atof(next));
            ++i;
        } else if (std::strcmp(a, "--policy") == 0 && next) {
            policy = std::strcmp(next, "reckless") == 0 ? Bot::Policy::Reckless : Bot::Policy::Play;
            ++i;
        } else if (std::strcmp(a, "--seed") == 0 && next) {
            seed = std::strtoull(next, nullptr, 10);
            ++i;
        } else {
            std::printf("[sim] unknown argument %s\n", a);
            return 2;
        }
    }
    auto* species = new (std::nothrow) SpeciesTable;
    if (!species || !species->load()) return 2;
    const float bpm = trackBpm();
    int code = 2;
    if (std::strcmp(mode, "determinism") == 0) {
        // Both bots: the careful one (long runs, Novas, Dashes) and the reckless one (hits, deaths, retries).
        const float m = minutes > 0.0f ? minutes : 2.0f;
        const int a = runDeterminism(*species, m, seed, bpm, Bot::Policy::Play);
        const int b = runDeterminism(*species, m, seed, bpm, Bot::Policy::Reckless);
        code = a ? a : b;
    } else if (std::strcmp(mode, "soak") == 0) {
        code = runSoak(*species, seconds > 0.0f ? seconds : (minutes > 0.0f ? minutes : 10.0f) * 60.0f, seed, bpm, false, policy);
    } else if (std::strcmp(mode, "stress") == 0) {
        code = runSoak(*species, seconds > 0.0f ? seconds : (minutes > 0.0f ? minutes * 60.0f : 120.0f), seed, bpm, true, policy);
    } else {
        std::printf("usage: raveform_headless --sim determinism|soak|stress [--minutes M] [--seconds S] [--seed N] [--policy play|reckless]\n");
    }
    delete species;
    return code;
}
