// Enemy behaviour per family (GAME_DESIGN 7.2), driven by the species table and the heard musical time.
// What `a` and `b` hold per family:
//   drifter  a = wander turn rate for this beat (rad/s)            b = -
//   hound    a = -                                                b = -
//   shoal    a = the heading the flock wants (rad)                 b = -
//   ram      a, b = the committed dash direction
//   carom    a = speed factor (1 at the start, +gain per bounce)    b = -
//   sentry   a = -                                                b = the arms' phase (rad)
// R4: every ranged or dash attack is telegraphed for at least one beat and resolves on a beat.

#include <cmath>

#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

i32 World::nearestPlayer(float x, float y) const {
    i32 best = -1;
    float bestD = 0.0f;
    for (u32 p = 0; p < playerCount_; ++p) {
        const PlayerState& pl = players_[p];
        if (!pl.alive) continue;
        const float dx = pl.pos.x - x, dy = pl.pos.y - y;
        const float d = dx * dx + dy * dy;
        if (best < 0 || d < bestD) {
            best = static_cast<i32>(p);
            bestD = d;
        }
    }
    return best;
}

bool World::engagedBy(float x, float y) const {
    for (u32 p = 0; p < playerCount_; ++p) {
        const PlayerState& pl = players_[p];
        if (pl.alive && std::fabs(x - pl.pos.x) <= kEngageHalfW && std::fabs(y - pl.pos.y) <= kEngageHalfH) return true;
    }
    return false;
}

void World::landEnemy(u32 i) {
    // The moment it becomes solid: each family starts its motion.
    Enemies& e = enemies_;
    const Species& s = species_->get(e.species[i]);
    const FamilyData& fd = species_->family(s.family);
    e.state[i] = kMoving;
    e.until[i] = m1_;
    const i32 tp = nearestPlayer(e.x[i], e.y[i]);
    if (coop()) e.target[i] = tp >= 0 ? static_cast<u8>(tp) : 0xFF;  // M4: its first player is the nearest
    if (e.flags[i] & kFlagCalled) {
        // M5: a mini-boss's called body (the Alpha's pack, the Hive's darts) starts what it was called for.
        minionLand(i);
        e.pheading[i] = e.heading[i];
        return;
    }
    const Vec2 to = tp >= 0 ? players_[tp].pos - Vec2{e.x[i], e.y[i]} : dirOf(e.heading[i]);
    const float toward = angleOf(to);
    switch (s.family) {
        case Family::Drifter:
            e.heading[i] = randRange(e.rng[i], 0.0f, kTau);
            e.a[i] = randRange(e.rng[i], -fd.wanderTurn, fd.wanderTurn);
            break;
        case Family::Hound:
        case Family::Ram:
        case Family::Sentry:
            e.heading[i] = toward;
            e.b[i] = randRange(e.rng[i], 0.0f, kTau);
            break;
        case Family::Shoal:
            e.heading[i] = toward + randRange(e.rng[i], -0.4f, 0.4f);
            e.a[i] = e.heading[i];
            break;
        case Family::Carom: {
            e.heading[i] = toward + randRange(e.rng[i], -kCaromAimSpread, kCaromAimSpread);
            e.a[i] = 1.0f;
            const Vec2 v = dirOf(e.heading[i]) * s.speed;
            e.vx[i] = v.x;
            e.vy[i] = v.y;
            break;
        }
        default: break;
    }
    landFamily(i, s, fd, to);  // M5 part 2: the new families, and the Hound C's shield
    e.pheading[i] = e.heading[i];
}

