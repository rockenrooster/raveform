#include "sim/world.h"

#include <cmath>
#include <cstring>

#include "sim/tuning.h"

namespace rf::sim {

using namespace tuning;

namespace {
constexpr float kMaxEnemyRadius = 40.0f;  // larger than any species' radius (ram V: 18 x 1.3 = 23.4)
constexpr float kGridSlack = 32.0f;       // enemies move at most ~25 wu a tick after the grid was built

bool insideArena(float x, float y) { return std::fabs(x) <= kArenaHalfW && std::fabs(y) <= kArenaHalfH; }
}  // namespace

// ---------------------------------------------------------------------------------------------- set-up

void World::init(const SpeciesTable* species) {
    species_ = species;
    std::memset(&enemies_, 0, sizeof enemies_);
    std::memset(&bullets_, 0, sizeof bullets_);
    std::memset(&shots_, 0, sizeof shots_);
    std::memset(&shards_, 0, sizeof shards_);
    std::memset(events_, 0, sizeof events_);
    tick_ = 0;
    resetRun(1, 0.0, 1);
}

void World::resetRun(u64 seed, double startTick, u32 players) {
    seed_ = seed;
    rng_.reseed(seed * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull);
    Enemies& e = enemies_;
    // Free every slot; generations keep counting, so a handle from the last run stays stale.
    e.count = 0;
    e.freeCount = kMaxEnemies;
    for (u32 s = 0; s < kMaxEnemies; ++s) e.freeSlots[s] = kMaxEnemies - 1 - s;  // slot 0 is taken first
    bullets_.count = shots_.count = shards_.count = 0;
    serial_ = 0;
    dipS_ = 0.0f;
    threatInView_ = 0.0f;
    restartPending_ = false;
    m0_ = m1_ = startTick;
    playerCount_ = players < 1 ? 1 : (players > kMaxPlayers ? kMaxPlayers : players);
    for (u32 p = 0; p < kMaxPlayers; ++p) {
        PlayerState& pl = players_[p];
        pl = PlayerState{};
        pl.present = p < playerCount_;
        pl.alive = pl.present;
        pl.pos = pl.prevPos = Vec2{static_cast<float>(p) * 60.0f, 0.0f};
    }
    const Spawner::Mode mode = spawner_.mode;
    spawner_ = Spawner{};
    spawner_.mode = mode;
    spawner_.startTick = startTick;
}

// ---------------------------------------------------------------------------------------------- tick

void World::tick(const TickInput& in) {
    ++tick_;
    eventCount_ = 0;
    if (in.retry) restartPending_ = true;
    const double prev = m1_;
    double next = in.music1 > prev ? in.music1 : prev;
    restarted_ = false;
    if (restartPending_ && crossed(prev, next, kTicksPerBar)) {
        // Tap to retry: the new run begins on this bar line (the poses of this tick still describe the old
        // run, so they are not applied; the controllers restart from the world's start positions).
        const double bar = (std::floor(prev / kTicksPerBar) + 1.0) * kTicksPerBar;
        const u64 seed = seed_ + 0x9E3779B97F4A7C15ull;
        resetRun(seed, bar, playerCount_);
        restarted_ = true;
        ++counters_.restarts;
        Event ev;
        ev.type = EventType::Restart;
        ev.tick = bar;
        emit(ev);
    }
    m0_ = m1_;
    m1_ = next;
    bpm_ = in.bpm > 1.0f ? in.bpm : 120.0f;
    latencyTicks_ = in.latencyTicks;
    const float dt = kDt * timeScale();
    if (dipS_ > 0.0f) dipS_ -= kDt;

    applyPlayers(in);
    spawnShots(in);
    updateNovas();
    bool anyAlive = false;
    for (u32 p = 0; p < playerCount_; ++p) anyAlive = anyAlive || players_[p].alive;
    if (anyAlive) runSpawner();

    // The grid holds the solid enemies as the tick begins (indices stay valid until compact()); flocking
    // and the collisions below query it with slack for the movement since.
    for (u32 i = 0; i < enemies_.count; ++i)
        solid_[i] = (enemies_.flags[i] & kFlagDead) == 0 && enemies_.state[i] != kWarping;
    grid_.build(enemies_.x, enemies_.y, enemies_.count, solid_);

    updateEnemies(dt);
    moveProjectiles(dt);
    collide();
    compact();
}

void World::emit(const Event& e) {
    if (eventCount_ >= kMaxEvents) {
        ++counters_.eventsDropped;
        return;
    }
    events_[eventCount_++] = e;
}

float World::tickAgeOf(double tick) const {
    const double s = (m1_ - tick) / static_cast<double>(ticksPerSecond());
    return s > 0.0 ? static_cast<float>(s) : 0.0f;
}

// ---------------------------------------------------------------------------------------------- players

void World::applyPlayers(const TickInput& in) {
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        const PlayerPose& ps = in.players[p];
        pl.prevPos = pl.pos;
        pl.prevHeading = pl.heading;
        if (pl.invulS > 0.0f) pl.invulS -= kDt;
        if (restarted_) continue;
        if (!pl.alive || !ps.present) {
            pl.fire = 0;
            pl.dashing = false;
            pl.vel = Vec2{};
            continue;
        }
        pl.pos = Vec2{clampf(ps.pos.x, -kArenaHalfW, kArenaHalfW), clampf(ps.pos.y, -kArenaHalfH, kArenaHalfH)};
        pl.vel = ps.vel;
        pl.aim = ps.aim;
        pl.heading = ps.heading;
        pl.fire = ps.fire;
        pl.dashing = ps.dashing;
        if (ps.novaPressed && rules_.novas && pl.novas > 0 && !pl.novaPending) {
            // R6: invulnerable at once; the blast lands on the next beat. The band plays the inhale from
            // the press and the blast on a beat of its write time, which is ahead of the heard time by
            // the output latency, and moves to the following beat when the next one is closer than six
            // ticks (engine.cpp, CmdType::Nova): the same rule here, so picture and sound land together.
            --pl.novas;
            ++pl.novasUsed;
            pl.novaPending = true;
            const double earliest = m1_ + static_cast<double>(latencyTicks_) + 6.0;
            pl.novaBlastTick = std::ceil(earliest / kTicksPerBeat) * kTicksPerBeat;
            pl.novaInvulS = 1e9f;
            Event ev;
            ev.type = EventType::NovaPress;
            ev.a = static_cast<u8>(p);
            ev.x = pl.pos.x;
            ev.y = pl.pos.y;
            ev.tick = pl.novaBlastTick;
            emit(ev);
        }
        if (ps.dashStarted) {
            ++pl.dashes;
            if (ps.perfectDash) ++pl.perfectDashes;
            Event ev;
            ev.type = EventType::Dash;
            ev.a = static_cast<u8>(p);
            ev.b = ps.perfectDash ? 1 : 0;
            ev.x = ps.dashFrom.x;
            ev.y = ps.dashFrom.y;
            ev.x2 = ps.dashTo.x;
            ev.y2 = ps.dashTo.y;
            emit(ev);
        }
    }
}

