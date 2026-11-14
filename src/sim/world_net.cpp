// Co-op in the world (M4; TECH_SPEC 8.1, GAME_DESIGN 10). Every device simulates the whole world; the host's
// director spawns and its word on enemy health stands; each player's own device decides that player's hits,
// Shards, Trios, Boosts, downs and revives. Here: the roles, the network's operations (applied inside a tick
// so their events reach the view and the band like any other), the host's log of spawns and verdicts, the
// client's claims and predicted kills, corrections, the census, enemies' choice of player, down and revive,
// and the crowd for every player's frame. None of it runs in solo.

#include <cmath>
#include <cstring>

#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
// Trio orbs: top, lower left, lower right (as world_upgrades.cpp places them).
constexpr u32 kIdMask = 8192u - 1u;
u32 idHash(u32 serial) { return (serial * 2654435761u) >> (32u - 13u); }
u64 censusMix(u32 serial) {
    u64 z = static_cast<u64>(serial) + 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
}  // namespace

// ---------------------------------------------------------------------------------------------- roles

void World::setNet(NetRole role, u8 localMask) {
    role_ = role;
    localMask_ = role == NetRole::Solo ? 0xF : static_cast<u8>(localMask & 0xF);
    viewPlayer_ = 0;
    for (u32 p = 0; p < kMaxPlayers; ++p)
        if (isLocal(p)) {
            viewPlayer_ = p;
            break;
        }
    std::memset(idKey_, 0, sizeof idKey_);
    if (role_ != NetRole::Solo)
        for (u32 i = 0; i < enemies_.count; ++i) idInsert(enemies_.serial[i], enemies_.handle[i] & 0xFFFFu);
    netLog_.clear();
    netLog_.overflow = 0;
    netOpCount_ = 0;
}

u32 World::presentCount() const {
    u32 n = 0;
    for (u32 p = 0; p < playerCount_; ++p) n += players_[p].present ? 1u : 0u;
    return n ? n : 1u;
}

i32 World::firstAlive() const {
    for (u32 p = 0; p < playerCount_; ++p)
        if (players_[p].alive) return static_cast<i32>(p);
    return -1;
}

u32 World::aliveCap() const { return kDirectorAliveCap + (coop() ? kCoopAliveCapPerPlayer * (presentCount() - 1) : 0u); }

float World::coopBudgetScale() const {
    return coop() ? 1.0f + kCoopBudgetPerPlayer * static_cast<float>(presentCount() - 1) : 1.0f;
}

i32 World::targetOf(u32 i) {
    Enemies& e = enemies_;
    if (!coop()) return nearestPlayer(e.x[i], e.y[i]);
    // GAME_DESIGN 10: enemies split their attention by distance with some stickiness. Each one looks again on
    // every kTargetRethinkSteps-th sixteenth (staggered by spawn number, so on the same musical steps on every
    // device) and changes only to a player kTargetStick times nearer than its own.
    const u8 t = e.target[i];
    const bool valid = t < playerCount_ && players_[t].alive;
    bool rethink = !valid;
    if (!rethink) {
        const i64 k0 = static_cast<i64>(std::floor(m0_ / kTicksPerStep)) + 1, k1 = static_cast<i64>(std::floor(m1_ / kTicksPerStep));
        for (i64 k = k0; k <= k1 && !rethink; ++k)
            rethink = ((((k + static_cast<i64>(e.serial[i])) % kTargetRethinkSteps) + kTargetRethinkSteps) % kTargetRethinkSteps) == 0;
    }
    if (!rethink) return t;
    const i32 n = nearestPlayer(e.x[i], e.y[i]);
    if (n < 0) {
        e.target[i] = 0xFF;
        return -1;
    }
    if (valid && n != static_cast<i32>(t)) {
        const Vec2 at{e.x[i], e.y[i]};
        const float dn = lengthSq(players_[n].pos - at), dt = lengthSq(players_[t].pos - at);
        if (dn >= kTargetStick * kTargetStick * dt) return t;
    }
    e.target[i] = static_cast<u8>(n);
    return n;
}

Vec2 World::formationCentre(u32* player) {
    if (!coop()) {
        if (player) *player = 0;
        return players_[0].pos;
    }
    // Co-op: the players in turn, so every frame gets its formations.
    for (u32 tries = 0; tries < playerCount_; ++tries) {
        const u32 p = formationTurn_++ % playerCount_;
        if (!players_[p].alive) continue;
        if (player) *player = p;
        return players_[p].pos;
    }
    if (player) *player = 0;
    return players_[0].pos;
}

void World::scheduleRun(double bar, u64 seed, u32 players, u16 runId, u8 presentMask) {
    restartPending_ = true;
    restartAt_ = bar;
    restartSeed_ = seed;
    restartPlayers_ = players < 1 ? 1 : (players > kMaxPlayers ? kMaxPlayers : players);
    restartMask_ = presentMask ? presentMask : 0xF;
    nextRunId_ = runId;
}

// ---------------------------------------------------------------------------------------------- operations

bool World::netPush(const NetOp& op) {
    if (netOpCount_ >= kNetOps) {
        ++counters_.netOpsDropped;
        return false;
    }
    netOps_[netOpCount_++] = op;
    return true;
}