void World::flock(u32 i, const Species& s, float dt) {
    // Shoal (GAME_DESIGN 7.2): boids in schools of 12-40 that seek the nearest player. Neighbours are
    // gathered every other tick (staggered by spawn number); the heading steers toward the wanted one
    // every tick.
    Enemies& e = enemies_;
    const FamilyData& fd = species_->family(Family::Shoal);
    // M4 co-op: the run's tick count (the same on every device) picks the gathering ticks, so schools flock alike.
    const u64 phase = coop() ? runTicks_ : tick_;
    if (((e.serial[i] + static_cast<u32>(phase)) & 1u) == 0u) {
        const float R = fd.flockRadius, R2 = R * R, sep = fd.separation;
        const float x = e.x[i], y = e.y[i];
        Vec2 centre{}, align{}, push{};
        u32 n = 0;
        grid_.query(x - R, y - R, x + R, y + R, [&](u32 j) {
            if (j == i || (e.flags[j] & (kFlagDead | kFlagCalled))) return;  // M5: the Hive's darts are not a school
            if (species_->get(e.species[j]).family != Family::Shoal) return;
            const float dx = e.x[j] - x, dy = e.y[j] - y;
            const float d2 = dx * dx + dy * dy;
            if (d2 > R2) return;
            centre = centre + Vec2{e.x[j], e.y[j]};
            align = align + Vec2{e.vx[j], e.vy[j]};
            ++n;
            if (d2 < sep * sep && d2 > 1e-6f) {
                const float d = std::sqrt(d2);
                push = push - Vec2{dx, dy} * ((sep - d) / (sep * d));
            }
        });
        Vec2 want{};
        // M4 co-op: the school seeks the player each fish attends to (updateEnemies chose it this tick).
        const i32 tp = coop() ? (e.target[i] < playerCount_ && players_[e.target[i]].alive ? static_cast<i32>(e.target[i]) : -1)
                              : nearestPlayer(x, y);
        if (tp >= 0) {
            // C2: while a boss lives the school gives the fight room: it keeps out of kCrowdBossRoomWu of a ship.
            const Vec2 to = players_[tp].pos - Vec2{x, y};
            const float seek = boss_.active ? (lengthSq(to) < kCrowdBossRoomWu * kCrowdBossRoomWu ? -kShoalSeek : 0.0f) : kShoalSeek;
            want = normalizeOr(to, Vec2{}) * seek;
        }
        if (n) {
            const Vec2 c = centre * (1.0f / static_cast<float>(n));
            want = want + normalizeOr(c - Vec2{x, y}, Vec2{}) * kShoalCohesion;
            want = want + normalizeOr(align, Vec2{}) * kShoalAlignment;
            want = want + push * kShoalSeparation;
        }
        if (lengthSq(want) > 1e-8f) e.a[i] = angleOf(want);
    }
    e.heading[i] = turnToward(e.heading[i], e.a[i], kShoalTurn * dt);
    const Vec2 v = dirOf(e.heading[i]) * (s.speed * kShoalSpeedScale);
    e.vx[i] = v.x;
    e.vy[i] = v.y;
}