void World::spawnShots(const TickInput& in) {
    for (u32 p = 0; p < playerCount_; ++p) {
        const PlayerState& pl = players_[p];
        if (!pl.alive) continue;
        if (in.bandShots) {
            // The band's lead rule: one shot for each heard grid step with fire held (TECH_SPEC 6.3).
            for (u32 k = 0; k < in.shotCount; ++k)
                if (in.shots[k].player == p) fireShot(p, in.shots[k].advanceS < 0.0f ? 0.0f : in.shots[k].advanceS);
        } else if (pl.fire) {
            // The same rule on the sim's own clock: a shot on every 1/16 step crossed while fire is held,
            // every other step on the assist's half-rate grid; each advanced by the time since its step.
            forCrossings(m0_, m1_, kTicksPerStep, [&](double t, i64 k) {
                if (pl.fire == 2 && (k & 1) != 0) return;
                fireShot(p, tickAgeOf(t));
            });
        }
    }
}

void World::fireShot(u32 p, float advanceS) {
    PlayerState& pl = players_[p];
    Shots& s = shots_;
    if (s.count >= Shots::kCap) {
        ++counters_.shotsDropped;
        return;
    }
    const Vec2 d = normalizeOr(pl.aim, dirOf(pl.heading));
    const Vec2 muzzle = pl.pos + d * kMuzzleWu;
    const u32 i = s.count++;
    s.px[i] = muzzle.x;
    s.py[i] = muzzle.y;
    s.x[i] = muzzle.x + d.x * kShotSpeed * advanceS;
    s.y[i] = muzzle.y + d.y * kShotSpeed * advanceS;
    s.dx[i] = d.x;
    s.dy[i] = d.y;
    s.speed[i] = kShotSpeed;
    s.life[i] = kShotLifeS - advanceS;
    s.damage[i] = 1.0f;  // species.json note: one player shot deals 1 damage
    s.owner[i] = static_cast<u8>(p);
    s.dead[i] = 0;
    ++pl.shotsFired;
    Event ev;
    ev.type = EventType::Shot;
    ev.a = static_cast<u8>(p);
    ev.x = muzzle.x;
    ev.y = muzzle.y;
    ev.x2 = d.x;
    ev.y2 = d.y;
    ev.age = advanceS;
    emit(ev);
}