void World::applyNetOps() {
    // In arrival order; those of a later run wait for the reset, those of an earlier one are stale.
    u32 keep = 0;
    for (u32 k = 0; k < netOpCount_; ++k) {
        const NetOp& op = netOps_[k];
        const std::int16_t ahead = static_cast<std::int16_t>(static_cast<u16>(op.run - runId_));
        if (ahead > 0) {
            netOps_[keep++] = op;
            continue;
        }
        if (ahead < 0) continue;
        if (op.kind == NetOp::Player && op.a == kPeLeave && op.tick > m1_) {
            // M4 stage B: a player leaves on its tick, the same on every device.
            netOps_[keep++] = op;
            continue;
        }
        applyNetOp(op);
    }
    netOpCount_ = keep;
}

void World::applyNetOp(const NetOp& op) {
    Enemies& e = enemies_;
    switch (op.kind) {
        case NetOp::Spawn: {
            if (role_ != NetRole::Client || indexOfSerial(op.serial) >= 0) break;
            if (op.tick <= m1_) ++counters_.lateSpawns;  // it landed while it travelled: solid at once
            const i32 i = spawnEnemyAs(op.species, Vec2{op.x, op.y}, op.f, op.tick, op.serial);
            if (i >= 0) {
                e.group[i] = op.group;
                e.flags[i] = static_cast<u8>(e.flags[i] | (op.a & (kFlagArriving | kFlagCalled)));
            }
            break;
        }
        case NetOp::Death: {
            if (role_ != NetRole::Client) break;
            const i32 i = indexOfSerial(op.serial);
            if (i < 0) {
                ++counters_.unknownIds;
                break;
            }
            finishKill(static_cast<u32>(i), op.b, op.a);
            break;
        }
        case NetOp::Claim: {
            if (role_ != NetRole::Host) break;
            const i32 i = indexOfSerial(op.serial);
            if (i < 0 || (e.flags[i] & kFlagDead)) break;  // already dead here: its verdict has gone out
            const u8 owner = op.player;
            if (op.a == kClaimHit) {
                applyingClaim_ = true;
                hitEnemy(static_cast<u32>(i), op.f, owner, Vec2{e.x[i], e.y[i]}, Vec2{1.0f, 0.0f});
                applyingClaim_ = false;
            } else if (e.state[i] == kWarping) {
                // A Nova's front reached it before it was solid: it dissolves (no Shards, no score).
                e.flags[i] |= kFlagDead;
                Event ev;
                ev.type = EventType::Kill;
                ev.species = e.species[i];
                ev.b = 0xFF;
                ev.c = 1;
                ev.x = e.x[i];
                ev.y = e.y[i];
                emit(ev);
                logDeath(static_cast<u32>(i), 0xFF, kDeathDissolve);
            } else {
                killEnemy(static_cast<u32>(i), owner);
            }
            break;
        }
        case NetOp::BossClaim:
            if (role_ == NetRole::Host && op.a >= kArenaPartBase) {  // M5 part 5: a hit on an arena body
                damageBody(static_cast<u32>(op.a - kArenaPartBase), op.f, op.player, Vec2{}, true);
                break;
            }
            if (role_ == NetRole::Host && boss_.active && !boss_.dying && m1_ >= boss_.fightTick)
                damageBossFinal(op.a, op.b, op.f, op.player, boss_.pos, Vec2{1.0f, 0.0f}, -1.0f);
            break;
        case NetOp::BossEnter:
            if (role_ == NetRole::Client && (!boss_.active || boss_.dying) && op.c < kBossKinds)
                spawnBossNow(op.tick, Vec2{op.x, op.y}, static_cast<BossKind>(op.c), Vec2{op.x2, op.y2});
            break;
        case NetOp::BossPair:
            if (op.a >= kArenaOpBase) {  // M5 part 5: the host's arena decisions
                applyArenaOp(op);
                break;
            }
            if (role_ != NetRole::Client || !boss_.active || boss_.dying) break;
            if (op.a < kBossParts) killBossPart(op.a, op.b);
            else if (op.a >= kBossGeomBase) bossGeom(static_cast<u8>(op.a - kBossGeomBase), Vec2{op.x, op.y}, Vec2{op.x2, op.y2}, op.tick, true);
            else if (op.a >= kBossStepBase) bossStep(static_cast<u8>(op.a - kBossStepBase), op.b);
            break;
        case NetOp::BossDeath:
            if (role_ == NetRole::Client && boss_.active && !boss_.dying) {
                const bool mini = isMiniBoss(boss_.kind);
                killBoss(op.b);
                // The host's bar line, not this device's reading of it.
                if (!mini) {
                    director_.changeBar = op.u;
                } else {
                    const u32 natural = miniNaturalEnd();
                    const u32 hold = op.u > natural ? op.u - natural : 0u;
                    director_.midHold = static_cast<u8>(hold < 8u ? hold : 8u);
                    director_.midHolding = false;
                }
            }
            break;
        case NetOp::BoostDrop:
            if (role_ == NetRole::Client) dropBoost(Vec2{op.x, op.y}, op.a);
            break;
        case NetOp::Player: applyPlayerOp(op); break;
        case NetOp::Schedule:
            scheduleRun(op.tick, static_cast<u64>(op.u) | (static_cast<u64>(op.u2) << 32), op.a, op.species, op.b ? op.b : 0xF);
            break;
        case NetOp::Director:
            if (role_ == NetRole::Client) {
                DirectorSync d;
                d.startTick = op.tick;
                d.trackBar = op.u;
                d.changeBar = op.u2;
                d.track = op.a;
                d.bossSpawned = op.b != 0;
                d.assist = op.f;
                d.midHold = op.c;
                d.midHolding = op.opts[0] != 0;
                d.miniSpawned = op.opts[1] != 0;
                d.lastMini = op.opts[2];
                netDirector(d);
            }
            break;
        case NetOp::Census: {
            // The host's enemies at this point of its stream against ours (spawns and verdicts before it are in).
            u32 count = 0;
            const u64 h = netCensus(count);
            ++counters_.censusChecks;
            const u64 want = static_cast<u64>(op.serial) | (static_cast<u64>(op.u2) << 32);
            if (count != op.u || h != want) {
                ++counters_.censusMismatches;
                counters_.censusLastDiff = static_cast<i32>(count) - static_cast<i32>(op.u);
            }
            break;
        }
    }
}

