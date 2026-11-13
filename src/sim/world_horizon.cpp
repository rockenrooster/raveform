// M5 part 4: Event Horizon, the boss of Track 4 (design/bosses/horizon.md, .json). A well that pulls in everything near it;
// what the players leave alive feeds it, and a bar in which it eats less than its hunger ends in a gasp: for the next bar
// it takes full damage. Fed, it erupts on the bar line and its shield holds.
//
// Co-op (horizon.json "coop", the lead's rule for M5): the host eats (bodies die by its word) and decides each bar line
// (bossDecide kGeomHorizonBar: a = (fed, meal)); the meal travels in the heartbeat's first part slot. The pull on bodies,
// bullets, shots and Shards, the eruption, the wave and the jets follow song time on every device.

#include <algorithm>
#include <cmath>
#include <initializer_list>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
constexpr float kLaneStartWu = prose::kHzLaneWu;
constexpr float kLaneWind = prose::kHzLaneWindDeg * kTau / 360.0f;
constexpr float kWaveSpeed = prose::kHzWaveSpeed, kWaveShove = prose::kHzWaveShove;
constexpr float kThrowWu = prose::kHzThrowWu;
constexpr float kArmTurnDeg = prose::kHzArmTurnDeg;
constexpr u32 kEruptBase = prose::kHzEruptBase, kEruptPer = prose::kHzEruptPer, kEruptMax = prose::kHzEruptMax;
constexpr float kAxisTurn = prose::kHzAxisTurnDeg * kTau / 360.0f;
constexpr float kJetLengthWu = kHzJetLengthWu, kJetHalfWu = kHzJetHalfWu;
constexpr u32 kShardsPerBody = prose::kHzShardsPerBody;
}  // namespace

float World::horizonPullAt(Vec2 at, float max) const {
    // Speed toward the Well: max x (photonRadius / distance)^1.2 inside the range (the phase's scale), capped at max x scale.
    const BossState& s = boss_;
    const HorizonData& d = species_->horizon;
    const float dist = length(at - s.pos);
    if (dist >= d.pullRange || dist < 1.0f) return 0.0f;
    const float sc = d.phases[s.hz.phase < 3 ? s.hz.phase : 2].pullScale;
    return max * sc * std::fmin(1.0f, std::pow(d.photonRadius / dist, 1.2f));
}

bool World::horizonOn() const {
    return boss_.active && !boss_.dying && boss_.kind == BossKind::EventHorizon && m1_ >= boss_.fightTick;
}

void World::enterHorizon(Vec2 centre) {
    BossState& s = boss_;
    const HorizonData& d = species_->horizon;
    s.fightTick = s.enterTick + kTicksPerBar;  // the entrance bar: the lights lean in, the disc opens on beat 3
    s.phaseTick = s.fightTick;
    s.coreMax = d.health * s.scale;
    s.coreHp = s.coreMax;
    s.alive = 0;
    const float m = d.ringRadius[2] + 200.0f;
    s.pos = s.prevPos = Vec2{clampf(centre.x, -kArenaHalfW + m, kArenaHalfW - m), clampf(centre.y, -kArenaHalfH + m, kArenaHalfH - m)};
    s.hz = HorizonState{};
    s.hz.shield = true;
}

float World::horizonScale() const {
    const HorizonData& d = species_->horizon;
    return boss_.hz.shield ? d.fed : d.starved;
}

void World::horizonBar(double t, bool fed, u32 meal) {
    // The bar line's verdict (horizon.md rule 4).
    BossState& s = boss_;
    HorizonState& h = s.hz;
    const HorizonData& d = species_->horizon;
    const HorizonData::Phase& ph = d.phases[h.phase < 3 ? h.phase : 2];
    h.shield = fed;
    if (fed) {
        h.eruptLeft = static_cast<u16>(std::min(kEruptMax, kEruptBase + kEruptPer * meal));
        h.eruptArms = static_cast<u8>(ph.lanes);
        h.eruptAngle = static_cast<float>(hash32(static_cast<u32>(t), 0x48u) % 360u) * (kTau / 360.0f);
        if (ph.jets) {
            h.jetFrom = t;
            h.jetUntil = t + static_cast<double>(d.jetBurnBeats) * kTicksPerBeat;
        }
        // (f: the meal against the meter, 0..1: louder the bigger the meal)
        emitCue(BossCue::HzFed, t, static_cast<u8>(meal < 255 ? meal : 255),
                std::fmin(1.0f, static_cast<float>(meal) / static_cast<float>(d.meterCells ? d.meterCells : 1u)), s.pos);
    } else {
        h.gaspTick = t;
        emitCue(BossCue::HzStarved, t, 0, 0.0f, s.pos);
    }
    h.meal = 0;
    h.shardsIn = 0;
}