void World::updateNovas() {
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        if (pl.novaPending && m1_ >= pl.novaBlastTick) {
            pl.novaPending = false;
            pl.novaFrontAge = tickAgeOf(pl.novaBlastTick);
            pl.novaAt = pl.pos;
            pl.novaInvulS = kNovaFrontS + kNovaGraceS;
            Event ev;
            ev.type = EventType::NovaBlast;
            ev.a = static_cast<u8>(p);
            ev.x = pl.pos.x;
            ev.y = pl.pos.y;
            ev.age = pl.novaFrontAge;
            ev.tick = pl.novaBlastTick;
            emit(ev);
        } else if (!pl.novaPending && pl.novaInvulS > 0.0f) {
            pl.novaInvulS -= kDt;
        }
        if (pl.novaFrontAge < 0.0f) continue;
        // The front clears enemy bullets and non-boss enemies in view as it passes (GAME_DESIGN 5).
        const float r = kNovaFrontWu * clampf(pl.novaFrontAge / kNovaFrontS, 0.0f, 1.0f);
        const float r2 = r * r;
        const Vec2 c = pl.novaAt;
        const auto inView = [&](float x, float y) {
            return std::fabs(x - c.x) <= kNovaViewHalfW && std::fabs(y - c.y) <= kNovaViewHalfH;
        };
        for (u32 b = 0; b < bullets_.count; ++b) {
            if (bullets_.dead[b]) continue;
            const float dx = bullets_.x[b] - c.x, dy = bullets_.y[b] - c.y;
            if (dx * dx + dy * dy <= r2 && inView(bullets_.x[b], bullets_.y[b])) bullets_.dead[b] = 1;
        }
        Enemies& e = enemies_;
        for (u32 i = 0; i < e.count; ++i) {
            if (e.flags[i] & kFlagDead) continue;
            const float dx = e.x[i] - c.x, dy = e.y[i] - c.y;
            if (dx * dx + dy * dy > r2 || !inView(e.x[i], e.y[i])) continue;
            if (e.state[i] == kWarping) {
                // Not yet solid: it dissolves (no Shards, no score).
                e.flags[i] |= kFlagDead;
                Event ev;
                ev.type = EventType::Kill;
                ev.species = e.species[i];
                ev.b = 0xFF;
                ev.c = 1;
                ev.x = e.x[i];
                ev.y = e.y[i];
                emit(ev);
            } else {
                killEnemy(i, static_cast<u8>(p));
            }
        }
        pl.novaFrontAge += kDt;
        if (pl.novaFrontAge > kNovaFrontS) pl.novaFrontAge = -1.0f;
    }
}

// ---------------------------------------------------------------------------------------------- motion

