// M5 part 4 (GAME_DESIGN 7.2, design/species.json families and variants): the families Tracks 4 to 6 bring, Wyrms,
// Tethers, Weavers, Wells and Mimics, with their B and C variants. As in part 2: behaviour per family and attack, never per
// species; the host makes every enemy-made spawn; what moves bodies is computed alike on every device.
//
// A creature of several bodies (a Wyrm's chain, a Tether's pair or triad) is spawned in one go: its bodies share a `group`
// and their serials run on from the first (a chain's head, a pair's first node), so every device finds the same order.
//
// Per-enemy state (Enemies a, b, until, step, state):
//   Wyrm    a = 1 for a head; step = its place in the chain as spawned; until = (C) when a cut-off half grows its head.
//   Tether  step = the nodes the creature was spawned with (2, or 3 for a triad); k (its place) is found by serial.
//   Weaver  a = its turn's sense; until = the next point of its trail; its trail is a World trail (owner = its handle).
//   Well    a = Shots swallowed since the last bar line (B), b = its scale (C grows it); step = its feed toward the next growth.
//   Mimic   a, b = the centre it mirrors the player through; step = the player's volleys it has seen (B); until = (C) the
//           Dash it last copied; kDash while it copies one (heading = its direction).

#include <cmath>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

// ---------------------------------------------------------------------------------------------- groups

void World::buildGroups() {
    // The bodies of each multi-body creature this tick, in serial order (a small open-addressed table).
    groupCount_ = 0;
    for (u32 k = 0; k < kGroupSlots; ++k) groupSlot_[k] = 0xFFFF;
    const Enemies& e = enemies_;
    for (u32 i = 0; i < e.count; ++i) {
        if (!e.group[i] || (e.flags[i] & kFlagDead)) continue;
        const Family f = species_->get(e.species[i]).family;
        if (f != Family::Wyrm && f != Family::Tether) continue;
        u32 h = (e.group[i] * 40503u) & (kGroupSlots - 1);
        while (groupSlot_[h] != 0xFFFF && groups_[groupSlot_[h]].id != e.group[i]) h = (h + 1) & (kGroupSlots - 1);
        if (groupSlot_[h] == 0xFFFF) {
            if (groupCount_ >= kMaxGroups) continue;
            groupSlot_[h] = static_cast<u16>(groupCount_);
            Group& g = groups_[groupCount_++];
            g.id = e.group[i];
            g.n = 0;
        }
        Group& g = groups_[groupSlot_[h]];
        if (g.n >= Group::kCap) continue;
        // Insert in serial order.
        u32 k = g.n++;
        while (k > 0 && e.serial[g.idx[k - 1]] > e.serial[i]) {
            g.idx[k] = g.idx[k - 1];
            --k;
        }
        g.idx[k] = static_cast<u16>(i);
    }
    // Each creature's centre as the tick begins (a Tether's members all turn round the same point).
    for (u32 k = 0; k < groupCount_; ++k) {
        Group& g = groups_[k];
        Vec2 c{};
        for (u32 j = 0; j < g.n; ++j) c = c + Vec2{e.x[g.idx[j]], e.y[g.idx[j]]};
        g.centre = g.n ? c * (1.0f / static_cast<float>(g.n)) : Vec2{};
    }
}

const World::Group* World::groupOf(u16 id) const {
    if (!id) return nullptr;
    u32 h = (id * 40503u) & (kGroupSlots - 1);
    while (groupSlot_[h] != 0xFFFF) {
        if (groups_[groupSlot_[h]].id == id) return &groups_[groupSlot_[h]];
        h = (h + 1) & (kGroupSlots - 1);
    }
    return nullptr;
}

void World::spawnCreature(u16 speciesIndex, Vec2 p, float heading, double landTick) {
    // A Wyrm's chain (head first, the body behind it) or a Tether's pair or triad round p, as one group.
    const Species& s = species_->get(speciesIndex);
    const FamilyData& fd = species_->family(s.family);
    u16 id = static_cast<u16>(spawner_.nextSchool++);
    if (!id) id = static_cast<u16>(spawner_.nextSchool++);
    if (s.family == Family::Wyrm) {
        const u32 n = fd.segmentsMin + (fd.segmentsMax > fd.segmentsMin ? rng_.below(fd.segmentsMax - fd.segmentsMin + 1) : 0u);
        const Vec2 back = dirOf(heading) * -fd.segmentSpacing;
        for (u32 k = 0; k < n; ++k) {
            const i32 i = spawnEnemy(speciesIndex, p + back * static_cast<float>(k), heading, landTick);
            if (i < 0) return;
            enemies_.group[i] = id;
            enemies_.step[i] = static_cast<u8>(k);
            enemies_.a[i] = k == 0 ? 1.0f : 0.0f;
        }
        return;
    }
    const u32 n = (s.variant == kVariantC && fd.v.triad) ? 3u : 2u;
    const float R = n == 3 ? fd.v.triadSideWu / std::sqrt(3.0f) : 0.5f * fd.pairDistance;
    for (u32 k = 0; k < n; ++k) {
        const Vec2 at = p + dirOf(heading + kTau * static_cast<float>(k) / static_cast<float>(n)) * R;
        const i32 i = spawnEnemy(speciesIndex, at, heading, landTick);
        if (i < 0) return;
        enemies_.group[i] = id;
        enemies_.step[i] = static_cast<u8>(n);
    }
}