void World::eatBody(u32 i) {
    // A body that crosses the horizon is eaten: no Shards, no score, one cell (the host's word; a client waits for it).
    Enemies& e = enemies_;
    if ((e.flags[i] & kFlagDead) || role_ == NetRole::Client) return;
    e.flags[i] |= kFlagDead;
    Event ev;
    ev.type = EventType::Kill;
    ev.species = e.species[i];
    ev.b = 0xFF;
    ev.c = 1;  // as a dissolve: nothing comes of it
    ev.x = e.x[i];
    ev.y = e.y[i];
    emit(ev);
    if (role_ == NetRole::Host) logDeath(i, 0xFF, kDeathDissolve);
    if (boss_.hz.meal < 0xFFFF) ++boss_.hz.meal;
    ++boss_.hz.eaten;
    emitCue(BossCue::HzEat, m1_, static_cast<u8>(boss_.hz.meal < 255 ? boss_.hz.meal : 255), 0.0f, Vec2{e.x[i], e.y[i]});
}

bool World::horizonLane(u32 i, float dt) {
    // A body walking a lane: along its spiral at its own speed plus the pull, into the horizon.
    Enemies& e = enemies_;
    const BossState& s = boss_;
    if (!(s.active && !s.dying && s.kind == BossKind::EventHorizon)) {
        e.state[i] = kMoving;
        e.flags[i] &= static_cast<u8>(~kFlagCalled);
        return false;
    }
    const HorizonData& d = species_->horizon;
    const u32 lanes = d.phases[s.hz.phase < 3 ? s.hz.phase : 2].lanes;
    const float lane0 = kTau * e.a[i] / static_cast<float>(lanes ? lanes : 1u);
    const float speed = species_->get(e.species[i]).speed + horizonPullAt(Vec2{e.x[i], e.y[i]}, d.pullFodder);
    e.b[i] = std::fmax(e.b[i] - speed * dt, 0.0f);
    const float theta = lane0 + kLaneWind * (1.0f - e.b[i] / kLaneStartWu);
    const Vec2 want = s.pos + dirOf(theta) * e.b[i];
    const Vec2 vel = (want - Vec2{e.x[i], e.y[i]}) * (1.0f / std::fmax(dt, 1e-4f));
    e.vx[i] = vel.x / std::fmax(loopSpeed_, 1e-3f);
    e.vy[i] = vel.y / std::fmax(loopSpeed_, 1e-3f);
    e.heading[i] = angleOf(vel);
    return true;
}

