// M5 part 2 (GAME_DESIGN 7.2, design/species.json families and variants): the families Tracks 2 and 3 bring (Flankers,
// Skitters, Moths, Brood, Blinks) and the variants their rosters use (Drifter B, Hound C, Carom B, Moth B, Brood B and C,
// Blink B, Flanker B). Behaviour is per family and per attack, never per species. Every ranged attack fires on the beat
// after a one-beat tell (species.json enemyBullet); every enemy-made spawn is the host's (co-op: the clients hear it).
//
// Per-enemy state (Enemies a, b, until, state, step, heading):
//   Flanker  a = its orbit's sense (+1 / -1), b = the radius it closes to; kGlint / kFiring while it shoots (B).
//   Skitter  until = when it may sidestep again (kMoving) or when the sidestep ends (kDodge, a, b = its direction).
//   Moth     a = its angle round the player, b = its radius; step counts its rosettes (B: each turned on by 20 degrees).
//   Brood    until = its next clutch (B); its minis (the broodling species) carry its serial in `group`.
//   Blink    kHop: a, b = the star's centre, heading = the point it stands on, step = hops made, until = the next hop;
//            then kTelegraph (a, b = the strike's line), kDash, kRest as a Ram's.
//   Hound C  b = its shield's facing.

#include <cmath>

#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
constexpr float kDeg = kTau / 360.0f;
i64 imod(i64 a, i64 m) { return ((a % m) + m) % m; }
}  // namespace

// ---------------------------------------------------------------------------------------------- landing

void World::landFamily(u32 i, const Species& s, const FamilyData& fd, Vec2 to) {
    Enemies& e = enemies_;
    const float toward = angleOf(to);
    const i32 tp = nearestPlayer(e.x[i], e.y[i]);
    const Vec2 P = tp >= 0 ? players_[tp].pos : Vec2{e.x[i], e.y[i]};
    switch (s.family) {
        case Family::Flanker:
            e.a[i] = rand01(e.rng[i]) < 0.5f ? 1.0f : -1.0f;
            e.b[i] = fd.orbitRadius;
            e.heading[i] = toward + e.a[i] * kFlankerArcRad;
            break;
        case Family::Skitter:
            e.heading[i] = toward;
            e.until[i] = m1_;
            break;
        case Family::Moth: {
            const Vec2 rel = Vec2{e.x[i], e.y[i]} - P;
            e.a[i] = angleOf(rel);
            e.b[i] = clampf(length(rel), fd.minRadius, std::fmax(fd.orbitRadius, fd.minRadius));
            e.heading[i] = e.a[i] + 0.5f * kPi;
            break;
        }
        case Family::Brood:
            if (static_cast<u16>(e.species[i]) == SpeciesTable::kBroodling) {
                e.heading[i] = toward;  // a mini chases at once
                break;
            }
            e.heading[i] = toward;
            e.until[i] = std::floor(m1_ / kTicksPerBar + 1.0) * kTicksPerBar;  // its first clutch on the next bar line (B)
            break;
        case Family::Blink: {
            // A star round the nearest player: it stands on the point nearest where it landed.
            e.a[i] = P.x;
            e.b[i] = P.y;
            e.heading[i] = angleOf(Vec2{e.x[i], e.y[i]} - P);
            const Vec2 at = P + dirOf(e.heading[i]) * fd.hopWu;
            e.x[i] = e.px[i] = at.x;
            e.y[i] = e.py[i] = at.y;
            e.state[i] = kHop;
            e.step[i] = 0;
            e.until[i] = (std::floor(m1_ / kTicksPerBeat) + 1.0) * kTicksPerBeat;
            break;
        }
        default: break;
    }
    if (s.family == Family::Hound) e.b[i] = toward;  // Hound C: the shield faces the player
}

// ---------------------------------------------------------------------------------------------- motion