// ---------------------------------------------------------------------------------------------- landing

void World::landFamily4(u32 i, const Species& s, const FamilyData& fd, Vec2 to) {
    Enemies& e = enemies_;
    const i32 tp = nearestPlayer(e.x[i], e.y[i]);
    const Vec2 P = tp >= 0 ? players_[tp].pos : Vec2{e.x[i], e.y[i]};
    switch (s.family) {
        case Family::Weaver:
            e.a[i] = rand01(e.rng[i]) < 0.5f ? 1.0f : -1.0f;
            e.heading[i] = angleOf(to) + e.a[i] * 0.6f;
            e.until[i] = m1_;
            break;
        case Family::Well:
            e.a[i] = 0.0f;
            e.b[i] = 1.0f;
            e.step[i] = 0;
            break;
        case Family::Mimic:
            // The centre it mirrors the player through: halfway between them as it lands.
            e.a[i] = 0.5f * (e.x[i] + P.x);
            e.b[i] = 0.5f * (e.y[i] + P.y);
            e.step[i] = tp >= 0 ? static_cast<u8>(players_[tp].shotsFired) : 0;
            e.until[i] = tp >= 0 ? players_[tp].lastDashTick : -1.0;
            break;
        default: break;
    }
    (void)fd;
}

// ---------------------------------------------------------------------------------------------- motion

