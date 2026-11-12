// The director (GAME_DESIGN 4, 8; M3 stage B) and the stress crowd. The director walks the phrase cycle
// bar by bar (sim/director.h), spends the threat budget on formations, lands the Drop's mega-wave on the
// downbeat, spotlights debuts at the end of a Break, dissolves fodder at the start of one, opens the
// Trios, brings the boss in on the last Drop and drops a Boost roughly every 20-30 s.

#include <cmath>

#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
// How many of a family one formation brings (the most the bank buys, within these).
u32 packMax(Family f) {
    switch (f) {
        case Family::Drifter: return 4;
        case Family::Hound: return 4;
        case Family::Carom: return 3;
        case Family::Ram: return 2;
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
        case Family::Ram: return 60.0f;
        default: return 0.0f;
    }
}
// A debut's spotlight group (GAME_DESIGN 8: "a small group alone").
u32 spotlightCount(Family f, u32 groupMin) {
    switch (f) {
        case Family::Shoal: return groupMin;
        case Family::Sentry: return 1;
        case Family::Ram:
        case Family::Carom: return 2;
        default: return 3;
    }
}
}  // namespace

u32 World::barOf(double tick) const {
    const double rel = (tick - director_.startTick) / kTicksPerBar;
    return rel > 0.0 ? static_cast<u32>(std::floor(rel + 1e-9)) : 0u;
}

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
        // Never on top of a player, never inside the Trio's ring (screens.md).
        const Vec2 d = p - p0.pos;
        if (p0.alive && lengthSq(d) < kSpawnMinPlayerWu * kSpawnMinPlayerWu)
            p = p0.pos + normalizeOr(d, Vec2{1.0f, 0.0f}) * kSpawnMinPlayerWu;
        const TrioState& tr = trio_[0];
        if (tr.open) {
            const Vec2 dt = p - tr.centre;
            const float keep = kTrioRingWu + 80.0f;
            if (lengthSq(dt) < keep * keep) p = tr.centre + normalizeOr(dt, Vec2{1.0f, 0.0f}) * keep;
        }
        const i32 i = spawnEnemy(speciesIndex, p, angleOf(p0.pos - p), landTick);
        if (i < 0) return;  // the pool is full: stop spawning into it (TECH_SPEC 7.1)
        enemies_.group[i] = school;
    }
}

void World::spawnShape(u16 speciesIndex, u32 count, double landTick) {
    // GAME_DESIGN 8 names the formations; the first slice uses five: a cluster, a ring around the player,
    // a pincer from both sides, a sweep along one edge of the view and the four corners.
    const Species& s = species_->get(speciesIndex);
    const Vec2 c = players_[0].pos;
    enum { Cluster, Ring, Pincer, Sweep, Corners } shape = Cluster;
    const float r = rng_.uniform();
    switch (s.family) {
        case Family::Hound: shape = count >= 4 ? (r < 0.4f ? Ring : (r < 0.8f ? Pincer : Cluster)) : (r < 0.5f ? Pincer : Cluster); break;
        case Family::Drifter:
        case Family::Carom: shape = count >= 4 ? (r < 0.35f ? Cluster : (r < 0.7f ? Sweep : Corners)) : (r < 0.5f ? Cluster : Sweep); break;
        case Family::Ram: shape = count >= 2 ? Pincer : Cluster; break;
        default: shape = Cluster; break;
    }
    if (count < 2) shape = Cluster;
    switch (shape) {
        case Cluster: spawnFormation(speciesIndex, count, spawnAnchor(c), landTick); break;
        case Ring: {
            const float phase = rng_.range(0.0f, kTau);
            for (u32 k = 0; k < count; ++k) {
                const float a = phase + kTau * static_cast<float>(k) / static_cast<float>(count);
                spawnFormation(speciesIndex, 1, c + Vec2{std::cos(a) * 620.0f, std::sin(a) * 620.0f * 0.5f}, landTick);
            }
            break;
        }
        case Pincer: {
            const u32 half = count / 2;
            const float y = rng_.range(-220.0f, 220.0f);
            spawnFormation(speciesIndex, half, c + Vec2{-700.0f, y}, landTick);
            spawnFormation(speciesIndex, count - half, c + Vec2{700.0f, -y}, landTick);
            break;
        }
        case Sweep: {
            const float side = rng_.sign();
            for (u32 k = 0; k < count; ++k) {
                const float y = -380.0f + 760.0f * (static_cast<float>(k) + 0.5f) / static_cast<float>(count);
                spawnFormation(speciesIndex, 1, c + Vec2{side * 880.0f, y}, landTick);
            }
            break;
        }
        case Corners: {
            static const float kCx[4] = {-760.0f, 760.0f, -760.0f, 760.0f}, kCy[4] = {-330.0f, -330.0f, 330.0f, 330.0f};
            for (u32 k = 0; k < 4; ++k) {
                const u32 n = count / 4 + (k < count % 4 ? 1u : 0u);
                if (n) spawnFormation(speciesIndex, n, c + Vec2{kCx[k], kCy[k]}, landTick);
            }
            break;
        }
    }
}

