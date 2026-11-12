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
        if (lv == 0 && u == kSentinel) pl.sentinel = pl.prevSentinel = pl.pos;
        ++lv;
        return;
    }
    ++l.mods[u];
    if (u == kExtraCell) {
        // GAME_DESIGN 6.2: +1 Shield cell capacity and refill one.
        if (pl.cellCap < kMaxCells) ++pl.cellCap;
        if (pl.cells < pl.cellCap) ++pl.cells;
    }
}

void World::updateTrios() {
    for (u32 p = 0; p < playerCount_; ++p) {
        TrioState& tr = trio_[p];
        PlayerState& pl = players_[p];
        if (!tr.open || !isLocal(p)) continue;  // M4: another player's Trio is theirs to take (we hear of it)
        if (!pl.alive) {
            tr.open = false;
            continue;
        }
        if (tr.homing >= 0) {
            // The clock ran out: the chosen orb flies to the ship.
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
        for (u32 k = 0; k < tr.orbs; ++k) {
            const float r = kTrioOrbWu * (isRiff(tr.upgrade[k]) ? kTrioRiffScale : 1.0f) + kPlayerHitRadius - kTrioTouchWu;
            if (lengthSq(tr.orbPos[k] - pl.pos) <= r * r) {
                takeOrb(p, k);
                break;
            }
        }
        if (tr.open && tr.homing < 0 && m1_ >= tr.closeTick) tr.homing = static_cast<i8>(upgradeRng_.below(tr.orbs));
    }
}

// ---------------------------------------------------------------------------------------------- Riffs

void World::updateRiffs(float dt) {
    Enemies& e = enemies_;
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        pl.prevSentinel = pl.sentinel;
        if (!pl.alive) continue;
        const Loadout& l = pl.loadout;
        const bool hyper = pl.boost(kHyper);
        const u8 owner = static_cast<u8>(p);
        const bool local = isLocal(p);  // M4: another player's Riffs are drawn here, their hits are its own device's

        // Orbitals (GAME_DESIGN 6.3): level + 1 shards circle the ship once a bar (twice under Hyper); each
        // hurts what it touches once an eighth note (a sixteenth under Hyper).
        if (const u8 lv = l.riffs[riffIndex(kOrbitals)]) {
            const u32 n = lv + 1u;
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

        // Pulse Ring: a ring every two beats (every beat from level 2; Hyper doubles), a second ring from
        // level 3, wider from level 4, heavier at level 5. Each enemy the front passes takes its damage once.
        if (const u8 lv = l.riffs[riffIndex(kPulseRing)]) {
            double every = lv >= 2 ? kTicksPerBeat : 2.0 * kTicksPerBeat;
            if (hyper) every *= 0.5;
            forCrossings(m0_, m1_, every, [&](double t, i64) {
                for (u32 ring = 0; ring < (lv >= 3 ? 2u : 1u); ++ring) {
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
                    w->reach = lv >= 4 ? kPulseRadiusL4 : kPulseRadiusWu;
                    w->damage = kPulseDamage * (lv >= 5 ? 1.5f : 1.0f);
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

        // Sentinel: a drone beside the ship fires a burst (its level in shots) at the nearest enemy on every
        // off-beat (every eighth under Hyper).
        if (const u8 lv = l.riffs[riffIndex(kSentinel)]) {
            const Vec2 station = pl.pos + Vec2{kSentinelOffsetX, kSentinelOffsetY};
            pl.sentinel = pl.sentinel + (station - pl.sentinel) * (1.0f - std::exp(-dt / kSentinelFollowTau));
            const double every = hyper ? 2.0 * kTicksPerStep : kTicksPerBeat;
            const double offset = hyper ? 0.0 : 2.0 * kTicksPerStep;
            forCrossings(m0_ - offset, m1_ - offset, every, [&](double t0, i64) {
                const double t = t0 + offset;
                const Vec2 s = pl.sentinel;
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
                if (!found) return;
                const float base = angleOf(target - s);
                for (u32 k = 0; k < lv; ++k) {
                    const float a = base + (static_cast<float>(k) - 0.5f * static_cast<float>(lv - 1)) * kSentinelSpreadDeg * kDeg;
                    spawnShotRaw(p, s, dirOf(a), tickAgeOf(t), 1.0f, kShotSentinel, 0, kShotLifeS, 0, 0);
                }
                Event ev;
                ev.type = EventType::SentinelShot;
                ev.a = owner;
                ev.x = s.x;
                ev.y = s.y;
                const Vec2 d = dirOf(base);
                ev.x2 = d.x;
                ev.y2 = d.y;
                emit(ev);
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
