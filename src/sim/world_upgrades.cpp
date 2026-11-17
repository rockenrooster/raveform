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
}  // namespace

Vec2 trioOrbPlace(Vec2 centre, u32 k, u32 n, bool fusionLast) {
    // Three on their places (top, lower left, lower right; world y grows down the screen); more (M5: Lucky Break) evenly
    // round the ring from the top. With a Fusion on offer (screens.md "The Trio", built 2026-11-15) it is the last orb and
    // stands above the ring at kTrioFusionWu; the others turn point down (lower centre, upper right, upper left): spread
    // over the ring's lower 240 degrees round the bottom, at most 120 degrees apart, so none comes near the Fusion.
    if (fusionLast && n >= 2) {
        if (k + 1 == n) return centre + Vec2{0.0f, -kTrioFusionWu};
        const u32 m = n - 1;
        const float s = m >= 2 ? std::fmin(kTau * (2.0f / 3.0f) / static_cast<float>(m - 1), kTau / 3.0f) : 0.0f;
        // Orb k's place from the bottom: an odd count 0, -s, +s, -2s, +2s; an even one -s/2, +s/2, -3s/2, +3s/2.
        const bool odd = (m % 2u) != 0u;
        const float i = odd ? static_cast<float>((k + 1u) / 2u) : static_cast<float>(k / 2u) + 0.5f;
        const float sign = odd ? ((k % 2u) ? -1.0f : 1.0f) : ((k % 2u) ? 1.0f : -1.0f);
        return centre + dirOf(0.25f * kTau + sign * i * s) * kTrioRingWu;
    }
    static const Vec2 kOrbAt[3] = {{0.0f, -1.0f}, {-0.8660254f, 0.5f}, {0.8660254f, 0.5f}};
    const Vec2 d = (n == 3 && k < 3) ? kOrbAt[k] : dirOf(-0.25f * kTau + kTau * static_cast<float>(k) / static_cast<float>(n ? n : 1u));
    return centre + d * kTrioRingWu;
}

void placeTrioOrbs(TrioState& tr) {
    const u32 n = tr.orbs < kMaxOrbs ? tr.orbs : kMaxOrbs;
    const bool fusion = n >= 2 && isFusionOrb(tr.upgrade[n - 1]);
    for (u32 k = 0; k < n; ++k) tr.orbPos[k] = trioOrbPlace(tr.centre, k, n, fusion);
}

float trioOrbScale(u8 orb) { return isRiff(orb) ? kTrioRiffScale : (isFusionOrb(orb) ? kTrioFusionScale : 1.0f); }

double World::leadGrid(u32 p) const {
    const PlayerState& pl = players_[p];
    u32 idx = static_cast<u32>(pl.loadout.mods[kDoubleTime]) + phrase_.surgeGear() + (pl.streakTier >= 4 ? 2u : (pl.streakTier >= 2 ? 1u : 0u));  // (M8 SONGLINE)
    // M5: Afterparty, its Surge a step faster a stack, and the second gear held two bars longer a stack (the band's own rule,
    // audio leadGridIndex).
    if (afterparty(p)) idx = (phrase_.surge() ? idx : static_cast<u32>(pl.loadout.mods[kDoubleTime]) + 2u) + pl.loadout.mods[kAfterparty];
    if (idx > 2) idx = 2;
    double g = kLeadGrid[idx];
    if (pl.boost(kHyper) && std::fmod(g, 2.0) == 0.0) g *= 0.5;
    return g;
}

// ---------------------------------------------------------------------------------------------- shots

void World::spawnShotRaw(u32 p, Vec2 pos, Vec2 dir, float advanceS, float damage, u8 kind, u8 flags, float life, u8 pierce,
                         u8 bounces) {
    Shots& s = shots_;
    lastShot_ = -1;
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
    s.life0[i] = life;
    s.damage[i] = damage;
    s.radius[i] = kShotRadius * (1.0f + kOverchargeRadius * static_cast<float>(players_[p].loadout.mods[kOvercharge]));
    s.lastHit[i] = 0xFFFFFFFFu;
    s.owner[i] = static_cast<u8>(p);
    s.dead[i] = 0;
    s.pierce[i] = pierce;
    s.bounces[i] = bounces;
    s.kind[i] = kind;
    s.flags[i] = flags;
    s.mods[i] = 0;
    s.forks[i] = 0;
    s.arena[i] = 0;  // M5 part 5
    s.accent[i] = shotAccent_;
    lastShot_ = static_cast<i32>(i);  // M5: what the caller may still set on it
}

void World::hitEnemy(u32 i, float dmg, u8 owner, Vec2 at, Vec2 dir) {
    Enemies& e = enemies_;
    if (e.flags[i] & kFlagDead) return;
    // M5: a Hound C's front arc sheds the shot (no damage, the spark of an impact).
    const bool shed = shieldStops(i, dir) || eliteSheds(i, dir);  // (M5 part 5: Shielded)
    // M5: Freeze Frame, a frozen body dies in one hit (and its shatter chains through the frozen ones touching it).
    const bool frozen = !shed && e.frozenUntil[i] > m1_;
    if (frozen && dmg < e.hp[i] + 1.0f) dmg = e.hp[i] + 1.0f;
    if (role_ == NetRole::Client && !shed) logClaim(i, dmg, kClaimHit);  // M4: the host applies it; a kill here is predicted
    if (!shed) {
        e.hp[i] -= dmg;
        eliteHit(i, dmg);  // M5 part 5: Regenerating's quiet, a Twin's shared bar, Armored's shatter
    }
    if (e.hp[i] <= 0.0f) {
        // M5 part 3: this device's player's kill brings its Mods' and Boosts' kill effects (Arc, Shatter, Burn, Confetti).
        const bool extras = !applyingClaim_ && owner < kMaxPlayers && isLocal(owner);
        if (extras) killExtras(i, owner);
        killEnemy(i, owner);
        if (frozen && extras) {
            e.frozenUntil[i] = -1.0;
            freezeShatter(i, owner);
        }
        return;
    }
    // M5 part 4: Skitter C blinks aside when shot.
    if (!shed && species_->get(e.species[i]).family == Family::Skitter && species_->get(e.species[i]).variant == kVariantC) skitterHop(i, dir);
    if (applyingClaim_) return;  // M4 host: the claimant's shot already showed its impact here (a cosmetic copy)
    Event ev;
    ev.type = EventType::Impact;
    ev.species = e.species[i];
    ev.b = owner;  // play test 2: the shooter's build colours the sparks; a Prism shot's lane
    ev.c = shotHasLane(hitShotFlags_) ? static_cast<u8>(shotLane(hitShotFlags_) + 3) : 0;
    ev.x = at.x;
    ev.y = at.y;
    ev.x2 = dir.x;
    ev.y2 = dir.y;
    emit(ev);
}

