// The simple spawner of M3 stage A. Not the director (GAME_DESIGN 8): no phrase cycle, no formations
// beyond a cluster per pick, no assist. It exists so the fight can be played and tested; the director
// replaces runSpawner() and keeps spawnEnemy().

#include <cmath>

#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
// How many of a family one pick brings (the most the bank buys, within these).
u32 packMax(Family f) {
    switch (f) {
        case Family::Drifter: return 3;
        case Family::Hound: return 3;
        case Family::Carom: return 2;
        default: return 1;
    }
}
// Scatter of a pack around its anchor (wu).
float packSpread(Family f) {
    switch (f) {
        case Family::Drifter: return 110.0f;
        case Family::Hound: return 45.0f;
        case Family::Shoal: return 50.0f;
        case Family::Carom: return 70.0f;
        default: return 0.0f;
    }
}
}  // namespace

Vec2 World::spawnAnchor(Vec2 player) {
    // On a band around the player, squashed to the 20:9 screen, never within 260 wu of the player and
    // never inside the central third of the view (GAME_DESIGN 8 fairness; 450 wu is outside it).
    Vec2 best = player + Vec2{kSpawnFarWu, 0.0f};
    for (u32 tries = 0; tries < 12; ++tries) {
        const float a = rng_.range(0.0f, kTau);
        const float rx = rng_.range(kSpawnNearWu, kSpawnFarWu);
        Vec2 p = player + Vec2{std::cos(a) * rx, std::sin(a) * rx * kSpawnSquash};
        p.x = clampf(p.x, -kArenaHalfW + 60.0f, kArenaHalfW - 60.0f);
        p.y = clampf(p.y, -kArenaHalfH + 60.0f, kArenaHalfH - 60.0f);
        best = p;
        const Vec2 d = p - player;
        if (lengthSq(d) >= kSpawnMinPlayerWu * kSpawnMinPlayerWu) break;
    }
    return best;
}

void World::spawnFormation(u16 speciesIndex, u32 count, Vec2 anchor, double landTick) {
    const Species& s = species_->get(speciesIndex);
    const float spread = packSpread(s.family);
    const u16 school = s.family == Family::Shoal ? static_cast<u16>(spawner_.nextSchool++) : 0;
    const PlayerState& p0 = players_[0];
    for (u32 k = 0; k < count; ++k) {
        Vec2 p = anchor;
        if (spread > 0.0f) {
            const float a = rng_.range(0.0f, kTau), r = spread * std::sqrt(rng_.uniform());
            p = p + dirOf(a) * r;
        }
        // Never on top of a player.
        const Vec2 d = p - p0.pos;
        if (p0.alive && lengthSq(d) < kSpawnMinPlayerWu * kSpawnMinPlayerWu)
            p = p0.pos + normalizeOr(d, Vec2{1.0f, 0.0f}) * kSpawnMinPlayerWu;
        const i32 i = spawnEnemy(speciesIndex, p, angleOf(p0.pos - p), landTick);
        if (i < 0) return;  // the pool is full: stop spawning into it (TECH_SPEC 7.1)
        enemies_.group[i] = school;
    }
}