void World::moveProjectiles(float dt) {
    // Enemy bullets fly straight until their life ends or they meet a wall.
    Bullets& b = bullets_;
    for (u32 i = 0; i < b.count; ++i) {
        b.px[i] = b.x[i];
        b.py[i] = b.y[i];
        if (b.dead[i]) continue;
        b.x[i] += b.vx[i] * dt;
        b.y[i] += b.vy[i] * dt;
        b.life[i] -= dt;
        if (b.life[i] <= 0.0f || !insideArena(b.x[i], b.y[i])) b.dead[i] = 1;
    }
    Shots& s = shots_;
    for (u32 i = 0; i < s.count; ++i) {
        if (s.dead[i]) continue;
        s.px[i] = s.x[i];
        s.py[i] = s.y[i];
        s.x[i] += s.dx[i] * s.speed[i] * dt;
        s.y[i] += s.dy[i] * s.speed[i] * dt;
        s.life[i] -= dt;
        if (s.life[i] <= 0.0f || !insideArena(s.x[i], s.y[i])) s.dead[i] = 1;
    }
    // Shards: loose ones slow down and wait; inside a player's magnet they home in; after four bars they
    // are gone (GAME_DESIGN 5).
    Shards& h = shards_;
    const double lifeTicks = kShardLifeBars * kTicksPerBar;
    const float drag = std::exp(-kShardDrag * dt);
    for (u32 i = 0; i < h.count; ++i) {
        h.px[i] = h.x[i];
        h.py[i] = h.y[i];
        if (h.dead[i]) continue;
        if (m1_ - h.born[i] > lifeTicks) {
            h.dead[i] = 1;
            continue;
        }
        const PlayerState& pl = players_[h.owner[i]];
        bool home = false;
        Vec2 to{};
        float dist = 0.0f;
        if (pl.alive) {
            to = pl.pos - Vec2{h.x[i], h.y[i]};
            dist = length(to);
            const float magnet = kShardMagnetWu * rules_.magnetMultiplier;
            home = h.homing[i] || dist < magnet;
        }
        if (home && dist > 1e-3f) {
            h.homing[i] = 1;
            const float speed = std::fmin(length(Vec2{h.vx[i], h.vy[i]}) + kShardHomeAccel * dt, kShardHomeMax);
            const Vec2 d = to * (1.0f / dist);
            h.vx[i] = d.x * speed;
            h.vy[i] = d.y * speed;
        } else {
            h.vx[i] *= drag;
            h.vy[i] *= drag;
        }
        h.x[i] += h.vx[i] * dt;
        h.y[i] += h.vy[i] * dt;
    }
}

// ---------------------------------------------------------------------------------------------- collisions