void World::runSpawner() {
    if (spawner_.mode == Spawner::Mode::Stress) {
        bool anyAlive = false;
        for (u32 p = 0; p < playerCount_; ++p) anyAlive = anyAlive || players_[p].alive;
        if (anyAlive) runStress();
        return;
    }
    runDirector();
}

// ---------------------------------------------------------------------------------------------- director

i32 World::pickSpecies(int kind) {
    // What has debuted by this Track and cycle (all of it once the run loops); the newest debut is picked
    // twice as often.
    const Phrase& ph = phrase_;
    const u32 t = ph.track, c = ph.cycle ? ph.cycle : 1u;
    u16 cand[SpeciesTable::kMaxRoster];
    float weight[SpeciesTable::kMaxRoster];
    u32 n = 0, newest = 0;
    const auto debuted = [&](const SpeciesTable::RosterEntry& r) { return t >= 3 || r.track < t || (r.track == t && r.cycle <= c); };
    for (u32 k = 0; k < species_->rosterCount; ++k) {
        const SpeciesTable::RosterEntry& r = species_->roster[k];
        if (!debuted(r)) continue;
        const u32 order = static_cast<u32>(r.track) * 16 + r.cycle;
        if (order > newest) newest = order;
    }
    for (u32 k = 0; k < species_->rosterCount; ++k) {
        const SpeciesTable::RosterEntry& r = species_->roster[k];
        if (!debuted(r)) continue;
        const bool fodder = species_->get(r.species).hue == 0;  // look.md 2: hue 1 = fodder
        if ((kind == 1 && !fodder) || (kind == 2 && fodder)) continue;
        cand[n] = r.species;
        weight[n] = (t < 3 && static_cast<u32>(r.track) * 16 + r.cycle == newest) ? 2.0f : 1.0f;
        ++n;
    }
    if (!n) return -1;
    float total = 0.0f;
    for (u32 k = 0; k < n; ++k) total += weight[k];
    float pick = rng_.range(0.0f, total);
    u32 k = 0;
    for (; k + 1 < n; ++k) {
        if (pick < weight[k]) break;
        pick -= weight[k];
    }
    return cand[k];
}

void World::spendBudget(double landTick) {
    DirectorState& d = director_;
    // The threat budget buys threats only (GAME_DESIGN 8, since 2026-11-12: hunters and specials); the
    // fodder is the crowd's (spendCrowd). Pick formations until the bank cannot buy the next one; an
    // unaffordable pick waits for a later bar (the bank saves up to kBankBars of budget).
    for (u32 formations = 0; formations < 12; ++formations) {
        if (enemies_.count >= kDirectorAliveCap) return;
        if (d.pending < 0) d.pending = pickSpecies(2);
        if (d.pending < 0) return;
        const u16 species = static_cast<u16>(d.pending);
        const Species& s = species_->get(species);
        const FamilyData& fd = species_->family(s.family);
        u32 minCount = 1, maxCount = packMax(s.family);
        if (s.family == Family::Shoal) {
            minCount = fd.groupMin;
            maxCount = fd.groupMax < kShoalGroupCapStageA ? fd.groupMax : kShoalGroupCapStageA;
        }
        const float cost = s.threat;
        if (d.bank + 1e-4f < cost * static_cast<float>(minCount)) {
            if (cost * static_cast<float>(minCount) <= d.budgetPerBar * kBankBars) return;  // it waits
            d.pending = -1;  // this budget can never buy it
            continue;
        }
        u32 count = static_cast<u32>((d.bank + 1e-4f) / cost);
        if (count > maxCount) count = maxCount;
        if (count < minCount) count = minCount;
        const u32 before = counters_.spawned;
        spawnShape(species, count, landTick);
        const u32 made = counters_.spawned - before;
        d.bank -= cost * static_cast<float>(made);
        d.pending = -1;
        if (made < count) return;  // the pool is full
    }
}

