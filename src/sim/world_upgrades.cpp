// Upgrades in the world (GAME_DESIGN 6; M3 stage B): the Trio at every Break, what the Mods do to shots,
// the three Riffs (Orbitals, Pulse Ring, Sentinel) and the four Boosts (Prism, Hyper, Halo, Magnet Storm).

#include <cmath>

#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
constexpr double kLeadGrid[3] = {24.0, 16.0, 12.0};  // conductor.json lead.grid: 1/16, 1/16 triplet, 1/32
constexpr float kDeg = kTau / 360.0f;
// Grid queries reach this much past their radius: the largest enemy (40 wu) and how far one moves after the grid was
// built this tick (32 wu), as in world.cpp.
constexpr float kQuerySlackWu = 72.0f;
// Trio orbs: top, lower left, lower right (world y grows down the screen).
const Vec2 kOrbAt[3] = {{0.0f, -1.0f}, {-0.8660254f, 0.5f}, {0.8660254f, 0.5f}};
}  // namespace

double World::leadGrid(u32 p) const {
    const PlayerState& pl = players_[p];
    u32 idx = static_cast<u32>(pl.loadout.mods[kDoubleTime]) + phrase_.surgeGear();
    if (idx > 2) idx = 2;
    double g = kLeadGrid[idx];
    if (pl.boost(kHyper) && std::fmod(g, 2.0) == 0.0) g *= 0.5;
    return g;
}

// ---------------------------------------------------------------------------------------------- shots

void World::spawnShotRaw(u32 p, Vec2 pos, Vec2 dir, float advanceS, float damage, u8 kind, u8 flags, float life, u8 pierce,
                         u8 bounces) {
    Shots& s = shots_;
    if (s.count >= Shots::kCap) {
        ++counters_.shotsDropped;
        return;
    }
    // Play test 1: a player never has more than kShotsHardCap live shots (the pool's 1,024 a player, TECH_SPEC 7.1);
    // the soft cap's rules (fireShot, Prism) keep it from being reached.
    u32& live = shotsLive_[p < kMaxPlayers ? p : 0];
    if (live >= kShotsHardCap) {
        ++counters_.shotsCapped;
        return;
    }
    ++live;
    const u32 i = s.count++;
    s.px[i] = pos.x;
    s.py[i] = pos.y;
    s.x[i] = pos.x + dir.x * kShotSpeed * advanceS;
    s.y[i] = pos.y + dir.y * kShotSpeed * advanceS;
    s.dx[i] = dir.x;
    s.dy[i] = dir.y;
    s.speed[i] = kShotSpeed;
    s.life[i] = life - advanceS;
    s.damage[i] = damage;
    s.radius[i] = kShotRadius * (1.0f + kOverchargeRadius * static_cast<float>(players_[p].loadout.mods[kOvercharge]));
    s.lastHit[i] = 0xFFFFFFFFu;
    s.owner[i] = static_cast<u8>(p);
    s.dead[i] = 0;
    s.pierce[i] = pierce;
    s.bounces[i] = bounces;
    s.kind[i] = kind;
    s.flags[i] = flags;
}

void World::hitEnemy(u32 i, float dmg, u8 owner, Vec2 at, Vec2 dir) {
    Enemies& e = enemies_;
    if (e.flags[i] & kFlagDead) return;
    if (role_ == NetRole::Client) logClaim(i, dmg, kClaimHit);  // M4: the host applies it; a kill here is predicted
    e.hp[i] -= dmg;
    if (e.hp[i] <= 0.0f) {
        killEnemy(i, owner);
        return;
    }
    if (applyingClaim_) return;  // M4 host: the claimant's shot already showed its impact here (a cosmetic copy)
    Event ev;
    ev.type = EventType::Impact;
    ev.species = e.species[i];
    ev.x = at.x;
    ev.y = at.y;
    ev.x2 = dir.x;
    ev.y2 = dir.y;
    emit(ev);
}

// ---------------------------------------------------------------------------------------------- Trio