void World::collide() {
    Enemies& e = enemies_;
    // Shots x enemies: the shot's path this tick (a capsule) against each enemy circle; the first enemy
    // along the path takes the hit (ties broken by index, so the result does not depend on visit order).
    Shots& s = shots_;
    for (u32 k = 0; k < s.count; ++k) {
        if (s.dead[k]) continue;
        const float ax = s.px[k], ay = s.py[k], bx = s.x[k], by = s.y[k];
        const float dx = bx - ax, dy = by - ay;
        const float len2 = dx * dx + dy * dy;
        const float reach = kShotRadius + kMaxEnemyRadius + kGridSlack;
        i32 best = -1;
        float bestT = 2.0f;
        grid_.query(std::fmin(ax, bx) - reach, std::fmin(ay, by) - reach, std::fmax(ax, bx) + reach, std::fmax(ay, by) + reach,
                    [&](u32 i) {
                        if (e.flags[i] & kFlagDead) return;
                        const float cx = e.x[i] - ax, cy = e.y[i] - ay;
                        const float t = len2 > 1e-6f ? clampf((cx * dx + cy * dy) / len2, 0.0f, 1.0f) : 0.0f;
                        const float qx = dx * t - cx, qy = dy * t - cy;
                        const float rr = e.radius[i] + kShotRadius;
                        if (qx * qx + qy * qy > rr * rr) return;
                        if (t < bestT || (t == bestT && static_cast<i32>(i) < best)) {
                            bestT = t;
                            best = static_cast<i32>(i);
                        }
                    });
        if (best < 0) continue;
        s.dead[k] = 1;
        const u32 i = static_cast<u32>(best);
        e.hp[i] -= s.damage[k];
        if (e.hp[i] <= 0.0f) {
            killEnemy(i, s.owner[k]);
        } else {
            Event ev;
            ev.type = EventType::Impact;
            ev.species = e.species[i];
            ev.x = ax + dx * bestT;
            ev.y = ay + dy * bestT;
            ev.x2 = s.dx[k];
            ev.y2 = s.dy[k];
            emit(ev);
        }
    }

    // Players x enemy bodies and enemy bullets (GAME_DESIGN 5: hit radius 5 wu).
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        if (!pl.alive) continue;
        if (pl.invulnerable() || rules_.godMode) continue;
        const Vec2 c = pl.pos;
        i32 contact = -1;
        const float reach = kMaxEnemyRadius + kGridSlack + kPlayerHitRadius;
        grid_.query(c.x - reach, c.y - reach, c.x + reach, c.y + reach, [&](u32 i) {
            if ((e.flags[i] & kFlagDead) || contact >= 0) return;
            const float r = e.radius[i] * kContactShrink + kPlayerHitRadius;
            const float dx = e.x[i] - c.x, dy = e.y[i] - c.y;
            if (dx * dx + dy * dy <= r * r) contact = static_cast<i32>(i);
        });
        if (contact >= 0) {
            hitPlayer(p, Vec2{e.x[contact], e.y[contact]});
            continue;
        }
        const float rb = species_->bulletRadius + kPlayerHitRadius;
        for (u32 b = 0; b < bullets_.count; ++b) {
            if (bullets_.dead[b]) continue;
            const float dx = bullets_.x[b] - c.x, dy = bullets_.y[b] - c.y;
            if (dx * dx + dy * dy > rb * rb) continue;
            bullets_.dead[b] = 1;
            hitPlayer(p, Vec2{bullets_.x[b], bullets_.y[b]});
            break;
        }
    }

    // Shards x their player.
    Shards& h = shards_;
    for (u32 i = 0; i < h.count; ++i) {
        if (h.dead[i]) continue;
        PlayerState& pl = players_[h.owner[i]];
        if (!pl.alive) continue;
        const float dx = h.x[i] - pl.pos.x, dy = h.y[i] - pl.pos.y;
        if (dx * dx + dy * dy > kShardPickupWu * kShardPickupWu) continue;
        h.dead[i] = 1;
        ++pl.chain;  // GAME_DESIGN 5: each Shard collected adds +1 to that player's Chain
        if (pl.chain > pl.bestChain) pl.bestChain = pl.chain;
        ++pl.pickups;
        Event ev;
        ev.type = EventType::Pickup;
        ev.a = h.owner[i];
        ev.x = h.x[i];
        ev.y = h.y[i];
        emit(ev);
    }
}

void World::hitPlayer(u32 p, Vec2 from) {
    PlayerState& pl = players_[p];
    ++pl.hitsTaken;
    if (pl.cells == 0) {
        // GAME_DESIGN 4.3: a hit with no Shield cells left ends the run.
        pl.alive = false;
        pl.fire = 0;
        pl.novaPending = false;
        ++counters_.deaths;
        Event ev;
        ev.type = EventType::Death;
        ev.a = static_cast<u8>(p);
        ev.x = pl.pos.x;
        ev.y = pl.pos.y;
        emit(ev);
        return;
    }
    // GAME_DESIGN 5: a hit removes one cell, fires a panic pulse (clears bullets within 220 wu, shoves
    // enemies), grants 1.5 s of invulnerability and halves the Chain.
    --pl.cells;
    pl.invulS = kInvulnerableS;
    pl.chain /= 2;
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
    ev.x = c.x;
    ev.y = c.y;
    ev.x2 = from.x;
    ev.y2 = from.y;
    emit(ev);
}

void World::killEnemy(u32 i, u8 killer) {
    Enemies& e = enemies_;
    if (e.flags[i] & kFlagDead) return;
    e.flags[i] |= kFlagDead;
    const Species& s = species_->get(e.species[i]);
    Event ev;
    ev.type = EventType::Kill;
    ev.species = e.species[i];
    ev.a = s.killSize();
    ev.b = killer;
    ev.x = e.x[i];
    ev.y = e.y[i];
    ev.f = e.heading[i];
    emit(ev);
    ++counters_.kills;
    if (killer < kMaxPlayers && players_[killer].present) {
        // Score = enemy value x Chain (GAME_DESIGN 5); the Chain starts at 0, so at least x1.
        PlayerState& pl = players_[killer];
        ++pl.kills;
        const float chain = static_cast<float>(pl.chain > kChainFloor ? pl.chain : kChainFloor);
        pl.score += static_cast<u64>(std::llround(static_cast<double>(s.score) * chain * rules_.scoreMultiplier));
    }
    dropShards(i, killer);
}