void World::moveFamily(u32 i, const Species& s, const FamilyData& fd, float dt, i32 tp, Vec2 to, bool engaged) {
    Enemies& e = enemies_;
    const float x = e.x[i], y = e.y[i];
    const float dist = length(to);
    const FamilyData::Variants& v = fd.v;
    const float bulletSpeed = species_->bulletSpeed;
    const auto glint = [&](double until) {
        e.state[i] = kGlint;
        e.until[i] = until;
        Event ev;
        ev.type = EventType::Telegraph;
        ev.species = e.species[i];
        ev.x = x;
        ev.y = y;
        ev.tick = until;
        emit(ev);
    };
    const auto shot = [&]() {
        Event ev;
        ev.type = EventType::EnemyShot;
        ev.species = e.species[i];
        ev.x = x;
        ev.y = y;
        emit(ev);
    };
    switch (s.family) {
        case Family::Flanker: {
            // GAME_DESIGN 7.2: approach on wide arcs, encircle. Out of its orbit it comes in on a slant; on it, it circles and
            // closes closeRate wu/s (species.json motion).
            const Vec2 radial = dist > 1e-3f ? to * (1.0f / dist) : dirOf(e.heading[i]);
            const Vec2 tangent = perpOf(radial) * e.a[i];
            Vec2 want;
            const bool circling = dist < e.b[i] * kFlankerOrbitBand;
            if (!circling) {
                want = radial + tangent * kFlankerApproachSlant;
            } else {
                e.b[i] = std::fmax(e.b[i] - fd.closeRate * dt, 0.0f);
                want = tangent + radial * clampf((dist - e.b[i]) / kFlankerRadialWu, -1.0f, 1.0f);
            }
            if (tp >= 0) e.heading[i] = turnToward(e.heading[i], angleOf(want), kFlankerTurn * dt);
            const Vec2 vel = dirOf(e.heading[i]) * s.speed;
            e.vx[i] = vel.x;
            e.vy[i] = vel.y;
            // B: it fires inward, at the centre of its circle, on every beat while it circles, after a beat's tell.
            if (s.variant == kVariantB && tp >= 0) {
                if (e.state[i] == kMoving && circling && engaged) {
                    forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64) {
                        if (e.state[i] == kMoving) glint(t + kTicksPerBeat);
                    });
                } else if (e.state[i] == kGlint || e.state[i] == kFiring) {
                    if (!circling || !engaged) {
                        e.state[i] = kMoving;
                    } else {
                        const double every = kTicksPerStep * static_cast<double>(v.sideShotEverySteps ? v.sideShotEverySteps : 4u);
                        const auto fire = [&](double t) {
                            const u32 n = shooterCount(v.sideBullets ? v.sideBullets : 1u, e.rng[i]);
                            for (u32 k = 0; k < n; ++k)
                                spawnBullet(Vec2{x, y}, dirOf(angleOf(to) + (static_cast<float>(k) - 0.5f * static_cast<float>(n - 1)) * kHoundBurstSpread) * bulletSpeed,
                                            tickAgeOf(t));
                            shot();
                        };
                        if (e.state[i] == kGlint && m1_ >= e.until[i]) {
                            e.state[i] = kFiring;
                            fire(e.until[i]);
                        } else if (e.state[i] == kFiring) {
                            forCrossings(m0_, m1_, every, [&](double t, i64) { fire(t); });
                        }
                    }
                }
            }
            break;
        }
        case Family::Skitter: {
            // GAME_DESIGN 7.2: it sidesteps out of the aim line. In a player's line of fire (and not cooling down) it steps
            // dodgeDistance aside, square to that line, then may not again for dodgeCooldownBeats.
            if (e.state[i] == kDodge) {
                e.vx[i] = e.a[i] * (fd.dodgeDistance / kSkitterDodgeS);
                e.vy[i] = e.b[i] * (fd.dodgeDistance / kSkitterDodgeS);
                if (m1_ >= e.until[i]) {
                    e.state[i] = kMoving;
                    e.until[i] = m1_ + static_cast<double>(fd.dodgeCooldownBeats) * kTicksPerBeat;
                }
                break;
            }
            if (tp >= 0) e.heading[i] = turnToward(e.heading[i], angleOf(to), kSkitterTurn * dt);
            const Vec2 vel = dirOf(e.heading[i]) * s.speed;
            e.vx[i] = vel.x;
            e.vy[i] = vel.y;
            if (m1_ < e.until[i]) break;
            for (u32 p = 0; p < playerCount_; ++p) {
                const PlayerState& pl = players_[p];
                if (!pl.alive || !pl.fire) continue;
                const Vec2 rel = Vec2{x, y} - pl.pos;
                const float along = dot(rel, pl.aim);
                const float side = pl.aim.x * rel.y - pl.aim.y * rel.x;
                if (along <= 0.0f || along > kSkitterSeeWu || std::fabs(side) > kSkitterLaneWu + e.radius[i]) continue;
                const Vec2 away = perpOf(pl.aim) * (side >= 0.0f ? 1.0f : -1.0f);
                e.state[i] = kDodge;
                e.a[i] = away.x;
                e.b[i] = away.y;
                e.until[i] = m1_ + static_cast<double>(kSkitterDodgeS) * ticksPerSecond();
                break;
            }
            break;
        }
        case Family::Moth: {
            // GAME_DESIGN 7.2: it spirals inward round the player: its place turns turnsPerBar a bar and steps closePerBar / 4
            // inward on every beat, down to minRadius; the Moth flies to its place.
            const Vec2 P = tp >= 0 ? players_[tp].pos : Vec2{x, y};
            const float barS = static_cast<float>(kTicksPerBar) / ticksPerSecond();
            e.a[i] += kTau * fd.turnsPerBar / barS * dt;
            forCrossings(m0_, m1_, kTicksPerBeat, [&](double, i64) { e.b[i] = std::fmax(e.b[i] - 0.25f * fd.closePerBar, fd.minRadius); });
            const Vec2 want = P + dirOf(e.a[i]) * e.b[i] - Vec2{x, y};
            const float l = length(want);
            const float sp = std::fmin(s.speed * kMothCatchUp, l / std::fmax(dt, 1e-4f));
            const Vec2 vel = l > 1e-3f ? want * (sp / l) : Vec2{};
            e.vx[i] = vel.x;
            e.vy[i] = vel.y;
            e.heading[i] = e.a[i] + 0.5f * kPi;
            // B: petals outward on the eighths (two opposite, each pair turned on 20 degrees), after a beat's tell.
            if (s.variant == kVariantB) {
                if (e.state[i] == kMoving && engaged && tp >= 0) {
                    forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64) {
                        if (e.state[i] == kMoving) glint(t + kTicksPerBeat);
                    });
                } else if (e.state[i] == kGlint || e.state[i] == kFiring) {
                    if (!engaged) {
                        e.state[i] = kMoving;
                    } else {
                        const double every = kTicksPerStep * static_cast<double>(v.petalEverySteps ? v.petalEverySteps : 2u);
                        const auto rosette = [&](double t) {
                            const u32 n = shooterCount(v.petals ? v.petals : 2u, e.rng[i]);
                            const float turn = e.a[i] + static_cast<float>(e.step[i]) * v.petalRotateDeg * kDeg;
                            for (u32 k = 0; k < n; ++k)
                                spawnBullet(Vec2{x, y}, dirOf(turn + kTau * static_cast<float>(k) / static_cast<float>(n)) * v.petalSpeed, tickAgeOf(t));
                            e.step[i] = static_cast<u8>(e.step[i] + 1u);
                            shot();
                        };
                        if (e.state[i] == kGlint && m1_ >= e.until[i]) {
                            e.state[i] = kFiring;
                            rosette(e.until[i]);
                        } else if (e.state[i] == kFiring) {
                            forCrossings(m0_, m1_, every, [&](double t, i64) { rosette(t); });
                        }
                    }
                }
            }
            break;
        }
        case Family::Brood: {
            if (static_cast<u16>(e.species[i]) == SpeciesTable::kBroodling) {
                // A mini: it chases like a Hound (GAME_DESIGN 7.2, species.json brood.motion).
                if (tp >= 0) e.heading[i] = turnToward(e.heading[i], angleOf(to), species_->family(Family::Hound).turnRate * dt);
                const Vec2 vel = dirOf(e.heading[i]) * s.speed;
                e.vx[i] = vel.x;
                e.vy[i] = vel.y;
                break;
            }
            // A slow carrier drifting toward the nearest player.
            if (tp >= 0) e.heading[i] = turnToward(e.heading[i], angleOf(to), 1.0f * dt);
            const Vec2 vel = dirOf(e.heading[i]) * s.speed;
            e.vx[i] = vel.x;
            e.vy[i] = vel.y;
            // B: a clutch every hatchEveryBars bars on the bar line (the host's), while it has fewer than maxMinisEach.
            if (s.variant == kVariantB && m1_ >= e.until[i]) {
                const double every = static_cast<double>(v.hatchEveryBars > 0.0f ? v.hatchEveryBars : 1.0f) * kTicksPerBar;
                const double t = e.until[i];
                e.until[i] = t + every;
                if (role_ != NetRole::Client && engaged) hatchBroodlings(i, v.hatchCount, t);
            }
            break;
        }
        case Family::Blink: {
            // GAME_DESIGN 7.2: it blinks on the beats along a star round the player (every second point), then strikes
            // through where the player stood at the tell; species.json motion. B: three bullets along each hop.
            e.vx[i] = e.vy[i] = 0.0f;
            switch (e.state[i]) {
                case kHop: {
                    if (m1_ < e.until[i]) break;
                    const double t = e.until[i];
                    const Vec2 C{e.a[i], e.b[i]};
                    if (e.step[i] >= fd.hops) {
                        // The tell: a line through where the nearest player stands now.
                        const Vec2 P = tp >= 0 ? players_[tp].pos : C;
                        const Vec2 d = normalizeOr(P - Vec2{x, y}, dirOf(e.heading[i] + kPi));
                        e.state[i] = kTelegraph;
                        e.a[i] = d.x;
                        e.b[i] = d.y;
                        e.until[i] = t + static_cast<double>(fd.strikeTellBeats) * kTicksPerBeat;
                        Event ev;
                        ev.type = EventType::Telegraph;
                        ev.species = e.species[i];
                        ev.x = x;
                        ev.y = y;
                        ev.x2 = d.x;
                        ev.y2 = d.y;
                        ev.tick = e.until[i];
                        emit(ev);
                        break;
                    }
                    const Vec2 from{x, y};
                    e.heading[i] += 2.0f * kTau / 5.0f;  // the next point in star order: every second point of five
                    const Vec2 at = C + dirOf(e.heading[i]) * fd.hopWu;
                    e.x[i] = e.px[i] = at.x;
                    e.y[i] = e.py[i] = at.y;
                    e.step[i] = static_cast<u8>(e.step[i] + 1u);
                    e.until[i] = t + static_cast<double>(fd.hopEveryBeats) * kTicksPerBeat;
                    if (s.variant == kVariantB && v.pathBullets) {
                        // B: bullets left along the hop, flying on along it.
                        const Vec2 d = normalizeOr(at - from, dirOf(e.heading[i]));
                        const u32 n = shooterCount(v.pathBullets, e.rng[i]);
                        for (u32 k = 0; k < n; ++k) {
                            const float u = static_cast<float>(k + 1) / static_cast<float>(n + 1);
                            spawnBullet(from + (at - from) * u, d * v.pathBulletSpeed, tickAgeOf(t));
                        }
                        shot();
                    }
                    Event ev;  // a hop is a blink (the view's flash)
                    ev.type = EventType::WarpLand;
                    ev.species = e.species[i];
                    ev.x = at.x;
                    ev.y = at.y;
                    ev.age = tickAgeOf(t);
                    emit(ev);
                    break;
                }
                case kTelegraph:
                    if (m1_ >= e.until[i]) {
                        e.state[i] = kDash;
                        e.until[i] += static_cast<double>(fd.strikeBeats) * kTicksPerBeat;
                    }
                    break;
                case kDash:
                    e.vx[i] = e.a[i] * fd.strikeSpeed * (fd.speed > 0.0f ? s.speed / fd.speed : 1.0f);
                    e.vy[i] = e.b[i] * fd.strikeSpeed * (fd.speed > 0.0f ? s.speed / fd.speed : 1.0f);
                    if (m1_ >= e.until[i]) {
                        e.state[i] = kRest;
                        e.until[i] += static_cast<double>(fd.restBeats) * kTicksPerBeat;
                    }
                    break;
                case kRest:
                    if (m1_ >= e.until[i]) {
                        // The next star, round the player again.
                        const Vec2 P = tp >= 0 ? players_[tp].pos : Vec2{x, y};
                        e.a[i] = P.x;
                        e.b[i] = P.y;
                        e.heading[i] = angleOf(Vec2{x, y} - P);
                        const Vec2 at = P + dirOf(e.heading[i]) * fd.hopWu;
                        e.x[i] = e.px[i] = at.x;
                        e.y[i] = e.py[i] = at.y;
                        e.state[i] = kHop;
                        e.step[i] = 0;
                        e.until[i] = (std::floor(m1_ / kTicksPerBeat) + 1.0) * kTicksPerBeat;
                    }
                    break;
                default:
                    e.state[i] = kRest;
                    e.until[i] = m1_;
                    break;
            }
            break;
        }
        default: break;
    }
}