void World::openTrio(u32 p, double closeTick) {
    TrioState& tr = trio_[p];
    PlayerState& pl = players_[p];
    u8 opts[3];
    const u32 n = drawTrio(pl.loadout, tr.sinceRiff, upgradeRng_, opts);
    tr.open = n > 0;
    tr.homing = -1;
    if (!n) return;
    tr.orbs = static_cast<u8>(n);
    tr.centre = pl.pos;
    // Keep the whole ring inside the arena.
    const float m = kTrioRingWu + 60.0f;
    tr.centre.x = clampf(tr.centre.x, -kArenaHalfW + m, kArenaHalfW - m);
    tr.centre.y = clampf(tr.centre.y, -kArenaHalfH + m, kArenaHalfH - m);
    tr.openTick = m1_;
    tr.closeTick = closeTick;
    tr.holdOrb = -1;
    tr.holdTicks = 0.0;
    for (u32 k = 0; k < 4; ++k) tr.upgrade[k] = kNoUpgrade;
    for (u32 k = 0; k < n; ++k) {
        tr.upgrade[k] = opts[k];
        tr.level[k] = static_cast<u8>(pl.loadout.level(opts[k]) + 1);
        tr.orbPos[k] = tr.centre + kOrbAt[k] * kTrioRingWu;
    }
    ++tr.opened;
    Event ev;
    ev.type = EventType::TrioOpen;
    ev.a = static_cast<u8>(p);
    ev.b = static_cast<u8>(n);
    ev.x = tr.centre.x;
    ev.y = tr.centre.y;
    ev.tick = closeTick;
    emit(ev);
}

void World::openTrioNow(u32 p, double closeTick, const u8* forced) {
    openTrio(p, closeTick);
    TrioState& tr = trio_[p];
    if (!forced || !tr.open) return;
    for (u32 k = 0; k < 3; ++k) {
        if (forced[k] >= kUpgradeCount) continue;
        tr.upgrade[k] = forced[k];
        tr.level[k] = static_cast<u8>(players_[p].loadout.level(forced[k]) + 1);
    }
    tr.orbs = 3;
}

void World::takeOrb(u32 p, u32 k) {
    TrioState& tr = trio_[p];
    const u8 u = tr.upgrade[k];
    applyUpgrade(p, u);
    Event ev;
    ev.type = EventType::TrioTake;
    ev.a = static_cast<u8>(p);
    ev.b = u;
    ev.c = static_cast<u8>(players_[p].loadout.level(u));
    ev.x = tr.orbPos[k].x;
    ev.y = tr.orbPos[k].y;
    emit(ev);
    for (u32 j = 0; j < tr.orbs; ++j) {
        if (j == k) continue;
        Event pop;
        pop.type = EventType::TrioPop;
        pop.a = static_cast<u8>(p);
        pop.b = tr.upgrade[j];
        pop.x = tr.orbPos[j].x;
        pop.y = tr.orbPos[j].y;
        emit(pop);
    }
    tr.lastTaken = u;
    tr.lastTakenTick = m1_;
    tr.lastTakenAt = tr.orbPos[k];
    tr.open = false;
    tr.homing = -1;
    tr.holdOrb = -1;
    tr.holdTicks = 0.0;
    if (tr.queued > 0 && phrase_.section == Section::Break) {
        // The boss's bonus Trio was this one: the Break's own follows, with the rest of the Break as its clock.
        --tr.queued;
        openTrio(p, tickOfBar(phrase_.bar - phrase_.sectionBar + phrase_.sectionBars));
    } else {
        tr.queued = 0;
    }
}

void World::applyUpgrade(u32 p, u8 u) {
    PlayerState& pl = players_[p];
    Loadout& l = pl.loadout;
    if (!l.canTake(u)) return;
    if (!l.order[u]) l.order[u] = static_cast<u8>(l.taken < 254 ? l.taken + 1 : 255);
    if (l.taken < 255) ++l.taken;
    if (isRiff(u)) {
        u8& lv = l.riffs[riffIndex(u)];
        const u32 before = u == kSentinel ? sentinelDrones(lv) : 0u;
        ++lv;
        // A new drone (levels 1, 3 and 5) starts at the ship.
        if (u == kSentinel)
            for (u32 k = before; k < sentinelDrones(lv) && k < kMaxDrones; ++k) pl.sentinel[k] = pl.prevSentinel[k] = pl.pos;
        return;
    }
    ++l.mods[u];
    if (u == kExtraCell) {
        // GAME_DESIGN 6.2 (play test 1): +1 Shield cell capacity and every cell refilled.
        if (pl.cellCap < kMaxCells) ++pl.cellCap;
        pl.cells = pl.cellCap;
    }
}

