// The director (GAME_DESIGN 4, 8; M3 stage B) and the stress crowd. The director walks the phrase cycle
// bar by bar (sim/director.h), spends the threat budget on formations, lands the Drop's mega-wave on the
// downbeat, spotlights debuts at the end of a Break, dissolves fodder at the start of one, opens the
// Trios, brings the boss in on the last Drop and drops a Boost roughly every 20-30 s.

#include <cmath>

#include "sim/boss_prose.h"
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
        // M5 part 2 (chosen: docs/PROGRESS.md): Flankers come from several sides, Moths make a ring.
        case Family::Flanker: return 4;
        case Family::Skitter: return 3;
        case Family::Moth: return 6;
        case Family::Blink: return 2;
        // M5 part 4 (chosen): a Tether pack is pairs; Weavers come in threes, Mimics in twos; a Wyrm and a Well alone.
        case Family::Tether: return 2;
        case Family::Weaver: return 3;
        case Family::Mimic: return 2;
        default: return 1;  // Sentry, Brood, Wyrm, Well
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
        case Family::Flanker: return 90.0f;
        case Family::Skitter: return 60.0f;
        case Family::Moth: return 120.0f;
        case Family::Blink: return 80.0f;
        case Family::Tether: return 220.0f;
        case Family::Weaver: return 120.0f;
        case Family::Mimic: return 160.0f;
        default: return 0.0f;
    }
}
// A debut's spotlight group (GAME_DESIGN 8: "a small group alone").
u32 spotlightCount(Family f, u32 groupMin) {
    switch (f) {
        case Family::Shoal: return groupMin;
        case Family::Sentry:
        case Family::Brood: return 1;
        case Family::Blink:
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

Vec2 World::playersCentre() const {
    // The centre of the players standing (M5: the Pen, Longwave's path, the Choir's Round are round it).
    Vec2 sum{};
    u32 n = 0;
    for (u32 p = 0; p < playerCount_; ++p)
        if (players_[p].alive) {
            sum = sum + players_[p].pos;
            ++n;
        }
    return n ? sum * (1.0f / static_cast<float>(n)) : players_[0].pos;
}

Vec2 World::spawnAnchor(Vec2 player) {
    // On a band around the player, squashed to the 20:9 screen, never within 260 wu of the player and
    // never inside the central third of the view (GAME_DESIGN 8 fairness; 450 wu is outside it).
    Vec2 best = player + Vec2{kSpawnFarWu, 0.0f};
    for (u32 tries = 0; tries < 12; ++tries) {
        const float a = rng_.range(0.0f, kTau);
        const float rx = rng_.range(kSpawnNearWu, kSpawnFarWu);
        Vec2 p = player + Vec2{std::cos(a) * rx, std::sin(a) * rx * kSpawnSquash};
        p.x = clampf(p.x, -halfW() + 60.0f, halfW() - 60.0f);
        p.y = clampf(p.y, -halfH() + 60.0f, halfH() - 60.0f);
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
    // screens.md "Transitions (M6)", the drop-in's grace: in a run's first two bars nothing hostile comes within 600 wu of a ship.
    const float clear = m1_ < director_.startTick + static_cast<double>(kGraceBars) * kTicksPerBar ? kGraceClearWu : kSpawnMinPlayerWu;
    for (u32 k = 0; k < count; ++k) {
        Vec2 p = anchor;
        if (spread > 0.0f) {
            const float a = rng_.range(0.0f, kTau), r = spread * std::sqrt(rng_.uniform());
            p = p + dirOf(a) * r;
        }
        float heading = 0.0f;
        if (coop()) {
            // M4: away from every ship and out of every player's Trio ring; facing the nearest ship.
            i32 near = -1;
            float nearD = 0.0f;
            for (u32 q = 0; q < playerCount_; ++q) {
                const PlayerState& pl = players_[q];
                if (!pl.alive) continue;
                const Vec2 d = p - pl.pos;
                if (lengthSq(d) < clear * clear) p = pl.pos + normalizeOr(d, Vec2{1.0f, 0.0f}) * clear;
                const TrioState& tr = trio_[q];
                if (tr.open) {
                    const Vec2 dt = p - tr.centre;
                    const float keep = kTrioRingWu + 80.0f;
                    if (lengthSq(dt) < keep * keep) p = tr.centre + normalizeOr(dt, Vec2{1.0f, 0.0f}) * keep;
                }
                const float dd = lengthSq(pl.pos - p);
                if (near < 0 || dd < nearD) {
                    near = static_cast<i32>(q);
                    nearD = dd;
                }
            }
            heading = angleOf((near >= 0 ? players_[near].pos : p0.pos) - p);
        } else {
            // Never on top of a player, never inside the Trio's ring (screens.md).
            const Vec2 d = p - p0.pos;
            if (p0.alive && lengthSq(d) < clear * clear) p = p0.pos + normalizeOr(d, Vec2{1.0f, 0.0f}) * clear;
            const TrioState& tr = trio_[0];
            if (tr.open) {
                const Vec2 dt = p - tr.centre;
                const float keep = kTrioRingWu + 80.0f;
                if (lengthSq(dt) < keep * keep) p = tr.centre + normalizeOr(dt, Vec2{1.0f, 0.0f}) * keep;
            }
            heading = angleOf(p0.pos - p);
        }
        if (s.family == Family::Wyrm || s.family == Family::Tether) {
            spawnCreature(speciesIndex, p, heading, landTick);  // M5 part 4: a chain, a pair or a triad
            continue;
        }
        const i32 i = spawnEnemy(speciesIndex, p, heading, landTick);
        if (i < 0) return;  // the pool is full: stop spawning into it (TECH_SPEC 7.1)
        enemies_.group[i] = school;
    }
}

void World::spawnShape(u16 speciesIndex, u32 count, double landTick) {
    if (setup_.mode == Mode::Songline) {
        songFormation(speciesIndex, count, landTick, false);  // (M8 songline.md rule 14: along the line ahead)
        return;
    }
    // GAME_DESIGN 8 names the formations; the first slice uses five: a cluster, a ring around the player,
    // a pincer from both sides, a sweep along one edge of the view and the four corners.
    const Species& s = species_->get(speciesIndex);
    const Vec2 c = formationCentre();  // solo: player 0; co-op: the players in turn
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
    // M5: the roster runs to Track 10; in the loop (after the set list) everything that debuted in the set list's Tracks,
    // and only families with behaviour code (sim/species.h familyBuilt).
    const bool loop = inLoop(t, director_.songs);
    // M5 part 7 (GAME_DESIGN 6.6): no shooting species while the enemy bullets alive are above 70% of their pool.
    const bool poolFull = static_cast<float>(bullets_.count) > kBulletPoolShooters * static_cast<float>(Bullets::kCap);
    const bool tracers = setup_.mode == Mode::Tracers;
    const auto debuted = [&](const SpeciesTable::RosterEntry& r) {
        if (!familyBuilt(species_->get(r.species).family)) return false;
        if (tracers && !tracersBody(r.species)) return false;  // (M8 tracers.md rule 13: contact only)
        if (poolFull && species_->get(r.species).shoots()) return false;
        if (loop) return r.track <= director_.songs;
        return r.track < t || (r.track == t && r.cycle <= c);
    };
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
        if (loop) {
            // M5 part 7 (species.json: "every species comes back one tier higher for each time round"), to tier 5.
            const Species& sp = species_->get(r.species);
            const u32 rounds = director_.songs ? (t - 1u) / director_.songs : 0u;
            const u32 tier = sp.tier + rounds < 5u ? sp.tier + rounds : 5u;
            if (r.species < SpeciesTable::kFamilySpecies) cand[n] = SpeciesTable::index(sp.family, tier, sp.variant);
        }
        weight[n] = (!loop && static_cast<u32>(r.track) * 16 + r.cycle == newest) ? 2.0f : 1.0f;
        // M6 (A4 round 2): from Track 4 on the shooters are bought less often (the novice's hits outside the fights were
        // mostly the crowd's bullets).
        if (t >= kShooterEaseTrack && species_->get(cand[n]).shoots()) weight[n] *= kShooterEaseWeight;
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
        if (enemies_.count >= aliveCap()) return;
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

void World::spawnCrowd(u16 speciesIndex, u32 count, double landTick, Vec2 c, u32 framePlayer) {
    if (setup_.mode == Mode::Songline) {
        songFormation(speciesIndex, count, landTick, true);  // (M8 songline.md rule 14: along the line ahead)
        return;
    }
    // GAME_DESIGN 8 and species.json firstSlice.crowd.arrive (M3 stage C2): floods along one edge of the
    // frame, floodOutsideWu outside it; rings on an ellipse ringFrameScale x the frame; swarms
    // cornerOutsideWu outside its corners. Formations keep 260 wu from a ship and out of the Trio's ring.
    const SpeciesTable::Crowd& cr = species_->crowd;
    const float hw = kFrameHalfWWu, hh = kFrameHalfHWu;
    const float limX = halfW() - 60.0f, limY = halfH() - 60.0f;
    const auto inArena = [&](Vec2 p) { return Vec2{clampf(p.x, -limX, limX), clampf(p.y, -limY, limY)}; };
    const u32 first = enemies_.count;  // what this call adds is arriving: counted on its way in
    const float r = rng_.uniform();
    if (r < kCrowdFloodShare) {
        // A flood: clusters of up to 8 along one edge (left, right, top, bottom), each its own distance out;
        // an edge beyond the arena's wall gives way to the opposite one.
        // The edge the ship is heading for is likelier (it flies into the flood); every edge stays possible.
        u32 edge = 0;
        {
            const Vec2 v = players_[framePlayer < kMaxPlayers ? framePlayer : 0].vel;
            const float sp = length(v);
            const Vec2 d = sp > 1.0f ? v * (1.0f / sp) : Vec2{};
            const float w[4] = {kCrowdEdgeBase + std::fmax(-d.x, 0.0f), kCrowdEdgeBase + std::fmax(d.x, 0.0f), kCrowdEdgeBase + std::fmax(-d.y, 0.0f),
                                kCrowdEdgeBase + std::fmax(d.y, 0.0f)};
            float pick = rng_.range(0.0f, w[0] + w[1] + w[2] + w[3]);
            while (edge < 3 && pick >= w[edge]) pick -= w[edge++];
        }
        const float out0 = cr.floodOutsideWu[0], out1 = cr.floodOutsideWu[1];
        const auto edgeFits = [&](u32 k) {
            switch (k) {
                case 0: return c.x - hw - out0 > -limX;
                case 1: return c.x + hw + out0 < limX;
                case 2: return c.y - hh - out0 > -limY;
                default: return c.y + hh + out0 < limY;
            }
        };
        if (!edgeFits(edge)) edge ^= 1u;
        const bool vertical = edge < 2;  // a left or right edge runs up and down
        const float len = 2.0f * (vertical ? hh : hw);
        const u32 clusters = count < 8 ? 1u : (count + 7u) / 8u;
        const float spacing = std::fmin(kCrowdFloodSpacingWu, len / static_cast<float>(clusters));
        const float span = spacing * static_cast<float>(clusters - 1);
        const float slack = 0.5f * (len - span);
        const float mid = slack > 0.0f ? rng_.range(-slack, slack) : 0.0f;
        const float sign = (edge & 1u) ? 1.0f : -1.0f;
        u32 left = count;
        for (u32 k = 0; k < clusters && left; ++k) {
            const u32 n = (left + (clusters - k) - 1) / (clusters - k);
            const float along = mid + (static_cast<float>(k) - 0.5f * static_cast<float>(clusters - 1)) * spacing;
            const float outside = rng_.range(out0, out1);
            const Vec2 at = vertical ? Vec2{c.x + sign * (hw + outside), c.y + along} : Vec2{c.x + along, c.y + sign * (hh + outside)};
            spawnFormation(speciesIndex, n, inArena(at), landTick);
            left -= n;
        }
    } else if (r < kCrowdFloodShare + kCrowdRingShare) {
        // A ring: up to 12 groups on the ellipse ringFrameScale x the frame.
        const float phase = rng_.range(0.0f, kTau);
        const u32 groups = count < 12 ? count : 12u;
        const float rx = cr.ringFrameScale * hw, ry = cr.ringFrameScale * hh;
        u32 left = count;
        for (u32 k = 0; k < groups && left; ++k) {
            const u32 n = (left + (groups - k) - 1) / (groups - k);
            const float a = phase + kTau * static_cast<float>(k) / static_cast<float>(groups);
            spawnFormation(speciesIndex, n, inArena(c + Vec2{std::cos(a) * rx, std::sin(a) * ry}), landTick);
            left -= n;
        }
    } else {
        // Corner swarms, cornerOutsideWu outside each corner of the frame.
        static const float kCx[4] = {-1.0f, 1.0f, -1.0f, 1.0f}, kCy[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
        const float ox = hw + cr.cornerOutsideWu, oy = hh + cr.cornerOutsideWu;
        u32 left = count;
        for (u32 k = 0; k < 4 && left; ++k) {
            const u32 n = (left + (4 - k) - 1) / (4 - k);
            spawnFormation(speciesIndex, n, inArena(c + Vec2{kCx[k] * ox, kCy[k] * oy}), landTick);
            left -= n;
        }
    }
    for (u32 i = first; i < enemies_.count; ++i) enemies_.flags[i] |= kFlagArriving;
}

u32 World::crowdCount(Vec2 c) const {
    // In and just around the frame (count.frameScale each way), and the crowd's fodder still on its way in
    // (it lands outside the frame; counting it keeps the next bar from ordering it twice). M8: on the Stage, all of it (one room).
    if (stage_) {
        u32 n = 0;
        for (u32 i = 0; i < enemies_.count; ++i)
            if (!(enemies_.flags[i] & kFlagDead)) ++n;
        return n;
    }
    if (setup_.mode == Mode::Songline) {
        // (M8 SONGLINE: the crowd lands along the line ahead, out of the frame, and what the line leaves behind dissolves: every body
        // alive is the frame's)
        (void)c;
        u32 n = 0;
        for (u32 i = 0; i < enemies_.count; ++i)
            if (!(enemies_.flags[i] & kFlagDead)) ++n;
        return n;
    }
    const SpeciesTable::Crowd& cr = species_->crowd;
    const float cx = kFrameHalfWWu * cr.countFrameScale, cy = kFrameHalfHWu * cr.countFrameScale;
    const float sx = kFrameHalfWWu * cr.strayFrameScale, sy = kFrameHalfHWu * cr.strayFrameScale;
    const Enemies& e = enemies_;
    u32 n = 0;
    for (u32 i = 0; i < e.count; ++i) {
        if (e.flags[i] & kFlagDead) continue;
        const float dx = std::fabs(e.x[i] - c.x), dy = std::fabs(e.y[i] - c.y);
        if ((dx <= cx && dy <= cy) || ((e.flags[i] & kFlagArriving) && dx <= sx && dy <= sy)) ++n;
    }
    return n;
}

void World::spendCrowd(double landTick, const Phrase& ph) {
    // GAME_DESIGN 8, the crowd (M3 stage C2): count the enemies in and just around each player's frame (and
    // the crowd's fodder still warping in), order the neediest frame's whole gap as fodder formations at its
    // edges (species.json firstSlice.crowd; its refillBars 2 held the frame near half its target in view with
    // the bot killing 550 a minute), plus the kills not yet replaced on a beat and a running debt. The target
    // holds for every frame.
    const SpeciesTable::Crowd& c = species_->crowd;
    const u32 lost = director_.crowdLost;
    director_.crowdLost = 0;
    float target = crowdTarget(c, ph.track, ph.cycle ? ph.cycle : 1u, director_.songs) * crowdScale(c, ph);
    if (director_.actForm) target *= 0.5f;  // M8 HEADLINERS: the Build's fodder at half the Track's crowd (modes.json headliners)
    if (director_.formCount && ph.wave && ph.section == Section::Drop && setup_.mode == Mode::FullBlast && species_) {
        // M8 FULL BLAST (full_blast.md "Numbers"): the wave's crowd, from its first scale to its last over the Drop, of the Track's
        // cycle-2 target (the Drop's own section scale left out), never above 400.
        const ModesData::Blast::Wave& wv = species_->modes.blast.wave[(ph.wave - 1u) % 4u];
        const float u = ph.sectionBars ? static_cast<float>(ph.sectionBar) / static_cast<float>(ph.sectionBars) : 0.0f;
        const float base = crowdTarget(c, ph.track, 2u, director_.songs);
        target = std::fmin(base * (wv.crowd[0] + (wv.crowd[1] - wv.crowd[0]) * u), 400.0f);
    }
    if (pen_.up && boss_.active && isMiniBoss(boss_.kind) && c.miniDrop > 0.0f) {
        // minibosses.json common.crowd (the lead, 2026-11-13): in a Pen fight the crowd is penScale of the Drop's (not
        // sectionScale's), never more than penMax bodies, so the lock line and the bounces can be followed.
        const MiniData& md = species_->minis;
        target = std::fmin(target / c.miniDrop * md.penCrowdScale, md.penCrowdMax);
    }
    if (setup_.mode == Mode::Songline) target *= species_->modes.songline.crowdScale;  // (M8 songline.md rule 14: 0.7)
    if (setup_.mode == Mode::Jam) target = jamCrowdTarget(ph);  // (M8 jam.md rule 4: twenty dancers a layer on)
    if (setup_.mode == Mode::Tracers) {
        // M8 (tracers.md rule 13, rule 11): half the Track's crowd, 15% more a house, scaled by the players as the set's budget is,
        // never above crowd.max on the Stage.
        target = std::fmin(target * tracersCrowdScale() * coopBudgetScale(), species_->modes.tracers.crowdMax);
    }
    crowdGoal_ = target;
    // Arrivals stop being on their way once they land (kCrowdArrivingBars after it) or reach a counted frame.
    {
        const float cx = kFrameHalfWWu * c.countFrameScale, cy = kFrameHalfHWu * c.countFrameScale;
        Enemies& e = enemies_;
        for (u32 i = 0; i < e.count; ++i) {
            if (!(e.flags[i] & kFlagArriving) || e.state[i] == kWarping) continue;
            bool in = m1_ - e.until[i] > kCrowdArrivingBars * kTicksPerBar;
            for (u32 p = 0; p < playerCount_ && !in; ++p)
                in = players_[p].alive && std::fabs(e.x[i] - players_[p].pos.x) <= cx && std::fabs(e.y[i] - players_[p].pos.y) <= cy;
            if (in) e.flags[i] = static_cast<u8>(e.flags[i] & ~kFlagArriving);
        }
    }
    const u32 killed = director_.crowdKills;
    director_.crowdKills = 0;
    if (coop() && !stage_) {
        // M4: the target holds for every player's frame (GAME_DESIGN 8). (M8: on the Stage every player's frame is the one room.)
        spendCrowdCoop(landTick, target, lost, killed);
        return;
    }
    i32 needy = -1;
    u32 counted = 0;
    for (u32 p = 0; p < playerCount_; ++p) {
        if (!players_[p].alive) continue;
        const u32 n = crowdCount(players_[p].pos);
        if (needy < 0 || n < counted) {
            needy = static_cast<i32>(p);
            counted = n;
        }
    }
    if (needy < 0) return;
    crowdInFrame_ = counted;
    const float gap = target - static_cast<float>(counted);
    // The bar-line count is the frame at its emptiest (the bar's kills drain it, its formations are still
    // coming in): a running debt of the shortfall keeps the frame at its target on average while the players
    // kill (C2; it never exceeds the last bar's kills, so an idle frame is not overfilled).
    director_.crowdDebt = clampf(director_.crowdDebt * 0.5f + gap * kCrowdDebtGain, 0.0f, std::fmin(0.5f * target, static_cast<float>(killed)));
    if (gap < 1.0f) return;
    u32 want = static_cast<u32>(std::ceil(gap + director_.crowdDebt)) + lost;
    if (want < c.formationMin && gap >= static_cast<float>(c.formationMin)) want = c.formationMin;
    const Vec2 centre = stage_ ? Vec2{} : players_[needy].pos;  // (M8: on the Stage the crowd comes in at its walls)
    for (u32 formations = 0; formations < 8 && want; ++formations) {
        if (enemies_.count >= aliveCap()) return;
        const u32 room = aliveCap() - enemies_.count;
        const i32 sp = pickSpecies(1);
        if (sp < 0) return;
        u32 n = want < c.formationMax ? want : c.formationMax;
        if (n > room) n = room;
        const u32 before = counters_.spawned;
        spawnCrowd(static_cast<u16>(sp), n, landTick, centre, static_cast<u32>(needy));
        const u32 made = counters_.spawned - before;
        if (made == 0) return;
        want = want > made ? want - made : 0;
    }
}

void World::strayFodder() {
    // Fodder outside every player's frame grown by stray.frameScale for stray.bars bars dissolves without
    // Shards (GAME_DESIGN 8, species.json firstSlice.crowd.stray); the crowd replaces it at the frame. (M8: nothing strays on the
    // Stage: the whole room is in view.)
    if (stage_) return;
    const SpeciesTable::Crowd& c = species_->crowd;
    Enemies& e = enemies_;
    const float sx = kFrameHalfWWu * c.strayFrameScale, sy = kFrameHalfHWu * c.strayFrameScale;
    for (u32 i = 0; i < e.count; ++i) {
        if ((e.flags[i] & kFlagDead) || e.state[i] == kLane) continue;  // (M5 part 4: Event Horizon's lanes walk in from afar)
        const Species& s = species_->get(e.species[i]);
        if (s.hue != 0) continue;
        bool near = false;
        for (u32 p = 0; p < playerCount_ && !near; ++p)
            near = players_[p].alive && std::fabs(e.x[i] - players_[p].pos.x) <= sx && std::fabs(e.y[i] - players_[p].pos.y) <= sy;
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
        if (role_ == NetRole::Host) logDeath(i, 0xFF, kDeathDissolve);
    }
}
void World::megaWave(double landTick) {
    // GAME_DESIGN 4.1: the Drop's mega-wave lands on its downbeat (it warps in over the Build's last two
    // beats): kMegaWaveBars bars of the Drop's budget at once, as formations.
    DirectorState& d = director_;
    const Phrase next = phraseAt(d, barOf(landTick + 1.0));
    const float base = baseBudget(*species_, next.track, next.cycle, d.songs);
    float wave = kMegaWaveBars * base * kDropScaleStart * d.assist * coopBudgetScale() * loopRamp(kLoopThreatRamp, next.track, d.songs);
    if (setup_.mode == Mode::Tracers) wave *= species_->modes.tracers.threatScale * tracersCrowdScale() / species_->modes.tracers.crowdScale;
    if (setup_.mode == Mode::Songline) wave *= species_->modes.songline.threatScale;
    for (u32 formations = 0; formations < 16 && wave > 0.0f; ++formations) {
        if (enemies_.count >= aliveCap()) return;
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
    if (inLoop(next.track, director_.songs)) return;  // the loop has no debuts
    const Vec2 p0 = formationCentre();  // solo: player 0
    float side = p0.x > 0.0f ? -1.0f : 1.0f;
    for (u32 k = 0; k < species_->rosterCount; ++k) {
        const SpeciesTable::RosterEntry& r = species_->roster[k];
        if (r.track != next.track || r.cycle != next.cycle) continue;
        if (setup_.mode == Mode::Tracers && !tracersBody(r.species)) continue;  // (M8 tracers.md rule 13: contact only)
        const Species& s = species_->get(r.species);
        const Vec2 at = p0 + Vec2{side * kSpotlightWu, -60.0f * side};
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
        if (role_ == NetRole::Host) logDeath(i, 0xFF, e.state[i] == kWarping ? kDeathDissolve : kDeathShards);
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
        d.miniSpawned = false;  // M5: the new Track's middle Drop brings its own mini-boss
        d.midHold = 0;
        d.midHolding = false;
    }
    phrase_ = phraseAt(d, bar);
    if (setup_.versus && role_ != NetRole::Client) versusBar(bar);  // M8: the crown and the underdog
    if (d.formCount && setup_.mode == Mode::FullBlast && phrase_.wave && species_ && !phrase_.ended) {
        // M8 FULL BLAST: a Boost orb every 4, 3, 2, then 1 bars of a wave (the next kill drops it).
        const u32 every = species_->modes.blast.wave[(phrase_.wave - 1u) % 4u].boostEvery;
        const u32 inWave = phrase_.section == Section::Drop ? phrase_.sectionBar + 4u : phrase_.sectionBar;
        if (every && inWave % every == 0) d.boostDue = true;
    }
    if (d.formCount && setup_.mode == Mode::Tracers && species_ && !phrase_.ended && bar > 0 && species_->modes.tracers.boostEveryBars &&
        bar % species_->modes.tracers.boostEveryBars == 0)
        d.boostDue = true;  // M8 TRACERS (rule 15): a Boost orb every six bars (the next kill drops it)
    if (d.actForm && phrase_.trackChange && phrase_.sectionBar == 0 && species_) {
        // M8 HEADLINERS (headliners.md rule 1): the top-up: whatever a player is short of the kit the next act's boss was tuned
        // against, landed over the Break's first two bars (the act's own stream).
        const u32 act = phrase_.track;
        for (u32 p = 0; p < playerCount_; ++p) {
            if (!players_[p].present || act < 1 || act > 10) continue;
            const u32 want = species_->modes.kit.atBoss[act - 1u], held = players_[p].loadout.taken;
            if (want > held) {
                dealKit(p, want - held, act);
                kit_[p].from = tickOfBar(bar);
            }
        }
    }
    if (d.actForm && phrase_.trackChange && phrase_.sectionBar == 2 && (spawner_.mode == Spawner::Mode::Slice || spawner_.trios)) {
        // headliners.md: the fallen boss's bonus Trio on the Break's third bar, its ring the clock for the six bars left.
        for (u32 p = 0; p < playerCount_; ++p) {
            if (!players_[p].alive || !isLocal(p) || trio_[p].open) continue;
            trio_[p].queued = 0;
            openTrio(p, tickOfBar(bar + kTrackChangeBars - 2u));
        }
    }
    const Phrase& ph = phrase_;
    bool anyAlive = false;
    for (u32 p = 0; p < playerCount_; ++p) anyAlive = anyAlive || players_[p].alive;
    // GAME_DESIGN 8 (the halo burns down): on the first downbeat of every loop Track from its kLoopHaloBurnFrom-th (M5: counted
    // from the set list's length, never from a fixed Track number; not in its
    // Track-change Break), once a Track. A client follows the host's Track the same way.
    if (ph.track >= d.songs + kLoopHaloBurnFrom && !ph.trackChange && ph.track > d.burnedTrack) {
        d.burnedTrack = ph.track;
        burnHalos();
    }

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
                if (ph.bossDrop && !d.bossSpawned && spawner_.mode == Spawner::Mode::Slice && anyAlive && role_ != NetRole::Client) {
                    // The Track's own boss enters on the last Drop's downbeat (M5: Kaleidos, Longwave, The Choir, round
                    // again in the loop). Kaleidos toward the arena's centre from the nearest player (kaleidos.md);
                    // Longwave and the Choir round the players' centre. Co-op: from the first player standing; the host's
                    // call.
                    const i32 fp = coop() ? firstAlive() : 0;
                    const Vec2 pl = players_[fp >= 0 ? fp : 0].pos;
                    const Vec2 toCentre = normalizeOr(Vec2{} - pl, Vec2{1.0f, 0.0f});
                    const BossKind kind = bossOfSong(ph.song);
                    spawnBossNow(barTick, kind == BossKind::Kaleidos ? pl + toCentre * kBossEnterWu : playersCentre(), kind);
                }
                if (ph.miniDrop && !d.miniSpawned && spawner_.mode == Spawner::Mode::Slice && anyAlive && role_ != NetRole::Client) {
                    // M5 (minibosses.md): the middle cycle's Drop from Track 2 on brings a mini-boss on its downbeat, in place
                    // of the mega-wave; a Pen fight raises its Pen round the players' centre.
                    const i32 fp = coop() ? firstAlive() : 0;
                    const Vec2 pl = players_[fp >= 0 ? fp : 0].pos;
                    const Vec2 toCentre = normalizeOr(Vec2{} - pl, Vec2{1.0f, 0.0f});
                    spawnBossNow(barTick, pl + toCentre * kBossEnterWu, pickMini(d.formCount && ph.track < 2 ? 2u : ph.track), playersCentre());
                }
                break;
            case Section::Break:
                // Play test 1 (GAME_DESIGN 5): every Break gives back one Shield cell and one Nova (up to the caps); from the
                // loop's first Track on, none (GAME_DESIGN 8: the loop is where a run ends).
                for (u32 p = 0; p < playerCount_; ++p) {
                    PlayerState& pl = players_[p];
                    if (!pl.present || !pl.alive || (inLoop(ph.track, d.songs) && !kLoopBreakRefills)) continue;
                    if (pl.cells < pl.cellCap) ++pl.cells;
                    if (pl.novas < pl.novaCap) ++pl.novas;
                    Event ev;
                    ev.type = EventType::Refill;
                    ev.a = static_cast<u8>(p);
                    ev.b = pl.cells;
                    ev.c = pl.novas;
                    ev.x = pl.pos.x;
                    ev.y = pl.pos.y;
                    emit(ev);
                }
                if (anyAlive && ph.trackChange && !d.actForm && (spawner_.mode == Spawner::Mode::Slice || spawner_.trios)) {
                    // The boss's Trio (GAME_DESIGN 6.1), the whole Track-change Break its clock. a667116 (the owner: "Remove The
                    // Break's Trio"): a cycle's Break deals none; the Trios are the Shard levels' and the bosses'.
                    // Co-op: each device opens its own players' Trios (loot is instanced; the others hear of it).
                    // Play test 2: a level-up Trio still open finishes first; the boss's then opens (takeOrb).
                    const double end = tickOfBar(bar + ph.sectionBars);
                    for (u32 p = 0; p < playerCount_; ++p) {
                        if (!players_[p].alive || !isLocal(p)) continue;
                        TrioState& tr = trio_[p];
                        if (tr.open && tr.kind == kTrioLevel) {
                            tr.breakWaiting = 1;
                            continue;
                        }
                        tr.queued = 0;
                        openTrio(p, end);
                    }
                }
                // GAME_DESIGN 10: a player down revives at the next Break (this device's own players).
                if (coop())
                    for (u32 p = 0; p < playerCount_; ++p)
                        if (players_[p].present && players_[p].down && isLocal(p)) revivePlayer(p, false);
                d.bank = 0.0f;
                d.pending = -1;
                break;
            default: break;
        }
    }
    // Play test 2 (GAME_DESIGN 6.1): Regen counts the bar; a banked Shard level opens its Trio on this bar line. Each device
    // its own players'.
    regenBar();
    if (anyAlive && (spawner_.mode == Spawner::Mode::Slice || spawner_.trios) && setup_.mode != Mode::Tracers && setup_.mode != Mode::Songline &&
        setup_.mode != Mode::Jam)
        openLevelTrios(bar);  // (M8 TRACERS: no Trio, a level lengthens the tracer; SONGLINE: a level gives a Nova and a cell)
    // M5 part 5: the arena's structures and events (the host places and starts them; every device keeps them).
    if (anyAlive) arenaBar(ph, bar, barTick);
    // M4: a client follows the host's phrase; the host's director alone spawns.
    if (role_ == NetRole::Client) return;
    if (spawner_.mode != Spawner::Mode::Slice || !anyAlive) return;
    // screens.md "Transitions (M6)", the drop-in's grace: nothing spawns in a run's first bar (the first formations come in
    // from the frame's edges on the second; spawnFormation keeps them 600 wu off for two bars).
    if (bar < kGraceQuietBars) return;
    // M8 FULL BLAST (full_blast.md "The run"): nothing hostile in the opening's four bars; the first flood warps in on bar 4.
    if (setup_.mode == Mode::FullBlast && d.formCount && bar < 3u) return;
    const double land = barTick + kWarpBeats * kTicksPerBeat;
    strayFodder();
    spendCrowd(land, ph);
    // The threats: the budget by Track, cycle and section (none during the boss's Drop: kaleidos.md); co-op x
    // (1 + 0.6 x (players - 1)) (GAME_DESIGN 8).
    const float base = baseBudget(*species_, ph.track, ph.cycle ? ph.cycle : 1u, d.songs);
    // Play test 1: the threat budget's lever (tuning::kThreatBudgetScale, 1 = the first slice's numbers); play test 2's review:
    // the loop ramps it a Track at a time (GAME_DESIGN 8).
    const bool miniFight = boss_.active && !boss_.dying && isMiniBoss(boss_.kind);  // M5: fodder only while it lives
    d.budgetPerBar = ph.bossDrop || miniFight ? 0.0f
                                 : base * sectionScale(ph) * d.assist * coopBudgetScale() * kThreatBudgetScale * loopRamp(kLoopThreatRamp, ph.track, d.songs);
    if (setup_.mode == Mode::Tracers) d.budgetPerBar *= species_->modes.tracers.threatScale * tracersCrowdScale() / species_->modes.tracers.crowdScale;  // (M8: half the hunters', 15% more a house)
    if (setup_.mode == Mode::Songline) d.budgetPerBar *= species_->modes.songline.threatScale;  // (M8 SONGLINE: 0.7)
    if (setup_.mode == Mode::Jam) d.budgetPerBar = 0.0f;  // (M8 JAM: dancers only; their elites are the room's)
    if (d.budgetPerBar <= 0.0f) return;
    d.bank = std::fmin(d.bank + d.budgetPerBar, d.budgetPerBar * kBankBars);
    eliteBar(ph, land);  // M5 part 5: an elite first, when one is due (traits.md)
    spendBudget(land);
}

void World::replaceCrowd(double beatTick) {
    // M3 stage C2: the crowd replaces its fodder as the players kill it, on the beat, a formation at a time
    // (at least formationSize's minimum, so a few kills wait for more); the bar's count closes the gap.
    const SpeciesTable::Crowd& c = species_->crowd;
    if (director_.crowdLost < c.formationMin || crowdScale(c, phrase_) <= 0.0f) return;
    i32 needy = -1;
    u32 counted = 0;
    for (u32 p = 0; p < playerCount_; ++p) {
        if (!players_[p].alive) continue;
        const u32 n = crowdCount(players_[p].pos);
        if (needy < 0 || n < counted) {
            needy = static_cast<i32>(p);
            counted = n;
        }
    }
    if (needy < 0 || enemies_.count >= aliveCap()) return;
    const i32 sp = pickSpecies(1);
    if (sp < 0) return;
    u32 n = director_.crowdLost < c.formationMax ? director_.crowdLost : c.formationMax;
    const u32 room = aliveCap() - enemies_.count;
    if (n > room) n = room;
    const u32 before = counters_.spawned;
    spawnCrowd(static_cast<u16>(sp), n, beatTick + kWarpBeats * kTicksPerBeat, stage_ ? Vec2{} : players_[needy].pos, static_cast<u32>(needy));
    const u32 made = counters_.spawned - before;
    director_.crowdLost = director_.crowdLost > made ? director_.crowdLost - made : 0u;
}

void World::directorBeat(double beatTick) {
    if (spawner_.mode == Spawner::Mode::Slice) replaceCrowd(beatTick);
    const u32 bar = barOf(beatTick + 1.0);
    const double inBar = beatTick - tickOfBar(bar);
    if (spawner_.mode != Spawner::Mode::Slice) return;
    const Phrase cur = phraseAt(director_, bar);
    const Phrase next = phraseAt(director_, bar + 1);
    // M3 stage C2: the Groove's crowd lands on the third beat of the Break's last bar (ordered on its first),
    // so it has come into the frame by the downbeat; a Groove does not begin empty.
    if (std::fabs(inBar) < 1.0 && cur.section == Section::Break && next.section == Section::Groove)
        spendCrowd(beatTick + kWarpBeats * kTicksPerBeat, next);
    // The third beat of a bar: what lands on the next downbeat warps in now (R4: two beats).
    if (std::fabs(inBar - 2.0 * kTicksPerBeat) > 1.0) return;
    const double land = tickOfBar(bar + 1);
    if (cur.section == Section::Build && next.section == Section::Drop && !next.bossDrop) {
        if (!next.miniDrop) megaWave(land);  // M5: a mini-boss comes in place of the mega-wave
        spendCrowd(land, next);  // C2: the Drop's crowd lands on its downbeat, with the mega-wave
    }
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
    const bool client = role_ == NetRole::Client;  // M4: the host spawns, dissolves and drops; a client follows
    if (!client) forCrossings(m0_, m1_, kTicksPerBeat, [&](double beatTick, i64) { directorBeat(beatTick); });
    // M5 part 5: the arena's beat (Bumpers, Pylons, Meteor Rain, the Storm Cell, the Comet's Shards) on every device.
    forCrossings(m0_, m1_, kTicksPerBeat, [&](double beatTick, i64) { arenaBeat(beatTick); });
    const Phrase& ph = phrase_;
    // Surge: score x2 and magnet x2 for the first half of the Drop (GAME_DESIGN 4.1); M5 part 5: an Eclipse's kills x 2.
    rules_.scoreMultiplier = (ph.surge() ? 2.0f : 1.0f) * (eclipse() ? prose::kArEclipseScore : 1.0f);
    rules_.magnetMultiplier = ph.surge() ? 2.0f : 1.0f;
    if (spawner_.mode == Spawner::Mode::Slice) cellClock();  // play test 2: each device its own players' Cells
    if (client) return;
    if (ph.section == Section::Break && ph.sectionBar == 0 && setup_.mode != Mode::Tracers)  // (M8 TRACERS: what is left drifts)
        dissolveFodder(static_cast<float>((m1_ - tickOfBar(ph.bar)) / kTicksPerBar));
    // Boosts: one is due every 20-30 s of play outside the Breaks; the next kill drops it. (M8 FULL BLAST: on the waves' bar lines.)
    if (spawner_.mode == Spawner::Mode::Slice && ph.section != Section::Break && !d.boostDue && !d.formCount && --d.boostTicks <= 0)
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