void World::spawnCrowd(u16 speciesIndex, u32 count, double landTick) {
    // GAME_DESIGN 8: floods of 16-80 bodies from one side, rings and corner swarms, warping in outside
    // the central third of the view (the fairness rules) and never within 260 wu of a ship.
    const Vec2 c = players_[0].pos;
    const float r = rng_.uniform();
    if (r < kCrowdFloodShare) {
        // A flood: clusters along a line just outside the view on one side, all coming in.
        const float a = rng_.range(0.0f, kTau);
        const Vec2 out = dirOf(a), side = perpOf(out);
        const u32 clusters = count < 8 ? 1u : (count + 7u) / 8u;
        u32 left = count;
        for (u32 k = 0; k < clusters && left; ++k) {
            const u32 n = (left + (clusters - k) - 1) / (clusters - k);
            const float along = (static_cast<float>(k) - 0.5f * static_cast<float>(clusters - 1)) * kCrowdFloodSpacingWu;
            const Vec2 at = c + Vec2{out.x * kCrowdFloodWu, out.y * kCrowdFloodWu * kSpawnSquash * 1.6f} + side * along;
            spawnFormation(speciesIndex, n, at, landTick);
            left -= n;
        }
    } else if (r < kCrowdFloodShare + kCrowdRingShare) {
        // A ring around the ship.
        const float phase = rng_.range(0.0f, kTau);
        const u32 groups = count < 6 ? count : 6u;
        u32 left = count;
        for (u32 k = 0; k < groups && left; ++k) {
            const u32 n = (left + (groups - k) - 1) / (groups - k);
            const float a = phase + kTau * static_cast<float>(k) / static_cast<float>(groups);
            spawnFormation(speciesIndex, n, c + Vec2{std::cos(a) * kCrowdRingWu, std::sin(a) * kCrowdRingWu * 0.5f}, landTick);
            left -= n;
        }
    } else {
        // Corner swarms.
        static const float kCx[4] = {-1.0f, 1.0f, -1.0f, 1.0f}, kCy[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
        u32 left = count;
        for (u32 k = 0; k < 4 && left; ++k) {
            const u32 n = (left + (4 - k) - 1) / (4 - k);
            spawnFormation(speciesIndex, n, c + Vec2{kCx[k] * kCrowdCornerXWu, kCy[k] * kCrowdCornerYWu}, landTick);
            left -= n;
        }
    }
}

void World::spendCrowd(double landTick) {
    // GAME_DESIGN 8, the crowd: count the enemies near the ships, close the gap to the population target
    // over refillBars bars with fodder formations (species.json firstSlice.crowd).
    const SpeciesTable::Crowd& c = species_->crowd;
    const Phrase& ph = phrase_;
    const float target = crowdTarget(c, ph.track, ph.cycle ? ph.cycle : 1u) * crowdScale(c, ph);
    crowdGoal_ = target;
    const Enemies& e = enemies_;
    const float near2 = c.nearWu * c.nearWu;
    u32 near = 0;
    for (u32 i = 0; i < e.count; ++i) {
        if (e.flags[i] & kFlagDead) continue;
        for (u32 p = 0; p < playerCount_; ++p) {
            if (!players_[p].alive) continue;
            if (lengthSq(Vec2{e.x[i], e.y[i]} - players_[p].pos) <= near2) {
                ++near;
                break;
            }
        }
    }
    crowdNear_ = near;
    const float gap = target - static_cast<float>(near);
    if (gap < 1.0f) return;
    u32 want = static_cast<u32>(std::ceil(gap / (c.refillBars > 1.0f ? c.refillBars : 1.0f)));
    if (want < c.formationMin && gap >= static_cast<float>(c.formationMin)) want = c.formationMin;
    for (u32 formations = 0; formations < 8 && want; ++formations) {
        if (enemies_.count >= kDirectorAliveCap) return;
        const u32 room = kDirectorAliveCap - enemies_.count;
        const i32 sp = pickSpecies(1);
        if (sp < 0) return;
        u32 n = want < c.formationMax ? want : c.formationMax;
        if (n > room) n = room;
        const u32 before = counters_.spawned;
        spawnCrowd(static_cast<u16>(sp), n, landTick);
        const u32 made = counters_.spawned - before;
        if (made == 0) return;
        want = want > made ? want - made : 0;
    }
}

void World::strayFodder() {
    // Fodder left beyond strayWu of every ship for strayBars bars dissolves without Shards (GAME_DESIGN 8);
    // the crowd replaces it near the players.
    const SpeciesTable::Crowd& c = species_->crowd;
    Enemies& e = enemies_;
    const float far2 = c.strayWu * c.strayWu;
    for (u32 i = 0; i < e.count; ++i) {
        if (e.flags[i] & kFlagDead) continue;
        const Species& s = species_->get(e.species[i]);
        if (s.hue != 0) continue;
        bool near = false;
        for (u32 p = 0; p < playerCount_ && !near; ++p)
            near = players_[p].alive && lengthSq(Vec2{e.x[i], e.y[i]} - players_[p].pos) <= far2;
        if (near) {
            e.far[i] = 0;
            continue;
        }
        if (e.far[i] < 255) ++e.far[i];
        if (e.far[i] < c.strayBars) continue;
        e.flags[i] |= kFlagDead;
        Event ev;
        ev.type = EventType::Kill;
        ev.species = e.species[i];
        ev.a = s.killSize();
        ev.b = 0xFF;
        ev.c = 1;  // dissolved, no Shards
        ev.x = e.x[i];
        ev.y = e.y[i];
        emit(ev);
    }
}

void World::megaWave(double landTick) {
    // GAME_DESIGN 4.1: the Drop's mega-wave lands on its downbeat (it warps in over the Build's last two
    // beats): kMegaWaveBars bars of the Drop's budget at once, as formations.
    DirectorState& d = director_;
    const Phrase next = phraseAt(d, barOf(landTick + 1.0));
    const float base = baseBudget(species_->budget, species_->budgetCycles, next.track, next.cycle);
    float wave = kMegaWaveBars * base * kDropScaleStart * d.assist;
    for (u32 formations = 0; formations < 16 && wave > 0.0f; ++formations) {
        if (enemies_.count >= kDirectorAliveCap) return;
        const i32 sp = pickSpecies(2);  // threats (the crowd brings the Drop's fodder)
        if (sp < 0) return;
        const Species& s = species_->get(static_cast<u16>(sp));
        const FamilyData& fd = species_->family(s.family);
        u32 minCount = 1, maxCount = packMax(s.family) * 2;
        if (s.family == Family::Shoal) {
            minCount = fd.groupMin;
            maxCount = fd.groupMax < kShoalGroupCapStageA ? fd.groupMax : kShoalGroupCapStageA;
        }
        if (wave + 1e-4f < s.threat * static_cast<float>(minCount)) continue;
        u32 count = static_cast<u32>((wave + 1e-4f) / s.threat);
        if (count > maxCount) count = maxCount;
        if (count < minCount) count = minCount;
        const u32 before = counters_.spawned;
        spawnShape(static_cast<u16>(sp), count, landTick);
        const u32 made = counters_.spawned - before;
        if (made == 0) return;
        wave -= s.threat * static_cast<float>(made);
    }
}

void World::spotlight(double landTick, const Phrase& next) {
    // GAME_DESIGN 8: each debut gets a spotlight at the end of a Break, a small group alone with its name
    // on screen, before it joins the pool. It warps in over the Break's last two beats.
    if (next.track >= 3) return;  // the loop has no debuts
    const PlayerState& p0 = players_[0];
    float side = p0.pos.x > 0.0f ? -1.0f : 1.0f;
    for (u32 k = 0; k < species_->rosterCount; ++k) {
        const SpeciesTable::RosterEntry& r = species_->roster[k];
        if (r.track != next.track || r.cycle != next.cycle) continue;
        const Species& s = species_->get(r.species);
        const Vec2 at = p0.pos + Vec2{side * kSpotlightWu, -60.0f * side};
        spawnFormation(r.species, spotlightCount(s.family, species_->family(s.family).groupMin), at, landTick);
        Event ev;
        ev.type = EventType::Spotlight;
        ev.species = r.species;
        ev.x = at.x;
        ev.y = at.y;
        ev.tick = landTick;
        emit(ev);
        side = -side;
    }
}

void World::dissolveFodder(float progress) {
    // GAME_DESIGN 4.1: in the Break's first bar the fodder dissolves into Shards, one by one.
    Enemies& e = enemies_;
    for (u32 i = 0; i < e.count; ++i) {
        if (e.flags[i] & kFlagDead) continue;
        const Species& s = species_->get(e.species[i]);
        if (s.hue != 0) continue;
        const float when = static_cast<float>(hash32(e.serial[i], 0x5EEDu) % 1000u) / 1000.0f;
        if (when > progress) continue;
        e.flags[i] |= kFlagDead;
        Event ev;
        ev.type = EventType::Kill;
        ev.species = e.species[i];
        ev.a = s.killSize();
        ev.b = 0xFF;
        ev.c = e.state[i] == kWarping ? 1 : 2;  // 2: dissolved into Shards
        ev.x = e.x[i];
        ev.y = e.y[i];
        ev.f = e.heading[i];
        emit(ev);
        if (e.state[i] != kWarping) dropShards(i, 0xFF);
    }
}

void World::directorBar(double barTick, u32 bar) {
    DirectorState& d = director_;
    if (d.lastBar != kNoBar && bar <= d.lastBar) return;
    d.lastBar = bar;
    if (d.changeBar != kNoBar && bar >= d.changeBar + kTrackChangeBars) {
        // The Track-change Break is over: the new Track's first Groove.
        d.track = static_cast<u8>(d.track + 1);
        d.trackBar = d.changeBar + kTrackChangeBars;
        d.changeBar = kNoBar;
        d.bossSpawned = false;
    }
    phrase_ = phraseAt(d, bar);
    const Phrase& ph = phrase_;
    bool anyAlive = false;
    for (u32 p = 0; p < playerCount_; ++p) anyAlive = anyAlive || players_[p].alive;

    if (ph.sectionBar == 0) {
        Event ev;
        ev.type = EventType::Section;
        ev.a = static_cast<u8>(ph.section);
        ev.b = static_cast<u8>(ph.sectionBars);
        ev.c = ph.track;
        ev.f = ph.trackChange ? 1.0f : 0.0f;
        ev.tick = barTick;
        emit(ev);
        switch (ph.section) {
            case Section::Groove:
                // GAME_DESIGN 8, assist: two clean cycles raise the budget 10% (a hit lowers it at once).
                if (bar > 0) {
                    if (!d.hitThisCycle) {
                        if (++d.cleanCycles >= 2) {
                            d.assist = std::fmin(d.assist + kAssistStep, kAssistMax);
                            d.cleanCycles = 0;
                        }
                    } else {
                        d.cleanCycles = 0;
                    }
                }
                d.hitThisCycle = false;
                break;
            case Section::Drop:
                if (ph.bossDrop && !d.bossSpawned && spawner_.mode == Spawner::Mode::Slice && anyAlive) {
                    // The boss enters on the last Drop's downbeat, toward the arena's centre from the
                    // nearest player (kaleidos.md).
                    const Vec2 pl = players_[0].pos;
                    const Vec2 toCentre = normalizeOr(Vec2{} - pl, Vec2{1.0f, 0.0f});
                    spawnBossNow(barTick, pl + toCentre * kBossEnterWu);
                }
                break;
            case Section::Break:
                if (anyAlive && (spawner_.mode == Spawner::Mode::Slice || spawner_.trios)) {
                    // The Trio (GAME_DESIGN 6.1); after a boss, its bonus Trio first, each with half the Break.
                    const double end = tickOfBar(bar + ph.sectionBars);
                    for (u32 p = 0; p < playerCount_; ++p) {
                        if (!players_[p].alive) continue;
                        trio_[p].queued = ph.trackChange ? 1 : 0;
                        openTrio(p, ph.trackChange ? tickOfBar(bar + ph.sectionBars / 2) : end);
                    }
                }
                d.bank = 0.0f;
                d.pending = -1;
                break;
            default: break;
        }
    }
    if (spawner_.mode != Spawner::Mode::Slice || !anyAlive) return;
    const double land = barTick + kWarpBeats * kTicksPerBeat;
    strayFodder();
    spendCrowd(land);
    // The threats: the budget by Track, cycle and section (none during the boss's Drop: kaleidos.md).
    const float base = baseBudget(species_->budget, species_->budgetCycles, ph.track, ph.cycle ? ph.cycle : 1u);
    d.budgetPerBar = ph.bossDrop ? 0.0f : base * sectionScale(ph) * d.assist;
    if (d.budgetPerBar <= 0.0f) return;
    d.bank = std::fmin(d.bank + d.budgetPerBar, d.budgetPerBar * kBankBars);
    spendBudget(land);
}

void World::directorBeat(double beatTick) {
    // The third beat of a bar: what lands on the next downbeat warps in now (R4: two beats).
    const u32 bar = barOf(beatTick + 1.0);
    const double inBar = beatTick - tickOfBar(bar);
    if (std::fabs(inBar - 2.0 * kTicksPerBeat) > 1.0) return;
    const Phrase cur = phraseAt(director_, bar);
    const Phrase next = phraseAt(director_, bar + 1);
    const double land = tickOfBar(bar + 1);
    if (spawner_.mode != Spawner::Mode::Slice) return;
    if (cur.section == Section::Build && next.section == Section::Drop && !next.bossDrop) megaWave(land);
    if (cur.section == Section::Break && next.section == Section::Groove) spotlight(land, next);
}

void World::runDirector() {
    DirectorState& d = director_;
    if (d.lastBar == kNoBar) directorBar(d.startTick, 0);
    forCrossings(m0_, m1_, kTicksPerBar, [&](double barTick, i64) {
        const double rel = (barTick - d.startTick) / kTicksPerBar;
        if (rel < 0.5) return;
        directorBar(barTick, static_cast<u32>(rel + 0.5));
    });
    bool anyAlive = false;
    for (u32 p = 0; p < playerCount_; ++p) anyAlive = anyAlive || players_[p].alive;
    if (!anyAlive) return;
    forCrossings(m0_, m1_, kTicksPerBeat, [&](double beatTick, i64) { directorBeat(beatTick); });
    const Phrase& ph = phrase_;
    // Surge: score x2 and magnet x2 for the first half of the Drop (GAME_DESIGN 4.1).
    rules_.scoreMultiplier = ph.surge() ? 2.0f : 1.0f;
    rules_.magnetMultiplier = ph.surge() ? 2.0f : 1.0f;
    if (ph.section == Section::Break && ph.sectionBar == 0)
        dissolveFodder(static_cast<float>((m1_ - tickOfBar(ph.bar)) / kTicksPerBar));
    // Boosts: one is due every 20-30 s of play outside the Breaks; the next kill drops it.
    if (spawner_.mode == Spawner::Mode::Slice && ph.section != Section::Break && !d.boostDue && --d.boostTicks <= 0)
        d.boostDue = true;
}

// ---------------------------------------------------------------------------------------------- stress

void World::runStress() {
    // Hold kStressEnemies around the ship (about 700 in view) with kStressSentries of them Sentries firing
    // (about 1,500 bullets alive). Refilled on every beat; enemies left far behind are recycled.
    Spawner& sp = spawner_;
    const PlayerState& p0 = players_[0];
    const double land = kWarpBeats * kTicksPerBeat;
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
}

}  // namespace rf::sim