void World::updateTrios() {
    // Play test 1 (GAME_DESIGN 6.1, screens.md "The Trio"): a choice is never an accident. Nothing can be taken
    // for the Break's first two beats; then the orb whose thin ring the ship stays inside fills over two beats and
    // is taken when it closes; leaving empties it at once. If none is taken, the one nearest the ship is, at the end.
    const double lockTicks = kTrioLockBeats * kTicksPerBeat, holdTicks = kTrioHoldBeats * kTicksPerBeat;
    for (u32 p = 0; p < playerCount_; ++p) {
        TrioState& tr = trio_[p];
        PlayerState& pl = players_[p];
        if (!tr.open || !isLocal(p)) continue;  // M4: another player's Trio is theirs to take (we hear of it)
        if (!pl.alive) {
            tr.open = false;
            continue;
        }
        if (tr.homing >= 0) {
            // The clock ran out: the nearest orb flies to the ship.
            Vec2& o = tr.orbPos[tr.homing];
            const Vec2 d = pl.pos - o;
            const float dist = length(d);
            const float step = kTrioHomeSpeed * kDt;
            if (dist <= step + kPlayerVisualRadius) {
                takeOrb(p, static_cast<u32>(tr.homing));
                continue;
            }
            o = o + d * (step / dist);
            continue;
        }
        const double unlock = tr.openTick + lockTicks;
        if (m0_ < unlock && m1_ >= unlock) {
            Event ev;
            ev.type = EventType::TrioUnlock;
            ev.a = static_cast<u8>(p);
            ev.x = tr.centre.x;
            ev.y = tr.centre.y;
            ev.tick = unlock;
            emit(ev);
        }
        i32 inside = -1;
        if (m1_ >= unlock) {
            for (u32 k = 0; k < tr.orbs; ++k) {
                const float r = kTrioHoldRing * kTrioOrbWu * (isRiff(tr.upgrade[k]) ? kTrioRiffScale : 1.0f);
                if (lengthSq(tr.orbPos[k] - pl.pos) <= r * r) {
                    inside = static_cast<i32>(k);
                    break;
                }
            }
        }
        if (inside < 0) {
            tr.holdOrb = -1;
            tr.holdTicks = 0.0;
        } else {
            const double since = m0_ > unlock ? m1_ - m0_ : m1_ - unlock;
            if (tr.holdOrb != inside) {
                tr.holdOrb = static_cast<i8>(inside);
                tr.holdTicks = 0.0;
            }
            tr.holdTicks += since > 0.0 ? since : 0.0;
            if (tr.holdTicks >= holdTicks) {
                takeOrb(p, static_cast<u32>(inside));
                continue;
            }
        }
        if (tr.open && tr.homing < 0 && m1_ >= tr.closeTick) {
            u32 nearest = 0;
            float best = 1e30f;
            for (u32 k = 0; k < tr.orbs; ++k) {
                const float d2 = lengthSq(tr.orbPos[k] - pl.pos);
                if (d2 < best) {
                    best = d2;
                    nearest = k;
                }
            }
            tr.homing = static_cast<i8>(nearest);
            tr.holdOrb = -1;
            tr.holdTicks = 0.0;
        }
    }
}

void World::critBurst(u32 hit, u8 owner, Vec2 at, float damage) {
    // On-Beat Crit (play test 1): the hit bursts over 80 wu; every other enemy in it takes a share of the damage.
    Enemies& e = enemies_;
    const float r = kCritBurstWu, reach = r + kQuerySlackWu;
    grid_.query(at.x - reach, at.y - reach, at.x + reach, at.y + reach, [&](u32 i) {
        if (i == hit || (e.flags[i] & kFlagDead)) return;
        const float rr = r + e.radius[i];
        const Vec2 d = Vec2{e.x[i], e.y[i]} - at;
        if (lengthSq(d) > rr * rr) return;
        hitEnemy(i, damage, owner, Vec2{e.x[i], e.y[i]}, normalizeOr(d, Vec2{1.0f, 0.0f}));
    });
    Event ev;
    ev.type = EventType::CritBurst;
    ev.a = owner;
    ev.x = at.x;
    ev.y = at.y;
    ev.f = r;
    emit(ev);
}