void World::moveFamily4(u32 i, const Species& s, const FamilyData& fd, float dt, i32 tp, Vec2 to, bool engaged) {
    Enemies& e = enemies_;
    const FamilyData::Variants& v = fd.v;
    const float bulletSpeed = species_->bulletSpeed;
    const auto shotEvent = [&](Vec2 at) {
        Event ev;
        ev.type = EventType::EnemyShot;
        ev.species = e.species[i];
        ev.x = at.x;
        ev.y = at.y;
        emit(ev);
    };
    switch (s.family) {
        case Family::Wyrm: {
            // GAME_DESIGN 7.2: a head leads a chain of segments. Each segment follows the one spawned before it (spacing apart);
            // one whose leader is gone closes up to the next alive (A, B) or, cut off, grows a head in two beats (C).
            const Group* g = groupOf(e.group[i]);
            i32 lead = -1;
            bool cut = false;
            if (g) {
                for (u32 k = 0; k < g->n; ++k) {
                    const u32 j = g->idx[k];
                    if (j == i) break;
                    if (e.serial[j] < e.serial[i]) lead = static_cast<i32>(j);
                }
                // (cut: the segment spawned just before this one is gone)
                cut = lead < 0 ? e.step[i] > 0 : e.serial[lead] + 1u != e.serial[i];
            }
            if (s.variant == kVariantC && cut && e.a[i] < 0.5f) {
                // C: severed, it waits for its new head to grow, then leads its half.
                if (e.until[i] <= 0.0) e.until[i] = m1_ + v.regrowHeadBeats * kTicksPerBeat;
                if (m1_ >= e.until[i]) {
                    e.a[i] = 1.0f;
                    lead = -1;
                } else {
                    e.vx[i] = e.vy[i] = 0.0f;
                    break;
                }
            } else if (s.variant != kVariantC && lead < 0) {
                e.a[i] = 1.0f;  // A, B: the front of what is left leads
            }
            if (s.variant == kVariantC && e.a[i] > 0.5f) lead = -1;  // a grown head leads its own half
            if (lead < 0) {
                // The head: toward its player at turnRate, weaving across its line (waveAmplitude, wavePeriodBeats).
                if (tp >= 0) e.heading[i] = turnToward(e.heading[i], angleOf(to), fd.turnRate * dt);
                const float w = kTau / (fd.wavePeriodBeats * 60.0f / std::fmax(bpm_, 30.0f));
                const float ph = static_cast<float>(std::fmod(m1_ / (fd.wavePeriodBeats * kTicksPerBeat), 1.0)) * kTau + static_cast<float>(e.group[i]);
                const Vec2 f = dirOf(e.heading[i]) * s.speed + perpOf(dirOf(e.heading[i])) * (fd.waveAmplitude * w * std::cos(ph));
                e.vx[i] = f.x;
                e.vy[i] = f.y;
            } else {
                // A segment: kept spacing behind its leader (the leader's place this tick, it moved first or not at all).
                const Vec2 L{e.x[lead], e.y[lead]};
                const Vec2 d = Vec2{e.x[i], e.y[i]} - L;
                const float dist = length(d);
                Vec2 want = Vec2{e.x[i], e.y[i]};
                if (dist > fd.segmentSpacing) want = L + d * (fd.segmentSpacing / dist);
                const Vec2 vel = (want - Vec2{e.x[i], e.y[i]}) * (1.0f / std::fmax(dt, 1e-4f));
                e.vx[i] = vel.x / std::fmax(loopSpeed_, 1e-3f);  // (the integration multiplies own motion by the loop's ramp)
                e.vy[i] = vel.y / std::fmax(loopSpeed_, 1e-3f);
                if (dist > 1e-3f) e.heading[i] = angleOf(L - Vec2{e.x[i], e.y[i]});
            }
            // B: every third segment fires sideways, both ways, every sideShotEverySteps sixteenths (on the beat, told a beat
            // before by its head).
            if (s.variant == kVariantB && engaged && v.sideShotEverySteps) {
                const double every = static_cast<double>(v.sideShotEverySteps) * kTicksPerStep;
                if (e.a[i] > 0.5f)
                    forCrossings(m0_ + kTicksPerBeat, m1_ + kTicksPerBeat, every, [&](double t, i64) {
                        Event ev;
                        ev.type = EventType::Telegraph;
                        ev.species = e.species[i];
                        ev.x = e.x[i];
                        ev.y = e.y[i];
                        ev.tick = t;
                        emit(ev);
                    });
                if (v.fromEveryNthSegment && e.step[i] % v.fromEveryNthSegment == 0)
                    forCrossings(m0_, m1_, every, [&](double t, i64) {
                        const Vec2 side = perpOf(dirOf(e.heading[i]));
                        const float adv = tickAgeOf(t);
                        spawnBullet(Vec2{e.x[i], e.y[i]}, side * bulletSpeed, adv);
                        spawnBullet(Vec2{e.x[i], e.y[i]}, side * -bulletSpeed, adv);
                        shotEvent(Vec2{e.x[i], e.y[i]});
                    });
            }
            break;
        }
        case Family::Tether: {
            // A pair (or C: a triad) round a centre that comes for its player, turning orbitTurnsPerBar (B: twice) as they
            // go; a lethal beam joins them (tetherBeams). With one gone, the beam goes and the rest wander slowly as Drifters.
            const Group* g = groupOf(e.group[i]);
            const u32 n = e.step[i] ? e.step[i] : 2u;
            if (!g || g->n < n) {
                if (rand01(e.rng[i]) < dt * 1.5f) e.heading[i] += randRange(e.rng[i], -1.2f, 1.2f);
                const Vec2 vel = dirOf(e.heading[i]) * (0.6f * s.speed);
                e.vx[i] = vel.x;
                e.vy[i] = vel.y;
                break;
            }
            u32 k = 0;
            for (u32 j = 0; j < g->n; ++j)
                if (g->idx[j] == i) k = j;
            Vec2 c = g->centre;
            if (tp >= 0) c = c + normalizeOr(players_[tp].pos - c, Vec2{}) * (s.speed * dt * loopSpeed_);
            const float turns = s.variant == kVariantB ? v.orbitB : fd.orbitTurnsPerBar;
            const float ph = static_cast<float>(std::fmod(m1_ / kTicksPerBar * turns, 1.0)) * kTau + static_cast<float>(e.group[i] % 7u);
            const float R = n == 3 ? v.triadSideWu / std::sqrt(3.0f) : 0.5f * fd.pairDistance;
            const Vec2 want = c + dirOf(ph + kTau * static_cast<float>(k) / static_cast<float>(n)) * R;
            const Vec2 vel = (want - Vec2{e.x[i], e.y[i]}) * (1.0f / std::fmax(dt, 1e-4f));
            e.vx[i] = vel.x / std::fmax(loopSpeed_, 1e-3f);
            e.vy[i] = vel.y / std::fmax(loopSpeed_, 1e-3f);
            e.heading[i] = ph;
            break;
        }
        case Family::Weaver: {
            // Curving paths (curveTurnRate, its sense flipping each bar and leaning to its player) that leave a lethal trail
            // for trailBars (weaverTrails). B: every loopEveryBars it turns a full circle in a bar, closing its trail into a loop.
            const double barPos = m1_ / kTicksPerBar;
            const bool looping = s.variant == kVariantB && v.loopEveryBars > 0.0f &&
                                 std::fmod(barPos, static_cast<double>(v.loopEveryBars)) >= static_cast<double>(v.loopEveryBars) - 1.0;
            float turn = fd.curveTurnRate * e.a[i];
            if (looping) {
                turn = e.a[i] * kTau / (4.0f * 60.0f / std::fmax(bpm_, 30.0f));  // a full turn in a bar
            } else if (tp >= 0) {
                const float off = wrapAngle(angleOf(to) - e.heading[i]);
                turn += clampf(off, -1.0f, 1.0f) * 0.6f * fd.curveTurnRate;
            }
            forCrossings(m0_, m1_, kTicksPerBar, [&](double, i64) {
                if (!looping) e.a[i] = -e.a[i];
            });
            e.heading[i] += turn * dt;
            const Vec2 vel = dirOf(e.heading[i]) * s.speed;
            e.vx[i] = vel.x;
            e.vy[i] = vel.y;
            break;
        }
        case Family::Well: {
            // Drifts slowly toward its player (its pull on ships and shots: wellPull, wellShots).
            const float sc = e.b[i] > 0.0f ? e.b[i] : 1.0f;
            e.radius[i] = s.radius * sc;
            const Vec2 vel = normalizeOr(to, Vec2{}) * s.speed;
            e.vx[i] = vel.x;
            e.vy[i] = vel.y;
            // B and C: the shots it swallowed come back as a ring on beat 1 (the host's: the bullets go out alike from its
            // swallowed count, which the corrections carry).
            if (s.variant != kVariantA)
                forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
                    if (e.a[i] < 1.0f || !v.ringBullets) return;
                    const float a0 = static_cast<float>(e.serial[i] % 360u) * (kTau / 360.0f);
                    for (u32 k = 0; k < v.ringBullets; ++k)
                        spawnBullet(Vec2{e.x[i], e.y[i]}, dirOf(a0 + kTau * static_cast<float>(k) / static_cast<float>(v.ringBullets)) * bulletSpeed,
                                    tickAgeOf(t));
                    shotEvent(Vec2{e.x[i], e.y[i]});
                    e.a[i] = 0.0f;
                });
            break;
        }
        case Family::Mimic: {
            // It stands where its player would be if the frame were turned half round its centre (the point it landed
            // halfway to them): on each beat it steps up to beatStepWu toward that place; between beats it stands.
            e.vx[i] = e.vy[i] = 0.0f;
            if (tp < 0) break;
            const PlayerState& pl = players_[tp];
            if (e.state[i] == kDash) {
                // C: copying the player's last Dash, turned half round.
                if (m1_ >= e.until[i] + kDashS * ticksPerSecond()) e.state[i] = kMoving;
                const Vec2 vel = dirOf(e.heading[i]) * (kDashWu / kDashS);
                e.vx[i] = vel.x / std::fmax(loopSpeed_, 1e-3f);
                e.vy[i] = vel.y / std::fmax(loopSpeed_, 1e-3f);
                break;
            }
            const Vec2 mirror = Vec2{2.0f * e.a[i], 2.0f * e.b[i]} - pl.pos;
            forCrossings(m0_, m1_, kTicksPerBeat, [&](double, i64) {
                const Vec2 d = mirror - Vec2{e.x[i], e.y[i]};
                const float l = length(d);
                if (l < 1e-3f) return;
                const Vec2 to2 = Vec2{e.x[i], e.y[i]} + d * (std::fmin(l, fd.beatStepWu) / l);
                e.x[i] = clampf(to2.x, -kArenaHalfW, kArenaHalfW);
                e.y[i] = clampf(to2.y, -kArenaHalfH, kArenaHalfH);
                e.heading[i] = angleOf(d);
            });
            // B: it fires (one shot, at its player) as the player fires, every everyNthShot of their volleys.
            if (s.variant == kVariantB && v.everyNthShot) {
                const u8 seen = static_cast<u8>(pl.shotsFired);
                if (static_cast<u8>(seen - e.step[i]) >= v.everyNthShot) {
                    e.step[i] = static_cast<u8>(e.step[i] + v.everyNthShot);
                    if (engaged) {
                        spawnBullet(Vec2{e.x[i], e.y[i]}, normalizeOr(pl.pos - Vec2{e.x[i], e.y[i]}, Vec2{1.0f, 0.0f}) * bulletSpeed, 0.0f);
                        shotEvent(Vec2{e.x[i], e.y[i]});
                    }
                }
            }
            // C: a beat after the player dashes, it dashes the same way turned half round.
            if (s.variant == kVariantC && pl.lastDashTick > e.until[i] && m1_ >= pl.lastDashTick + v.copyDashAfterBeats * kTicksPerBeat) {
                e.until[i] = pl.lastDashTick;
                const Vec2 dv = pl.lastDashTo - pl.lastDashFrom;
                if (lengthSq(dv) > 1.0f) {
                    e.heading[i] = angleOf(dv * -1.0f);
                    e.state[i] = kDash;
                }
            }
            break;
        }
        default: break;
    }
}