// ---------------------------------------------------------------------------------------------- Trio

void World::openTrio(u32 p, double closeTick, u8 kind, u16 level) {
    if (rules_.frozenLoadout) return;  // (the no-trap table's measurements)
    TrioState& tr = trio_[p];
    PlayerState& pl = players_[p];
    u8 opts[kMaxOrbs];
    const u32 n = drawTrio(pl.loadout, tr.sinceRiff, upgradeRng_, opts, tr.opened == 0);  // (a667116: the first, Split and Double-Time)
    tr.open = n > 0;
    tr.homing = -1;
    if (!n) return;
    tr.kind = kind;
    tr.levelShown = kind == kTrioLevel ? level : 0;
    tr.orbs = static_cast<u8>(n);
    tr.centre = pl.pos;
    // Keep the whole ring inside the arena, and a Fusion's orb above it (2026-11-15).
    const bool fusion = n >= 2 && isFusionOrb(opts[n - 1]);
    const float m = kTrioRingWu + 60.0f;
    const float mTop = fusion ? kTrioFusionWu + kTrioHoldRing * kTrioOrbWu * kTrioFusionScale + 30.0f : m;
    tr.centre.x = clampf(tr.centre.x, -kArenaHalfW + m, kArenaHalfW - m);
    tr.centre.y = clampf(tr.centre.y, -kArenaHalfH + mTop, kArenaHalfH - m);
    tr.openTick = m1_;
    tr.closeTick = closeTick;
    tr.holdOrb = -1;
    tr.holdTicks = 0.0;
    for (u32 k = 0; k < kMaxOrbs; ++k) tr.upgrade[k] = kNoUpgrade;
    for (u32 k = 0; k < n; ++k) {
        tr.upgrade[k] = opts[k];
        tr.level[k] = isBoostOrb(opts[k]) ? 0 : static_cast<u8>(pl.loadout.level(opts[k]) + 1);
    }
    placeTrioOrbs(tr);  // three on their places; more evenly round the ring; a Fusion above it
    ++tr.opened;
    Event ev;
    ev.type = EventType::TrioOpen;
    ev.a = static_cast<u8>(p);
    ev.b = static_cast<u8>(n);
    ev.c = tr.kind;
    ev.x = tr.centre.x;
    ev.y = tr.centre.y;
    ev.x2 = static_cast<float>(tr.levelShown);  // M4: the others hear the kind and the level (net/session.cpp)
    ev.y2 = static_cast<float>(tr.kind);
    ev.tick = closeTick;
    emit(ev);
}

void World::openTrioNow(u32 p, double closeTick, const u8* forced, u8 kind, u32 forcedCount) {
    openTrio(p, closeTick, kind, players_[p].level);
    TrioState& tr = trio_[p];
    if (!forced || !tr.open) return;
    const u32 n = forcedCount < kMaxOrbs ? forcedCount : kMaxOrbs;
    for (u32 k = 0; k < n; ++k) {
        if (!validOrb(forced[k])) continue;
        tr.upgrade[k] = forced[k];
        tr.level[k] = isBoostOrb(forced[k]) ? 0 : static_cast<u8>(players_[p].loadout.level(forced[k]) + 1);
    }
    tr.orbs = static_cast<u8>(n);
    placeTrioOrbs(tr);  // (2026-11-15: a forced Fusion stands above the ring)
}