i32 World::ricochetTarget(u32 hit, Vec2 at) const {
    // Ricochet (play test 1): the nearest other solid enemy within 300 wu of the hit (ties by index).
    const Enemies& e = enemies_;
    const float r = kRicochetSeekWu, reach = r + kQuerySlackWu;
    i32 best = -1;
    float bestD = r * r;
    grid_.query(at.x - reach, at.y - reach, at.x + reach, at.y + reach, [&](u32 i) {
        if (i == hit || (e.flags[i] & kFlagDead) || e.state[i] == kWarping) return;
        const float d2 = lengthSq(Vec2{e.x[i], e.y[i]} - at);
        if (d2 < bestD || (d2 == bestD && static_cast<i32>(i) < best)) {
            bestD = d2;
            best = static_cast<i32>(i);
        }
    });
    return best;
}

void World::orbitalBullets(const PlayerState& pl, u32 n) {
    // Orbitals (play test 1): the shards also destroy the enemy bullets they touch (on every device, like the Nova).
    Bullets& b = bullets_;
    const double turn = pl.boost(kHyper) ? kTicksPerBar * 0.5 : kTicksPerBar;
    const double ph = std::fmod(m1_ - director_.startTick, turn) / turn;
    const float hit = kOrbitalHitWu + species_->bulletRadius;
    const float reach = kOrbitalRadiusWu + hit;
    Vec2 o[16];
    const u32 m = n < 16 ? n : 16u;
    for (u32 k = 0; k < m; ++k) {
        const float a = static_cast<float>(ph) * kTau + kTau * static_cast<float>(k) / static_cast<float>(m) - 0.25f * kTau;
        o[k] = pl.pos + dirOf(a) * kOrbitalRadiusWu;
    }
    for (u32 i = 0; i < b.count; ++i) {
        if (b.dead[i]) continue;
        const float dx = b.x[i] - pl.pos.x, dy = b.y[i] - pl.pos.y;
        if (std::fabs(dx) > reach || std::fabs(dy) > reach) continue;
        for (u32 k = 0; k < m; ++k)
            if (lengthSq(Vec2{b.x[i], b.y[i]} - o[k]) <= hit * hit) {
                b.dead[i] = 1;
                break;
            }
    }
}

// ---------------------------------------------------------------------------------------------- Riffs