void World::updateHorizon(float dt) {
    BossState& s = boss_;
    HorizonState& h = s.hz;
    const HorizonData& d = species_->horizon;
    if (m1_ < s.fightTick) return;
    // The phase by health.
    const float share = s.coreMax > 0.0f ? s.coreHp / s.coreMax : 0.0f;
    u8 ph = 0;
    while (ph < 2 && share < d.phases[ph].healthLo) ++ph;
    if (ph != h.phase) {
        h.phase = ph;
        s.phase = ph;
        s.phaseTick = m1_;
        Event ev;
        ev.type = EventType::BossPair;
        ev.a = ph;
        ev.c = static_cast<u8>(BossKind::EventHorizon);
        ev.x = s.pos.x;
        ev.y = s.pos.y;
        emit(ev);
    }
    const HorizonData::Phase& P = d.phases[ph];
    const u32 extra = presentCount() > 1 ? presentCount() - 1 : 0u;
    // Phase 3: the Well drifts toward the players.
    if (P.drift > 0.0f) {
        const Vec2 to = playersCentre() - s.pos;
        const float l = length(to);
        if (l > d.ringRadius[2]) s.pos = s.pos + to * (P.drift * dt / l);
    }
    // The stream (the host's): streamPerBar bodies a bar, spread over it, walking the lanes in turn.
    if (role_ != NetRole::Client)
        forCrossings(m0_, m1_, kTicksPerBar / static_cast<double>(P.stream ? P.stream : 1u), [&](double t, i64) {
            const i32 sp = pickSpecies(1);
            if (sp < 0) return;
            const u32 lane = h.laneSerial++ % (P.lanes ? P.lanes : 1u);
            const float a = kTau * static_cast<float>(lane) / static_cast<float>(P.lanes ? P.lanes : 1u);
            const Vec2 at = s.pos + dirOf(a) * kLaneStartWu;
            if (std::fabs(at.x) > kArenaHalfW - 20.0f || std::fabs(at.y) > kArenaHalfH - 20.0f) return;
            const i32 i = spawnEnemy(static_cast<u16>(sp), at, a + kPi, t);
            if (i < 0) return;
            enemies_.flags[i] |= kFlagCalled;
            enemies_.state[i] = kLane;
            enemies_.a[i] = static_cast<float>(lane);
            enemies_.b[i] = kLaneStartWu;
        });
    // The pull on the crowd (lane walkers move by their lane), and eating what crosses the horizon.
    Enemies& e = enemies_;
    const float eatR = d.horizonRadius;
    for (u32 i = 0; i < e.count; ++i) {
        if (e.flags[i] & kFlagDead) continue;
        const Vec2 p{e.x[i], e.y[i]};
        const Vec2 to = s.pos - p;
        const float l = length(to);
        if (l > d.pullRange) continue;
        if (l < eatR + e.radius[i] * 0.5f) {
            eatBody(i);
            continue;
        }
        if (e.state[i] == kLane || e.state[i] == kWarping || e.frozenUntil[i] > m1_) continue;
        const float v = horizonPullAt(p, d.pullFodder);
        e.x[i] += to.x / l * v * dt;
        e.y[i] += to.y / l * v * dt;
    }
    // Enemy bullets: pulled (the pull is a speed added to their own, as the page's wu/s read: slow ones curl back); those
    // that cross the horizon are gone (they do not feed it).
    for (u32 b = 0; b < bullets_.count; ++b) {
        if (bullets_.dead[b]) continue;
        const Vec2 to = s.pos - Vec2{bullets_.x[b], bullets_.y[b]};
        const float l = length(to);
        if (l > d.pullRange) continue;
        if (l < eatR) {
            bullets_.dead[b] = 1;
            continue;
        }
        const float v = horizonPullAt(Vec2{bullets_.x[b], bullets_.y[b]}, d.pullBullets);
        bullets_.x[b] += to.x / l * v * dt;
        bullets_.y[b] += to.y / l * v * dt;
    }
    // Player shots bend toward it (aim ahead of the curve).
    for (u32 k = 0; k < shots_.count; ++k) {
        if (shots_.dead[k]) continue;
        const Vec2 to = s.pos - Vec2{shots_.x[k], shots_.y[k]};
        const float l = length(to);
        if (l > d.pullRange || l < 1.0f) continue;
        const float v = horizonPullAt(Vec2{shots_.x[k], shots_.y[k]}, d.pullShots);
        const Vec2 vel = Vec2{shots_.dx[k], shots_.dy[k]} * shots_.speed[k] + to * (v * dt * 3.0f / l);
        const float sp = length(vel);
        if (sp < 1.0f) continue;
        shots_.dx[k] = vel.x / sp;
        shots_.dy[k] = vel.y / sp;
    }
    // Shards: pulled; five that fall in count as a body (this device's own; the host's count is the meal).
    for (u32 k = 0; k < shards_.count; ++k) {
        if (shards_.dead[k] || shards_.homing[k]) continue;
        const Vec2 to = s.pos - Vec2{shards_.x[k], shards_.y[k]};
        const float l = length(to);
        if (l > d.pullRange) continue;
        if (l < eatR) {
            shards_.dead[k] = 1;
            if (role_ != NetRole::Client && ++h.shardsIn >= kShardsPerBody) {
                h.shardsIn = 0;
                if (h.meal < 0xFFFF) ++h.meal;
            }
            continue;
        }
        const float v = horizonPullAt(Vec2{shards_.x[k], shards_.y[k]}, d.pullShards);
        shards_.x[k] += to.x / l * v * dt;
        shards_.y[k] += to.y / l * v * dt;
    }
    // The bar line (the host's verdict; a client keeps the shield up until it hears).
    forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
        if (t < s.fightTick + 1.0) return;  // (the fight's first bar line: nothing eaten yet, the shield holds)
        if (P.jets) h.axis += kAxisTurn;
        if (role_ == NetRole::Client) return;
        const u32 hunger = P.hunger + 2u * extra;
        const bool fed = h.meal >= hunger;
        bossDecide(kGeomHorizonBar, Vec2{fed ? 1.0f : 0.0f, static_cast<float>(h.meal)}, Vec2{}, t);
    });
    // The eruption: one bullet a sixteenth on each arm, the arms turning 12 degrees a bullet. They leave from the outer
    // ring (chosen: from the photon ring the Collapse's pull, 270 wu/s there, would take them back at once).
    if (h.eruptLeft)
        forCrossings(m0_, m1_, kTicksPerStep, [&](double t, i64) {
            for (u32 a = 0; a < h.eruptArms && h.eruptLeft; ++a, --h.eruptLeft) {
                const float ang = h.eruptAngle + kTau * static_cast<float>(a) / static_cast<float>(h.eruptArms ? h.eruptArms : 1u);
                spawnBullet(s.pos + dirOf(ang) * d.ringRadius[2], dirOf(ang) * d.bulletSpeed, tickAgeOf(t), d.bulletLife);
            }
            h.eruptAngle += kArmTurnDeg * (kTau / 360.0f);
        });
    // Phase 2: the tidal wave on beat 3 of every bar, shoving ships and Shards 140 wu outward as it passes.
    if (P.wave)
        forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64 beat) {
            if (((beat % 4) + 4) % 4 == 2) {
                h.waveTick = t;
                emitCue(BossCue::HzWave, t, 0, 0.0f, s.pos);
            }
        });
    if (m1_ - h.waveTick < 4.0 * kTicksPerBar) {
        const float tps = ticksPerSecond();
        const float r0 = kWaveSpeed * static_cast<float>((m0_ - h.waveTick) / tps), r1 = kWaveSpeed * static_cast<float>((m1_ - h.waveTick) / tps);
        if (r1 > 0.0f && r0 < d.pullRange) {
            for (u32 p = 0; p < playerCount_; ++p) {
                PlayerState& pl = players_[p];
                if (!pl.alive || !isLocal(p)) continue;
                const float l = length(pl.pos - s.pos);
                if (l > r0 && l <= r1) knockPlayer(p, normalizeOr(pl.pos - s.pos, Vec2{1.0f, 0.0f}) * kWaveShove);
            }
            for (u32 k = 0; k < shards_.count; ++k) {
                if (shards_.dead[k]) continue;
                const Vec2 rel = Vec2{shards_.x[k], shards_.y[k]} - s.pos;
                const float l = length(rel);
                if (l > r0 && l <= r1 && l > 1.0f) {
                    shards_.x[k] += rel.x / l * kWaveShove;
                    shards_.y[k] += rel.y / l * kWaveShove;
                }
            }
        }
    }
    // Touching the horizon: a Shield cell, and the ship is thrown 300 wu out.
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        if (!pl.alive || !isLocal(p)) continue;
        const Vec2 rel = pl.pos - s.pos;
        if (lengthSq(rel) > (eatR + kPlayerHitRadius) * (eatR + kPlayerHitRadius)) continue;
        knockPlayer(p, normalizeOr(rel, Vec2{1.0f, 0.0f}) * kThrowWu);
        if (!pl.invulnerable() && !rules_.godMode) bossHitPlayer(p, s.pos);
    }
    (void)dt;
}

bool World::horizonJetHits(Vec2 at, Vec2& from) const {
    // Phase 3's jets: two opposite beams along the Well's axis while they burn.
    const BossState& s = boss_;
    const HorizonState& h = s.hz;
    if (!(m1_ >= h.jetFrom && m1_ < h.jetUntil)) return false;
    for (float sgn : {-1.0f, 1.0f}) {
        const Vec2 dir = dirOf(h.axis) * sgn;
        const Vec2 rel = at - s.pos;
        const float along = rel.x * dir.x + rel.y * dir.y;
        if (along < species_->horizon.photonRadius || along > kJetLengthWu) continue;
        const float side = std::fabs(rel.x * dir.y - rel.y * dir.x);
        if (side <= kJetHalfWu + kPlayerHitRadius) {
            from = s.pos + dir * along;
            return true;
        }
    }
    return false;
}

void World::knockPlayer(u32 p, Vec2 by) {
    // The ship is moved (the controller takes it on its next update: PlayerState::knock).
    PlayerState& pl = players_[p];
    pl.knock = pl.knock + by;
    ++pl.knockSerial;
}

}  // namespace rf::sim