void World::dropShards(u32 i, u8 killer) {
    (void)killer;
    // Every enemy death drops its Shards for each player separately (GAME_DESIGN 5, 10).
    Enemies& e = enemies_;
    const Species& s = species_->get(e.species[i]);
    for (u32 p = 0; p < playerCount_; ++p) {
        if (!players_[p].present) continue;
        for (u32 k = 0; k < s.shards; ++k) {
            const float a = randRange(e.rng[i], 0.0f, kTau);
            const float v = randRange(e.rng[i], kShardBurstLo, kShardBurstHi);
            spawnShard(Vec2{e.x[i], e.y[i]}, dirOf(a) * v, static_cast<u8>(p));
        }
    }
}

// ---------------------------------------------------------------------------------------------- pools

i32 World::indexOf(u32 handle) const {
    const u32 slot = handle & 0xFFFFu;
    if (slot >= kMaxEnemies || enemies_.gen[slot] != (handle >> 16)) return -1;
    const u32 i = enemies_.denseOf[slot];
    return i < enemies_.count && enemies_.handle[i] == handle ? static_cast<i32>(i) : -1;
}

i32 World::spawnEnemy(u16 speciesIndex, Vec2 pos, float heading, double landTick) {
    Enemies& e = enemies_;
    if (e.count >= Enemies::kCap || e.freeCount == 0) {
        ++counters_.enemiesDropped;
        return -1;
    }
    const Species& s = species_->get(speciesIndex);
    const u32 slot = e.freeSlots[--e.freeCount];
    const u32 i = e.count++;
    e.denseOf[slot] = i;
    e.handle[i] = (static_cast<u32>(e.gen[slot]) << 16) | slot;
    pos.x = clampf(pos.x, -kArenaHalfW + s.radius, kArenaHalfW - s.radius);
    pos.y = clampf(pos.y, -kArenaHalfH + s.radius, kArenaHalfH - s.radius);
    e.x[i] = e.px[i] = pos.x;
    e.y[i] = e.py[i] = pos.y;
    e.vx[i] = e.vy[i] = e.kx[i] = e.ky[i] = 0.0f;
    e.heading[i] = e.pheading[i] = heading;
    e.hp[i] = s.health;
    e.radius[i] = s.radius;
    e.a[i] = e.b[i] = 0.0f;
    e.species[i] = speciesIndex;
    e.group[i] = 0;
    e.flags[i] = 0;
    e.serial[i] = ++serial_;
    e.rng[i] = hash32(static_cast<u32>(seed_ ^ (seed_ >> 32)), serial_);
    if (landTick > m1_) {
        e.state[i] = kWarping;
        e.until[i] = landTick;
        Event ev;
        ev.type = EventType::WarpStart;
        ev.species = speciesIndex;
        ev.x = pos.x;
        ev.y = pos.y;
        ev.tick = landTick;
        emit(ev);
    } else {
        landEnemy(i);
    }
    ++counters_.spawned;
    return static_cast<i32>(i);
}

bool World::spawnBullet(Vec2 pos, Vec2 vel, float advanceS) {
    Bullets& b = bullets_;
    if (b.count >= Bullets::kCap) {
        ++counters_.bulletsDropped;
        return false;
    }
    const u32 i = b.count++;
    b.px[i] = pos.x;
    b.py[i] = pos.y;
    b.x[i] = pos.x + vel.x * advanceS;
    b.y[i] = pos.y + vel.y * advanceS;
    b.vx[i] = vel.x;
    b.vy[i] = vel.y;
    b.life[i] = kBulletLifeS - advanceS;
    b.dead[i] = 0;
    return true;
}