void World::updateRiffs(float dt) {
    Enemies& e = enemies_;
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        for (u32 k = 0; k < kMaxDrones; ++k) pl.prevSentinel[k] = pl.sentinel[k];
        if (!pl.alive) continue;
        const Loadout& l = pl.loadout;
        const bool hyper = pl.boost(kHyper);
        const u8 owner = static_cast<u8>(p);
        const bool local = isLocal(p);  // M4: another player's Riffs are drawn here, their hits are its own device's

        // Orbitals (GAME_DESIGN 6.3, play test 1): 3, 5, 7, 9, 11 shards circle the ship once a bar (twice under
        // Hyper); each hurts what it touches once an eighth note (a sixteenth under Hyper) and destroys the enemy
        // bullets it touches.
        if (const u8 lv = l.riffs[riffIndex(kOrbitals)]) {
            const u32 n = orbitalCount(lv);
            orbitalBullets(pl, n);
            const double turn = hyper ? kTicksPerBar * 0.5 : kTicksPerBar;
            forCrossings(m0_, m1_, hyper ? kTicksPerStep : 2.0 * kTicksPerStep, [&](double t, i64) {
                const double ph = std::fmod(t - director_.startTick, turn) / turn;
                for (u32 k = 0; k < n; ++k) {
                    const float a = static_cast<float>(ph) * kTau + kTau * static_cast<float>(k) / static_cast<float>(n) - 0.25f * kTau;
                    const Vec2 o = pl.pos + dirOf(a) * kOrbitalRadiusWu;
                    const float reach = kOrbitalHitWu + 40.0f;
                    if (!local) continue;
                    grid_.query(o.x - reach, o.y - reach, o.x + reach, o.y + reach, [&](u32 i) {
                        if (e.flags[i] & kFlagDead) return;
                        const float r = e.radius[i] + kOrbitalHitWu;
                        if (lengthSq(Vec2{e.x[i], e.y[i]} - o) <= r * r) hitEnemy(i, kOrbitalDamage, owner, o, dirOf(a + 0.25f * kTau));
                    });
                    if (boss_.active && !boss_.dying) {
                        u8 part = 0, pair = 0;
                        float tt = 0.0f;
                        if (bossHitTest(o, o, kOrbitalHitWu, part, pair, tt)) damageBoss(part, pair, kOrbitalDamage, owner, o, dirOf(a));
                    }
                }
            });
        }

        // Pulse Ring (play test 1): a ring on every beat from level 1 at twice M3's damage; a second ring from level
        // 2, wider from level 3, heavier from level 4, every eighth at level 5 (Hyper doubles). Each enemy the front
        // passes takes its damage once.
        if (const u8 lv = l.riffs[riffIndex(kPulseRing)]) {
            double every = lv >= 5 ? 0.5 * kTicksPerBeat : kTicksPerBeat;
            if (hyper) every *= 0.5;
            forCrossings(m0_, m1_, every, [&](double t, i64) {
                for (u32 ring = 0; ring < (lv >= 2 ? 2u : 1u); ++ring) {
                    PulseWave* w = nullptr;
                    for (PulseWave& c : pl.pulses)
                        if (!c.active) {
                            w = &c;
                            break;
                        }
                    if (!w) break;
                    w->active = true;
                    w->centre = pl.pos;
                    w->age = tickAgeOf(t) - (ring ? kPulseSecondRingS : 0.0f);
                    w->reach = lv >= 3 ? kPulseRadiusWide : kPulseRadiusWu;
                    w->damage = kPulseDamage * (lv >= 4 ? 1.5f : 1.0f);
                    w->lastR = 0.0f;
                    w->level = lv;
                    Event ev;
                    ev.type = EventType::PulseRing;
                    ev.a = owner;
                    ev.b = lv;
                    ev.c = static_cast<u8>(ring);
                    ev.x = pl.pos.x;
                    ev.y = pl.pos.y;
                    ev.age = w->age;
                    emit(ev);
                }
            });
        }
        for (PulseWave& w : pl.pulses) {
            if (!w.active) continue;
            w.age += dt;
            if (w.age <= 0.0f) continue;
            const float r = w.reach * clampf(w.age / kPulseGrowS, 0.0f, 1.0f);
            const float r0 = w.lastR;
            const Vec2 c = w.centre;
            const float reach = r + 40.0f;
            if (local)
                grid_.query(c.x - reach, c.y - reach, c.x + reach, c.y + reach, [&](u32 i) {
                    if (e.flags[i] & kFlagDead) return;
                    const float d = length(Vec2{e.x[i], e.y[i]} - c);
                    if (d + e.radius[i] <= r0 || d - e.radius[i] > r) return;  // the front did not pass it this tick
                    hitEnemy(i, w.damage, owner, Vec2{e.x[i], e.y[i]}, normalizeOr(Vec2{e.x[i], e.y[i]} - c, Vec2{1.0f, 0.0f}));
                });
            if (local && boss_.active && !boss_.dying) {
                const Vec2 to = boss_.pos - c;
                const float bd = length(to);
                if (bd - species_->boss.radius <= r && bd + species_->boss.radius > r0) {
                    // The point of the front nearest the boss's centre.
                    const Vec2 q = c + normalizeOr(to, Vec2{1.0f, 0.0f}) * clampf(bd, r0, r);
                    u8 part = 0, pair = 0;
                    float tt = 0.0f;
                    if (bossHitTest(q, q, 8.0f, part, pair, tt)) damageBoss(part, pair, w.damage, owner, q, normalizeOr(to, Vec2{1.0f, 0.0f}));
                }
            }
            w.lastR = r;
            if (w.age >= kPulseGrowS) w.active = false;
        }

        // Sentinel (play test 1): drones beside the ship (a second at level 3, a third at 5) each fire a burst (3,
        // then 4, then 5 shots) at the enemy nearest it on every off-beat (every eighth under Hyper).
        if (const u8 lv = l.riffs[riffIndex(kSentinel)]) {
            const u32 drones = sentinelDrones(lv), burst = sentinelBurst(lv);
            const Vec2 stations[kMaxDrones] = {{kSentinelOffsetX, kSentinelOffsetY}, {kSentinel2X, kSentinel2Y}, {kSentinel3X, kSentinel3Y}};
            const float follow = 1.0f - std::exp(-dt / kSentinelFollowTau);
            for (u32 k = 0; k < drones && k < kMaxDrones; ++k) pl.sentinel[k] = pl.sentinel[k] + (pl.pos + stations[k] - pl.sentinel[k]) * follow;
            const double every = hyper ? 2.0 * kTicksPerStep : kTicksPerBeat;
            const double offset = hyper ? 0.0 : 2.0 * kTicksPerStep;
            forCrossings(m0_ - offset, m1_ - offset, every, [&](double t0, i64) {
                const double t = t0 + offset;
                for (u32 dr = 0; dr < drones && dr < kMaxDrones; ++dr) {
                    const Vec2 s = pl.sentinel[dr];
                    Vec2 target{};
                    float best = kSentinelRangeWu * kSentinelRangeWu;
                    bool found = false;
                    grid_.query(s.x - kSentinelRangeWu, s.y - kSentinelRangeWu, s.x + kSentinelRangeWu, s.y + kSentinelRangeWu, [&](u32 i) {
                        if (e.flags[i] & kFlagDead) return;
                        const float d2 = lengthSq(Vec2{e.x[i], e.y[i]} - s);
                        if (d2 < best) {
                            best = d2;
                            target = Vec2{e.x[i], e.y[i]};
                            found = true;
                        }
                    });
                    if (boss_.active && !boss_.dying && m1_ >= boss_.fightTick) {
                        const float d2 = lengthSq(boss_.pos - s);
                        if (d2 < best) {
                            target = boss_.pos;
                            found = true;
                        }
                    }
                    if (!found) continue;
                    const float base = angleOf(target - s);
                    for (u32 k = 0; k < burst; ++k) {
                        const float a = base + (static_cast<float>(k) - 0.5f * static_cast<float>(burst - 1)) * kSentinelSpreadDeg * kDeg;
                        spawnShotRaw(p, s, dirOf(a), tickAgeOf(t), 1.0f, kShotSentinel, 0, kShotLifeS, 0, 0);
                    }
                    Event ev;
                    ev.type = EventType::SentinelShot;
                    ev.a = owner;
                    ev.b = static_cast<u8>(dr);
                    ev.x = s.x;
                    ev.y = s.y;
                    const Vec2 d = dirOf(base);
                    ev.x2 = d.x;
                    ev.y2 = d.y;
                    emit(ev);
                }
            });
        }
    }
}

