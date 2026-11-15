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
//   raveform_headless --sim crowd [--seeds 20] [--minutes 30] [--seed 1] [--policy play|reckless|novice|assist] [--god]
//       The crowd and the difficulty over many seeds (M3 stage C): enemies near the ship during Groove
//       against the target per Track and cycle, Drop peaks, first death, Tracks reached, kills a minute.
//       --god: hits cost nothing, so every Track and cycle is measured (no difficulty numbers then).
//       --trace: one line a minute (Track, section, enemies, kills, ship, boss).
//       M5 (GAME_DESIGN 7.6): every fight's first meeting in each run; --armour0 (armour takes nothing, as on
//       the build the user played), --health id=x,... (a factor on a fight's health), --handover N (the
//       careful bot plays to Track N with hits costing nothing, then the policy asked for goes on with its build).
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
#include "sim/tuning.h"

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
    const char* pname = policy == Bot::Policy::Reckless ? "reckless" : (policy == Bot::Policy::Novice ? "novice" : (policy == Bot::Policy::Assist ? "assist" : "play"));
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
    // Minute by minute (M3 stage B: the bot's 30-minute table).
    struct Minute {
        u32 kills = 0, deaths = 0, enemiesMax = 0, upgrades = 0, boosts = 0, bossKills = 0, track = 1, trackMax = 1;
        double enemiesSum = 0.0;
        u32 ticks = 0;
    };
    static Minute minutesRow[128];
    struct Moment {
        u32 tick;
        EventType type;
        u8 a, b;
    };
    static Moment moments[256];
    u32 momentCount = 0;
    for (Minute& m : minutesRow) m = Minute{};
    u32 trackMax = 1, killsBefore = 0, deathsBefore = 0;
    const u64 allocs0 = heapAllocCountThisThread();
    const double wall0 = nowMs();
    for (u32 t = 0; t < ticks; ++t) {
        const double t0 = nowMs();
        runner.step();
        tickMs[t] = static_cast<float>(nowMs() - t0);
        const World& w = *world;
        {
            Minute& m = minutesRow[(t / 3600u) < 127u ? t / 3600u : 127u];
            ++m.ticks;
            m.enemiesSum += w.enemies().count;
            if (w.enemies().count > m.enemiesMax) m.enemiesMax = w.enemies().count;
            for (u32 k = 0; k < w.eventCount(); ++k) {
                const EventType ty = w.events()[k].type;
                if (ty == EventType::TrioTake) ++m.upgrades;
                if (ty == EventType::BoostTake) ++m.boosts;
                if (ty == EventType::BossDeath) ++m.bossKills;
                if ((ty == EventType::BossEnter || ty == EventType::BossPair || ty == EventType::BossDeath || ty == EventType::Death ||
                     (ty == EventType::Section && w.events()[k].f > 0.5f)) && momentCount < 256)
                    moments[momentCount++] = Moment{t, ty, w.events()[k].a, w.events()[k].c};
            }
            m.track = w.phrase().track;
            if (m.track > trackMax) trackMax = m.track;
            m.trackMax = trackMax;
            const u32 kills = w.counters().kills, deaths = runner.deaths();
            m.kills += kills - killsBefore;
            m.deaths += deaths - deathsBefore;
            killsBefore = kills;
            deathsBefore = deaths;
        }
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
    for (u32 k = 0; k < momentCount && k < 40; ++k) {
        const Moment& mo = moments[k];
        const char* what = mo.type == EventType::BossEnter ? "boss enters" : mo.type == EventType::BossPair ? "boss pair down"
                         : mo.type == EventType::BossDeath ? "boss dies" : mo.type == EventType::Death ? "player dies" : "Track change";
        std::printf("[sim]   %3u:%04.1f %s (%u)\n", mo.tick / 3600u, static_cast<double>(mo.tick % 3600u) / 60.0, what,
                    mo.type == EventType::Section ? mo.b : mo.a);
    }
    if (!stress) {
        std::printf("[sim]   minute  kills deaths enemies(mean/max) upgrades boosts bosses track(max)  tick ms P95/max\n");
        for (u32 mi = 0; mi < 128 && minutesRow[mi].ticks; ++mi) {
            const Minute& m = minutesRow[mi];
            const Pct mp = percentiles(tickMs + mi * 3600u, m.ticks);
            std::printf("[sim]   %6u %6u %6u %8.0f/%-5u %8u %6u %6u %5u(%u)  %.3f/%.3f\n", mi + 1, m.kills, m.deaths, m.enemiesSum / m.ticks,
                        m.enemiesMax, m.upgrades, m.boosts, m.bossKills, m.track, m.trackMax, mp.p95, mp.max);
            logTagged("RFSIM", "{\"test\":\"soak_minute\",\"minute\":%u,\"kills\":%u,\"deaths\":%u,\"enemies_mean\":%.1f,\"enemies_max\":%u,\"upgrades\":%u,\"boosts\":%u,\"boss_kills\":%u,\"track\":%u,\"track_max\":%u,\"tick_p95\":%.4f,\"tick_max\":%.4f}",
                      mi + 1, m.kills, m.deaths, m.enemiesSum / m.ticks, m.enemiesMax, m.upgrades, m.boosts, m.bossKills, m.track,
                      m.trackMax, mp.p95, mp.max);
        }
    }
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

// The crowd and the difficulty over many seeds (M3 stage C, C2): per Track and cycle the mean number of
// enemies in view (the plain frame around the ship) during Groove against the crowd's target, the mean the
// director counts (the frame x count.frameScale), the Drop's peak in view, kills a minute, the first death
// and the Tracks reached.
// M5 (GAME_DESIGN 7.6, calibration and tuning): armour at nothing as on the build the user played, and a health factor per
// fight on top of tuning.h (--armour0, --health longwave=0.8,choir=1.2).
bool gArmourNothing = false;
float gHealthLever[kBossKinds] = {};
// --handover N: the careful bot plays (hits costing nothing) until Track N starts, then the policy asked for takes over with
// the careful bot's build of that moment; only its fights count, and a seed ends once it has met Track N's boss.
u32 gHandover = 0;
// --nopace (play test 3's balance pass): the bosses keep no pace, so the novice's own eight-bar damage is measured as it is.
bool gNoPace = false;

bool gBulletsAsWritten = false;  // 0449eb3: every enemy bullet as written (the count before "Enemy bullets by Track")

void applyLevers(World& w) {
    w.rules().bulletsAsWritten = gBulletsAsWritten;
    w.rules().armourNothing = gArmourNothing;
    w.rules().noPace = gNoPace;
    for (u32 k = 0; k < kBossKinds; ++k) w.rules().healthLever[k] = gHealthLever[k] > 0.0f ? gHealthLever[k] : 1.0f;
}

// M5 part 7 (GAME_DESIGN 6.6, "No pick is a trap"): on one reference build (Split 1 and Double-Time 1; the careful bot, hits
// costing nothing, the loadout frozen, no boss pace) a stack of each weapon and rhythm Mod and a level of each Riff, against a
// stack of Overcharge: kills a minute in Track 3's second Groove, and seconds to kill the Choir from its start (no crowd). Each
// should be worth 0.6 to 1.5 of Overcharge in what it is made for (here: the better of the two). And the Downbeat Cannon: its
// two ingredients at their caps, with and without the Fusion.
int runPicks(const SpeciesTable& species, u32 seeds, u64 seed0) {
    auto* world = new (std::nothrow) World;
    if (!world) return 2;
    struct Res {
        double kpm = 0.0, secs = 0.0;
        u32 won = 0, runs = 0;
    };
    const auto setup = [&](SimRunner& runner, u64 seed, const u8* ups, u32 nUps, u8 fusions, bool fight) {
        world->init(&species);
        runner.start(world, seed, fight ? Spawner::Mode::Off : Spawner::Mode::Slice, Bot::Policy::Play, 120.0f, true);
        World& w = *world;
        w.rules().frozenLoadout = true;
        w.rules().noPace = true;
        w.directorMut().track = 3;
        for (u32 k = 0; k < nUps; ++k) w.grantUpgrade(0, ups[k]);
        w.playerMut(0).loadout.fusions = fusions;
    };
    const auto measure = [&](const u8* ups, u32 nUps, u8 fusions) {
        Res r;
        for (u32 s = 0; s < seeds; ++s) {
            SimRunner runner;
            // Track 3's second Groove.
            setup(runner, seed0 + s, ups, nUps, fusions, false);
            World& w = *world;
            u32 kills0 = 0;
            u64 ticks = 0;
            bool in = false;
            // (a minute from the second cycle's Groove on: one Groove's 15 seconds swung by a third from seed to seed)
            for (u32 t = 0; t < 60u * 60u * 5u; ++t) {
                runner.step();
                const Phrase& ph = w.phrase();
                const bool g = ph.track == 3 && ph.cycle == 2 && ph.section == Section::Groove;
                if (g && !in) {
                    in = true;
                    kills0 = w.counters().kills;
                }
                if (in) ++ticks;
                if (in && ticks >= 3600u) break;
            }
            if (ticks) r.kpm += static_cast<double>(w.counters().kills - kills0) / (static_cast<double>(ticks) / 3600.0);
            // The Choir from its start, alone.
            SimRunner fightRunner;
            setup(fightRunner, seed0 + s, ups, nUps, fusions, true);
            for (u32 t = 0; t < 60; ++t) fightRunner.step();
            const double enter = (std::floor(w.music() / kTicksPerBar) + 1.0) * kTicksPerBar;
            w.spawnBossNow(enter, w.player(0).pos + Vec2{0.0f, -200.0f}, BossKind::Choir);
            u32 t = 0;
            for (; t < 60u * 60u * 4u && w.boss().active && !w.boss().dying; ++t) fightRunner.step();
            const double secs = (w.music() - w.boss().fightTick) / static_cast<double>(w.ticksPerSecond());
            if (w.boss().dying) {
                r.secs += secs;
                ++r.won;
            }
            ++r.runs;
        }
        r.kpm /= static_cast<double>(r.runs ? r.runs : 1u);
        r.secs = r.won ? r.secs / r.won : 0.0;
        return r;
    };
    const u8 refUps[2] = {kSplit, kDoubleTime};
    u8 ups[8];
    const auto withRef = [&](u8 extra, u32 times) {
        u32 n = 0;
        ups[n++] = refUps[0];
        ups[n++] = refUps[1];
        for (u32 k = 0; k < times; ++k) ups[n++] = extra;
        return n;
    };
    const Res base = measure(refUps, 2, 0);
    const Res oc = measure(ups, withRef(kOvercharge, 1), 0);
    const Res sp = measure(ups, withRef(kSplit, 1), 0);
    std::printf("[picks] reference (Split 1, Double-Time 1): %.0f kills a minute, the Choir in %.1f s (%u of %u); + Overcharge: %.0f, %.1f s; + Split: %.0f, %.1f s\n",
                base.kpm, base.secs, base.won, base.runs, oc.kpm, oc.secs, sp.kpm, sp.secs);
    std::printf("[picks]   (each pick's gain over the reference: in kills a minute against a stack of Split's, in the Choir's seconds against a stack\n"
                "[picks]    of Overcharge's; the better of the two; band 0.60-1.50, ea472ab)\n");
    u32 outside = 0;
    const auto row = [&](const char* name, const Res& r) {
        const double crowd = sp.kpm - base.kpm > 1.0 ? (r.kpm - base.kpm) / (sp.kpm - base.kpm) : 0.0;
        const double boss = r.secs > 0.0 && base.secs - oc.secs > 0.1 ? (base.secs - r.secs) / (base.secs - oc.secs) : 0.0;
        const double best = crowd > boss ? crowd : boss;
        const bool in = best >= 0.6 && best <= 1.5;
        if (!in) ++outside;
        std::printf("[picks]   %-16s %6.0f kpm (%.2f)  %6.1f s (%.2f)  -> %.2f %s\n", name, r.kpm, crowd, r.secs, boss, best, in ? "" : "OUTSIDE");
        logTagged("RFSIM", "{\"test\":\"picks\",\"pick\":\"%s\",\"kpm\":%.1f,\"crowd\":%.3f,\"secs\":%.1f,\"boss\":%.3f,\"best\":%.3f}", name, r.kpm, crowd, r.secs,
                  boss, best);
    };
    for (u32 u = 0; u < kUpgradeCount; ++u) {
        const bool weapon = u <= kGravityRounds, rhythm = u >= kOnBeatCrit && u <= kAfterparty, riff = isRiff(static_cast<u8>(u));
        if (!(weapon || rhythm || riff) || u == kOvercharge) continue;
        const Res r = measure(ups, withRef(static_cast<u8>(u), 1), 0);
        row(upgradeInfo(static_cast<u8>(u)).name, r);
    }
    // The Downbeat Cannon: its ingredients at their caps, then with the Fusion.
    const FusionInfo& fi = fusionInfo(kDownbeatCannon);
    u32 n = 0;
    ups[n++] = refUps[0];
    ups[n++] = refUps[1];
    for (u32 k = 0; k < upgradeInfo(fi.a).cap && n < 8; ++k) ups[n++] = fi.a;
    for (u32 k = 0; k < upgradeInfo(fi.b).cap && n < 8; ++k) ups[n++] = fi.b;
    const Res ing = measure(ups, n, 0);
    const Res fus = measure(ups, n, static_cast<u8>(1u << kDownbeatCannon));
    row("(its two capped)", ing);
    row(fi.name, fus);
    std::printf("[picks]   the Downbeat Cannon over its ingredients alone: kills a minute x %.2f, the Choir x %.2f\n", ing.kpm > 0.0 ? fus.kpm / ing.kpm : 0.0,
                fus.secs > 0.0 ? ing.secs / fus.secs : 0.0);
    std::printf("[picks] %u outside the band\n", outside);
    delete world;
    return 0;
}

int runCrowd(const SpeciesTable& species, u32 seeds, float minutes, u64 seed0, Bot::Policy policy, bool god, bool trace) {
    const u32 ticks = static_cast<u32>(minutes * 3600.0f);
    auto* world = new (std::nothrow) World;
    if (!world) return 2;
    struct Cell {
        double grooveSum = 0.0, countedSum = 0.0, aliveSum = 0.0;
        u64 grooveTicks = 0;
        double dropPeakSum = 0.0;
        u32 dropRuns = 0;
    };
    static Cell cells[4][5];
    for (auto& row : cells)
        for (Cell& c : row) c = Cell{};
    static float firstDeath[256];
    static u32 firstDeathTrack[256];  // play test 2's review: the Track of the first death (the first pass is Tracks 1-3)
    static u32 maxTrack[256];
    u64 kills = 0, deaths = 0, overflow = 0, allocs = 0, capped = 0;
    u32 nan = 0, shotsPeak = 0;
    // What hit the ship (C2 tuning): a body of each family within reach, else a bullet or a beam.
    u64 hitsBy[kFamilyCount + 1] = {}, hitsInBossFight = 0, hitsBySection[4] = {};
    u64 livesTaken = 0, livesSpent = 0, ringBreaks = 0;  // play test 3
    // M6 (the lead's A4): the hits outside the fights from Track 4 on, by what took them (World's hit cause; [f] a family's bodies,
    // [f + 16] its bullets; 32 a fight, 33 an event or a Volatile blast, 34 unknown), and the hits that ended a run by the same.
    u64 lateBy[35] = {}, endBy[35] = {};
    // (the lead, 2026-11-13: what takes the cells in Track 2 before Longwave, in the first run: bodies by family, bullets or
    // beams, the mini-boss's fight; and the cells regained there)
    u64 t2Hits[kFamilyCount + 2] = {}, t2Runs = 0, t2Regained = 0;
    u64 bossWon = 0, bossLost = 0;
    double bossWonTicks = 0.0;
    // M5 part 1: every fight by its kind: fights, won, lost (the run ended in it), warped out (a mini-boss's hold ran out),
    // the time to kill by Track, the hits the ship took in it.
    struct FightStat {
        u32 fights = 0, won = 0, lost = 0, gone = 0, hits = 0, hitsBody = 0, hitsBeam = 0;
        double cellsIn = 0.0, capIn = 0.0;  // (the lead, 2026-11-13: the player's cells as each fight begins)
        double wonSecs[16] = {};
        u32 wonAt[16] = {};
    };
    static FightStat fightStat[kBossKinds];
    for (FightStat& f : fightStat) f = FightStat{};
    // M5 (GAME_DESIGN 7.6): each run's first meeting with each fight (1 won, 2 lost, 3 warped out; 0 none), its seconds when
    // won, and whether the first run (before any death) reached Track 2's boss.
    static u8 firstMeet[256][kBossKinds];
    static float firstPace[256][kBossKinds], firstPaceDmg[256][kBossKinds];  // M5 part 7: the fight's pace and the 8-bar damage a player
    static float firstSecs[256][kBossKinds];
    static u8 firstCells[256][kBossKinds];  // the cells as the first meeting began
    static bool reachedT2[256];
    // Play test 2: the Shard level pace by Track (alive time and levels in each), and the levels and upgrades held at
    // minutes 3, 6 and 10 in the first run (the runs still in it then).
    double paceTicks[4] = {}, paceLevels[4] = {}, paceShards[4] = {};
    constexpr u32 kMarks = 3;
    const u32 markMinute[kMarks] = {3, 6, 10};
    double levelAt[kMarks] = {}, takenAt[kMarks] = {}, modsAt[kMarks] = {}, riffsAt[kMarks] = {}, cellsAt[kMarks] = {};
    u32 runsAt[kMarks] = {};
    u64 levelTrios = 0, levelUps = 0, cellsTaken = 0, cellsRegrown = 0;
    // a667116 (the Trio only at Shard levels and a boss's death): the first run's Trios by Track, level-ups and the boss's (the
    // boss's in the Track-change Break counts for the Track whose boss it was), and the first runs that played in each Track.
    double triosLevel[11] = {}, triosBoss[11] = {}, runsInTrack[11] = {};
    // 0449eb3 (GAME_DESIGN 8, "Enemy bullets by Track"): the enemy bullets alive in the first run's Grooves, by Track (their sum
    // over the Groove's ticks, the ticks, the most at once).
    double grooveBullets[11] = {}, grooveTicks[11] = {}, grooveSpawned[11] = {}, grooveAsked[11] = {}, grooveKept[11] = {};
    u32 grooveBulletsMax[11] = {};
    static double grooveByFamily[11][kFamilyCount + 1];  // (bullet-ticks by the family that fired them; [kFamilyCount] a fight's or other)
    for (auto& row : grooveByFamily)
        for (double& v : row) v = 0.0;
    // M5 part 3 (GAME_DESIGN 6.1, how a Trio is dealt): the first run's build as Track 3 ends (Mods at their cap) and as
    // Track 5 ends (a Fusion held).
    u32 t3Runs = 0, t3Capped2 = 0, t5Runs = 0, t5Fusion = 0;
    // 2026-11-15 (the owner: "I don't think I ever got any [Fusion] in my playtesting"): the first run's first Fusion, when it
    // came due (both ingredients at their caps: its gold orb is dealt from then on) and when it was taken; the minute, the
    // Track and the upgrades taken by then, and which Fusion it was.
    static float fuseDueMin[256], fuseHeldMin[256];
    static u32 fuseDueTrack[256], fuseDueTaken[256];
    u32 fuseKind[kFusionCount] = {};
    // M5 part 5: elites, the arena's events and its bodies broken.
    u64 elites = 0, eliteKills = 0, events[kEventKinds] = {}, broken = 0, pylonsTuned = 0, splits = 0, bombs = 0;
    double t3Capped = 0.0;
    const SpeciesTable::Crowd& cr = species.crowd;
    const float hw = tuning::kFrameHalfWWu, hh = tuning::kFrameHalfHWu;
    const float cw = hw * cr.countFrameScale, ch = hh * cr.countFrameScale;
    for (u32 s = 0; s < seeds && s < 256; ++s) {
        world->init(&species);
        SimRunner runner;
        const bool handover = gHandover > 1;
        bool handed = !handover;
        runner.start(world, seed0 + s, Spawner::Mode::Slice, handover ? Bot::Policy::Play : policy, 120.0f, god || handover);
        applyLevers(*world);
        // The world's counters run on across init (they are the session's): this seed's share is the difference.
        const WorldCounters c0 = world->counters();
        float peak[4][5] = {};
        bool sawDrop[4][5] = {};
        firstDeath[s] = minutes + 1.0f;
        firstDeathTrack[s] = 0;
        fuseDueMin[s] = fuseHeldMin[s] = -1.0f;
        fuseDueTrack[s] = fuseDueTaken[s] = 0;
        for (u32 k = 0; k < kBossKinds; ++k) {
            firstMeet[s][k] = 0;
            firstSecs[s][k] = 0.0f;
        }
        reachedT2[s] = false;
        maxTrack[s] = 1;
        u32 countedTrack = 0;  // (the Trio count's Tracks this seed has entered)
        u32 bulletsAll0 = world->counters().bulletsAll;
        u64 asked0 = world->bulletsAsked(), kept0 = world->bulletsKept();
        u32 lastHits = 0, lastBar = 0xFFFFFFFFu, killsAtBar = 0;
        bool bossOn = false;
        u32 bossFrom = 0, bossTrack = 1, bossHits0 = 0, fightHits = 0, fightBody = 0, fightBeam = 0;
        BossKind bossKind = BossKind::Kaleidos;
        // The camera as PlayView draws it (GAME_DESIGN 9, play test 1): a lead along the ship's velocity (at most
        // cruise speed x 0.125 s) and 30 wu along its aim while firing, eased over 0.25 s; 0.85x and a lean of 30% (at
        // most 320 wu) toward the boss while it lives within 1,800 wu. In view is that camera's frame.
        Vec2 lookAhead{}, lean{};
        float zoom = 1.0f;
        const u64 a0 = heapAllocCountThisThread();
        for (u32 t = 0; t < ticks; ++t) {
            const u32 d0 = runner.deaths();
            runner.step();
            const World& w = *world;
            if (!handed && w.phrase().track >= gHandover && w.player(0).alive) {
                handed = true;
                runner.setPolicy(policy);
                world->rules().godMode = god;
            }
            if (handover && handed && firstMeet[s][static_cast<u32>(bossOfSong(gHandover - 1u))]) break;
            if (w.shots().count > shotsPeak) shotsPeak = w.shots().count;
            if (runner.deaths() > d0 && firstDeath[s] > minutes) {
                firstDeath[s] = static_cast<float>(t) / 3600.0f;
                firstDeathTrack[s] = world->phrase().track;
            }
            const Phrase& ph = w.phrase();
            const PlayerState& p = w.player(0);
            if (runner.deaths() == 0 && (fuseDueMin[s] < 0.0f || fuseHeldMin[s] < 0.0f)) {
                const Loadout& lo = p.loadout;
                if (fuseDueMin[s] < 0.0f)
                    for (u8 f = 0; f < kFusionCount; ++f)
                        if (lo.fused(f) || lo.fusionDue(f)) {
                            fuseDueMin[s] = static_cast<float>(t) / 3600.0f;
                            fuseDueTrack[s] = ph.track;
                            fuseDueTaken[s] = lo.taken;
                            ++fuseKind[f];
                            break;
                        }
                if (fuseHeldMin[s] < 0.0f && lo.fusions) fuseHeldMin[s] = static_cast<float>(t) / 3600.0f;
            }
            if (ph.track > maxTrack[s] && runner.deaths() == 0 && p.alive) {
                if (ph.track == 4) {
                    u32 capped = 0;
                    for (u8 u = 0; u < kModCount; ++u) capped += p.loadout.mods[u] >= upgradeInfo(u).cap ? 1u : 0u;
                    ++t3Runs;
                    t3Capped += capped;
                    t3Capped2 += capped >= 2 ? 1u : 0u;
                }
                if (ph.track == 6) {
                    ++t5Runs;
                    t5Fusion += p.loadout.fusions ? 1u : 0u;
                }
            }
            if (ph.track > countedTrack && runner.deaths() == 0 && p.alive && !ph.trackChange && ph.track <= 10 && (!handover || handed)) {
                runsInTrack[ph.track] += 1.0;  // (once the Track proper has begun)
                countedTrack = ph.track;
            }
            if (ph.track > maxTrack[s]) maxTrack[s] = ph.track;
            const u32 spawnedNow = w.counters().bulletsAll - bulletsAll0;
            bulletsAll0 = w.counters().bulletsAll;
            const u64 askedNow = w.bulletsAsked() - asked0, keptNow = w.bulletsKept() - kept0;
            asked0 = w.bulletsAsked();
            kept0 = w.bulletsKept();
            if (runner.deaths() == 0 && ph.section == Section::Groove && !ph.trackChange && ph.track >= 1 && ph.track <= 10 && (!handover || handed)) {
                grooveBullets[ph.track] += static_cast<double>(w.bullets().count);
                grooveTicks[ph.track] += 1.0;
                grooveSpawned[ph.track] += static_cast<double>(spawnedNow);
                grooveAsked[ph.track] += static_cast<double>(askedNow);
                grooveKept[ph.track] += static_cast<double>(keptNow);
                if (w.bullets().count > grooveBulletsMax[ph.track]) grooveBulletsMax[ph.track] = w.bullets().count;
                const Bullets& bb = w.bullets();
                for (u32 k = 0; k < bb.count; ++k) {
                    if (bb.dead[k]) continue;
                    const u16 src = bb.src[k];
                    const u32 fam = src < SpeciesTable::kCount ? static_cast<u32>(species.get(src).family) : kFamilyCount;
                    grooveByFamily[ph.track][fam < kFamilyCount ? fam : kFamilyCount] += 1.0;
                }
            }
            if (p.alive) paceTicks[(ph.track < 4 ? ph.track : 4u) - 1] += 1.0;
            for (u32 k = 0; k < w.eventCount(); ++k) {
                const Event& ev = w.events()[k];
                if (ev.type == EventType::BossCue) {
                    const BossCue cue = static_cast<BossCue>(ev.a);
                    if (cue == BossCue::ElArrive) ++elites;
                    if (cue == BossCue::EvAnnounce && ev.b < kEventKinds) ++events[ev.b];
                    if (cue == BossCue::VaBreak || cue == BossCue::CoBreak || (cue == BossCue::RiClose && ev.b != 0)) ++broken;
                    if (cue == BossCue::PyTuned) ++pylonsTuned;
                    if (cue == BossCue::ElSplit) ++splits;
                    if (cue == BossCue::ElBurst) ++bombs;
                }
                if (ev.type == EventType::Kill && ev.a == 2 && ev.c == 0) ++eliteKills;
                if (ev.type == EventType::TrioOpen && ev.a == 0 && runner.deaths() == 0 && (!handover || handed)) {
                    const u32 tt = ev.c == kTrioLevel ? ph.track : (ph.trackChange && ph.track > 1 ? ph.track - 1u : ph.track);
                    if (tt >= 1 && tt <= 10) (ev.c == kTrioLevel ? triosLevel : triosBoss)[tt] += 1.0;
                }
                if (w.events()[k].type == EventType::LevelUp) {
                    paceLevels[(ph.track < 4 ? ph.track : 4u) - 1] += 1.0;
                    ++levelUps;
                }
                if (w.events()[k].type == EventType::Pickup && w.events()[k].a == 0) paceShards[(ph.track < 4 ? ph.track : 4u) - 1] += 1.0;
            }
            for (u32 m = 0; m < kMarks; ++m)
                if (t + 1 == markMinute[m] * 3600u && runner.deaths() == 0 && p.alive) {
                    ++runsAt[m];
                    levelAt[m] += p.level;
                    takenAt[m] += p.loadout.taken;
                    cellsAt[m] += p.cellCap;
                    for (u32 u = 0; u < kModCount; ++u) modsAt[m] += p.loadout.mods[u];
                    for (u32 r = 0; r < kRiffCount; ++r) riffsAt[m] += p.loadout.riffs[r];
                }
            {
                // Boss fights: from its entrance to its death (won) or to the run's end (lost); a mini-boss whose hold ran
                // out warped out (M5). "Boss fights" (GAME_DESIGN 8's target) are the Tracks' bosses; the mini-bosses apart.
                const bool on = w.boss().active && !w.boss().dying && handed;
                if (on && !bossOn) {
                    bossFrom = t;
                    bossKind = w.boss().kind;
                    bossTrack = ph.track;
                    bossHits0 = p.hitsTaken;
                    ++fightStat[static_cast<u32>(bossKind)].fights;
                    fightStat[static_cast<u32>(bossKind)].cellsIn += p.cells;
                    fightStat[static_cast<u32>(bossKind)].capIn += p.cellCap;
                    if (!firstMeet[s][static_cast<u32>(bossKind)]) firstCells[s][static_cast<u32>(bossKind)] = static_cast<u8>(p.cells);
                    if (!isMiniBoss(bossKind) && ph.track == 2 && runner.deaths() == 0) reachedT2[s] = true;
                }
                if (on) {
                    // (Kept while it lasts: a run that ends in the fight starts again with all of them at 0.)
                    fightHits = p.hitsTaken >= bossHits0 ? p.hitsTaken - bossHits0 : fightHits;
                    fightBody = w.boss().hitsBody;
                    fightBeam = w.boss().hitsBeam;
                }
                if (!on && bossOn) {
                    FightStat& f = fightStat[static_cast<u32>(bossKind)];
                    bool gone = false;
                    for (u32 k = 0; k < w.eventCount(); ++k) gone = gone || w.events()[k].type == EventType::BossGone;
                    f.hits += fightHits;
                    f.hitsBody += fightBody;
                    f.hitsBeam += fightBeam;
                    fightHits = fightBody = fightBeam = 0;
                    u8& fm = firstMeet[s][static_cast<u32>(bossKind)];
                    if (!fm) {
                        firstPace[s][static_cast<u32>(bossKind)] = w.boss().pace;
                        firstPaceDmg[s][static_cast<u32>(bossKind)] = w.boss().paceMeasured;
                        fm = w.boss().dying ? 1 : (gone ? 3 : 2);
                        if (fm == 1) firstSecs[s][static_cast<u32>(bossKind)] = static_cast<float>(t - bossFrom) / 60.0f;
                    }
                    if (w.boss().dying) {
                        ++f.won;
                        const u32 tr = bossTrack < 16 ? bossTrack : 15u;
                        f.wonSecs[tr] += static_cast<double>(t - bossFrom) / 60.0;
                        ++f.wonAt[tr];
                        if (!isMiniBoss(bossKind)) {
                            ++bossWon;
                            bossWonTicks += static_cast<double>(t - bossFrom);
                        }
                    } else if (gone) {
                        ++f.gone;
                    } else {
                        ++f.lost;
                        if (!isMiniBoss(bossKind)) ++bossLost;
                    }
                }
                bossOn = on;
            }
            if (trace && w.boss().active && !w.boss().dying && t % 60u == 0u) {
                const sim::BossState& b = w.boss();
                const Enemies& e = w.enemies();
                u32 near300 = 0, between = 0, bul = 0;
                const Vec2 to = b.pos - p.pos;
                const float L = length(to);
                for (u32 i = 0; i < e.count; ++i) {
                    if (e.flags[i] & kFlagDead) continue;
                    const Vec2 d = Vec2{e.x[i], e.y[i]} - p.pos;
                    near300 += lengthSq(d) < 300.0f * 300.0f ? 1u : 0u;
                    if (L > 1.0f) {
                        const float along = dot(d, to) / L;
                        const float off = std::fabs(d.x * to.y - d.y * to.x) / L;
                        between += (along > 0.0f && along < L - 150.0f && off < 40.0f) ? 1u : 0u;
                    }
                }
                const Bullets& bb = w.bullets();
                for (u32 k = 0; k < bb.count; ++k) bul += lengthSq(Vec2{bb.x[k], bb.y[k]} - p.pos) < 300.0f * 300.0f ? 1u : 0u;
                // M5: the nearest of its parts, its phase and what hit the ship (bodies, beams).
                BossPart parts[kBossBodies + 4];
                const u32 pn = w.bossParts(parts, kBossBodies + 4);
                float nearPart = 1e9f;
                for (u32 k = 0; k < pn; ++k) nearPart = std::fmin(nearPart, length(parts[k].pos - p.pos) - parts[k].radius);
                std::printf("[boss] %6.1f s %s ph %u dist %4.0f part %4.0f near %3u in the line %2u bullets %3u cells %u novas %u parts %x core %4.0f hits %u "
                            "taken body %u beam %u%s%s\n",
                            t / 60.0f, bossId(b.kind), b.phase, L, pn ? nearPart : -1.0f, near300, between, bul, p.cells, p.novas, static_cast<unsigned>(b.alive),
                            b.coreHp, b.hits, b.hitsBody, b.hitsBeam, p.alive ? "" : " DOWN",
                            w.pen().up ? (w.penInside(p.pos, 0.0f) ? " in the Pen" : " OUTSIDE the Pen") : "");
            }
            for (u32 k = 0; k < w.eventCount(); ++k) {
                // Play test 3: Extra Lives taken and spent, the Orbitals' guard ring broken.
                const Event& ev = w.events()[k];
                if (ev.type == EventType::ExtraLife) ++(ev.b ? livesSpent : livesTaken);
                if (ev.type == EventType::Hit && ev.c == 1) ++ringBreaks;
            }
            if (p.hitsTaken < lastHits) lastHits = p.hitsTaken;  // a new run
            if (p.hitsTaken > lastHits) {
                lastHits = p.hitsTaken;
                if (w.boss().active) ++hitsInBossFight;
                ++hitsBySection[static_cast<u32>(w.phrase().section) & 3u];
                const Enemies& e = w.enemies();
                i32 best = -1;
                float bestD = 1e30f;
                for (u32 i = 0; i < e.count; ++i) {
                    if (e.flags[i] & kFlagDead) continue;
                    const float d = length(Vec2{e.x[i], e.y[i]} - p.pos) - e.radius[i];
                    if (d < bestD) {
                        bestD = d;
                        best = static_cast<i32>(i);
                    }
                }
                const u32 cause = (best >= 0 && bestD < 90.0f) ? static_cast<u32>(species.get(e.species[best]).family) : kFamilyCount;
                ++hitsBy[cause];
                {
                    const u16 hc = p.lastHitCause;
                    const u32 slot = hc < kCauseBossBase ? (static_cast<u32>(species.get(hc).family) + (p.lastHitBullet ? 16u : 0u))
                                                         : (hc < kCauseEventBase ? 32u : (hc != kCauseNone ? 33u : 34u));
                    if (w.phrase().track >= 4 && !w.boss().active) ++lateBy[slot < 35 ? slot : 34];
                    if (!p.alive) ++endBy[slot < 35 ? slot : 34];
                }
                if (w.phrase().track == 2 && runner.deaths() == 0 && !(w.boss().active && w.boss().kind == BossKind::Longwave))
                    ++t2Hits[w.boss().active && isMiniBoss(w.boss().kind) ? kFamilyCount + 1 : cause];
                if (trace && w.boss().active && !w.boss().dying) {
                    const sim::BossState& b = w.boss();
                    BossPart parts[kBossBodies + 4];
                    const u32 pn = w.bossParts(parts, kBossBodies + 4);
                    float nearPart = 1e9f;
                    for (u32 k = 0; k < pn; ++k) nearPart = std::fmin(nearPart, length(parts[k].pos - p.pos) - parts[k].radius);
                    std::printf("[hit]  %6.1f s %s ph %u, beat %.2f of the bar: nearest part %.0f wu, taken body %u beam %u, ship (%.0f, %.0f) moving %.0f wu/s\n",
                                t / 60.0f, bossId(b.kind), b.phase, std::fmod(w.music(), kTicksPerBar) / kTicksPerBeat, pn ? nearPart : -1.0f, b.hitsBody,
                                b.hitsBeam, p.pos.x, p.pos.y, length(p.vel));
                    if (b.kind == BossKind::Morgana) {
                        // (M5 part 6: where the calm was)
                        const Vec2 o = w.morganaOasis(w.music());
                        Vec2 mp;
                        bool lit = false;
                        const bool mirage = w.morganaMirage(w.music(), mp, lit);
                        std::printf("       oasis (%.0f, %.0f) %.0f wu from the ship%s%s, resting %d, leap %.2f bars ago\n", o.x, o.y, length(o - p.pos),
                                    mirage ? ", a mirage " : "", mirage ? (length(mp - p.pos) < 260.0f ? "round the ship" : "elsewhere") : "",
                                    w.morganaResting(w.music()) ? 1 : 0, b.mg.leapTick >= 0.0 ? (w.music() - b.mg.leapTick) / kTicksPerBar : -1.0);
                    }
                }
            }
            if (trace && t % 3600u == 3599u) {
                u32 fod = 0, thr = 0, fodFar = 0, thrFar = 0;
                for (u32 i = 0; i < w.enemies().count; ++i) {
                    const Enemies& e = w.enemies();
                    if (e.flags[i] & kFlagDead) continue;
                    const bool far = std::fabs(e.x[i] - p.pos.x) > 2.0f * hw || std::fabs(e.y[i] - p.pos.y) > 2.0f * hh;
                    if (species.get(e.species[i]).hue == 0) { ++fod; fodFar += far ? 1u : 0u; } else { ++thr; thrFar += far ? 1u : 0u; }
                }
                std::printf("[alive] fodder %u (%u beyond 2 frames), threats %u (%u beyond 2 frames)\n", fod, fodFar, thr, thrFar);
                const sim::BossState& b = w.boss();
                std::printf("[trace] min %3u  T%u c%u %s%s  alive %3u  kills %6u  ship (%6.0f %6.0f) %s  boss %s pairs %u core %.0f/%.0f hits %u%s\n",
                            t / 3600u + 1u, ph.track, ph.cycle, sectionLabel(ph.section), ph.bossDrop ? " boss" : "", w.enemies().count,
                            w.counters().kills - c0.kills, p.pos.x, p.pos.y, p.alive ? "alive" : "down", b.active ? "on" : "off",
                            static_cast<unsigned>(b.alive), b.coreHp, b.coreMax, b.hits, b.dying ? " dying" : "");
            }
            if (!p.alive || w.restartPending()) continue;
            {
                const float sp = length(p.vel);
                Vec2 lead = (sp > kPlayerSpeed ? p.vel * (kPlayerSpeed / sp) : p.vel) * 0.125f;
                if (p.fire) lead = lead + p.aim * 30.0f;
                lookAhead = lookAhead + (lead - lookAhead) * (1.0f - std::exp(-kDt / 0.25f));
                const sim::BossState& b = w.boss();
                Vec2 leanTo{};
                float zoomTo = 1.0f;
                if (b.active && !b.dying && b.kind != BossKind::Longwave && lengthSq(b.pos - p.pos) < 1800.0f * 1800.0f) {
                    leanTo = (b.pos - p.pos) * 0.3f;
                    const float l = length(leanTo);
                    if (l > 320.0f) leanTo = leanTo * (320.0f / l);
                    zoomTo = 0.85f;
                }
                // M5: Longwave and the Choir hold their page's zoom for the whole fight (PlayView).
                if (b.active && !b.dying && b.kind == BossKind::Longwave) zoomTo = species.longwave.zoom;
                if (b.active && !b.dying && b.kind == BossKind::Choir) zoomTo = species.choir.zoom;
                if (b.active && !b.dying && b.kind == BossKind::EventHorizon) zoomTo = species.horizon.zoom;
                if (b.active && !b.dying && b.kind == BossKind::Polyrhythm) zoomTo = species.poly.zoom;
                if (b.active && !b.dying && b.kind == BossKind::Feedback) zoomTo = species.feedback.zoom;
                lean = lean + (leanTo - lean) * (1.0f - std::exp(-kDt / 0.5f));
                zoom += (zoomTo - zoom) * (1.0f - std::exp(-kDt / 2.0f));
            }
            const Vec2 cam = p.pos + lookAhead + lean;
            const float vw = hw / zoom, vh = hh / zoom;
            u32 near = 0, counted = 0;  // in the camera's frame (in view); in the frame the director counts
            const Enemies& e = w.enemies();
            for (u32 i = 0; i < e.count; ++i) {
                if (e.flags[i] & kFlagDead) continue;
                near += (std::fabs(e.x[i] - cam.x) <= vw && std::fabs(e.y[i] - cam.y) <= vh) ? 1u : 0u;
                counted += (std::fabs(e.x[i] - p.pos.x) <= cw && std::fabs(e.y[i] - p.pos.y) <= ch) ? 1u : 0u;
            }
            if (trace && ph.bar != lastBar) {
                // A line a bar while tracing: the director's count and goal, what is in view, alive, kills.
                const u32 k = w.counters().kills - c0.kills;
                std::printf("[bar] %4u T%u c%u %-6s goal %5.0f counted %4u (director %4u) in view %4u alive %4u kills %4u\n", ph.bar, ph.track,
                            ph.cycle, sectionLabel(ph.section), w.crowdGoal(), counted, w.crowdInFrame(), near, e.count, k - killsAtBar);
                killsAtBar = k;
                lastBar = ph.bar;
            }
            const u32 tr = ph.track < 4 ? ph.track : 4u, cy = ph.cycle < 5 ? ph.cycle : 4u;
            if (!cy) continue;
            if (ph.section == Section::Groove) {
                cells[tr - 1][cy].grooveSum += near;
                cells[tr - 1][cy].countedSum += counted;
                cells[tr - 1][cy].aliveSum += e.count;
                ++cells[tr - 1][cy].grooveTicks;
            } else if (ph.section == Section::Drop && !ph.bossDrop) {
                sawDrop[tr - 1][cy] = true;
                if (static_cast<float>(near) > peak[tr - 1][cy]) peak[tr - 1][cy] = static_cast<float>(near);
            }
        }
        allocs += heapAllocCountThisThread() - a0;
        for (u32 tr = 0; tr < 4; ++tr)
            for (u32 cy = 1; cy < 5; ++cy)
                if (sawDrop[tr][cy]) {
                    cells[tr][cy].dropPeakSum += peak[tr][cy];
                    ++cells[tr][cy].dropRuns;
                }
        const WorldCounters& c = world->counters();
        const u32 seedKills = c.kills - c0.kills, seedShards = c.shardsDropped - c0.shardsDropped;
        levelTrios += world->player(0).levelTrios;
        cellsTaken += world->player(0).cellsTaken;
        cellsRegrown += world->player(0).cellsRegrown;
        kills += seedKills;
        deaths += runner.deaths();
        overflow += (c.enemiesDropped - c0.enemiesDropped) + (c.bulletsDropped - c0.bulletsDropped) + (c.shotsDropped - c0.shotsDropped) +
                    seedShards + (c.eventsDropped - c0.eventsDropped);
        capped += c.shotsCapped - c0.shotsCapped;
        if (!world->finite()) ++nan;
        std::printf("[sim]   seed %llu: first death %.2f min, Track %u reached, %u kills, %u deaths, Shards dropped %u; dropped enemies %u, "
                    "bullets %u, shots %u, events %u\n",
                    static_cast<unsigned long long>(seed0 + s), firstDeath[s] > minutes ? -1.0 : firstDeath[s], maxTrack[s], seedKills,
                    runner.deaths(), seedShards, c.enemiesDropped - c0.enemiesDropped, c.bulletsDropped - c0.bulletsDropped,
                    c.shotsDropped - c0.shotsDropped, c.eventsDropped - c0.eventsDropped);
    }
    const char* botName = policy == Bot::Policy::Reckless ? "careless" : (policy == Bot::Policy::Novice ? "novice" : (policy == Bot::Policy::Assist ? "assist" : "careful"));
    std::printf("[sim] crowd (%s bot%s): %u seeds x %.0f min\n", botName, god ? ", god mode" : "", seeds, minutes);
    std::printf("[sim]   Track cycle  target  in view (Groove)  ratio  counted  alive  Drop peak in view (runs)  peak/target\n");
    for (u32 tr = 0; tr < 4; ++tr) {
        for (u32 cy = 1; cy <= cyclesOfTrack(tr + 1); ++cy) {
            const Cell& c = cells[tr][cy];
            if (tr == 3 && !c.grooveTicks) continue;  // the loop's first Track, when a run got there
            const float target = crowdTarget(cr, tr + 1, cy, kBossesBuilt);  // (the set list: the Tracks with their fights)
            const double mean = c.grooveTicks ? c.grooveSum / static_cast<double>(c.grooveTicks) : 0.0;
            const double cm = c.grooveTicks ? c.countedSum / static_cast<double>(c.grooveTicks) : 0.0;
            const double pk = c.dropRuns ? c.dropPeakSum / c.dropRuns : 0.0;
            const double am = c.grooveTicks ? c.aliveSum / static_cast<double>(c.grooveTicks) : 0.0;
            std::printf("[sim]   %5u %5u  %6.0f  %16.1f  %5.2f  %7.1f  %5.0f  %16.1f (%2u)  %5.2f\n", tr + 1, cy, target, mean, target > 0 ? mean / target : 0.0, cm,
                        am, pk, c.dropRuns, target > 0 ? pk / target : 0.0);
            logTagged("RFSIM", "{\"test\":\"crowd_cell\",\"track\":%u,\"cycle\":%u,\"target\":%.0f,\"in_view_mean\":%.1f,\"counted_mean\":%.1f,\"drop_peak_in_view\":%.1f,\"drop_runs\":%u}",
                      tr + 1, cy, target, mean, cm, pk, c.dropRuns);
        }
    }
    {
        std::printf("[sim]   hits by cause:");
        for (u32 f = 0; f < kFamilyCount; ++f)
            if (hitsBy[f]) std::printf(" %s %llu,", familyName(static_cast<Family>(f)), static_cast<unsigned long long>(hitsBy[f]));
        std::printf(" bullets or beams %llu; boss fights won %llu (mean %.0f s), lost %llu; hits in boss fights %llu; by section G/B/D/Br %llu/%llu/%llu/%llu\n", static_cast<unsigned long long>(hitsBy[kFamilyCount]),
                    static_cast<unsigned long long>(bossWon), bossWon ? bossWonTicks / static_cast<double>(bossWon) / 60.0 : 0.0,
                    static_cast<unsigned long long>(bossLost), static_cast<unsigned long long>(hitsInBossFight), static_cast<unsigned long long>(hitsBySection[0]),
                    static_cast<unsigned long long>(hitsBySection[1]), static_cast<unsigned long long>(hitsBySection[2]),
                    static_cast<unsigned long long>(hitsBySection[3]));
    }
    float sorted[256];
    const u32 n = seeds < 256 ? seeds : 256;
    for (u32 k = 0; k < n; ++k) sorted[k] = firstDeath[k];
    std::sort(sorted, sorted + n);
    const float median = n ? (n % 2 ? sorted[n / 2] : 0.5f * (sorted[n / 2 - 1] + sorted[n / 2])) : 0.0f;
    u32 t2 = 0, t3 = 0;
    for (u32 k = 0; k < n; ++k) {
        t2 += maxTrack[k] >= 2 ? 1u : 0u;
        t3 += maxTrack[k] >= 3 ? 1u : 0u;
    }
    std::printf("[sim]   first death: median %.2f min (%s); Track 2 reached in %u of %u runs, Track 3 in %u\n", median,
                median > minutes ? "no death in most runs" : "minutes", t2, n, t3);
    {
        // Play test 2's review (GAME_DESIGN 8): deaths in the first pass (Tracks 1-3) and the first deaths' Tracks.
        u32 firstPass = 0, firstSet = 0;
        for (u32 k = 0; k < n; ++k) firstPass += (firstDeath[k] <= minutes && firstDeathTrack[k] <= 3) ? 1u : 0u;
        // M5 part 4: the whole first pass of the set list (Tracks 1 to kBossesBuilt).
        for (u32 k = 0; k < n; ++k) firstSet += (firstDeath[k] <= minutes && firstDeathTrack[k] <= kBossesBuilt) ? 1u : 0u;
        // The Track the median first death falls in (the runs sorted by first death; no death counts as the latest).
        u32 order[256];
        for (u32 k = 0; k < n; ++k) order[k] = k;
        std::sort(order, order + n, [&](u32 a, u32 b) { return firstDeath[a] < firstDeath[b]; });
        const u32 mid = n ? order[(n - 1) / 2] : 0u, mid2 = n ? order[n / 2] : 0u;
        const u32 ta = firstDeath[mid] <= minutes ? firstDeathTrack[mid] : 0u, tb = firstDeath[mid2] <= minutes ? firstDeathTrack[mid2] : 0u;
        if (ta == tb)
            std::printf("[sim]   the median first death falls in Track %u (0: no death)\n", ta);
        else
            std::printf("[sim]   the median first death falls between Tracks %u and %u (the two middle runs; 0: no death)\n", ta, tb);
        // Play test 3 (GAME_DESIGN 7.6): the runs that finished the set list (reached the Track after its last before a first death).
        u32 finished = 0;
        for (u32 k = 0; k < n; ++k)
            finished += (firstDeath[k] <= minutes ? firstDeathTrack[k] > kBossesBuilt : maxTrack[k] > kBossesBuilt) ? 1u : 0u;
        {
            const auto table = [&](const char* title, const u64* by) {
                std::printf("[sim]   %s:", title);
                for (u32 k = 0; k < 35; ++k) {
                    if (!by[k]) continue;
                    if (k < 32) std::printf(" %s%s %llu,", familyName(static_cast<Family>(k % 16u)), k >= 16 ? " bullets" : "", static_cast<unsigned long long>(by[k]));
                    else std::printf(" %s %llu,", k == 32 ? "fights" : (k == 33 ? "events and blasts" : "unknown"), static_cast<unsigned long long>(by[k]));
                }
                std::printf("\n");
            };
            table("hits outside the fights from Track 4 on, by what took them", lateBy);
            table("the hits that ended a run (the ship lost), by what took them", endBy);
        }
        std::printf("[sim]   finished the %u Tracks (the first run reached Track %u): %u of %u runs; Extra Lives taken %llu, spent %llu; guard rings broken %llu\n",
                    kBossesBuilt, kBossesBuilt + 1, finished, n, static_cast<unsigned long long>(livesTaken), static_cast<unsigned long long>(livesSpent),
                    static_cast<unsigned long long>(ringBreaks));
        std::printf("[sim]   first deaths in Tracks 1-3: %u of %u runs; in the first pass (Tracks 1-%u): %u; first deaths by run (min/Track):", firstPass, n,
                    kBossesBuilt, firstSet);
        for (u32 k = 0; k < n; ++k) {
            if (firstDeath[k] <= minutes) std::printf(" %.1f/T%u", firstDeath[k], firstDeathTrack[k]);
            else std::printf(" -");
        }
        std::printf("\n");
    }
    std::printf("[sim]   kills a minute %.0f, deaths a run %.1f; overflow %llu, NaN runs %u, heap allocations in the loops %llu\n",
                static_cast<double>(kills) / (static_cast<double>(n) * minutes), static_cast<double>(deaths) / n,
                static_cast<unsigned long long>(overflow), nan, static_cast<unsigned long long>(allocs));
    {
        std::printf("[sim]   Track 2 before Longwave (first runs): hits by");
        for (u32 f = 0; f < kFamilyCount; ++f)
            if (t2Hits[f]) std::printf(" %s %llu", familyName(static_cast<Family>(f)), static_cast<unsigned long long>(t2Hits[f]));
        std::printf(" bullets or beams %llu, in the mini-boss fight %llu\n", static_cast<unsigned long long>(t2Hits[kFamilyCount]),
                    static_cast<unsigned long long>(t2Hits[kFamilyCount + 1]));
        (void)t2Runs;
        (void)t2Regained;
    }
    // M5 part 5: elites and the arena.
    std::printf("[sim]   elites %llu (killed as large kills %llu, Splitting %llu, Volatile bursts %llu); bodies broken %llu, Pylons tuned %llu; events:",
                static_cast<unsigned long long>(elites), static_cast<unsigned long long>(eliteKills), static_cast<unsigned long long>(splits),
                static_cast<unsigned long long>(bombs), static_cast<unsigned long long>(broken), static_cast<unsigned long long>(pylonsTuned));
    for (u32 k = 0; k < kEventKinds; ++k) std::printf(" %s %llu", eventId(static_cast<EventKind>(k)), static_cast<unsigned long long>(events[k]));
    std::printf("\n");
    // M5 part 1: every fight by its kind (seconds to kill by the Track it came in).
    std::printf("[sim]   fights (kind: fought, won, lost, warped out, hits taken a fight [bullets / bodies / beams], cells as it began / of; seconds to kill by Track):\n");
    for (u32 k = 0; k < kBossKinds; ++k) {
        const FightStat& f = fightStat[k];
        if (!f.fights) continue;
        const double nf = static_cast<double>(f.fights);
        const u32 bullets = f.hits >= f.hitsBody + f.hitsBeam ? f.hits - f.hitsBody - f.hitsBeam : 0u;
        std::printf("[sim]     %-13s %3u %3u %3u %3u  %5.2f [%.2f / %.2f / %.2f] %4.1f/%4.1f ", bossId(static_cast<BossKind>(k)), f.fights, f.won, f.lost,
                    f.gone, static_cast<double>(f.hits) / nf, bullets / nf, f.hitsBody / nf, f.hitsBeam / nf, f.cellsIn / nf, f.capIn / nf);
        for (u32 tr = 1; tr < 16; ++tr)
            if (f.wonAt[tr]) std::printf(" T%u %.0f s (%u)", tr, f.wonSecs[tr] / f.wonAt[tr], f.wonAt[tr]);
        std::printf("\n");
        logTagged("RFSIM", "{\"test\":\"fight\",\"kind\":\"%s\",\"fights\":%u,\"won\":%u,\"lost\":%u,\"gone\":%u,\"hits\":%u}", bossId(static_cast<BossKind>(k)),
                  f.fights, f.won, f.lost, f.gone, f.hits);
    }
    {
        // 0449eb3: the enemy bullets alive in the first run's Grooves (mean and most at once, by Track).
        std::printf("[sim]   enemy bullets alive in the first run's Grooves%s (Track: mean, most; seconds counted):",
                    gBulletsAsWritten ? " (as written)" : "");
        for (u32 t = 1; t <= 10; ++t)
            if (grooveTicks[t] > 0.0)
                std::printf(" T%u %.0f, %u (%.0f s);", t, grooveBullets[t] / grooveTicks[t], grooveBulletsMax[t], grooveTicks[t] / 60.0);
        std::printf("\n");
        std::printf("[sim]   ... fired a Groove second by Track, and the families whose bullets were alive (mean alive):");
        for (u32 t = 1; t <= 10; ++t) {
            if (grooveTicks[t] <= 0.0) continue;
            std::printf("\n[sim]     T%u %.1f a second (the volleys' pages asked %.0f, the Track's count fired %.0f: x %.2f):", t,
                        grooveSpawned[t] / (grooveTicks[t] / 60.0), grooveAsked[t], grooveKept[t], grooveAsked[t] > 0.0 ? grooveKept[t] / grooveAsked[t] : 1.0);
            for (u32 f = 0; f <= kFamilyCount; ++f)
                if (grooveByFamily[t][f] / grooveTicks[t] >= 0.5)
                    std::printf(" %s %.1f,", f < kFamilyCount ? familyName(static_cast<Family>(f)) : "other", grooveByFamily[t][f] / grooveTicks[t]);
        }
        std::printf("\n");
    }
    {
        // M5 (GAME_DESIGN 7.6): the first meetings (won of met, the seconds of those won, the hits of all fights above).
        u32 t2 = 0;
        for (u32 s = 0; s < n; ++s) t2 += reachedT2[s] ? 1u : 0u;
        std::printf("[sim]   the first run reached Track 2's boss in %u of %u runs; first meetings (kind: met, won, lost, warped out; mean seconds of those won; cells as it began, won / lost):\n", t2, n);
        for (u32 k = 0; k < kBossKinds; ++k) {
            u32 met = 0, won = 0, lost = 0, gone = 0, paced = 0;
            double secs = 0.0, cellsWon = 0.0, cellsLost = 0.0, pace = 0.0, paceDmg = 0.0;
            for (u32 s = 0; s < n; ++s) {
                const u8 fm = firstMeet[s][k];
                if (!fm) continue;
                ++met;
                if (firstPaceDmg[s][k] >= 0.0f) {
                    ++paced;
                    pace += firstPace[s][k];
                    paceDmg += firstPaceDmg[s][k];
                }
                if (fm == 1) {
                    ++won;
                    secs += firstSecs[s][k];
                    cellsWon += firstCells[s][k];
                } else if (fm == 2) {
                    ++lost;
                    cellsLost += firstCells[s][k];
                } else {
                    ++gone;
                }
            }
            if (!met) continue;
            std::printf("[sim]     %-13s %3u %3u %3u %3u  %5.0f s  cells %.1f / %.1f  pace %.2f (8-bar damage a player %.0f, %u fights)\n",
                        bossId(static_cast<BossKind>(k)), met, won, lost, gone, won ? secs / won : 0.0, won ? cellsWon / won : 0.0,
                        lost ? cellsLost / lost : 0.0, paced ? pace / paced : 1.0, paced ? paceDmg / paced : 0.0, paced);
            logTagged("RFSIM", "{\"test\":\"first_meeting\",\"kind\":\"%s\",\"met\":%u,\"won\":%u,\"lost\":%u,\"gone\":%u,\"secs\":%.1f}",
                      bossId(static_cast<BossKind>(k)), met, won, lost, gone, won ? secs / won : 0.0);
        }
    }
    std::printf("[sim]   boss fights lost %.1f%%; shots alive at most %u (pool %u), held back by a player's cap %llu\n",
                bossWon + bossLost ? 100.0 * static_cast<double>(bossLost) / static_cast<double>(bossWon + bossLost) : 0.0, shotsPeak,
                Shots::kCap, static_cast<unsigned long long>(capped));
    {
        // Play test 2: the level pace by Track and what the first run holds at minutes 3, 6 and 10.
        std::printf("[sim]   Trios in the first run by Track, a run that played there (level-ups + the boss's; a667116):");
        for (u32 tr = 1; tr <= 10; ++tr)
            if (runsInTrack[tr] > 0.0)
                std::printf(" T%u %.1f+%.1f (%.0f)", tr, triosLevel[tr] / runsInTrack[tr], triosBoss[tr] / runsInTrack[tr], runsInTrack[tr]);
        std::printf("\n");
        std::printf("[sim]   Shard levels (%s): seconds a level by Track:", god ? "god mode" : "as played");
        for (u32 tr = 0; tr < 4; ++tr)
            if (paceTicks[tr] > 0.0)
                std::printf(" T%u%s %.1f s (%.0f levels in %.1f min, %.0f Shards a second)", tr + 1, tr == 3 ? "+" : "",
                            paceLevels[tr] > 0.0 ? paceTicks[tr] / 60.0 / paceLevels[tr] : 0.0, paceLevels[tr], paceTicks[tr] / 3600.0,
                            paceShards[tr] / (paceTicks[tr] / 60.0));
        std::printf("\n");
        for (u32 m = 0; m < kMarks; ++m) {
            const double n = runsAt[m] ? static_cast<double>(runsAt[m]) : 1.0;
            std::printf("[sim]   minute %2u (first run, %u of %u runs): level %.1f, upgrades taken %.1f (Mod stacks %.1f, Riff levels %.1f), cells %.1f\n",
                        markMinute[m], runsAt[m], n > 0 ? seeds : 0u, levelAt[m] / n, takenAt[m] / n, modsAt[m] / n, riffsAt[m] / n, cellsAt[m] / n);
            logTagged("RFSIM", "{\"test\":\"crowd_levels\",\"minute\":%u,\"runs\":%u,\"level\":%.2f,\"taken\":%.2f,\"mods\":%.2f,\"riffs\":%.2f}", markMinute[m], runsAt[m],
                      levelAt[m] / n, takenAt[m] / n, modsAt[m] / n, riffsAt[m] / n);
        }
        std::printf("[sim]   level-ups %llu, level-up Trios opened %llu; Cells taken %llu, cells grown back by Regen %llu\n", static_cast<unsigned long long>(levelUps),
                    static_cast<unsigned long long>(levelTrios), static_cast<unsigned long long>(cellsTaken), static_cast<unsigned long long>(cellsRegrown));
        // M5 part 3 (GAME_DESIGN 6.1): a build deepens (two Mods at their cap by Track 3) and a Fusion is reachable (held by
        // the end of Track 5 in half the careful bot's runs).
        std::printf("[sim]   the first run's build: as Track 3 ends (%u runs) %.2f Mods at their cap, two or more in %u; as Track 5 ends (%u runs) "
                    "a Fusion held in %u\n",
                    t3Runs, t3Runs ? t3Capped / t3Runs : 0.0, t3Capped2, t5Runs, t5Fusion);
        logTagged("RFSIM", "{\"test\":\"crowd_build\",\"t3_runs\":%u,\"t3_capped_mean\":%.2f,\"t3_capped2\":%u,\"t5_runs\":%u,\"t5_fusion\":%u}", t3Runs,
                  t3Runs ? t3Capped / t3Runs : 0.0, t3Capped2, t5Runs, t5Fusion);
        // 2026-11-15: the first run's first Fusion.
        static float dueM[256], heldM[256];
        u32 nd = 0, nh = 0, dueBy[12] = {};
        double dueTaken = 0.0;
        for (u32 s = 0; s < n; ++s) {
            if (fuseDueMin[s] >= 0.0f) {
                dueM[nd++] = fuseDueMin[s];
                dueTaken += fuseDueTaken[s];
                ++dueBy[fuseDueTrack[s] < 11 ? fuseDueTrack[s] : 11];
            }
            if (fuseHeldMin[s] >= 0.0f) heldM[nh++] = fuseHeldMin[s];
        }
        std::sort(dueM, dueM + nd);
        std::sort(heldM, heldM + nh);
        std::printf("[sim]   the first Fusion (first run): due in %u of %u runs (median minute %.1f, %.1f upgrades taken by then), taken in %u (median "
                    "minute %.1f); due in Track",
                    nd, n, nd ? dueM[nd / 2] : 0.0f, nd ? dueTaken / nd : 0.0, nh, nh ? heldM[nh / 2] : 0.0f);
        for (u32 tr = 1; tr <= 11; ++tr)
            if (dueBy[tr]) std::printf(" %u: %u", tr, dueBy[tr]);
        std::printf("; which:");
        for (u8 f = 0; f < kFusionCount; ++f)
            if (fuseKind[f]) std::printf(" %s %u", fusionInfo(f).name, fuseKind[f]);
        std::printf("\n");
        logTagged("RFSIM", "{\"test\":\"crowd_fusion\",\"due\":%u,\"due_median_min\":%.2f,\"due_taken_mean\":%.2f,\"held\":%u,\"held_median_min\":%.2f}", nd,
                  nd ? dueM[nd / 2] : 0.0f, nd ? dueTaken / nd : 0.0, nh, nh ? heldM[nh / 2] : 0.0f);
    }
    logTagged("RFSIM", "{\"test\":\"crowd\",\"seeds\":%u,\"minutes\":%.0f,\"policy\":\"%s\",\"first_death_median\":%.2f,\"track2\":%u,\"track3\":%u,"
                       "\"kills_per_min\":%.1f,\"overflow\":%llu,\"nan\":%u,\"allocs\":%llu}",
              n, minutes, botName, median, t2, t3,
              static_cast<double>(kills) / (static_cast<double>(n) * minutes), static_cast<unsigned long long>(overflow), nan,
              static_cast<unsigned long long>(allocs));
    delete world;
    return overflow == 0 && nan == 0 && allocs == 0 ? 0 : 1;
}

}  // namespace