// ---------------------------------------------------------------------------------------------- hazards

u32 World::tetherBeams(Vec2* a, Vec2* b, u32 cap) const {
    // The lethal beams of every whole pair and triad this tick.
    const Enemies& e = enemies_;
    u32 n = 0;
    for (u32 k = 0; k < groupCount_ && n < cap; ++k) {
        const Group& g = groups_[k];
        if (!g.n || species_->get(e.species[g.idx[0]]).family != Family::Tether) continue;
        const u32 want = e.step[g.idx[0]] ? e.step[g.idx[0]] : 2u;
        if (g.n < want) continue;
        bool landed = true;
        for (u32 j = 0; j < g.n; ++j) landed = landed && e.state[g.idx[j]] != kWarping && !(e.flags[g.idx[j]] & kFlagDead);
        if (!landed) continue;
        const u32 beams = g.n == 2 ? 1u : g.n;
        for (u32 j = 0; j < beams && n < cap; ++j) {
            const u32 p = g.idx[j], q = g.idx[(j + 1) % g.n];
            a[n] = Vec2{e.x[p], e.y[p]};
            b[n] = Vec2{e.x[q], e.y[q]};
            ++n;
        }
    }
    return n;
}

void World::updateTrails() {
    // Weavers lay a point every trailBars / trailSegments; Ram B's dash lays its wake. A point lives trailBars (the wake:
    // wakeBars). Weaver C: on each bar line its trail's points blow (trailBlastWu) and the trail is spent.
    Enemies& e = enemies_;
    for (u32 i = 0; i < e.count; ++i) {
        if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping) continue;
        const Species& s = species_->get(e.species[i]);
        const FamilyData& fd = species_->family(s.family);
        const bool weaver = s.family == Family::Weaver;
        const bool wake = s.family == Family::Ram && s.variant == kVariantB && e.state[i] == kDash;
        if (!weaver && !wake) continue;
        const double every = weaver ? fd.trailBars * kTicksPerBar / std::fmax(static_cast<float>(fd.trailSegments), 1.0f) : kTicksPerStep * 0.5;
        if (m1_ < e.until[i] && weaver) continue;
        if (weaver) e.until[i] = m1_ + every;
        Trail* t = nullptr;
        for (Trail& c : trails_)
            if (c.owner == e.handle[i] && c.n) t = &c;
        if (!t)
            for (Trail& c : trails_)
                if (!c.n) {
                    t = &c;
                    t->owner = e.handle[i];
                    t->head = 0;
                    t->kind = weaver ? (s.variant == kVariantC ? 2 : 0) : 1;
                    t->width = weaver ? fd.trailWidth : fd.v.wakeWidth;
                    t->life = weaver ? fd.trailBars * kTicksPerBar : fd.v.wakeBars * kTicksPerBar;
                    break;
                }
        if (!t) continue;
        if (wake && t->n && m1_ - t->t[(t->head + Trail::kCap - 1) % Trail::kCap] < every) continue;
        t->x[t->head] = e.x[i];
        t->y[t->head] = e.y[i];
        t->t[t->head] = m1_;
        t->head = static_cast<u16>((t->head + 1) % Trail::kCap);
        if (t->n < Trail::kCap) ++t->n;
    }
    for (Trail& t : trails_) {
        if (!t.n) continue;
        // Expire the old points (the oldest first); a trail whose last point is gone is free.
        while (t.n) {
            const u32 oldest = (t.head + Trail::kCap - t.n) % Trail::kCap;
            if (m1_ - t.t[oldest] <= t.life) break;
            --t.n;
        }
        if (t.kind == 2 && t.n)
            forCrossings(m0_, m1_, kTicksPerBar, [&](double tick, i64) {
                // Weaver C: the trail blows on the bar line.
                Event ev;
                ev.type = EventType::TrailBlast;
                ev.tick = tick;
                const float r = species_->family(Family::Weaver).v.trailBlastWu;
                for (u32 k = 0; k < t.n; ++k) {
                    const u32 j = (t.head + Trail::kCap - 1 - k) % Trail::kCap;
                    const Vec2 at{t.x[j], t.y[j]};
                    if (k % 4 == 0) {
                        ev.x = at.x;
                        ev.y = at.y;
                        emit(ev);
                    }
                    for (u32 p = 0; p < playerCount_; ++p) {
                        const PlayerState& pl = players_[p];
                        if (!pl.alive || !isLocal(p) || pl.invulnerable() || rules_.godMode || sheltered(p)) continue;
                        if (lengthSq(pl.pos - at) > (r + kPlayerHitRadius) * (r + kPlayerHitRadius)) continue;
                        if (!haloBlocks(p, at)) hitPlayer(p, at, SpeciesTable::index(Family::Weaver, 1, kVariantC));
                    }
                }
                t.blastTick = tick;
                t.n = 0;
            });
    }
}