// ---------------------------------------------------------------------------------------------- Boosts

void World::dropBoost(Vec2 pos, u8 type) {
    BoostPickups& b = boostPickups_;
    const float x = clampf(pos.x, -kArenaHalfW + 40.0f, kArenaHalfW - 40.0f);
    const float y = clampf(pos.y, -kArenaHalfH + 40.0f, kArenaHalfH - 40.0f);
    const u8 t = type < kBoostCount ? type : kPrism;
    // Loot is instanced (GAME_DESIGN 10): in co-op one orb per player, each only its owner can take.
    u32 made = 0;
    for (u32 p = 0; p < (coop() ? playerCount_ : 1u); ++p) {
        if (coop() && !players_[p].present) continue;
        if (b.count >= BoostPickups::kCap) break;
        const u32 i = b.count++;
        b.x[i] = x;
        b.y[i] = y;
        b.born[i] = m1_;
        b.type[i] = t;
        b.dead[i] = 0;
        b.owner[i] = static_cast<u8>(p);
        ++made;
    }
    if (!made) return;
    ++director_.boostsDropped;
    Event ev;
    ev.type = EventType::BoostDrop;
    ev.b = t;
    ev.x = x;
    ev.y = y;
    emit(ev);
    if (role_ == NetRole::Host && netLog_.boostCount < NetLog::kBoosts) {
        NetLog::Boost& o = netLog_.boosts[netLog_.boostCount++];
        o.x = pos.x;
        o.y = pos.y;
        o.type = type;
    }
}