bool World::spawnShard(Vec2 pos, Vec2 vel, u8 owner) {
    Shards& h = shards_;
    if (h.count >= Shards::kCap) {
        ++counters_.shardsDropped;
        return false;
    }
    const u32 i = h.count++;
    h.x[i] = h.px[i] = pos.x;
    h.y[i] = h.py[i] = pos.y;
    h.vx[i] = vel.x;
    h.vy[i] = vel.y;
    h.born[i] = m1_;
    h.owner[i] = owner < kMaxPlayers ? owner : 0;
    h.homing[i] = 0;
    h.dead[i] = 0;
    return true;
}

void World::removeEnemyAt(u32 i) {
    Enemies& e = enemies_;
    const u32 slot = e.handle[i] & 0xFFFFu;
    e.gen[slot] = static_cast<u16>(e.gen[slot] + 1);  // every handle to this slot is now stale
    e.freeSlots[e.freeCount++] = slot;
    const u32 last = --e.count;
    if (i != last) {
        e.x[i] = e.x[last];
        e.y[i] = e.y[last];
        e.px[i] = e.px[last];
        e.py[i] = e.py[last];
        e.vx[i] = e.vx[last];
        e.vy[i] = e.vy[last];
        e.kx[i] = e.kx[last];
        e.ky[i] = e.ky[last];
        e.heading[i] = e.heading[last];
        e.pheading[i] = e.pheading[last];
        e.hp[i] = e.hp[last];
        e.radius[i] = e.radius[last];
        e.a[i] = e.a[last];
        e.b[i] = e.b[last];
        e.until[i] = e.until[last];
        e.species[i] = e.species[last];
        e.group[i] = e.group[last];
        e.state[i] = e.state[last];
        e.flags[i] = e.flags[last];
        e.handle[i] = e.handle[last];
        e.serial[i] = e.serial[last];
        e.rng[i] = e.rng[last];
        e.denseOf[e.handle[i] & 0xFFFFu] = i;
    }
}

void World::compact() {
    // Swap-remove in index order (TECH_SPEC 7.1): deterministic, no holes.
    for (u32 i = 0; i < enemies_.count;) {
        if (enemies_.flags[i] & kFlagDead) {
            removeEnemyAt(i);
        } else {
            ++i;
        }
    }
    Bullets& b = bullets_;
    for (u32 i = 0; i < b.count;) {
        if (!b.dead[i]) {
            ++i;
            continue;
        }
        const u32 l = --b.count;
        b.x[i] = b.x[l];
        b.y[i] = b.y[l];
        b.px[i] = b.px[l];
        b.py[i] = b.py[l];
        b.vx[i] = b.vx[l];
        b.vy[i] = b.vy[l];
        b.life[i] = b.life[l];
        b.dead[i] = b.dead[l];
    }
    Shots& s = shots_;
    for (u32 i = 0; i < s.count;) {
        if (!s.dead[i]) {
            ++i;
            continue;
        }
        const u32 l = --s.count;
        s.x[i] = s.x[l];
        s.y[i] = s.y[l];
        s.px[i] = s.px[l];
        s.py[i] = s.py[l];
        s.dx[i] = s.dx[l];
        s.dy[i] = s.dy[l];
        s.speed[i] = s.speed[l];
        s.life[i] = s.life[l];
        s.damage[i] = s.damage[l];
        s.owner[i] = s.owner[l];
        s.dead[i] = s.dead[l];
    }
    Shards& h = shards_;
    for (u32 i = 0; i < h.count;) {
        if (!h.dead[i]) {
            ++i;
            continue;
        }
        const u32 l = --h.count;
        h.x[i] = h.x[l];
        h.y[i] = h.y[l];
        h.px[i] = h.px[l];
        h.py[i] = h.py[l];
        h.vx[i] = h.vx[l];
        h.vy[i] = h.vy[l];
        h.born[i] = h.born[l];
        h.owner[i] = h.owner[l];
        h.homing[i] = h.homing[l];
        h.dead[i] = h.dead[l];
    }
}

// ---------------------------------------------------------------------------------------------- checks