bool World::hazardHits(u32 p, Vec2& from, u16& cause) {
    // A player's body against the lethal lines: Tether beams, Weaver trails, Ram B's wake, a Weaver C trail as it blows.
    const Vec2 c = players_[p].pos;
    const auto seg = [&](Vec2 a, Vec2 b, float half) {
        const Vec2 ab = b - a;
        const float l2 = lengthSq(ab);
        const float t = l2 > 1e-6f ? clampf(((c.x - a.x) * ab.x + (c.y - a.y) * ab.y) / l2, 0.0f, 1.0f) : 0.0f;
        const Vec2 q = a + ab * t;
        if (lengthSq(c - q) > (half + kPlayerHitRadius) * (half + kPlayerHitRadius)) return false;
        from = q;
        return true;
    };
    Vec2 ba[64], bb[64];
    const u32 nb = tetherBeams(ba, bb, 64);
    for (u32 k = 0; k < nb; ++k)
        if (seg(ba[k], bb[k], 0.5f * species_->family(Family::Tether).beamWidth)) {
            cause = SpeciesTable::index(Family::Tether, 1, kVariantA);  // (M6: named by its family)
            return true;
        }
    // Skitter B's mines: set off by a ship within their radius (spent).
    for (Spot& m : emines_) {
        if (m.until < m1_) continue;
        if (lengthSq(c - m.at) > (m.r + kPlayerHitRadius) * (m.r + kPlayerHitRadius)) continue;
        m.until = -1.0;
        from = m.at;
        cause = SpeciesTable::index(Family::Skitter, 1, kVariantB);
        return true;
    }
    for (const Trail& t : trails_) {
        if (t.n < 2) continue;
        for (u32 k = 1; k < t.n; ++k) {
            const u32 j0 = (t.head + Trail::kCap - k) % Trail::kCap, j1 = (t.head + Trail::kCap - k - 1) % Trail::kCap;
            if (seg(Vec2{t.x[j0], t.y[j0]}, Vec2{t.x[j1], t.y[j1]}, 0.5f * t.width)) {
                cause = SpeciesTable::index(t.kind == 1 ? Family::Ram : Family::Weaver, 1, t.kind == 1 ? kVariantB : kVariantA);
                return true;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------------------------- Wells

bool World::wellSwallow(u32 i, u32 k) {
    // B and C swallow the shots that reach them (collide(); every device, a teammate's copies too): counted for the ring on
    // beat 1; C grows by a quarter for each growPerShotsSwallowed and, overfed (burstAtScale), bursts into a ring (the host's).
    Enemies& e = enemies_;
    const Species& sp = species_->get(e.species[i]);
    if (sp.family != Family::Well || sp.variant == kVariantA) return false;
    const FamilyData& fd = species_->family(Family::Well);
    shots_.dead[k] = 1;
    e.a[i] += 1.0f;
    if (sp.variant == kVariantC) {
        e.step[i] = static_cast<u8>(e.step[i] + 1);
        if (e.step[i] >= fd.v.growPerShots) {
            e.step[i] = 0;
            e.b[i] = (e.b[i] > 0.0f ? e.b[i] : 1.0f) + 0.25f;
            if (e.b[i] >= fd.v.burstAtScale && role_ != NetRole::Client) {
                for (u32 q = 0; q < fd.v.wellBurstBullets; ++q)
                    spawnBullet(Vec2{e.x[i], e.y[i]}, dirOf(kTau * static_cast<float>(q) / static_cast<float>(fd.v.wellBurstBullets)) * species_->bulletSpeed, 0.0f);
                killEnemy(i, 0xFF);
            }
        }
    }
    return true;
}

Vec2 World::wellPull(Vec2 pos) const {
    // A Well pulls a ship within pullRadius toward it, hardest at pullPeakAt (pullSpeed wu/s), nothing at its edge. (Asked
    // between ticks by the controller: every body is looked at.)
    const Enemies& e = enemies_;
    Vec2 v{};
    if (horizonOn()) {
        // M5 part 4: Event Horizon pulls the ship too (half its own speed at the photon ring; a Dash ignores it).
        const Vec2 to = boss_.pos - pos;
        const float l = length(to);
        if (l > 1.0f) v = v + to * (horizonPullAt(pos, species_->horizon.pullShip) / l);
    }
    if (boss_.active && !boss_.dying && boss_.kind == BossKind::DeepWell && m1_ >= boss_.fightTick) {
        // M5 part 6: Deep Well draws ships within 300 wu in at up to 120 wu/s (most at its edge; a Dash ignores it).
        const Vec2 to = boss_.body[0] - pos;
        const float l = length(to);
        if (l > 1.0f && l < prose::kDwPull) v = v + to * (prose::kDwPullMax * (1.0f - l / prose::kDwPull) / l);
    }
    if (repriseHorizonOn()) {
        // M5 part 6: the Reprise's Event Horizon arm draws ships toward the wheel at 200 wu/s for its two bars.
        const Vec2 to = boss_.pos - pos;
        const float l = length(to);
        if (l > 1.0f) v = v + to * (prose::kRpPullWu / l);
    }
    for (u32 i = 0; i < e.count; ++i) {
        if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping || species_->get(e.species[i]).family != Family::Well) continue;
        const FamilyData& fd = species_->family(Family::Well);
        const float sc = e.b[i] > 0.0f ? e.b[i] : 1.0f;
        const float R = fd.pullRadius * sc, peak = fd.pullPeakAt * sc;
        const Vec2 d = Vec2{e.x[i], e.y[i]} - pos;
        const float l = length(d);
        if (l >= R || l < 1.0f) continue;
        const float f = l < peak ? l / peak : (R - l) / (R - peak);
        v = v + d * (fd.pullSpeed * f / l);
    }
    return v;
}

void World::findWells() {
    // The Wells, and Sentry C's domes (up while another body stands within shieldsNeighboursWithinWu of it), this tick.
    wellCount_ = 0;
    domeCount_ = 0;
    const Enemies& e = enemies_;
    for (u32 i = 0; i < e.count; ++i) {
        if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping) continue;
        const Species& s = species_->get(e.species[i]);
        if (s.family == Family::Well && wellCount_ < kMaxWells) wellIdx_[wellCount_++] = static_cast<u16>(i);
        if (s.family == Family::Sentry && s.variant == kVariantC && domeCount_ < 16) {
            const FamilyData& fd = species_->family(Family::Sentry);
            const float R = fd.v.domeWithinWu;
            bool near = false;
            grid_.query(e.x[i] - R, e.y[i] - R, e.x[i] + R, e.y[i] + R, [&](u32 j) {
                if (j == i || near || (e.flags[j] & kFlagDead)) return;
                near = lengthSq(Vec2{e.x[j] - e.x[i], e.y[j] - e.y[i]}) <= R * R;
            });
            if (near) {
                domeAt_[domeCount_] = Vec2{e.x[i], e.y[i]};
                domeR_[domeCount_++] = fd.v.domeRadius;
            }
        }
    }
}

u32 World::domes(Vec2* at, float* r, u32 cap) const {
    u32 n = 0;
    for (u32 k = 0; k < domeCount_ && n < cap; ++k, ++n) {
        at[n] = domeAt_[k];
        r[n] = domeR_[k];
    }
    return n;
}

void World::layMine(Vec2 at, float r, float bars) {
    for (Spot& m : emines_)
        if (m.until < m1_) {
            m.at = at;
            m.r = r;
            m.until = m1_ + static_cast<double>(bars) * kTicksPerBar;
            return;
        }
}

void World::addDecoy(Vec2 at, float r, double until) {
    for (Spot& d : decoys_)
        if (d.until < m1_) {
            d.at = at;
            d.r = r;
            d.until = until;
            return;
        }
}

void World::familyDeath4(u32 i, const Species& s) {
    // Drifter C splits into splitInto smaller ones (a tier down; none from tier 1); a Flanker's death quickens the Flanker
    // C near it. The host's (killEnemy).
    Enemies& e = enemies_;
    const FamilyData& fd = species_->family(s.family);
    const Vec2 at{e.x[i], e.y[i]};
    if (s.family == Family::Drifter && s.variant == kVariantC && s.tier > fd.v.childTierDrop) {
        const u16 child = SpeciesTable::index(Family::Drifter, s.tier - fd.v.childTierDrop, kVariantA);
        for (u32 k = 0; k < fd.v.splitInto; ++k) {
            const float a = e.heading[i] + kTau * static_cast<float>(k) / static_cast<float>(fd.v.splitInto ? fd.v.splitInto : 1u);
            spawnEnemy(child, at + dirOf(a) * 16.0f, a, m1_);
        }
    }
    if (s.family == Family::Flanker) {
        const FamilyData& ff = species_->family(Family::Flanker);
        const float R = ff.v.speedWithinWu;
        grid_.query(at.x - R, at.y - R, at.x + R, at.y + R, [&](u32 j) {
            if (j == i || (e.flags[j] & kFlagDead)) return;
            const Species& o = species_->get(e.species[j]);
            if (o.family != Family::Flanker || o.variant != kVariantC || lengthSq(Vec2{e.x[j], e.y[j]} - at) > R * R) return;
            if (e.step[j] < 250) e.step[j] = static_cast<u8>(e.step[j] + 1);
        });
    }
}

void World::shoalVariant(u32 i, const Species& s, const FamilyData& fd, i32 tp, Vec2 to) {
    // Shoal B: every second bar, for its first beat, the school streams through its player in streamLines lines at lineSpeed.
    // Shoal C: on each bar's first beat, after a beat's tell, it charges as a spear at spearSpeed.
    Enemies& e = enemies_;
    if (tp < 0) return;
    const double inBar = std::fmod(m1_, kTicksPerBar);
    const i64 bar = static_cast<i64>(std::floor(m1_ / kTicksPerBar));
    const Vec2 d = normalizeOr(to, dirOf(e.heading[i]));
    if (s.variant == kVariantB) {
        if ((bar & 1) || inBar >= kTicksPerBeat) return;
        const u32 lines = fd.v.streamLines ? fd.v.streamLines : 1u;
        const float lane = (static_cast<float>(e.serial[i] % lines) - 0.5f * static_cast<float>(lines - 1)) * 60.0f;
        const Vec2 aim = normalizeOr(to + perpOf(d) * lane, d);
        e.vx[i] = aim.x * fd.v.lineSpeed;
        e.vy[i] = aim.y * fd.v.lineSpeed;
        e.heading[i] = angleOf(aim);
        return;
    }
    // C
    if (inBar >= kTicksPerBar - kTicksPerBeat * static_cast<double>(fd.v.spearTellBeats) && e.serial[i] % 6u == 0) {
        if (m0_ < bar * kTicksPerBar + kTicksPerBar - kTicksPerBeat * fd.v.spearTellBeats &&
            m1_ >= bar * kTicksPerBar + kTicksPerBar - kTicksPerBeat * fd.v.spearTellBeats) {
            Event ev;
            ev.type = EventType::Telegraph;
            ev.species = e.species[i];
            ev.x = e.x[i];
            ev.y = e.y[i];
            ev.x2 = d.x;
            ev.y2 = d.y;
            ev.tick = (bar + 1) * kTicksPerBar;
            emit(ev);
        }
    }
    if (inBar >= kTicksPerBeat) return;
    e.vx[i] = d.x * fd.v.spearSpeed;
    e.vy[i] = d.y * fd.v.spearSpeed;
    e.heading[i] = angleOf(d);
}

void World::caromSplit(u32 i, const Species& s, const FamilyData& fd) {
    // Carom C: every splitEveryBounces bounces it splits (at most maxSplits times): a copy flies off at 45 degrees, each with
    // half the health. step counts its bounces, b its splits. The host's.
    Enemies& e = enemies_;
    e.step[i] = static_cast<u8>(e.step[i] + 1);
    if (!fd.v.splitEveryBounces || e.step[i] % fd.v.splitEveryBounces || e.b[i] >= static_cast<float>(fd.v.maxSplits) || role_ == NetRole::Client) return;
    e.b[i] += 1.0f;
    e.hp[i] *= 0.5f;
    const Vec2 v{e.vx[i], e.vy[i]};
    const float h = angleOf(v) + 0.25f * kPi;
    const i32 c = spawnEnemy(e.species[i], Vec2{e.x[i], e.y[i]}, h, m1_);
    if (c < 0) return;
    e.hp[c] = e.hp[i];
    e.a[c] = e.a[i];
    e.b[c] = e.b[i];
    e.step[c] = e.step[i];
    const Vec2 nv = dirOf(h) * length(v);
    e.vx[c] = nv.x;
    e.vy[c] = nv.y;
    (void)s;
}

void World::skitterHop(u32 i, Vec2 dir) {
    // Skitter C: shot (and not cooling down), it blinks blinkHopWu aside, square to the shot.
    Enemies& e = enemies_;
    const FamilyData& fd = species_->family(Family::Skitter);
    if (m1_ < e.until[i] || e.state[i] == kDodge) return;
    const Vec2 side = perpOf(dir) * ((e.serial[i] & 1u) ? 1.0f : -1.0f);
    e.x[i] = clampf(e.x[i] + side.x * fd.v.blinkHopWu, -kArenaHalfW, kArenaHalfW);
    e.y[i] = clampf(e.y[i] + side.y * fd.v.blinkHopWu, -kArenaHalfH, kArenaHalfH);
    e.px[i] = e.x[i];
    e.py[i] = e.y[i];
    e.until[i] = m1_ + static_cast<double>(fd.v.blinkCooldownBeats) * kTicksPerBeat;
    Event ev;
    ev.type = EventType::WarpLand;
    ev.species = e.species[i];
    ev.x = e.x[i];
    ev.y = e.y[i];
    emit(ev);
}

void World::wellShots(float dt) {
    // Wells bend the shots passing within pullRadius toward them (shotBend: a turn of that many radians a second at the
    // well, none at the edge; swallowing: wellSwallow, from collide()). Blink C's decoys and Sentry C's domes stop shots.
    Shots& s = shots_;
    bool decoys = false;
    for (const Spot& d : decoys_) decoys = decoys || d.until >= m1_;
    if (decoys || domeCount_)
        for (u32 k = 0; k < s.count; ++k) {
            if (s.dead[k]) continue;
            const Vec2 p{s.x[k], s.y[k]};
            bool stop = false;
            for (const Spot& d : decoys_)
                stop = stop || (d.until >= m1_ && lengthSq(p - d.at) <= (d.r + s.radius[k]) * (d.r + s.radius[k]));
            for (u32 q = 0; q < domeCount_ && !stop; ++q) stop = lengthSq(p - domeAt_[q]) <= domeR_[q] * domeR_[q];
            if (!stop) continue;
            s.dead[k] = 1;
            Event ev;
            ev.type = EventType::Impact;
            ev.species = 0;
            ev.b = s.owner[k];
            ev.x = p.x;
            ev.y = p.y;
            ev.x2 = s.dx[k];
            ev.y2 = s.dy[k];
            emit(ev);
        }
    if (!wellCount_) return;
    Enemies& e = enemies_;
    const FamilyData& fd = species_->family(Family::Well);
    for (u32 k = 0; k < s.count; ++k) {
        if (s.dead[k]) continue;
        for (u32 w = 0; w < wellCount_; ++w) {
            const u32 i = wellIdx_[w];
            if (e.flags[i] & kFlagDead) continue;
            const float sc = e.b[i] > 0.0f ? e.b[i] : 1.0f;
            const float R = fd.pullRadius * sc;
            const Vec2 d = Vec2{e.x[i] - s.x[k], e.y[i] - s.y[k]};
            const float l2 = lengthSq(d);
            if (l2 > R * R) continue;
            const float l = std::sqrt(l2);
            const float turn = fd.shotBend * (1.0f - l / R) * dt * kTau;
            const float h = turnToward(angleOf(Vec2{s.dx[k], s.dy[k]}), angleOf(d), turn);
            s.dx[k] = std::cos(h);
            s.dy[k] = std::sin(h);
        }
    }
}

}  // namespace rf::sim