void World::takeOrb(u32 p, u32 k) {
    TrioState& tr = trio_[p];
    const u8 u = tr.upgrade[k];
    // Play test 2: a Boost orb (every Mod and Riff at its cap) gives its Boost; M5: a Fusion orb its Fusion.
    if (isBoostOrb(u))
        activateBoost(p, boostOfOrb(u));
    else if (isFusionOrb(u))
        players_[p].loadout.fusions = static_cast<u8>(players_[p].loadout.fusions | (1u << fusionOfOrb(u)));
    else
        applyUpgrade(p, u);
    Event ev;
    ev.type = EventType::TrioTake;
    ev.a = static_cast<u8>(p);
    ev.b = u;
    ev.c = (isBoostOrb(u) || isFusionOrb(u)) ? 0 : static_cast<u8>(players_[p].loadout.level(u));
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
    const bool level = tr.kind == kTrioLevel;
    const double breakEnd = tickOfBar(phrase_.bar - phrase_.sectionBar + phrase_.sectionBars);
    if (level && tr.breakWaiting > 0 && phrase_.section == Section::Break) {
        // Play test 2: the Break began while this level-up Trio was open; its own Trio opens now with the rest of the Break
        // as its clock (after a boss, the bonus Trio first with half of what is left, then the Break's).
        const u8 n = tr.breakWaiting;
        tr.breakWaiting = 0;
        tr.queued = static_cast<u8>(n - 1);
        openTrio(p, n > 1 ? m1_ + 0.5 * (breakEnd - m1_) : breakEnd);
    } else if (!level && tr.queued > 0 && phrase_.section == Section::Break) {
        // The boss's bonus Trio was this one: the Break's own follows, with the rest of the Break as its clock.
        --tr.queued;
        openTrio(p, breakEnd);
    } else {
        tr.queued = 0;
        tr.breakWaiting = 0;
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
    if (u == kNovaStock) {
        // M5 (GAME_DESIGN 6.2): +1 Nova capacity and one Nova.
        ++pl.novaCap;
        if (pl.novas < pl.novaCap) ++pl.novas;
    }
    if (u == kExtraCell) {
        // GAME_DESIGN 6.2 (play test 1): +1 Shield cell capacity and every cell refilled; in the loop inside the capacity the
        // burn has left (GAME_DESIGN 8). Since 2026-11-16 three ranks, the third adding two (tuning.h kExtraCellAdds).
        const u8 rank = l.mods[kExtraCell];
        for (u32 k = 0; k < kExtraCellAdds[rank < 3 ? rank : 3]; ++k)
            if (pl.cellCap < kMaxCells && pl.cellCap < pl.burnCap) ++pl.cellCap;
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
                const float r = kTrioHoldRing * kTrioOrbWu * trioOrbScale(tr.upgrade[k]);
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

void World::orbitalBullets(u32 p, u32 n) {
    // Orbitals (play test 1): the shards also destroy the enemy bullets they touch (on every device, like the Nova). Play
    // test 3: only the lit ones (a broken ring's dark shards do nothing until they relight).
    Bullets& b = bullets_;
    const PlayerState& pl = players_[p];
    const double turn = pl.boost(kHyper) ? kTicksPerBar * 0.5 : kTicksPerBar;
    const double ph = std::fmod(m1_ - director_.startTick, turn) / turn;
    const float hit = kOrbitalHitWu + species_->bulletRadius;
    const float reach = kOrbitalRadiusWu + hit;
    Vec2 o[16];
    const u32 m = n < 16 ? n : 16u;
    const u32 lit = orbitalsLit(p, m1_);
    for (u32 k = 0; k < m; ++k) {
        const float a = static_cast<float>(ph) * kTau + kTau * static_cast<float>(k) / static_cast<float>(m) - 0.25f * kTau;
        o[k] = pl.pos + dirOf(a) * kOrbitalRadiusWu;
    }
    for (u32 i = 0; i < b.count; ++i) {
        if (b.dead[i]) continue;
        const float dx = b.x[i] - pl.pos.x, dy = b.y[i] - pl.pos.y;
        if (std::fabs(dx) > reach || std::fabs(dy) > reach) continue;
        for (u32 k = 0; k < m && k < lit; ++k)
            if (lengthSq(Vec2{b.x[i], b.y[i]} - o[k]) <= hit * hit) {
                b.dead[i] = 1;
                break;
            }
    }
}

u32 World::orbitalsLit(u32 p, double tick) const {
    const PlayerState& pl = players_[p < kMaxPlayers ? p : 0];
    const u8 lv = pl.loadout.riffs[riffIndex(kOrbitals)];
    const u32 n = orbitalCount(lv);
    if (!n || pl.ringBroke < 0.0) return n;
    const double span = static_cast<double>(kRingRelightBars[lv < 5 ? lv : 5]) * kTicksPerBar;
    if (tick - pl.ringBroke >= span) return n;
    // One by one: shard k relights at (k + 1) / n of the span.
    const double u = (tick - pl.ringBroke) / span;
    const u32 lit = u > 0.0 ? static_cast<u32>(std::floor(u * static_cast<double>(n))) : 0u;
    return lit < n ? lit : n;
}

bool World::ringGuards(u32 p) const {
    const PlayerState& pl = players_[p < kMaxPlayers ? p : 0];
    return pl.loadout.riffs[riffIndex(kOrbitals)] > 0 && pl.ringBroke < 0.0;
}

bool World::breakRing(u32 p, Vec2 from) {
    // GAME_DESIGN 6.3 row 1 (play test 3): while every shard is lit the ring is a spare cell. The hit breaks it instead: the
    // panic pulse, kRingInvulS of invulnerability and the Chain halved, as for any hit; no cell is taken.
    if (!ringGuards(p)) return false;
    PlayerState& pl = players_[p];
    pl.ringBroke = m1_;
    ++pl.ringBreaks;
    ++pl.hitsTaken;
    pl.invulS = kRingInvulS;
    if (!chainKept(p)) pl.chain = pl.chain / 2 > 1 ? pl.chain / 2 : 1u;
    dipS_ = kHitDipS;
    const Vec2 c = pl.pos;
    const float r2 = kPanicRadiusWu * kPanicRadiusWu;
    for (u32 b = 0; b < bullets_.count; ++b) {
        const float dx = bullets_.x[b] - c.x, dy = bullets_.y[b] - c.y;
        if (dx * dx + dy * dy <= r2) bullets_.dead[b] = 1;
    }
    Enemies& e = enemies_;
    for (u32 i = 0; i < e.count; ++i) {
        const float dx = e.x[i] - c.x, dy = e.y[i] - c.y;
        const float d2 = dx * dx + dy * dy;
        if (d2 > r2 || d2 < 1e-6f) continue;
        const float d = std::sqrt(d2);
        e.kx[i] += dx / d * kPanicShoveWu;
        e.ky[i] += dy / d * kPanicShoveWu;
    }
    Event ev;
    ev.type = EventType::Hit;
    ev.a = static_cast<u8>(p);
    ev.b = pl.cells;
    ev.c = 1;  // the ring, not a cell
    ev.x = c.x;
    ev.y = c.y;
    ev.x2 = from.x;
    ev.y2 = from.y;
    ev.tick = m1_;
    emit(ev);
    hitExtras(p);  // (Crescendo starts again, Last Light: a hit as any other)
    return true;
}

void World::extraLifeDrop(Vec2 at, bool fromMini) {
    // GAME_DESIGN 5 (play test 3; c2f113a; the owner, 2026-11-15: "An Extra Life every Track instead of every other"; it was every
    // other Track since 2026-11-14): in every Track of the set list an Extra Life drops, an orb for each player who holds fewer
    // than kMaxExtraLives (co-op: each their own; the host decides): in Track 1 from a kill in the first cycle's Drop, from Track 2
    // on from the mini-boss as it dies or warps out. None in the loop. (Before 2026-11-14 one dropped in every Track but only for a
    // player who held none, and a mini-boss that warped out dropped none: one kept unspent stopped every later drop.)
    if (role_ == NetRole::Client) return;
    const Phrase& ph = phrase_;
    if (ph.trackChange || inLoop(ph.track, director_.songs)) return;
    if (fromMini ? ph.track < 2 : (ph.track != 1 || ph.cycle != 1 || ph.section != Section::Drop)) return;
    if (director_.extraLifeTrack == ph.track) return;
    director_.extraLifeTrack = ph.track;
    for (u32 p = 0; p < playerCount_; ++p) {
        const PlayerState& pl = players_[p];
        if (!pl.present || pl.extraLives >= kMaxExtraLives) continue;
        dropBoost(at, static_cast<u8>(kPickupExtraLife + p));
    }
}

void World::takeExtraLife(u32 p) {
    PlayerState& pl = players_[p];
    pl.extraLives = static_cast<u8>(pl.extraLives < kMaxExtraLives ? pl.extraLives + 1 : kMaxExtraLives);
    ++pl.extraLivesTaken;
    Event ev;
    ev.type = EventType::ExtraLife;
    ev.a = static_cast<u8>(p);
    ev.b = 0;
    ev.x = pl.pos.x;
    ev.y = pl.pos.y;
    emit(ev);
}

void World::spendExtraLife(u32 p, Vec2 from) {
    // GAME_DESIGN 5 (play test 3): spent on the hit that would end the run (after Auto Nova): the ship comes back at once with
    // two cells, a free Nova blast, one bar of invulnerability and half its Chain, under the words EXTRA LIFE. Co-op: spent
    // instead of going down.
    PlayerState& pl = players_[p];
    pl.extraLives = static_cast<u8>(pl.extraLives ? pl.extraLives - 1 : 0);  // (one of the two it may hold)
    ++pl.extraLivesSpent;
    ++pl.hitsTaken;
    pl.cells = static_cast<u8>(pl.cellCap < kExtraLifeCells ? pl.cellCap : kExtraLifeCells);
    pl.chain = pl.chain / 2 > 1 ? pl.chain / 2 : 1u;
    const float tps = ticksPerSecond();
    pl.invulS = tps > 0.0f ? static_cast<float>(kTicksPerBar) / tps : 2.0f;
    dipS_ = kHitDipS;
    Event ev;
    ev.type = EventType::Hit;
    ev.a = static_cast<u8>(p);
    ev.b = pl.cells;
    ev.c = 2;  // an Extra Life spent
    ev.x = pl.pos.x;
    ev.y = pl.pos.y;
    ev.x2 = from.x;
    ev.y2 = from.y;
    ev.tick = m1_;
    emit(ev);
    Event el;
    el.type = EventType::ExtraLife;
    el.a = static_cast<u8>(p);
    el.b = 1;
    el.x = pl.pos.x;
    el.y = pl.pos.y;
    emit(el);
    pressNova(p, true);  // the free Nova blast (it clears the bullets and the bodies in view)
}

void World::demoRevive(u32 p, Vec2 from) {
    // Rules::demoRevive (the title's demo): as an Extra Life spent, without one held and without the words: two cells, a free
    // Nova blast, a bar of invulnerability; the Chain kept.
    PlayerState& pl = players_[p];
    ++pl.hitsTaken;
    pl.cells = static_cast<u8>(pl.cellCap < kExtraLifeCells ? pl.cellCap : kExtraLifeCells);
    const float tps = ticksPerSecond();
    pl.invulS = tps > 0.0f ? static_cast<float>(kTicksPerBar) / tps : 2.0f;
    dipS_ = kHitDipS;
    Event ev;
    ev.type = EventType::Hit;
    ev.a = static_cast<u8>(p);
    ev.b = pl.cells;
    ev.c = 2;  // (drawn as an Extra Life spent: gold rings, the ship back with its cells)
    ev.x = pl.pos.x;
    ev.y = pl.pos.y;
    ev.x2 = from.x;
    ev.y2 = from.y;
    ev.tick = m1_;
    emit(ev);
    pressNova(p, true);
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
        // bullets it touches. Play test 3: a guard ring; once broken its shards are dark until they relight, one by one.
        if (const u8 lv = l.riffs[riffIndex(kOrbitals)]) {
            const u32 n = orbitalCount(lv);
            if (pl.ringBroke >= 0.0 && orbitalsLit(p, m1_) >= n) {
                pl.ringBroke = -1.0;  // every shard lit again: it guards
                emitFx(p, kFxRingLit, pl.pos, pl.pos, static_cast<u8>(n), 0.0f, m1_);
            }
            orbitalBullets(p, n);
            const double turn = hyper ? kTicksPerBar * 0.5 : kTicksPerBar;
            forCrossings(m0_, m1_, hyper ? kTicksPerStep : 2.0 * kTicksPerStep, [&](double t, i64) {
                const double ph = std::fmod(t - director_.startTick, turn) / turn;
                const u32 lit = orbitalsLit(p, t);
                for (u32 k = 0; k < n && k < lit; ++k) {
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
                    arenaArea(owner, o, kOrbitalHitWu, kOrbitalDamage);  // M6 (A2)
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
                    // M5: Tesla Bloom, the ring arcs lightning on from everything it touches.
                    if (l.fused(kTeslaBloom) && !(e.flags[i] & kFlagDead)) arcFrom(i, owner, 1, kArcDamage, 1);
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
            if (local) arenaRing(owner, c, r0, r, w.damage);  // M6 (A2)
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
    const float x = clampf(pos.x, -halfW() + 40.0f, halfW() - 40.0f);
    const float y = clampf(pos.y, -halfH() + 40.0f, halfH() - 40.0f);
    if (isExtraLifeDrop(type)) {
        if (director_.formCount) return;  // (M8: no Extra Lives fall in a timed mode; nothing ends its run)
        // Play test 3 (GAME_DESIGN 5): an Extra Life, one orb for its owner (kPickupExtraLife + the owner).
        const u32 owner = static_cast<u32>(type - kPickupExtraLife);
        if (b.count >= BoostPickups::kCap || owner >= playerCount_) return;
        const u32 i = b.count++;
        b.x[i] = x;
        b.y[i] = y;
        b.born[i] = m1_;
        b.type[i] = kPickupExtraLife;
        b.dead[i] = 0;
        b.owner[i] = static_cast<u8>(owner);
        if (role_ == NetRole::Host && netLog_.boostCount < NetLog::kBoosts) {
            NetLog::Boost& o = netLog_.boosts[netLog_.boostCount++];
            o.x = pos.x;
            o.y = pos.y;
            o.type = type;
        }
        return;
    }
    const u8 t = (type < kBoostCount || isGelDrop(type)) ? type : kPrism;  // (M8 TRACERS: a gel's orb drops as a Boost's)
    // Loot is instanced (GAME_DESIGN 10): in co-op one orb per player, each only its owner can take. M8 versus (V4): one orb, the
    // first ship to touch it has it (owner 0xFF).
    u32 made = 0;
    const bool shared = setup_.versus;
    for (u32 p = 0; p < (coop() && !shared ? playerCount_ : 1u); ++p) {
        if (coop() && !shared && !players_[p].present) continue;
        if (b.count >= BoostPickups::kCap) break;
        const u32 i = b.count++;
        b.x[i] = x;
        b.y[i] = y;
        b.born[i] = m1_;
        b.type[i] = t;
        b.dead[i] = 0;
        b.owner[i] = shared ? static_cast<u8>(0xFF) : static_cast<u8>(p);
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
    if (rules_.frozenLoadout) return;  // (the no-trap table's measurements)
    PlayerState& pl = players_[p];
    if (type >= kBoostCount) return;
    // Three at once (kBoostSlots; two until the owner's play of 2026-11-14); the same Boost refreshes itself, one more replaces the
    // oldest (GAME_DESIGN 6.4).
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
    // M5: Encore, every Boost one bar longer a stack.
    slot->until = m1_ + static_cast<double>(boostInfo(type).bars + pl.loadout.mods[kEncore]) * kTicksPerBar;
    if (type == kFreezeFrame) freezeView(p, slot->until);  // M5: the frame's bodies stop where they are
    slot->hits = type == kHalo ? static_cast<u8>(kHaloHits) : 0;
    ++pl.boostsTaken;
    if (type == kMagnetStorm) {
        // Every Shard of this player on the map flies in (play test 2: its Cells too, while it is hurt).
        for (u32 i = 0; i < shards_.count; ++i)
            if (shards_.owner[i] == p) shards_.homing[i] = 1;
        for (u32 i = 0; i < cellPickups_.count; ++i)
            if (cellPickups_.owner[i] == p && pl.cells < pl.cellCap) cellPickups_.homing[i] = 1;
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
    // (the review's fixes, H8, 2026-11-15: a sheltered ship's Halo keeps its hits; hitPlayer turns the hit away)
    if (sheltered(p)) return false;
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
        // The owner's play of f0624e5 ("some temporary power-ups don't seem like they work (like black hole)": it waited for a
        // Nova press, which a player on the assist alone never makes): Black Hole goes off by itself the tick after it is taken,
        // as a free Nova (GAME_DESIGN 6.4 row 9), on its owner's device; a teammate's arrives here as that player's Nova.
        if (pl.alive && isLocal(p) && pl.boost(kBlackHole) && !pl.novaPending) pressNova(p, true);
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
            if ((b.owner[i] != p && b.owner[i] != 0xFF) || !isLocal(p)) continue;  // M4: only its owner takes it (M8 versus: anyone)
            const float r = kBoostPickupWu + kPlayerVisualRadius;
            if (lengthSq(pl.pos - Vec2{b.x[i], b.y[i]}) > r * r) continue;
            if (b.type[i] == kPickupExtraLife) {
                // Play test 3 (GAME_DESIGN 5): an Extra Life; a player holds at most one (a second waits where it lies).
                if (pl.extraLives >= kMaxExtraLives) continue;
                b.dead[i] = 1;
                takeExtraLife(p);
                break;
            }
            b.dead[i] = 1;
            if (isGelDrop(b.type[i])) takeGel(p, static_cast<u8>(b.type[i] - kPickupGel), Vec2{b.x[i], b.y[i]});  // (M8 TRACERS)
            else activateBoost(p, b.type[i]);
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

// ---------------------------------------------------------------------------------------------- play test 2

void World::collectShard(u32 p) {
    // GAME_DESIGN 6.1 (Shard levels): collected Shards fill the level meter; each fill is a level, whose Trio opens round
    // the ship on a coming bar line (openLevelTrios; banked while a Trio is open).
    if (p >= kMaxPlayers) return;
    PlayerState& pl = players_[p];
    collectorShard(p);  // M5: Collector
    const u32 need = levelShards(static_cast<u32>(pl.level) + 1u);
    if (++pl.levelShards < need) return;
    pl.levelShards -= need;
    if (pl.level < 0xFFFF) ++pl.level;
    if (pl.levelsBanked < 255) ++pl.levelsBanked;
    // The owner, 2026-11-15: "leveling up via shards should also give a nova": one, up to the cap; none in the loop, where
    // the Breaks give none back either (GAME_DESIGN 8, kLoopBreakRefills).
    const bool nova = pl.novas < pl.novaCap && (kLoopBreakRefills || !inLoop(phrase_.track, director_.songs));
    if (nova) ++pl.novas;
    if (setup_.mode == Mode::Songline && pl.cells < pl.cellCap) ++pl.cells;  // (M8 songline.md rule 13: no Trio; a level gives a cell back too)
    if (setup_.mode == Mode::Jam && !setup_.versus && isLocal(p) && role_ != NetRole::Client)
        dropBoost(pl.pos + Vec2{60.0f, -40.0f}, static_cast<u8>(upgradeRng_.below(kBoostCount)));  // (M8 jam.md rule 7: a present)
    Event ev;
    ev.type = EventType::LevelUp;
    ev.a = static_cast<u8>(p);
    ev.c = nova ? 1 : 0;  // a Nova came with it (the HUD's Nova star flashes)
    ev.f = static_cast<float>(pl.level);
    ev.x = pl.pos.x;
    ev.y = pl.pos.y;
    emit(ev);
}

void World::openLevelTrios(u32 bar) {
    // A banked level opens its Trio round the ship on this bar line, for four bars, in the fight (the oldest level
    // first). Not while a Trio is open (the next waits for the bar line after it closes), and not when a boss's
    // Track-change Break begins within its four bars: the boss's Trio keeps its place and the level waits for it
    // (a667116: a cycle's Break deals no Trio, so it no longer holds a level back).
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        TrioState& tr = trio_[p];
        if (!pl.levelsBanked || !pl.present || !pl.alive || !isLocal(p) || tr.open || phrase_.outro) continue;  // (M8: no Trio in an outro)
        bool breakSoon = false;
        for (u32 k = 1; k <= kLevelTrioClearBars && !breakSoon; ++k) {
            const Phrase ph = phraseAt(director_, bar + k);
            breakSoon = ph.section == Section::Break && ph.sectionBar == 0 && ph.trackChange;
        }
        if (breakSoon) continue;
        const u16 level = static_cast<u16>(pl.level - pl.levelsBanked + 1u);
        --pl.levelsBanked;
        ++pl.levelTrios;
        openTrio(p, tickOfBar(bar + kLevelTrioBars), kTrioLevel, level);
    }
}

bool World::sheltered(u32 p) const {
    // A ship inside one of its own level-up Trio's thin orb rings cannot be hurt (until the clock ends).
    if (p >= kMaxPlayers) return false;
    const TrioState& tr = trio_[p];
    if (!tr.open || tr.kind != kTrioLevel || tr.homing >= 0) return false;
    const Vec2 s = players_[p].pos;
    for (u32 k = 0; k < tr.orbs; ++k) {
        const float r = kTrioHoldRing * kTrioOrbWu * trioOrbScale(tr.upgrade[k]);
        if (lengthSq(tr.orbPos[k] - s) <= r * r) return true;
    }
    return false;
}

void World::shelterTrios() {
    // The orbs of a level-up Trio are shelters: a body touching an orb's thin ring is set outside it and shoved off (on
    // every device: the Trio is known everywhere). The boss is not moved.
    Enemies& e = enemies_;
    for (u32 p = 0; p < playerCount_; ++p) {
        const TrioState& tr = trio_[p];
        if (!tr.open || tr.kind != kTrioLevel || tr.homing >= 0) continue;
        for (u32 k = 0; k < tr.orbs; ++k) {
            const Vec2 o = tr.orbPos[k];
            const float ring = kTrioHoldRing * kTrioOrbWu * trioOrbScale(tr.upgrade[k]);
            const float reach = ring + kQuerySlackWu;
            grid_.query(o.x - reach, o.y - reach, o.x + reach, o.y + reach, [&](u32 i) {
                if (e.flags[i] & kFlagDead) return;
                const Vec2 d = Vec2{e.x[i], e.y[i]} - o;
                const float rr = ring + e.radius[i];
                const float d2 = lengthSq(d);
                if (d2 >= rr * rr) return;
                const float l = std::sqrt(d2);
                const Vec2 n = l > 1e-3f ? d * (1.0f / l) : dirOf(static_cast<float>(e.serial[i] % 360u) * kDeg);
                e.x[i] = o.x + n.x * rr;
                e.y[i] = o.y + n.y * rr;
                e.kx[i] += n.x * kShelterShoveWu;
                e.ky[i] += n.y * kShelterShoveWu;
            });
        }
    }
}

void World::cellClock() {
    // Cells (GAME_DESIGN 6.1): a hurt player's Cell comes due about every 40 s with one cell missing and every 15 s on the
    // last cell (linear in between); the next kill near it drops the Cell (dropCellFor). At full health the clock rests.
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        // GAME_DESIGN 8 (the halo burns down): in the loop no kill drops a Cell (a boss's two remain).
        if (!pl.present || !pl.alive || !isLocal(p) || pl.cells >= pl.cellCap || inLoop(phrase_.track, director_.songs)) {
            pl.cellDue = 0.0f;
            continue;
        }
        float every = kCellEveryLastS;
        if (pl.cells > 1 && pl.cellCap > 2) {
            const float u = static_cast<float>(pl.cells - 1u) / static_cast<float>(pl.cellCap - 2u);  // 0 the last cell, 1 one missing
            every = kCellEveryLastS + (kCellEveryOneMissingS - kCellEveryLastS) * clampf(u, 0.0f, 1.0f);
        }
        every *= loopRamp(kLoopCellRamp, phrase_.track, director_.songs);  // the loop stretches the clock (GAME_DESIGN 8)
        pl.cellDue = std::fmin(pl.cellDue + kDt / every, 1.0f);
    }
}

u32 World::cellsLoose(u32 p) const {
    u32 n = 0;
    for (u32 i = 0; i < cellPickups_.count; ++i) n += (!cellPickups_.dead[i] && cellPickups_.owner[i] == p) ? 1u : 0u;
    return n;
}

bool World::dropCellFor(u32 p, Vec2 at, u32& rngState) {
    PlayerState& pl = players_[p];
    if (spawner_.mode != Spawner::Mode::Slice || pl.cellDue < 1.0f || pl.cells >= pl.cellCap || !pl.alive) return false;
    if (cellsLoose(p) >= kCellsLooseMax) return false;  // at most two loose (it stays due)
    pl.cellDue = 0.0f;
    spawnCell(at, dirOf(randRange(rngState, 0.0f, kTau)) * randRange(rngState, kShardBurstLo, kShardBurstHi), static_cast<u8>(p));
    return true;
}

void World::spawnCell(Vec2 at, Vec2 vel, u8 owner) {
    CellPickups& c = cellPickups_;
    if (c.count >= CellPickups::kCap || owner >= kMaxPlayers) return;
    const u32 i = c.count++;
    c.x[i] = c.px[i] = clampf(at.x, -kArenaHalfW + 40.0f, kArenaHalfW - 40.0f);
    c.y[i] = c.py[i] = clampf(at.y, -kArenaHalfH + 40.0f, kArenaHalfH - 40.0f);
    c.vx[i] = vel.x;
    c.vy[i] = vel.y;
    c.born[i] = m1_;
    c.owner[i] = owner;
    c.homing[i] = 0;
    c.dead[i] = 0;
    Event ev;
    ev.type = EventType::CellDrop;
    ev.a = owner;
    ev.x = c.x[i];
    ev.y = c.y[i];
    emit(ev);
}

void World::updateCells(float dt) {
    // A Cell drifts to a stop, homes in on its player inside the magnet while that player is hurt (Magnet as for Shards),
    // is taken when its ring touches the ship (a Shield cell back) and fades after eight bars. At full health it waits.
    CellPickups& c = cellPickups_;
    const double life = kCellLifeBars * kTicksPerBar;
    const float drag = std::exp(-kShardDrag * dt);
    for (u32 i = 0; i < c.count; ++i) {
        c.px[i] = c.x[i];
        c.py[i] = c.y[i];
        if (c.dead[i]) continue;
        if (m1_ - c.born[i] > life) {
            c.dead[i] = 1;
            continue;
        }
        PlayerState& pl = players_[c.owner[i]];
        const bool hurt = pl.present && pl.alive && pl.cells < pl.cellCap;
        Vec2 to{};
        float dist = 0.0f;
        if (hurt) {
            to = pl.pos - Vec2{c.x[i], c.y[i]};
            dist = length(to);
            if (dist <= kCellPickupWu) {
                c.dead[i] = 1;
                ++pl.cells;
                ++pl.cellsTaken;
                Event ev;
                ev.type = EventType::CellTake;
                ev.a = c.owner[i];
                ev.b = pl.cells;
                ev.c = 0;
                ev.x = c.x[i];
                ev.y = c.y[i];
                emit(ev);
                continue;
            }
            const float magnet = kShardMagnetWu * rules_.magnetMultiplier * (1.0f + kMagnetPerStack * static_cast<float>(pl.loadout.mods[kMagnet]));
            if (dist < magnet) c.homing[i] = 1;
        } else if (c.homing[i] == 1) {
            c.homing[i] = 0;  // (a boss's phase Cell, homing 2, keeps drifting to its player once it is hurt again)
        }
        if (c.homing[i] == 1 && dist > 1e-3f) {
            const float speed = std::fmin(length(Vec2{c.vx[i], c.vy[i]}) + kShardHomeAccel * dt, kShardHomeMax);
            const Vec2 d = to * (1.0f / dist);
            c.vx[i] = d.x * speed;
            c.vy[i] = d.y * speed;
        } else if (c.homing[i] == 2 && hurt && dist > 1e-3f) {
            // M6 (GAME_DESIGN 5): a boss's phase Cell drifts to its player
            const Vec2 d = to * (1.0f / dist);
            c.vx[i] = d.x * kBossPhaseCellDriftWu;
            c.vy[i] = d.y * kBossPhaseCellDriftWu;
        } else {
            c.vx[i] *= drag;
            c.vy[i] *= drag;
        }
        c.x[i] = clampf(c.x[i] + c.vx[i] * dt, -kArenaHalfW, kArenaHalfW);
        c.y[i] = clampf(c.y[i] + c.vy[i] * dt, -kArenaHalfH, kArenaHalfH);
    }
    for (u32 i = 0; i < c.count;) {
        if (!c.dead[i]) {
            ++i;
            continue;
        }
        const u32 l = --c.count;
        c.x[i] = c.x[l];
        c.y[i] = c.y[l];
        c.px[i] = c.px[l];
        c.py[i] = c.py[l];
        c.vx[i] = c.vx[l];
        c.vy[i] = c.vy[l];
        c.born[i] = c.born[l];
        c.owner[i] = c.owner[l];
        c.homing[i] = c.homing[l];
        c.dead[i] = c.dead[l];
    }
}

u32 World::regenPeriod(u32 p) const {
    // Regen's period by stacks (16, 8, 4 bars); in the loop Regen stops (GAME_DESIGN 8, the halo burns down: no more refills
    // means all of them), which leaves kLoopRegenStretch nothing to stretch.
    const u32 every = p < kMaxPlayers ? regenEveryBars(players_[p].loadout.mods[kRegen]) : 0u;
    if (!every || inLoop(phrase_.track, director_.songs)) return 0;
    return static_cast<u32>(std::ceil(static_cast<float>(every) * loopRamp(kLoopRegenStretch, phrase_.track, director_.songs) - 1e-3f));
}

void World::burnHalos() {
    // GAME_DESIGN 8 (the halo burns down): every ship loses kLoopHaloBurn cells of capacity (never below one) and the cells
    // held above it, on the loop Track's first downbeat; no panic ring, no invulnerability, the Chain untouched. Every
    // device burns every present ship alike (co-op: the owners' Stats agree), down ones too (a revive brings one cell back).
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        if (!pl.present) continue;
        const u8 before = pl.cells;
        const u8 cap = pl.cellCap > kLoopHaloBurn + 1u ? static_cast<u8>(pl.cellCap - kLoopHaloBurn) : static_cast<u8>(1);
        pl.cellCap = cap;
        pl.burnCap = cap;
        if (pl.cells > cap) pl.cells = cap;
        Event ev;
        ev.type = EventType::HaloBurn;
        ev.a = static_cast<u8>(p);
        ev.b = cap;
        ev.c = before;
        ev.x = pl.pos.x;
        ev.y = pl.pos.y;
        emit(ev);
    }
}

u32 World::shooterCount(u32 base, u32& rngState) const {
    // The loop raises every shooter's bullet count a Track at a time (GAME_DESIGN 8): base x the ramp, the fraction by
    // chance (the enemy's own stream), and only while a quarter of the enemy bullet pool is free (inside the pools).
    if (loopShooters_ <= 1.0f) return base;
    const float want = static_cast<float>(base) * loopShooters_;
    u32 n = static_cast<u32>(want);
    if (rand01(rngState) < want - static_cast<float>(n)) ++n;
    if (bullets_.count + n > Bullets::kCap - Bullets::kCap / 4) return base;
    return n;
}

float World::bulletTrackScale() const {
    // GAME_DESIGN 8, "Enemy bullets by Track" (the owner, 2026-11-14, 0449eb3): every enemy bullet at 0.5 in Tracks 1-3 and 0.75
    // in Tracks 4-7; Tracks 8-10 and the loop as written. (f0cd14f's 0.25 in Tracks 9 and 10 was a misreading of the owner's
    // "red line enemy projectiles in track 9", which were Fata Morgana's sand streaks: undone.)
    const u32 t = phrase_.track;
    if (rules_.bulletsAsWritten || t == 0 || inLoop(t, director_.songs)) return 1.0f;
    return t <= 3u ? kBulletsEarly : (t <= 7u ? kBulletsMiddle : 1.0f);
}

namespace {
i64 floorDiv(i64 a, i64 b) {
    i64 q = a / b;
    if ((a % b) != 0 && ((a < 0) != (b < 0))) --q;
    return q;
}
}  // namespace

u32 World::trackCount(u32 n, u32 least, i64 volley, u32 salt, bool tally) const {
    // A volley of n bullets at the Track's count (GAME_DESIGN 8, 0449eb3), so that over its volleys a pattern fires n x f.
    // A pattern loses bullets evenly (its caller spaces them by the count it is given, so a fan or a ring keeps its symmetry),
    // never below `least`, the smallest its page allows: where n x f is not whole the volleys alternate between the counts
    // either side of it (a fan of five at 0.5: 2, 3, 2, 3). A volley the count cannot thin (one bullet, a mirrored pair, a
    // pattern at its smallest) fires whole on that share of its volleys: every second at 0.5, three in four at 0.75, on its beat
    // as before. `volley`: the volley's index on its own grid (consecutive volleys, consecutive numbers), `salt` an enemy's
    // serial so neighbours do not skip together. Integers only: every device decides the same.
    const float f = bulletTrackScale();
    if (tally) bulletsAsked_ += n;
    if (f >= 1.0f || n == 0) {
        if (tally) bulletsKept_ += n;
        return n;
    }
    constexpr i64 kDen = 64;
    const i64 T = static_cast<i64>(n) * static_cast<i64>(std::lround(f * static_cast<float>(kDen)));  // n x f, in 64ths
    const i64 i = volley + static_cast<i64>(salt & 0xFFFFu);
    if (least < 1u) least = 1u;
    u32 out = 0;
    if (T >= static_cast<i64>(least) * kDen) {
        out = static_cast<u32>(floorDiv((i + 1) * T, kDen) - floorDiv(i * T, kDen));
    } else {
        const u32 k = n < least ? n : least;
        const i64 D = static_cast<i64>(k) * kDen;
        out = floorDiv((i + 1) * T, D) - floorDiv(i * T, D) > 0 ? k : 0u;
    }
    if (tally) bulletsKept_ += out;
    return out;
}

u32 World::bossCount(u32 n, u32 least, i64 volley, u32 salt, float extra) const {
    // A fight's pattern: its page's count with the bullet lever and the loop (bossBulletDensity), then the Track's (trackCount).
    return trackCount(bossScaledCount(n, bossBulletDensity() * extra, least), least, volley, salt);
}

i64 World::volleyIndex(double tick, double every) {
    return static_cast<i64>(std::floor(tick / (every > 0.0 ? every : kTicksPerBeat) + 0.5));
}

void World::regenBar() {
    // Regen (Mod 18, play test 2): while a cell is missing it grows back by itself every 16, 8, then 4 bars (counted on bar
    // lines; the view draws the growing arc), stretched in the loop. Each device its own players'.
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        const u32 every = regenPeriod(p);
        if (!every || !pl.present || !pl.alive || !isLocal(p) || pl.cells >= pl.cellCap) {
            pl.regenBars = 0;
            continue;
        }
        if (++pl.regenBars < every) continue;
        pl.regenBars = 0;
        ++pl.cells;
        ++pl.cellsRegrown;
        Event ev;
        ev.type = EventType::CellTake;
        ev.a = static_cast<u8>(p);
        ev.b = pl.cells;
        ev.c = 1;
        ev.x = pl.pos.x;
        ev.y = pl.pos.y;
        emit(ev);
    }
}

}  // namespace rf::sim