void World::activateBoost(u32 p, u8 type) {
    PlayerState& pl = players_[p];
    if (type >= kBoostCount) return;
    // Two at once; the same Boost refreshes itself, a third replaces the oldest (GAME_DESIGN 6.1).
    ActiveBoost* slot = nullptr;
    for (ActiveBoost& a : pl.boosts)
        if (a.type == type) slot = &a;
    if (!slot)
        for (ActiveBoost& a : pl.boosts)
            if (a.type == kNoBoost) {
                slot = &a;
                break;
            }
    if (!slot) {
        slot = &pl.boosts[0];
        for (ActiveBoost& a : pl.boosts)
            if (a.from < slot->from) slot = &a;
        Event end;
        end.type = EventType::BoostEnd;
        end.a = static_cast<u8>(p);
        end.b = slot->type;
        emit(end);
    }
    slot->type = type;
    slot->from = m1_;
    slot->until = m1_ + static_cast<double>(boostInfo(type).bars) * kTicksPerBar;
    slot->hits = type == kHalo ? static_cast<u8>(kHaloHits) : 0;
    ++pl.boostsTaken;
    if (type == kMagnetStorm) {
        // Every Shard of this player on the map flies in.
        for (u32 i = 0; i < shards_.count; ++i)
            if (shards_.owner[i] == p) shards_.homing[i] = 1;
    }
    Event ev;
    ev.type = EventType::BoostTake;
    ev.a = static_cast<u8>(p);
    ev.b = type;
    ev.x = pl.pos.x;
    ev.y = pl.pos.y;
    emit(ev);
}

bool World::haloBlocks(u32 p, Vec2 from) {
    PlayerState& pl = players_[p];
    for (ActiveBoost& a : pl.boosts) {
        if (a.type != kHalo || a.hits == 0) continue;
        --a.hits;
        pl.invulS = kHaloInvulS;
        Event ev;
        ev.type = EventType::HaloBlock;
        ev.a = static_cast<u8>(p);
        ev.b = a.hits;
        ev.x = pl.pos.x;
        ev.y = pl.pos.y;
        ev.x2 = from.x;
        ev.y2 = from.y;
        emit(ev);
        if (a.hits == 0) {
            Event end;
            end.type = EventType::BoostEnd;
            end.a = static_cast<u8>(p);
            end.b = kHalo;
            emit(end);
            a.type = kNoBoost;
        }
        return true;
    }
    return false;
}

void World::updateBoosts() {
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        for (ActiveBoost& a : pl.boosts) {
            if (a.type == kNoBoost || m1_ < a.until) continue;
            Event ev;
            ev.type = EventType::BoostEnd;
            ev.a = static_cast<u8>(p);
            ev.b = a.type;
            emit(ev);
            a.type = kNoBoost;
        }
    }
    BoostPickups& b = boostPickups_;
    const double life = kBoostLifeBars * kTicksPerBar;
    for (u32 i = 0; i < b.count; ++i) {
        if (b.dead[i]) continue;
        if (m1_ - b.born[i] > life) {
            b.dead[i] = 1;
            continue;
        }
        for (u32 p = 0; p < playerCount_; ++p) {
            const PlayerState& pl = players_[p];
            if (!pl.alive) continue;
            if (b.owner[i] != p || !isLocal(p)) continue;  // M4: only its owner takes it, on the owner's device
            const float r = kBoostPickupWu + kPlayerVisualRadius;
            if (lengthSq(pl.pos - Vec2{b.x[i], b.y[i]}) > r * r) continue;
            b.dead[i] = 1;
            activateBoost(p, b.type[i]);
            break;
        }
    }
    for (u32 i = 0; i < b.count;) {
        if (!b.dead[i]) {
            ++i;
            continue;
        }
        const u32 l = --b.count;
        b.x[i] = b.x[l];
        b.y[i] = b.y[l];
        b.born[i] = b.born[l];
        b.type[i] = b.type[l];
        b.dead[i] = b.dead[l];
        b.owner[i] = b.owner[l];
    }
}

}  // namespace rf::sim