int simHeadlessMain(int argc, char** argv) {
    const char* mode = argc > 2 ? argv[2] : "";
    float minutes = -1.0f, seconds = -1.0f;
    u64 seed = 1;
    u32 seeds = 20;
    Bot::Policy policy = Bot::Policy::Play;
    bool god = false, trace = false;
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
            policy = std::strcmp(next, "reckless") == 0 ? Bot::Policy::Reckless
                     : std::strcmp(next, "novice") == 0 ? Bot::Policy::Novice
                     : std::strcmp(next, "assist") == 0 ? Bot::Policy::Assist
                                                        : Bot::Policy::Play;
            ++i;
        } else if (std::strcmp(a, "--seed") == 0 && next) {
            seed = std::strtoull(next, nullptr, 10);
            ++i;
        } else if (std::strcmp(a, "--god") == 0) {
            god = true;
        } else if (std::strcmp(a, "--trace") == 0) {
            trace = true;
        } else if (std::strcmp(a, "--armour0") == 0) {
            gArmourNothing = true;
        } else if (std::strcmp(a, "--nopace") == 0) {
            gNoPace = true;
        } else if (std::strcmp(a, "--bullets-as-written") == 0) {
            gBulletsAsWritten = true;
        } else if (std::strcmp(a, "--handover") == 0 && next) {
            gHandover = static_cast<u32>(std::strtoul(next, nullptr, 10));
            ++i;
        } else if (std::strcmp(a, "--health") == 0 && next) {
            // id=factor[,id=factor...]
            char buf[512];
            std::snprintf(buf, sizeof buf, "%s", next);
            for (char* tok = std::strtok(buf, ","); tok; tok = std::strtok(nullptr, ",")) {
                char* eq = std::strchr(tok, '=');
                if (!eq) continue;
                *eq = 0;
                BossKind k;
                if (!bossFromId(tok, k)) {
                    std::printf("[sim] unknown fight %s\n", tok);
                    return 2;
                }
                gHealthLever[static_cast<u32>(k)] = static_cast<float>(std::atof(eq + 1));
            }
            ++i;
        } else if (std::strcmp(a, "--seeds") == 0 && next) {
            seeds = static_cast<u32>(std::strtoul(next, nullptr, 10));
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
    } else if (std::strcmp(mode, "crowd") == 0) {
        code = runCrowd(*species, seeds, minutes > 0.0f ? minutes : 30.0f, seed, policy, god, trace);
    } else if (std::strcmp(mode, "picks") == 0) {
        code = runPicks(*species, seeds, seed);  // M5 part 7: the no-trap table
    } else if (std::strcmp(mode, "stress") == 0) {
        code = runSoak(*species, seconds > 0.0f ? seconds : (minutes > 0.0f ? minutes * 60.0f : 120.0f), seed, bpm, true, policy);
    } else {
        std::printf("usage: raveform_headless --sim determinism|soak|crowd|picks|stress [--minutes M] [--seconds S] [--seed N] [--seeds K] [--policy play|reckless|novice|assist] [--god] [--trace] [--armour0] [--nopace] [--health id=x,...] [--handover N]\n");
    }
    delete species;
    return code;
}