void World::updateEnemies(float dt) {
    Enemies& e = enemies_;
    threatInView_ = 0.0f;
    const bool beat = crossed(m0_, m1_, kTicksPerBeat);
    const double beatTick = std::floor(m1_ / kTicksPerBeat) * kTicksPerBeat;  // the beat crossed (when beat)
    const float knock = std::exp(-kKnockbackDecay * dt);
    const PlayerState& p0 = players_[viewPlayer_];  // M4: the band's intensity is this device's player's view
    const float bulletSpeed = species_->bulletSpeed;
    const bool client = role_ == NetRole::Client;
    const float settle = std::exp(-kDt / kCorrectionTau);  // M4 client: a correction's drawn offset decays

    for (u32 i = 0; i < e.count; ++i) {
        e.px[i] = e.x[i];
        e.py[i] = e.y[i];
        e.pheading[i] = e.heading[i];
        if (client) {
            e.ox[i] *= settle;
            e.oy[i] *= settle;
        }
        if (e.flags[i] & kFlagDead) continue;
        if (e.state[i] == kWarping) {
            if (m1_ < e.until[i]) continue;
            landEnemy(i);
            Event ev;
            ev.type = EventType::WarpLand;
            ev.species = e.species[i];
            ev.x = e.x[i];
            ev.y = e.y[i];
            ev.age = tickAgeOf(e.until[i]);
            emit(ev);
        }
        if (e.frozenUntil[i] > m1_) continue;  // M5: Freeze Frame holds it where it is (no motion, no fire)
        const Species& s = species_->get(e.species[i]);
        const FamilyData& fd = species_->family(s.family);
        const float x = e.x[i], y = e.y[i];
        const i32 tp = targetOf(i);  // solo: the nearest player (M4 co-op: by distance, with some stickiness)
        const Vec2 target = tp >= 0 ? chaseTarget(static_cast<u32>(tp)) : Vec2{x, y};  // M5: Afterimage's decoy
        const Vec2 to = target - Vec2{x, y};
        const bool engaged = engagedBy(x, y);
        e.flags[i] = static_cast<u8>(engaged ? (e.flags[i] | kFlagEngaged) : (e.flags[i] & ~kFlagEngaged));
        if (p0.alive && std::fabs(x - p0.pos.x) <= kFrameHalfWWu && std::fabs(y - p0.pos.y) <= kFrameHalfHWu) threatInView_ += s.threat;

        switch (minionMotion(i, dt) ? Family::Count : s.family) {
            case Family::Drifter: {
                // Wander (a new turn rate each beat), leaning a little toward the nearest player.
                if (beat) e.a[i] = randRange(e.rng[i], -fd.wanderTurn, fd.wanderTurn);
                float h = e.heading[i] + e.a[i] * dt;
                if (tp >= 0) {
                    // C2: while a boss lives drifters give the fight room (they turn away inside kCrowdBossRoomWu).
                    const float home = boss_.active ? (lengthSq(to) < kCrowdBossRoomWu * kCrowdBossRoomWu ? -kDrifterHomeTurn : 0.0f) : kDrifterHomeTurn;
                    h += clampf(wrapAngle(angleOf(to) - h), -1.0f, 1.0f) * home * dt;
                }
                e.heading[i] = h;
                const Vec2 v = dirOf(h) * s.speed;
                e.vx[i] = v.x;
                e.vy[i] = v.y;
                drifterShot(i, s, fd, tp, to, engaged);  // M5: Drifter B
                break;
            }
            case Family::Hound: {
                houndShield(i, s, fd, tp, to, dt);  // M5: Hound C
                // Steer straight at the nearest player; a pack keeps a little apart.
                float h = e.heading[i];
                if (tp >= 0) h = turnToward(h, angleOf(to), fd.turnRate * dt);
                e.heading[i] = h;
                Vec2 v = dirOf(h) * s.speed;
                const float R = kHoundSeparationWu;
                Vec2 push{};
                grid_.query(x - R, y - R, x + R, y + R, [&](u32 j) {
                    if (j == i || (e.flags[j] & kFlagDead) || e.species[j] != e.species[i]) return;
                    const float dx = x - e.x[j], dy = y - e.y[j];
                    const float d2 = dx * dx + dy * dy;
                    if (d2 >= R * R || d2 < 1e-6f) return;
                    const float d = std::sqrt(d2);
                    push = push + Vec2{dx, dy} * ((R - d) / (R * d));
                });
                v = v + push * s.speed;
                e.vx[i] = v.x;
                e.vy[i] = v.y;
                if (s.variant == kVariantB) {
                    // A 3-shot burst on beat 1, after a glint on beat 4 (one beat of telegraph).
                    if (e.state[i] == kMoving && engaged && tp >= 0) {
                        forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64 k) {
                            if ((((k % 4) + 4) % 4) != 3 || e.state[i] != kMoving) return;
                            e.state[i] = kGlint;
                            e.until[i] = t + kTicksPerBeat;
                            Event ev;
                            ev.type = EventType::Telegraph;
                            ev.species = e.species[i];
                            ev.x = x;
                            ev.y = y;
                            ev.tick = e.until[i];
                            emit(ev);
                        });
                    }
                    if (e.state[i] == kGlint && m1_ >= e.until[i]) {
                        const float age = tickAgeOf(e.until[i]);
                        const float aim = tp >= 0 ? angleOf(to) : h;
                        const u32 n = shooterCount(3u, e.rng[i]);  // the loop adds to every volley (GAME_DESIGN 8)
                        for (u32 k = 0; k < n; ++k)
                            spawnBullet(Vec2{x, y}, dirOf(aim + (static_cast<float>(k) - 0.5f * static_cast<float>(n - 1)) * kHoundBurstSpread) * bulletSpeed,
                                        age);
                        e.state[i] = kMoving;
                        Event ev;
                        ev.type = EventType::EnemyShot;
                        ev.species = e.species[i];
                        ev.x = x;
                        ev.y = y;
                        emit(ev);
                    }
                }
                break;
            }
            case Family::Shoal:
                flock(i, s, dt);
                if (s.variant != kVariantA) shoalVariant(i, s, fd, tp, to);  // M5 part 4: B streams in lines, C spears
                break;
            case Family::Ram: {
                // Telegraph one beat, dash one beat in a straight line, rest two (species.json motion).
                const float dash = fd.dashSpeed * (fd.speed > 0.0f ? s.speed / fd.speed : 1.0f);
                switch (e.state[i]) {
                    case kTelegraph:
                        e.vx[i] = e.vy[i] = 0.0f;
                        if (m1_ >= e.until[i]) {
                            const float age = tickAgeOf(e.until[i]);
                            e.state[i] = kDash;
                            e.until[i] += static_cast<double>(fd.dashBeats) * kTicksPerBeat;
                            e.vx[i] = e.a[i] * dash;
                            e.vy[i] = e.b[i] * dash;
                            e.x[i] += e.vx[i] * loopSpeed_ * (age - dt);  // the dash began `age` ago (the motion below adds dt)
                            e.y[i] += e.vy[i] * loopSpeed_ * (age - dt);
                        }
                        break;
                    case kDash:
                        e.vx[i] = e.a[i] * dash;
                        e.vy[i] = e.b[i] * dash;
                        if (m1_ >= e.until[i]) {
                            e.state[i] = kRest;
                            e.until[i] += static_cast<double>(fd.restBeats) * kTicksPerBeat;
                        }
                        break;
                    case kRest:
                        e.vx[i] *= 0.85f;
                        e.vy[i] *= 0.85f;
                        if (m1_ >= e.until[i]) e.state[i] = kMoving;
                        break;
                    default: {
                        e.step[i] = 0;  // M5 part 4: Ram C's rebounds this dash
                        float h = e.heading[i];
                        if (tp >= 0) h = turnToward(h, angleOf(to), 3.0f * dt);
                        e.heading[i] = h;
                        const Vec2 v = dirOf(h) * s.speed;
                        e.vx[i] = v.x;
                        e.vy[i] = v.y;
                        if (beat && engaged && tp >= 0 && lengthSq(to) < kRamEngageWu * kRamEngageWu) {
                            const Vec2 d = normalizeOr(to, dirOf(h));
                            e.state[i] = kTelegraph;
                            e.until[i] = beatTick + static_cast<double>(fd.telegraphBeats) * kTicksPerBeat;
                            e.a[i] = d.x;
                            e.b[i] = d.y;
                            e.heading[i] = angleOf(d);
                            e.vx[i] = e.vy[i] = 0.0f;
                            Event ev;
                            ev.type = EventType::Telegraph;
                            ev.species = e.species[i];
                            ev.x = x;
                            ev.y = y;
                            ev.x2 = d.x;
                            ev.y2 = d.y;
                            ev.tick = e.until[i];
                            emit(ev);
                        }
                        break;
                    }
                }
                break;
            }
            case Family::Carom: {
                // Straight lines, reflecting off the walls (below), faster per bounce.
                const Vec2 d = normalizeOr(Vec2{e.vx[i], e.vy[i]}, dirOf(e.heading[i]));
                const Vec2 v = d * (s.speed * e.a[i]);
                e.vx[i] = v.x;
                e.vy[i] = v.y;
                e.heading[i] = angleOf(d);
                break;
            }
            case Family::Sentry: {
                // Stationary once placed; the B variant fires a rotating three-arm pattern every two
                // sixteenths while a player is near, after a glint of one beat.
                e.vx[i] = e.vy[i] = 0.0f;
                if (tp >= 0) e.heading[i] = turnToward(e.heading[i], angleOf(to), 2.0f * dt);
                if (s.variant != kVariantB) break;
                const auto volley = [&](double t) {
                    const float turn = static_cast<float>(t / kTicksPerBar) * kSentryTurnsPerBar * kTau;
                    const float age = tickAgeOf(t);
                    const u32 arms = shooterCount(fd.patternArms ? fd.patternArms : 3u, e.rng[i]);  // the loop adds arms (GAME_DESIGN 8)
                    for (u32 k = 0; k < arms; ++k) {
                        const float a = e.b[i] + turn + kTau * static_cast<float>(k) / static_cast<float>(arms);
                        spawnBullet(Vec2{x, y}, dirOf(a) * fd.bulletSpeed, age);
                    }
                    Event ev;
                    ev.type = EventType::EnemyShot;
                    ev.species = e.species[i];
                    ev.x = x;
                    ev.y = y;
                    emit(ev);
                };
                if (e.state[i] == kMoving) {
                    if (engaged && beat) {
                        e.state[i] = kGlint;
                        e.until[i] = beatTick + kTicksPerBeat;
                        Event ev;
                        ev.type = EventType::Telegraph;
                        ev.species = e.species[i];
                        ev.x = x;
                        ev.y = y;
                        ev.tick = e.until[i];
                        emit(ev);
                    }
                } else if (e.state[i] == kGlint) {
                    if (m1_ >= e.until[i]) {
                        e.state[i] = kFiring;
                        volley(e.until[i]);
                    }
                } else if (e.state[i] == kFiring) {
                    if (!engaged) {
                        e.state[i] = kMoving;
                    } else {
                        const double every = kTicksPerStep * static_cast<double>(fd.fireEverySteps ? fd.fireEverySteps : 2u);
                        forCrossings(m0_, m1_, every, [&](double t, i64) { volley(t); });
                    }
                }
                break;
            }
            case Family::Flanker:
            case Family::Skitter:
            case Family::Moth:
            case Family::Brood:
            case Family::Blink: moveFamily(i, s, fd, dt, tp, to, engaged); break;  // M5 part 2 (world_families.cpp)
            case Family::Wyrm:
            case Family::Tether:
            case Family::Weaver:
            case Family::Well:
            case Family::Mimic: moveFamily4(i, s, fd, dt, tp, to, engaged); break;  // M5 part 4 (world_families4.cpp)
            default: break;
        }

        // Integrate (own motion, faster in the loop (GAME_DESIGN 8), plus the decaying knockback), then the walls.
        const Vec2 was{e.x[i], e.y[i]};
        e.x[i] += (e.vx[i] * loopSpeed_ + e.kx[i]) * dt;
        e.y[i] += (e.vy[i] * loopSpeed_ + e.ky[i]) * dt;
        e.kx[i] *= knock;
        e.ky[i] *= knock;
        const float r = e.radius[i];
        if (pen_.up) {
            // M5: the Pen's walls stop bodies without hurting (a Carom reflects off them).
            Vec2 to{e.x[i], e.y[i]};
            const u8 hit = penKeep(was, to, r);
            if (hit) {
                e.x[i] = to.x;
                e.y[i] = to.y;
                if (s.family == Family::Carom) {
                    if (hit & 1u) e.vx[i] = -e.vx[i];
                    if (hit & 2u) e.vy[i] = -e.vy[i];
                    caromBounceShot(i, s, fd);  // M5: Carom B
                } else {
                    if (hit & 1u) e.kx[i] = 0.0f;
                    if (hit & 2u) e.ky[i] = 0.0f;
                }
            }
        }
        const float lx = kArenaHalfW - r, ly = kArenaHalfH - r;
        const bool hitX = e.x[i] < -lx || e.x[i] > lx;
        const bool hitY = e.y[i] < -ly || e.y[i] > ly;
        if (!hitX && !hitY) continue;
        e.x[i] = clampf(e.x[i], -lx, lx);
        e.y[i] = clampf(e.y[i], -ly, ly);
        if (s.family == Family::Drifter) {
            if (hitX) e.heading[i] = kPi - e.heading[i];
            if (hitY) e.heading[i] = -e.heading[i];
        } else if (s.family == Family::Carom) {
            if (hitX) e.vx[i] = -e.vx[i];
            if (hitY) e.vy[i] = -e.vy[i];
            e.a[i] = std::fmin(e.a[i] + fd.speedGainPerBounce, fd.maxSpeedFactor);
            caromBounceShot(i, s, fd);  // M5: Carom B
            if (s.variant == kVariantC) caromSplit(i, s, fd);  // M5 part 4
        } else {
            if (hitX) e.kx[i] = 0.0f;
            if (hitY) e.ky[i] = 0.0f;
            if (e.state[i] == kDash && s.family == Family::Ram && s.variant == kVariantC && e.step[i] < fd.v.reboundDashes) {
                // M5 part 4, Ram C: it rebounds off the wall for a second dash.
                if (hitX) e.a[i] = -e.a[i];
                if (hitY) e.b[i] = -e.b[i];
                e.heading[i] = angleOf(Vec2{e.a[i], e.b[i]});
                e.step[i] = static_cast<u8>(e.step[i] + 1);
                e.until[i] = m1_ + static_cast<double>(fd.dashBeats) * kTicksPerBeat;
            } else if (e.state[i] == kDash) {
                // A Ram meeting the wall stops its dash.
                e.state[i] = kRest;
                e.until[i] = std::floor(m1_ / kTicksPerBeat) * kTicksPerBeat + static_cast<double>(fd.restBeats) * kTicksPerBeat;
            }
        }
    }
}

}  // namespace rf::sim