// ---------------------------------------------------------------------------------------------- variants of the slice's families

void World::drifterShot(u32 i, const Species& s, const FamilyData& fd, i32 tp, Vec2 to, bool engaged) {
    // Drifter B (GAME_DESIGN 7.2): a slow aimed shot every aimedShotEveryBars bars, on the downbeat after a beat's tell; each
    // Drifter keeps its own bar of the cycle (by its spawn number).
    Enemies& e = enemies_;
    const FamilyData::Variants& v = fd.v;
    if (s.variant != kVariantB || tp < 0) return;
    const i64 every = static_cast<i64>(v.aimedShotEveryBars > 0.0f ? v.aimedShotEveryBars : 2.0f);
    if (e.state[i] == kMoving && engaged) {
        forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64 beat) {
            if (e.state[i] != kMoving || imod(beat, 4) != 3) return;
            const i64 bar = static_cast<i64>(std::floor(t / kTicksPerBar));
            if (imod(bar + static_cast<i64>(e.serial[i]), every) != every - 1) return;
            e.state[i] = kGlint;
            e.until[i] = t + kTicksPerBeat;
            Event ev;
            ev.type = EventType::Telegraph;
            ev.species = e.species[i];
            ev.x = e.x[i];
            ev.y = e.y[i];
            ev.tick = e.until[i];
            emit(ev);
        });
    } else if (e.state[i] == kGlint && m1_ >= e.until[i]) {
        const u32 n = shooterCount(v.aimedBullets ? v.aimedBullets : 1u, e.rng[i]);
        for (u32 k = 0; k < n; ++k)
            spawnBullet(Vec2{e.x[i], e.y[i]}, dirOf(angleOf(to) + (static_cast<float>(k) - 0.5f * static_cast<float>(n - 1)) * kHoundBurstSpread) * species_->bulletSpeed,
                        tickAgeOf(e.until[i]));
        e.state[i] = kMoving;
        Event ev;
        ev.type = EventType::EnemyShot;
        ev.species = e.species[i];
        ev.x = e.x[i];
        ev.y = e.y[i];
        emit(ev);
    }
}