void World::runSpawner() {
    Spawner& sp = spawner_;
    if (sp.mode == Spawner::Mode::Off) return;
    const PlayerState& p0 = players_[0];
    const double land = kWarpBeats * kTicksPerBeat;

    if (sp.mode == Spawner::Mode::Stress) {
        // Hold kStressEnemies around the ship (about 700 in view) with kStressSentries of them Sentries
        // firing (about 1,500 bullets alive). Refilled on every beat; enemies left far behind are recycled.
        if (!crossed(m0_, m1_, kTicksPerBeat) && sp.lastBar != 0) return;
        sp.lastBar = 1;
        const double beatTick = std::floor(m1_ / kTicksPerBeat) * kTicksPerBeat;
        Enemies& e = enemies_;
        u32 alive = 0, sentries = 0;
        for (u32 i = 0; i < e.count; ++i) {
            if (e.flags[i] & kFlagDead) continue;
            const Vec2 d = Vec2{e.x[i], e.y[i]} - p0.pos;
            if (std::fabs(d.x) > kStressRecycleWu || std::fabs(d.y) > kStressRecycleWu) {
                e.flags[i] |= kFlagDead;  // recycled silently (no event, no Shards)
                continue;
            }
            ++alive;
            if (species_->get(e.species[i]).family == Family::Sentry) ++sentries;
        }
        sp.sentries = sentries;
        const auto place = [&](float minR) {
            for (u32 t = 0; t < 8; ++t) {
                const Vec2 p = p0.pos + Vec2{rng_.range(-kStressHalfW, kStressHalfW), rng_.range(-kStressHalfH, kStressHalfH)};
                if (lengthSq(p - p0.pos) >= minR * minR) return p;
            }
            return p0.pos + Vec2{kStressHalfW, 0.0f};
        };
        const u16 sentry = SpeciesTable::index(Family::Sentry, 1, kVariantB);
        u32 budget = 160;  // per beat
        while (sentries < kStressSentries && alive < kStressEnemies && budget) {
            if (spawnEnemy(sentry, place(320.0f), 0.0f, beatTick + land) < 0) return;
            ++sentries;
            ++alive;
            --budget;
        }
        while (alive < kStressEnemies && budget) {
            const float r = rng_.uniform();
            u16 sp16;
            u32 n = 1;
            if (r < 0.40f) {
                sp16 = SpeciesTable::index(Family::Shoal, rng_.uniform() < 0.5f ? 1 : 2, kVariantA);
                n = 20;
            } else if (r < 0.62f) {
                const float v = rng_.uniform();
                sp16 = v < 0.33f ? SpeciesTable::index(Family::Hound, 2, kVariantB)
                                 : SpeciesTable::index(Family::Hound, v < 0.66f ? 2 : 1, kVariantA);
                n = 4;
            } else if (r < 0.80f) {
                sp16 = SpeciesTable::index(Family::Drifter, rng_.uniform() < 0.5f ? 1 : 2, kVariantA);
                n = 3;
            } else if (r < 0.93f) {
                sp16 = SpeciesTable::index(Family::Carom, rng_.uniform() < 0.5f ? 1 : 2, kVariantA);
                n = 2;
            } else {
                sp16 = SpeciesTable::index(Family::Ram, rng_.uniform() < 0.5f ? 1 : 2, kVariantA);
            }
            if (n > kStressEnemies - alive) n = kStressEnemies - alive;
            if (n > budget) n = budget;
            const u32 before = enemies_.count;
            spawnFormation(sp16, n, place(kSpawnMinPlayerWu + 40.0f), beatTick + land);
            const u32 made = enemies_.count - before;
            if (made == 0) return;
            alive += made;
            budget = budget > made ? budget - made : 0;
        }
        return;
    }

    // Slice: on every bar line, the Groove budget of the Track and cycle reached (Groove x 1.0; stage A
    // has no Build, Drop or Break), with debuts by elapsed bars.
    forCrossings(m0_, m1_, kTicksPerBar, [&](double barTick, i64) {
        const double rel = (barTick - sp.startTick) / kTicksPerBar;
        if (rel < 0.5) return;
        const u32 bar = static_cast<u32>(rel + 0.5);
        sp.lastBar = bar;
        const u32 t1Bars = SpeciesTable::kCycleBars[0] * species_->budgetCycles[0];
        if (bar < t1Bars) {
            sp.track = 1;
            sp.cycle = static_cast<u8>(bar / SpeciesTable::kCycleBars[0] + 1);
        } else {
            sp.track = 2;
            const u32 c = (bar - t1Bars) / SpeciesTable::kCycleBars[1] + 1;
            // After Track 2's last cycle the budget holds (the endless rules are the director's).
            sp.cycle = static_cast<u8>(c < species_->budgetCycles[1] ? c : species_->budgetCycles[1]);
        }
        sp.budgetPerBar = species_->budget[sp.track - 1][sp.cycle - 1];
        sp.bank = std::fmin(sp.bank + sp.budgetPerBar, sp.budgetPerBar * kBankBars);

        // What has debuted; the newest debut is picked twice as often.
        u16 cand[SpeciesTable::kMaxRoster];
        float weight[SpeciesTable::kMaxRoster];
        u32 n = 0;
        u32 newest = 0;
        for (u32 k = 0; k < species_->rosterCount; ++k) {
            const SpeciesTable::RosterEntry& r = species_->roster[k];
            if (r.track > sp.track || (r.track == sp.track && r.cycle > sp.cycle)) continue;
            const u32 order = static_cast<u32>(r.track) * 16 + r.cycle;
            if (order > newest) newest = order;
        }
        for (u32 k = 0; k < species_->rosterCount; ++k) {
            const SpeciesTable::RosterEntry& r = species_->roster[k];
            if (r.track > sp.track || (r.track == sp.track && r.cycle > sp.cycle)) continue;
            cand[n] = r.species;
            weight[n] = (static_cast<u32>(r.track) * 16 + r.cycle == newest) ? 2.0f : 1.0f;
            ++n;
        }
        if (!n) return;

        // Pick formations until the bank cannot buy the next one; an unaffordable pick waits for a later
        // bar (the bank saves up to kBankBars of budget), so Shoals and Sentries do arrive.
        for (u32 formations = 0; formations < 12; ++formations) {
            if (sp.pending < 0) {
                float total = 0.0f;
                for (u32 k = 0; k < n; ++k) total += weight[k];
                float pick = rng_.range(0.0f, total);
                u32 c = 0;
                for (; c + 1 < n; ++c) {
                    if (pick < weight[c]) break;
                    pick -= weight[c];
                }
                sp.pending = cand[c];
            }
            const u16 species = static_cast<u16>(sp.pending);
            const Species& s = species_->get(species);
            const FamilyData& fd = species_->family(s.family);
            u32 minCount = 1, maxCount = packMax(s.family);
            if (s.family == Family::Shoal) {
                minCount = fd.groupMin;
                maxCount = fd.groupMax < kShoalGroupCapStageA ? fd.groupMax : kShoalGroupCapStageA;
            }
            const float cost = s.threat;
            if (sp.bank + 1e-4f < cost * static_cast<float>(minCount)) {
                // The pick waits (the bank saves) unless this budget can never buy it.
                if (cost * static_cast<float>(minCount) <= sp.budgetPerBar * kBankBars) break;
                sp.pending = -1;
                continue;
            }
            u32 count = static_cast<u32>((sp.bank + 1e-4f) / cost);
            if (count > maxCount) count = maxCount;
            if (count < minCount) count = minCount;
            const u32 before = enemies_.count;
            spawnFormation(species, count, spawnAnchor(p0.pos), barTick + land);
            const u32 made = enemies_.count - before;
            sp.bank -= cost * static_cast<float>(made);
            sp.pending = -1;
            if (made < count) break;  // the pool is full
        }
    });
}

}  // namespace rf::sim