u64 World::hash() const {
    u64 h = 1469598103934665603ull;
    const auto mix = [&h](const void* data, size_t n) {
        const u8* p = static_cast<const u8*>(data);
        for (size_t i = 0; i < n; ++i) {
            h ^= p[i];
            h *= 1099511628211ull;
        }
    };
    const auto val = [&mix](const auto& v) { mix(&v, sizeof v); };
    val(tick_);
    val(m1_);
    val(bpm_);
    val(dipS_);
    val(serial_);
    val(seed_);
    val(rng_);
    val(spawner_.bank);
    val(spawner_.pending);
    val(spawner_.nextSchool);
    val(spawner_.lastBar);
    val(spawner_.startTick);
    const Enemies& e = enemies_;
    const u32 n = e.count;
    val(n);
    mix(e.x, n * sizeof(float));
    mix(e.y, n * sizeof(float));
    mix(e.vx, n * sizeof(float));
    mix(e.vy, n * sizeof(float));
    mix(e.kx, n * sizeof(float));
    mix(e.ky, n * sizeof(float));
    mix(e.heading, n * sizeof(float));
    mix(e.hp, n * sizeof(float));
    mix(e.a, n * sizeof(float));
    mix(e.b, n * sizeof(float));
    mix(e.until, n * sizeof(double));
    mix(e.species, n * sizeof(u16));
    mix(e.group, n * sizeof(u16));
    mix(e.state, n);
    mix(e.flags, n);
    mix(e.handle, n * sizeof(u32));
    mix(e.serial, n * sizeof(u32));
    mix(e.rng, n * sizeof(u32));
    const Bullets& b = bullets_;
    val(b.count);
    mix(b.x, b.count * sizeof(float));
    mix(b.y, b.count * sizeof(float));
    mix(b.vx, b.count * sizeof(float));
    mix(b.vy, b.count * sizeof(float));
    mix(b.life, b.count * sizeof(float));
    const Shots& s = shots_;
    val(s.count);
    mix(s.x, s.count * sizeof(float));
    mix(s.y, s.count * sizeof(float));
    mix(s.dx, s.count * sizeof(float));
    mix(s.dy, s.count * sizeof(float));
    mix(s.life, s.count * sizeof(float));
    const Shards& sh = shards_;
    val(sh.count);
    mix(sh.x, sh.count * sizeof(float));
    mix(sh.y, sh.count * sizeof(float));
    mix(sh.vx, sh.count * sizeof(float));
    mix(sh.vy, sh.count * sizeof(float));
    mix(sh.born, sh.count * sizeof(double));
    mix(sh.homing, sh.count);
    for (u32 p = 0; p < kMaxPlayers; ++p) {
        const PlayerState& pl = players_[p];
        val(pl.alive);
        val(pl.pos.x);
        val(pl.pos.y);
        val(pl.cells);
        val(pl.novas);
        val(pl.invulS);
        val(pl.novaInvulS);
        val(pl.novaPending);
        val(pl.novaBlastTick);
        val(pl.novaFrontAge);
        val(pl.chain);
        val(pl.bestChain);
        val(pl.score);
        val(pl.kills);
        val(pl.pickups);
        val(pl.shotsFired);
    }
    return h;
}

bool World::finite() const {
    const auto ok = [](const float* v, u32 n) {
        for (u32 i = 0; i < n; ++i)
            if (!std::isfinite(v[i])) return false;
        return true;
    };
    const Enemies& e = enemies_;
    if (!ok(e.x, e.count) || !ok(e.y, e.count) || !ok(e.vx, e.count) || !ok(e.vy, e.count) || !ok(e.heading, e.count) ||
        !ok(e.hp, e.count))
        return false;
    if (!ok(bullets_.x, bullets_.count) || !ok(bullets_.y, bullets_.count)) return false;
    if (!ok(shots_.x, shots_.count) || !ok(shots_.y, shots_.count)) return false;
    if (!ok(shards_.x, shards_.count) || !ok(shards_.y, shards_.count)) return false;
    for (u32 p = 0; p < playerCount_; ++p)
        if (!std::isfinite(players_[p].pos.x) || !std::isfinite(players_[p].pos.y)) return false;
    return true;
}

}  // namespace rf::sim