void World::caromBounceShot(u32 i, const Species& s, const FamilyData& fd) {
    // Carom B (GAME_DESIGN 7.2): it sheds bullets at each bounce, fanned round its new heading.
    if (s.variant != kVariantB) return;
    Enemies& e = enemies_;
    const FamilyData::Variants& v = fd.v;
    const u32 n = shooterCount(static_cast<u32>(v.bulletsPerBounce > 0.0f ? v.bulletsPerBounce : 3.0f), e.rng[i]);
    const float h = angleOf(Vec2{e.vx[i], e.vy[i]});
    const float spread = v.bounceFanDeg * kDeg;
    for (u32 k = 0; k < n; ++k) {
        const float u = n > 1 ? static_cast<float>(k) / static_cast<float>(n - 1) - 0.5f : 0.0f;
        spawnBullet(Vec2{e.x[i], e.y[i]}, dirOf(h + u * spread) * species_->bulletSpeed, 0.0f);
    }
    Event ev;
    ev.type = EventType::EnemyShot;
    ev.species = e.species[i];
    ev.x = e.x[i];
    ev.y = e.y[i];
    emit(ev);
}

void World::houndShield(u32 i, const Species& s, const FamilyData& fd, i32 tp, Vec2 to, float dt) {
    // Hound C: its frontal shield turns to keep facing the nearest player, slower than it steers.
    if (s.variant != kVariantC || tp < 0) return;
    enemies_.b[i] = turnToward(enemies_.b[i], angleOf(to), fd.v.shieldTurnRate * dt);
}