void World::applyPlayerOp(const NetOp& op) {
    const u32 p = op.player;
    if (p >= kMaxPlayers || isLocal(p)) return;  // never over this device's own player
    PlayerState& pl = players_[p];
    switch (op.a) {
        case kPeNova: {
            if (!pl.alive || pl.novaPending) break;
            pl.novaPending = true;
            pl.novaBlastTick = op.tick;  // the owner's beat
            {
                const Vec2 at{op.x, op.y};
                blackHoleNova(p, op.tick, &at);  // M5: Black Hole's singularity pulls here too (where and when its owner's does)
            }
            pl.novaInvulS = 1e9f;
            ++pl.novasUsed;
            Event ev;
            ev.type = EventType::NovaPress;
            ev.a = static_cast<u8>(p);
            ev.x = pl.pos.x;
            ev.y = pl.pos.y;
            ev.tick = op.tick;
            emit(ev);
            break;
        }
        case kPeWell: {
            // M5: a teammate's Singularity, placed where its owner placed it, pulling until its blow (the owner's beat).
            pl.wellAt = Vec2{op.x, op.y};
            pl.wellFrom = m1_;
            pl.wellBlow = op.tick;
            emitFx(p, kFxWell, pl.wellAt, pl.wellAt, op.b, 0.0f, op.tick);
            break;
        }
        case kPeDash: {
            ++pl.dashes;
            dashExtras(p, Vec2{op.x, op.y}, Vec2{op.x2, op.y2}, op.b != 0);  // M5: Afterimage's decoy here too (Shockstep: its owner's)
            pl.lastDashFrom = Vec2{op.x, op.y};  // M5 part 4: Mimic C
            pl.lastDashTo = Vec2{op.x2, op.y2};
            pl.lastDashTick = m1_;
            Event ev;
            ev.type = EventType::Dash;
            ev.a = static_cast<u8>(p);
            ev.b = op.b;
            ev.x = op.x;
            ev.y = op.y;
            ev.x2 = op.x2;
            ev.y2 = op.y2;
            emit(ev);
            break;
        }
        case kPeHit: {
            // Its panic pulse happens here too (bullets and shoves near it), so the worlds stay alike.
            pl.cells = op.b;
            pl.invulS = kInvulnerableS;
            ++pl.hitsTaken;
            const Vec2 c{op.x, op.y};
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
            // GAME_DESIGN 8, assist: any player's lost cell eases the budget (the host's director).
            if (role_ == NetRole::Host && !director_.hitThisCycle) {
                director_.hitThisCycle = true;
                director_.assist = std::fmax(director_.assist - kAssistStep, kAssistMin);
            }
            Event ev;
            ev.type = EventType::Hit;
            ev.a = static_cast<u8>(p);
            ev.b = op.b;
            ev.x = c.x;
            ev.y = c.y;
            ev.x2 = op.x2;
            ev.y2 = op.y2;
            emit(ev);
            break;
        }
        case kPeDown:
            if (!pl.present || pl.down) break;
            pl.pos = pl.prevPos = Vec2{op.x, op.y};
            downPlayer(p);
            break;
        case kPeRevive:
            if (!pl.present || !pl.down) break;
            pl.beacon = Vec2{op.x, op.y};
            revivePlayer(p, op.b != 0);
            break;
        case kPeTrioOpen: {
            TrioState& tr = trio_[p];
            tr.open = op.b > 0;
            tr.orbs = op.b > kMaxOrbs ? static_cast<u8>(kMaxOrbs) : op.b;
            tr.homing = -1;
            tr.centre = Vec2{op.x, op.y};
            tr.openTick = m1_;
            tr.closeTick = op.tick;
            // Play test 2: x2 = the level a level-up Trio shows, y2 = its kind (its orbs are shelters here too).
            tr.kind = op.y2 > 0.5f ? kTrioLevel : kTrioBreak;
            tr.levelShown = tr.kind == kTrioLevel && op.x2 > 0.0f && op.x2 < 65535.0f ? static_cast<u16>(op.x2) : 0;
            for (u32 k = 0; k < kMaxOrbs; ++k) tr.upgrade[k] = kNoUpgrade;
            for (u32 k = 0; k < tr.orbs; ++k) {
                tr.upgrade[k] = validOrb(op.opts[k]) ? op.opts[k] : kSplit;
                tr.level[k] = isBoostOrb(tr.upgrade[k]) ? 0 : static_cast<u8>(pl.loadout.level(tr.upgrade[k]) + 1);
                tr.orbPos[k] = trioOrbPlace(tr.centre, k, tr.orbs);
            }
            ++tr.opened;
            Event ev;
            ev.type = EventType::TrioOpen;
            ev.a = static_cast<u8>(p);
            ev.b = tr.orbs;
            ev.c = tr.kind;
            ev.x = op.x;
            ev.y = op.y;
            ev.x2 = static_cast<float>(tr.levelShown);
            ev.y2 = static_cast<float>(tr.kind);
            ev.tick = op.tick;
            emit(ev);
            break;
        }
        case kPeTrioTake: {
            if (!validOrb(op.b)) break;
            // Play test 2: a Boost orb's Boost arrives as its own kPeBoostTake (sent just before this).
            if (isFusionOrb(op.b)) pl.loadout.fusions = static_cast<u8>(pl.loadout.fusions | (1u << fusionOfOrb(op.b)));
            else if (!isBoostOrb(op.b)) applyUpgrade(p, op.b);
            TrioState& tr = trio_[p];
            tr.open = false;
            tr.lastTaken = op.b;
            tr.lastTakenTick = m1_;
            tr.lastTakenAt = Vec2{op.x, op.y};
            Event ev;
            ev.type = EventType::TrioTake;
            ev.a = static_cast<u8>(p);
            ev.b = op.b;
            ev.c = isBoostOrb(op.b) ? 0 : static_cast<u8>(pl.loadout.level(op.b));
            ev.x = op.x;
            ev.y = op.y;
            emit(ev);
            break;
        }
        case kPeBoostTake: {
            // The orb of that player's nearest the place it was taken.
            BoostPickups& b = boostPickups_;
            i32 best = -1;
            float bestD = 0.0f;
            for (u32 i = 0; i < b.count; ++i) {
                if (b.dead[i] || b.owner[i] != p) continue;
                const float d = lengthSq(Vec2{b.x[i] - op.x, b.y[i] - op.y});
                if (best < 0 || d < bestD) {
                    best = static_cast<i32>(i);
                    bestD = d;
                }
            }
            if (best >= 0) b.dead[best] = 1;
            if (op.b < kBoostCount) activateBoost(p, op.b);
            break;
        }
        case kPeHalo:
            for (ActiveBoost& a : pl.boosts) {
                if (a.type != kHalo) continue;
                a.hits = op.b;
                Event ev;
                ev.type = EventType::HaloBlock;
                ev.a = static_cast<u8>(p);
                ev.b = op.b;
                ev.x = pl.pos.x;
                ev.y = pl.pos.y;
                ev.x2 = op.x;
                ev.y2 = op.y;
                emit(ev);
                if (a.hits == 0) {
                    Event end;
                    end.type = EventType::BoostEnd;
                    end.a = static_cast<u8>(p);
                    end.b = kHalo;
                    emit(end);
                    a.type = kNoBoost;
                }
                break;
            }
            break;
        case kPeLeave: netSetPresent(p, false); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------------------------- verdicts

void World::predictKill(u32 i, u8 killer, bool dissolve) {
    // Client: its own hit killed it. Shown now; gone for everything else; kept in the pool until the host
    // answers (TECH_SPEC 8.1: a death not confirmed within 300 ms is undone).
    Enemies& e = enemies_;
    if (e.flags[i] & kFlagDead) return;
    e.flags[i] = static_cast<u8>(e.flags[i] | kFlagDead | kFlagPredicted);
    e.pred[i] = 0.0f;
    ++counters_.predicted;
    const Species& s = species_->get(e.species[i]);
    Event ev;
    ev.type = EventType::Kill;
    ev.species = e.species[i];
    ev.a = s.killSize();
    ev.b = killer;
    ev.c = dissolve ? 1 : 0;
    ev.x = e.x[i];
    ev.y = e.y[i];
    ev.f = e.heading[i];
    emit(ev);
}

void World::updatePredictions() {
    Enemies& e = enemies_;
    for (u32 i = 0; i < e.count; ++i) {
        if (!(e.flags[i] & kFlagPredicted)) continue;
        e.pred[i] += kDt;
        if (e.pred[i] <= predictUndoS_) continue;
        // No word from the host: it lives on (its health as the host last had it, at least a sliver), coming back with
        // a warp-in's small flash rather than popping in.
        e.flags[i] = static_cast<u8>(e.flags[i] & ~(kFlagDead | kFlagPredicted));
        if (e.hp[i] <= 0.0f) e.hp[i] = 0.01f;
        ++counters_.undone;
        Event ev;
        ev.type = EventType::WarpLand;
        ev.species = e.species[i];
        ev.x = e.x[i];
        ev.y = e.y[i];
        emit(ev);
    }
}

void World::finishKill(u32 i, u8 killer, u8 kind) {
    // Client: the host's verdict on enemy i (alive here, or a death this device predicted).
    Enemies& e = enemies_;
    const bool predicted = (e.flags[i] & kFlagPredicted) != 0;
    if ((e.flags[i] & kFlagDead) && !predicted) return;  // flagged already this tick
    const Species& s = species_->get(e.species[i]);
    if (predicted) {
        ++counters_.confirmed;
    } else {
        Event ev;
        ev.type = EventType::Kill;
        ev.species = e.species[i];
        ev.a = s.killSize();
        ev.b = killer;
        ev.c = kind == kDeathKill ? 0 : (kind == kDeathDissolve || e.state[i] == kWarping ? 1 : 2);
        ev.x = e.x[i];
        ev.y = e.y[i];
        ev.f = e.heading[i];
        emit(ev);
    }
    e.flags[i] = static_cast<u8>((e.flags[i] | kFlagDead) & ~kFlagPredicted);
    if (kind == kDeathKill) {
        ++counters_.kills;
        if (killer < kMaxPlayers && players_[killer].present && isLocal(killer)) {
            PlayerState& pl = players_[killer];
            ++pl.kills;
            const double elite = (e.traits[i] & kEliteBit) ? static_cast<double>(species_->traits.score) : 1.0;  // M5 part 5
            pl.score += static_cast<u64>(std::llround(static_cast<double>(s.score) * elite * static_cast<double>(pl.chain) * rules_.scoreMultiplier));
        }
    }
    if (kind != kDeathDissolve && e.state[i] != kWarping) dropShards(i, killer);
    if (kind == kDeathKill) eliteDeath(i, killer);  // M5 part 5: its traits' deaths (the host's spawns come as spawns)
}

// ---------------------------------------------------------------------------------------------- the log

void World::logSpawn(u32 i, double land) {
    NetLog& l = netLog_;
    if (l.spawnCount >= NetLog::kSpawns) {
        ++l.overflow;
        ++counters_.netLogDropped;
        return;
    }
    const Enemies& e = enemies_;
    NetSpawn& s = l.spawns[l.spawnCount];
    l.spawnIndex[l.spawnCount] = i;
    ++l.spawnCount;
    s.serial = e.serial[i];
    s.species = e.species[i];
    s.x = e.x[i];
    s.y = e.y[i];
    s.heading = e.heading[i];
    s.land = land;
    s.group = 0;
    s.flags = 0;
}

void World::finishNetLog() {
    // The spawns of this tick: their school and the crowd's mark are set after spawnEnemy returns.
    NetLog& l = netLog_;
    for (u32 k = l.spawnFinished; k < l.spawnCount; ++k) {
        const u32 i = l.spawnIndex[k];
        if (i < enemies_.count && enemies_.serial[i] == l.spawns[k].serial) {
            l.spawns[k].group = enemies_.group[i];
            l.spawns[k].flags = static_cast<u8>(enemies_.flags[i] & (kFlagArriving | kFlagCalled));
        }
    }
    l.spawnFinished = l.spawnCount;
}

void World::logDeath(u32 i, u8 killer, u8 kind) {
    NetLog& l = netLog_;
    if (l.deathCount >= NetLog::kDeaths) {
        ++l.overflow;
        ++counters_.netLogDropped;
        return;
    }
    NetDeath& d = l.deaths[l.deathCount++];
    d.serial = enemies_.serial[i];
    d.killer = killer;
    d.kind = kind;
}

void World::logClaim(u32 i, float damage, u8 kind) {
    NetLog& l = netLog_;
    if (l.claimCount >= NetLog::kClaims) {
        ++l.overflow;
        ++counters_.netLogDropped;
        return;
    }
    NetClaim& c = l.claims[l.claimCount++];
    c.serial = enemies_.serial[i];
    c.damage = damage;
    c.kind = kind;
}

// ---------------------------------------------------------------------------------------------- corrections

bool World::netSample(u32 i, NetEnemyState& s) const {
    const Enemies& e = enemies_;
    if (i >= e.count || (e.flags[i] & kFlagDead)) return false;
    s.serial = e.serial[i];
    s.x = e.x[i];
    s.y = e.y[i];
    s.vx = e.vx[i];
    s.vy = e.vy[i];
    s.kx = e.kx[i];
    s.ky = e.ky[i];
    s.heading = e.heading[i];
    s.a = e.a[i];
    s.b = e.b[i];
    s.hp = e.hp[i];
    s.until = e.until[i];
    s.rng = e.rng[i];
    s.state = e.state[i];
    s.flags = static_cast<u8>(e.flags[i] & (kFlagEngaged | kFlagArriving | kFlagCalled));
    s.target = e.target[i];
    s.far = e.far[i];
    s.step = e.step[i];
    s.frozen = e.frozenUntil[i] > m1_;
    return true;
}

float World::netCorrect(const NetEnemyState& s, double hostMusic) {
    const i32 idx = indexOfSerial(s.serial);
    if (idx < 0) return -1.0f;
    Enemies& e = enemies_;
    const u32 i = static_cast<u32>(idx);
    if (e.flags[i] & kFlagDead) return -1.0f;  // a death predicted here: the host will say
    // The host's position carried to this world's musical time by its own velocity (at most a quarter second).
    const float tps = ticksPerSecond();
    const float ds = clampf(static_cast<float>((m1_ - hostMusic) / static_cast<double>(tps)), -0.25f, 0.25f);
    const bool hostWarping = s.state == kWarping;
    Vec2 h{s.x, s.y};
    if (!hostWarping) h = h + Vec2{s.vx + s.kx, s.vy + s.ky} * ds;
    const Vec2 c{e.x[i], e.y[i]};
    const Vec2 d = h - c;
    const float err = length(d);
    // TECH_SPEC 8.1: the simulation takes the host's place; what is drawn keeps its place through an offset
    // that decays over about 150 ms; above 96 wu it snaps.
    e.x[i] = h.x;
    e.y[i] = h.y;
    e.px[i] += d.x;
    e.py[i] += d.y;
    if (err > kCorrectionSnapWu) {
        e.ox[i] = e.oy[i] = 0.0f;
    } else {
        e.ox[i] -= d.x;
        e.oy[i] -= d.y;
    }
    // The rest of its state, unless a beat lies between the host's sample and now (the musical behaviour has
    // moved on from what the sample knew: a new wander, a glint, a dash) or it has landed here already.
    // M5 part 3: Freeze Frame is the host's word here (a client freezes nothing itself): frozen for a bar more, or not.
    e.frozenUntil[i] = s.frozen ? std::fmax(e.frozenUntil[i], m1_ + kTicksPerBar) : -1.0;
    const bool beatBetween = crossed(std::fmin(hostMusic, m1_), std::fmax(hostMusic, m1_), kTicksPerBeat);
    const bool landedHere = hostWarping && e.state[i] != kWarping;
    if (!beatBetween && !landedHere) {
        e.vx[i] = s.vx;
        e.vy[i] = s.vy;
        e.kx[i] = s.kx;
        e.ky[i] = s.ky;
        e.pheading[i] += wrapAngle(s.heading - e.heading[i]);
        e.heading[i] = s.heading;
        e.a[i] = s.a;
        e.b[i] = s.b;
        e.until[i] = s.until;
        e.state[i] = s.state;
        e.rng[i] = s.rng;
        e.far[i] = s.far;
        e.step[i] = s.step;
        e.target[i] = s.target;
        e.flags[i] = static_cast<u8>((e.flags[i] & ~(kFlagEngaged | kFlagArriving | kFlagCalled)) | (s.flags & (kFlagEngaged | kFlagArriving | kFlagCalled)));
    }
    // Health: never above what this device has already taken off (its own claims may still be on their way).
    if (s.hp < e.hp[i]) e.hp[i] = s.hp;
    return err;
}

World::DirectorSync World::directorSync() const {
    DirectorSync d;
    d.startTick = director_.startTick;
    d.track = director_.track;
    d.trackBar = director_.trackBar;
    d.changeBar = director_.changeBar;
    d.bossSpawned = director_.bossSpawned;
    d.assist = director_.assist;
    d.midHold = director_.midHold;
    d.midHolding = director_.midHolding;
    d.miniSpawned = director_.miniSpawned;
    d.lastMini = director_.lastMini;
    return d;
}

void World::netDirector(const DirectorSync& d) {
    DirectorState& s = director_;
    s.startTick = d.startTick;
    s.track = d.track;
    s.trackBar = d.trackBar;
    s.changeBar = d.changeBar;
    s.bossSpawned = d.bossSpawned;
    s.assist = d.assist;
    s.midHold = d.midHold;
    s.midHolding = d.midHolding;
    s.miniSpawned = d.miniSpawned;
    s.lastMini = d.lastMini;
    if (s.lastBar != kNoBar) {
        // The bar being played, as the host's state places it (a boss death moved the Track's end).
        const u32 bar = barOf(m1_);
        const Phrase ph = phraseAt(s, bar);
        if (ph.bar == phrase_.bar) phrase_ = ph;
    }
}

void World::netBossState(const float* partHp, u32 parts, float coreHp, Vec2 pos) {
    BossState& b = boss_;
    if (!b.active || b.dying) return;
    if (b.kind == BossKind::EventHorizon && parts >= 1) b.hz.meal = static_cast<u16>(partHp[0] > 0.0f ? partHp[0] : 0.0f);  // M5 part 4
    for (u32 k = 0; k < parts && k < kBossParts; ++k)
        if ((b.alive & (1u << k)) && partHp[k] < b.partHp[k]) b.partHp[k] = partHp[k] > 0.0f ? partHp[k] : 0.0f;
    if (coreHp < b.coreHp) b.coreHp = coreHp > 0.0f ? coreHp : 0.0f;
    // The point that keeps this device's fight where the host's is (bossSyncPoint).
    switch (b.kind) {
        case BossKind::Kaleidos: b.pos = pos; break;
        case BossKind::Longwave: b.lw.anchor = pos; break;
        case BossKind::Choir: b.ch.centre = pos; break;
        case BossKind::EventHorizon:
        case BossKind::Polyrhythm: b.pos = pos; break;  // (the pivot; the Solo survivor)
        case BossKind::AlphaHound:
        case BossKind::BatteringRam: b.body[0] = pos; break;
        default: break;  // the Hive's Queen and the Carom King follow their song-time paths
    }
}

u32 World::bossPartCount() const {
    switch (boss_.kind) {
        case BossKind::Kaleidos: return 3;
        case BossKind::Longwave: return species_->longwave.segments;
        case BossKind::Choir: return ChoirData::kVoices;
        case BossKind::CaromKing: return 3;
        case BossKind::EventHorizon: return 1;  // M5 part 4: the meal (not a health)
        case BossKind::Polyrhythm: return 2;    // Three, Four
        default: return 0;
    }
}

Vec2 World::bossSyncPoint() const {
    switch (boss_.kind) {
        case BossKind::Longwave: return boss_.lw.anchor;
        case BossKind::Choir: return boss_.ch.centre;
        case BossKind::Kaleidos:
        case BossKind::EventHorizon:
        case BossKind::Polyrhythm: return boss_.pos;
        default: return boss_.body[0];
    }
}

void World::netSetPresent(u32 p, bool present) {
    if (p >= kMaxPlayers) return;
    PlayerState& pl = players_[p];
    const bool was = pl.present;
    pl.present = present;
    if (present) return;
    if (was) {
        // M4 stage B (GAME_DESIGN 10): the ship fades where it was; its seat opens; the run goes on.
        Event ev;
        ev.type = EventType::Leave;
        ev.a = static_cast<u8>(p);
        ev.x = pl.pos.x;
        ev.y = pl.pos.y;
        ev.f = pl.heading;
        emit(ev);
    }
    pl.alive = false;
    pl.down = false;
    pl.fire = 0;
    pl.dashing = false;
    pl.novaPending = false;
    trio_[p].open = false;
    checkAllDown();
}

u64 World::netCensus(u32& count) const {
    const Enemies& e = enemies_;
    u64 h = 0;
    count = 0;
    for (u32 i = 0; i < e.count; ++i) {
        if ((e.flags[i] & kFlagDead) && !(e.flags[i] & kFlagPredicted)) continue;
        h += censusMix(e.serial[i]);
        ++count;
    }
    return h;
}

// ---------------------------------------------------------------------------------------------- ids

void World::idInsert(u32 serial, u32 slot) {
    if (!serial) return;
    u32 h = idHash(serial);
    for (u32 n = 0; n < kIdSlots; ++n, h = (h + 1) & kIdMask) {
        if (idKey_[h] == 0 || idKey_[h] == serial) {
            idKey_[h] = serial;
            idSlot_[h] = static_cast<u16>(slot);
            return;
        }
    }
    ++counters_.netLogDropped;  // cannot happen: at most 3,072 live enemies in 8,192 places
}

i32 World::idSlot(u32 serial) const {
    if (!serial) return -1;
    u32 h = idHash(serial);
    for (u32 n = 0; n < kIdSlots; ++n, h = (h + 1) & kIdMask) {
        if (idKey_[h] == serial) return idSlot_[h];
        if (idKey_[h] == 0) return -1;
    }
    return -1;
}

void World::idErase(u32 serial) {
    if (!serial) return;
    u32 i = idHash(serial);
    u32 n = 0;
    while (idKey_[i] != serial) {
        if (idKey_[i] == 0 || ++n >= kIdSlots) return;
        i = (i + 1) & kIdMask;
    }
    // Backward-shift deletion: later entries of the same probe run move up into the hole.
    idKey_[i] = 0;
    u32 j = i;
    for (;;) {
        j = (j + 1) & kIdMask;
        if (idKey_[j] == 0) return;
        const u32 k = idHash(idKey_[j]);
        const bool stays = i <= j ? (i < k && k <= j) : (i < k || k <= j);
        if (stays) continue;
        idKey_[i] = idKey_[j];
        idSlot_[i] = idSlot_[j];
        idKey_[j] = 0;
        i = j;
    }
}

i32 World::indexOfSerial(u32 serial) const {
    const i32 slot = idSlot(serial);
    if (slot < 0) return -1;
    const u32 i = enemies_.denseOf[slot];
    return i < enemies_.count && enemies_.serial[i] == serial ? static_cast<i32>(i) : -1;
}

// ---------------------------------------------------------------------------------------------- down and revive

void World::updateDowned() {
    // GAME_DESIGN 10: a teammate standing in the beacon's ring for two beats revives it (decided by the downed
    // player's own device, which then tells the others).
    // Every device keeps the fill of every beacon (look.md 6 draws it); it empties twice as fast as it fills.
    if (!coop()) return;
    if (allDownDipS_ > 0.0f) allDownDipS_ -= kDt;
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        if (!pl.present || !pl.down) continue;
        bool ring = false;
        for (u32 q = 0; q < playerCount_ && !ring; ++q)
            ring = q != p && players_[q].alive && lengthSq(players_[q].pos - pl.beacon) <= kReviveRingWu * kReviveRingWu;
        const double step = m1_ - m0_;
        if (!ring) {
            pl.reviveTicks = pl.reviveTicks > 2.0 * step ? pl.reviveTicks - 2.0 * step : 0.0;
            continue;
        }
        pl.reviveTicks += step;
        if (isLocal(p) && pl.reviveTicks >= kReviveBeats * kTicksPerBeat) revivePlayer(p, true);
    }
}

void World::downPlayer(u32 p) {
    PlayerState& pl = players_[p];
    pl.alive = false;
    pl.down = true;
    pl.beacon = pl.pos;
    pl.reviveTicks = 0.0;
    pl.fire = 0;
    pl.dashing = false;
    pl.novaPending = false;
    pl.novaInvulS = 0.0f;
    pl.invulS = 0.0f;
    ++pl.downs;
    ++counters_.downs;
    trio_[p].open = false;
    Event ev;
    ev.type = EventType::Down;
    ev.a = static_cast<u8>(p);
    ev.x = pl.pos.x;
    ev.y = pl.pos.y;
    emit(ev);
    checkAllDown();
}

void World::revivePlayer(u32 p, bool byTeammate) {
    // GAME_DESIGN 10: back with one cell and every upgrade; half the Chain is lost.
    PlayerState& pl = players_[p];
    pl.alive = true;
    pl.down = false;
    pl.cells = 1;
    pl.chain = pl.chain / 2 > 1 ? pl.chain / 2 : 1u;
    pl.invulS = static_cast<float>(kTicksPerBar) / ticksPerSecond();  // look.md 6: it cannot be hurt for one bar
    pl.pos = pl.prevPos = pl.beacon;
    pl.vel = Vec2{};
    pl.reviveTicks = 0.0;
    ++pl.revives;
    ++counters_.revives;
    Event ev;
    ev.type = EventType::Revive;
    ev.a = static_cast<u8>(p);
    ev.b = byTeammate ? 1 : 0;
    ev.x = pl.beacon.x;
    ev.y = pl.beacon.y;
    emit(ev);
}

void World::checkAllDown() {
    // The run ends when all players are down at once (GAME_DESIGN 4.3, 10): one Death for the team.
    bool any = false;
    for (u32 p = 0; p < playerCount_; ++p) {
        if (players_[p].present && players_[p].alive) return;
        any = any || players_[p].present;
    }
    if (!any && playerCount_ == 0) return;
    ++counters_.deaths;
    allDownDipS_ = static_cast<float>(kTicksPerBeat) / ticksPerSecond();  // look.md 6: time dips to 50% for one beat
    Event ev;
    ev.type = EventType::Death;
    ev.a = 0xFF;
    emit(ev);
}

// ---------------------------------------------------------------------------------------------- the crowd

void World::spendCrowdCoop(double landTick, float target, u32 lost, u32 killed) {
    // GAME_DESIGN 8: the crowd target holds for each player's frame. Each living player's frame is counted
    // (after the formations ordered for the frames before it, so overlapping frames are not filled twice) and
    // its gap closed as the solo director closes its one frame's: the gap, a running debt of its own, and its
    // share of the kills not yet replaced.
    const SpeciesTable::Crowd& c = species_->crowd;
    u32 alive = 0;
    for (u32 p = 0; p < playerCount_; ++p) alive += players_[p].alive ? 1u : 0u;
    if (!alive) return;
    const u32 lostShare = (lost + alive - 1) / alive;
    const float killedShare = static_cast<float>(killed) / static_cast<float>(alive);
    u32 least = 0xFFFFFFFFu;
    for (u32 p = 0; p < playerCount_; ++p) {
        if (!players_[p].alive) continue;
        const Vec2 centre = players_[p].pos;
        const u32 counted = crowdCount(centre);
        if (counted < least) least = counted;
        const float gap = target - static_cast<float>(counted);
        float& debt = director_.crowdDebtP[p];
        debt = clampf(debt * 0.5f + gap * kCrowdDebtGain, 0.0f, std::fmin(0.5f * target, killedShare));
        if (gap < 1.0f) continue;
        u32 want = static_cast<u32>(std::ceil(gap + debt)) + lostShare;
        if (want < c.formationMin && gap >= static_cast<float>(c.formationMin)) want = c.formationMin;
        for (u32 formations = 0; formations < 8 && want; ++formations) {
            if (enemies_.count >= aliveCap()) break;
            const u32 room = aliveCap() - enemies_.count;
            const i32 sp = pickSpecies(1);
            if (sp < 0) return;
            u32 n = want < c.formationMax ? want : c.formationMax;
            if (n > room) n = room;
            const u32 before = counters_.spawned;
            spawnCrowd(static_cast<u16>(sp), n, landTick, centre, p);
            const u32 made = counters_.spawned - before;
            if (made == 0) break;
            want = want > made ? want - made : 0;
        }
    }
    crowdInFrame_ = least == 0xFFFFFFFFu ? 0u : least;
}

}  // namespace rf::sim