bool World::shieldStops(u32 i, Vec2 shotDir) const {
    // Hound C (GAME_DESIGN 7.2: "frontal shield, must be flanked"): a shot that meets its front arc is shed.
    const Enemies& e = enemies_;
    const Species& s = species_->get(e.species[i]);
    if (s.family != Family::Hound || s.variant != kVariantC) return false;
    const float half = 0.5f * species_->family(Family::Hound).v.shieldArcDeg * kDeg;
    return dot(shotDir * -1.0f, dirOf(e.b[i])) > std::cos(half);
}

void World::hatchBroodlings(u32 i, u32 n, double land) {
    // Brood: minis round the carrier (the host's spawns), at most maxMinisEach of its own alive at once.
    Enemies& e = enemies_;
    const FamilyData& fd = species_->family(Family::Brood);
    const u16 owner = static_cast<u16>(e.serial[i] & 0xFFFFu);
    u32 alive = 0;
    for (u32 j = 0; j < e.count; ++j)
        if (!(e.flags[j] & kFlagDead) && e.species[j] == SpeciesTable::kBroodling && e.group[j] == owner) ++alive;
    const Vec2 at{e.x[i], e.y[i]};
    const float r = e.radius[i];
    for (u32 k = 0; k < n && alive < fd.maxMinisEach; ++k) {
        const float a = randRange(e.rng[i], 0.0f, kTau);
        const i32 j = spawnEnemy(static_cast<u16>(SpeciesTable::kBroodling), at + dirOf(a) * (r + 6.0f), a, land);
        if (j < 0) break;
        e.group[static_cast<u32>(j)] = owner;
        ++alive;
    }
}

void World::familyDeath(u32 i, const Species& s) {
    // Brood C (GAME_DESIGN 7.2): it bursts into a swarm of minis on death (the host's, killEnemy).
    if (s.family != Family::Brood || s.variant != kVariantC || static_cast<u16>(enemies_.species[i]) == SpeciesTable::kBroodling) return;
    const FamilyData& fd = species_->family(Family::Brood);
    Enemies& e = enemies_;
    const Vec2 at{e.x[i], e.y[i]};
    const u32 n = fd.v.swarmOnDeath;
    for (u32 k = 0; k < n; ++k) {
        const float a = kTau * static_cast<float>(k) / static_cast<float>(n ? n : 1u);
        spawnEnemy(static_cast<u16>(SpeciesTable::kBroodling), at + dirOf(a) * 10.0f, a, m1_);
    }
}

}  // namespace rf::sim
