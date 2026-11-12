// Kaleidos (design/bosses/kaleidos.md; M3 stage B). The wheel's angle, who rests, every bullet and beam
// are functions of song time and of which pairs are alive; health is the only state the fight keeps.

#include <cmath>

#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
constexpr float kDeg = kTau / 360.0f;
constexpr float kArmStep = kTau / 6.0f;

u32 pairsIn(u8 mask) { return (mask & 1u) + ((mask >> 1) & 1u) + ((mask >> 2) & 1u); }

// A count scaled by a lever (play test 1), rounded, never below `least` (an exact count at scale 1).
u32 scaledCount(u32 n, float scale, u32 least) {
    if (scale == 1.0f) return n;
    const u32 m = static_cast<u32>(std::lround(static_cast<float>(n) * scale));
    return m < least ? least : m;
}

// Half-width of an arm's hull at `u` wu from the centre: the facets' widths, joined.
float armHalfWidth(const BossData& b, float u) {
    const BossData::Facet* f = b.facets;
    if (u <= f[0].armRadius) return f[0].halfWidth;
    for (u32 k = 0; k + 1 < 4; ++k) {
        if (u <= f[k + 1].armRadius) {
            const float s = (u - f[k].armRadius) / (f[k + 1].armRadius - f[k].armRadius);
            return f[k].halfWidth + (f[k + 1].halfWidth - f[k].halfWidth) * s;
        }
    }
    return u <= f[3].armRadius + f[3].length * 0.5f ? f[3].halfWidth : 10.0f;  // then the tip star
}

double barStartOf(double t) { return std::floor(t / kTicksPerBar) * kTicksPerBar; }
}  // namespace

u32 World::bossFightBar(double tick) const {
    if (tick < boss_.fightTick) return 0;
    return static_cast<u32>(std::floor((tick - boss_.fightTick) / kTicksPerBar + 1e-9));
}

float World::bossArmAngle(u32 arm) const { return boss_.angle + static_cast<float>(arm) * kArmStep; }

namespace {
float wheelAt(const BossState& s, const BossData& b, double t) {
    const BossData::Phase& ph = b.phases[s.phase < 4 ? s.phase : 3];
    if (t <= s.phaseTick) return s.angle0;
    return s.angle0 + ph.direction * kTau * static_cast<float>((t - s.phaseTick) / (static_cast<double>(ph.turnBars) * kTicksPerBar));
}
}  // namespace

bool World::bossResting(u32 pair, double tick) const {
    const BossState& s = boss_;
    if (!s.active || s.dying || tick < s.fightTick || !(s.alive & (1u << pair))) return false;
    const u32 fb = bossFightBar(tick);
    switch (s.phase) {
        case 0: return fb % 3 == pair;  // kaleidos.md rule 1: one pair rests each bar, A, B, C in turn
        case 1: {
            // The two pairs alternate bars: one fires, the other rests.
            u32 live[2] = {0, 0}, n = 0;
            for (u32 p = 0; p < 3 && n < 2; ++p)
                if (s.alive & (1u << p)) live[n++] = p;
            return live[(fb + 1) % 2] == pair;
        }
        case 2: return tick - barStartOf(tick) >= 3.0 * kTicksPerBeat;  // fires three beats, rests on beat 4
        default: return false;
    }
}

u8 World::bossFiringMask(double t) const {
    const BossState& s = boss_;
    if (!s.active || s.dying || t < s.fightTick) return 0;
    const BossData::Phase& ph = species_->boss.phases[s.phase < 4 ? s.phase : 3];
    if (ph.fireStepTicks <= 0.0) return 0;
    const double inStep = std::fmod(t, ph.fireStepTicks);
    if (inStep > 0.5 && ph.fireStepTicks - inStep > 0.5) return 0;
    const double inBar = t - barStartOf(t);
    u8 mask = 0;
    for (u32 p = 0; p < 3; ++p) {
        if (!(s.alive & (1u << p)) || bossResting(p, t)) continue;
        // Refraction: the firing pair opens its bar with the beam, then fans on the eighths.
        if (s.phase == 1 && inBar < kTicksPerBeat) continue;
        mask = static_cast<u8>(mask | (1u << p));
    }
    return mask;
}

u8 World::bossBeamState(u32 pair, double tick) const {
    const BossState& s = boss_;
    if (!s.active || s.dying || s.phase != 1 || tick < s.fightTick || !(s.alive & (1u << pair))) return 0;
    const u32 fb = bossFightBar(tick);
    const double inBar = tick - barStartOf(tick);
    // The pair that rests this bar fires next bar: telegraphed for the whole of this one (4 beats) ...
    if (bossResting(pair, tick) && fb >= s.phaseBar0) return 1;
    // ... and opens its bar with a beam one beat long, when it rested the bar before within this phase.
    if (!bossResting(pair, tick) && inBar < kTicksPerBeat && fb >= s.phaseBar0 + 1) return 2;
    return 0;
}

void World::spawnBossNow(double enterTick, Vec2 pos) {
    const BossData& b = species_->boss;
    BossState& s = boss_;
    s = BossState{};
    s.active = true;
    s.alive = 0x7;
    s.enterTick = enterTick;
    s.fightTick = enterTick + kTicksPerBar;  // the entrance bar: facets streak in and lock together
    s.phaseTick = s.fightTick;
    // Health: x (1 + 0.5 per extra player) as for every boss; Track 2's Kaleidos has 1.5x and Track 3's 2x
    // (the lead), and the loop adds 6% a Track after that.
    float scale = 1.0f + kCoopBossHealthPerPlayer * static_cast<float>(playerCount_ - 1);
    const u32 tr = director_.track < 1 ? 1u : director_.track;
    scale *= tr <= 3 ? kBossTrackHealth[tr - 1] : kBossTrackHealth[2] * std::pow(kBossEndlessHealth, static_cast<float>(tr - 3));
    // GAME_DESIGN 8 (play test 1, second pass): the first boss of a run, Track 1's, is the teacher: 0.7 of its health.
    if (tr == 1) scale *= kFirstBossHealth;
    // The loop ramps enemy health, the boss's too (GAME_DESIGN 8, the lead's review of play test 2).
    scale *= loopRamp(kLoopHealthRamp, tr);
    s.pairMax = b.pairHealth * scale;
    s.coreMax = b.coreHealth * scale;
    for (float& h : s.pairHp) h = s.pairMax;
    s.coreHp = s.coreMax;
    const float m = b.radius + 120.0f;
    pos.x = clampf(pos.x, -kArenaHalfW + m, kArenaHalfW - m);
    pos.y = clampf(pos.y, -kArenaHalfH + m, kArenaHalfH - m);
    s.pos = s.prevPos = pos;
    director_.bossSpawned = true;
    Event ev;
    ev.type = EventType::BossEnter;
    ev.x = pos.x;
    ev.y = pos.y;
    ev.tick = s.fightTick;
    emit(ev);
    if (role_ == NetRole::Host && netLog_.bossCount < NetLog::kBoss) {
        NetLog::Boss& o = netLog_.boss[netLog_.bossCount++];
        o = NetLog::Boss{};
        o.kind = 0;
        o.x = pos.x;
        o.y = pos.y;
        o.tick = enterTick;
    }
}

bool World::bossHitTest(Vec2 a, Vec2 bpt, float r, u8& part, u8& pair, float& tOut) const {
    const BossState& s = boss_;
    const BossData& b = species_->boss;
    if (!s.active || s.dying) return false;
    // Sample the path (a shot moves ~23 wu a tick; the thinnest part is 20 wu wide).
    static const float kT[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    const float lensHalf = b.lensHalfAngleDeg * kDeg;
    for (float t : kT) {
        const Vec2 p = a + (bpt - a) * t;
        const Vec2 rel = p - s.pos;
        const float dist = length(rel);
        if (dist > b.radius + r + 12.0f) continue;
        // Arms of the living pairs.
        for (u32 k = 0; k < 6; ++k) {
            const u32 pr = k % 3;
            if (!(s.alive & (1u << pr))) continue;
            const Vec2 d = dirOf(s.angle + static_cast<float>(k) * kArmStep);
            const float u = rel.x * d.x + rel.y * d.y;
            if (u < kBossArmInner || u > kBossArmOuter) continue;
            const float v = std::fabs(rel.x * d.y - rel.y * d.x);
            if (v <= armHalfWidth(b, u) + r) {
                part = 2;
                pair = static_cast<u8>(pr);
                tOut = t;
                return true;
            }
        }
        // Lens plates (two per living pair, centred on its arms) stop shots; between and behind them, the core.
        if (dist <= b.lensRadius + r) {
            const float ang = angleOf(rel);
            bool plate = false;
            u8 platePair = 0;
            for (u32 k = 0; k < 6 && !plate; ++k) {
                const u32 pr = k % 3;
                if (!(s.alive & (1u << pr))) continue;
                if (std::fabs(wrapAngle(ang - (s.angle + static_cast<float>(k) * kArmStep))) <= lensHalf) {
                    plate = true;
                    platePair = static_cast<u8>(pr);
                }
            }
            if (plate && dist >= b.lensInner - r - 8.0f) {
                part = 1;
                pair = platePair;
                tOut = t;
                return true;
            }
            if (dist <= b.coreRingRadius[0] + r) {
                part = plate ? 1 : 3;  // inside the plate's arc the plate took it first
                pair = platePair;
                tOut = t;
                return true;
            }
        }
    }
    return false;
}

u8 World::shootBoss(Vec2 a, Vec2 b, float damage) {
    u8 part = 0, pair = 0;
    float t = 0.0f;
    if (!bossHitTest(a, b, kShotRadius, part, pair, t)) return 0;
    damageBoss(part, pair, damage, 0, a + (b - a) * t, normalizeOr(b - a, Vec2{1.0f, 0.0f}));
    return part;
}

void World::damageBoss(u8 part, u8 pair, float dmg, u8 owner, Vec2 at, Vec2 dir) {
    BossState& s = boss_;
    const BossData& b = species_->boss;
    Event ev;
    ev.type = EventType::Impact;
    ev.species = kBossSpecies;
    ev.c = part;
    ev.a = pair;
    ev.b = owner;  // play test 2: the shooter
    ev.x = at.x;
    ev.y = at.y;
    ev.x2 = dir.x;
    ev.y2 = dir.y;
    if (!s.active || s.dying || m1_ < s.fightTick) {
        ev.c = 1;  // the entrance: nothing gets through yet
        emit(ev);
        return;
    }
    ++s.hits;
    if (part == 2) {
        // kaleidos.md rule 1: the resting pair takes triple damage, firing pairs a third.
        const float scale = bossResting(pair, m1_) ? b.dmgResting : b.dmgFiring;
        damageBossFinal(part, pair, dmg * scale, owner, at, dir, scale);
    } else if (part == 3) {
        // Rule 3: the iris opens for the whole first beat of every bar: triple damage through any open sector.
        const bool downbeat = m1_ - barStartOf(m1_) < kTicksPerBeat;
        const float scale = downbeat ? b.dmgCoreDownbeat : b.dmgCoreExposed;
        damageBossFinal(part, pair, dmg * scale, owner, at, dir, scale);
    } else {
        ev.f = b.dmgLens;
        emit(ev);
    }
}

void World::damageBossFinal(u8 part, u8 pair, float dmg, u8 owner, Vec2 at, Vec2 dir, float scale) {
    // The damage with its multiplier already applied: the claimant's (M4: a host applies a client's claim as
    // the client weighed it). `scale` < 0: no impact shown (a Nova's blast).
    BossState& s = boss_;
    if (!s.active || s.dying || pair > 2) return;
    if (scale >= 0.0f) {
        Event ev;
        ev.type = EventType::Impact;
        ev.species = kBossSpecies;
        ev.c = part;
        ev.a = pair;
        ev.b = owner;
        ev.x = at.x;
        ev.y = at.y;
        ev.x2 = dir.x;
        ev.y2 = dir.y;
        ev.f = scale;
        emit(ev);
    }
    const bool client = role_ == NetRole::Client;
    if (client && netLog_.bossClaimCount < NetLog::kBossClaims) {
        NetBossClaim& c = netLog_.bossClaims[netLog_.bossClaimCount++];
        c.part = part;
        c.pair = pair;
        c.damage = dmg;
    } else if (client) {
        ++netLog_.overflow;
    }
    if (part == 2) {
        if (!(s.alive & (1u << pair))) return;
        s.pairHp[pair] -= dmg;
        if (client) {
            if (s.pairHp[pair] < 0.0f) s.pairHp[pair] = 0.0f;  // the host decides when it breaks
        } else if (s.pairHp[pair] <= 0.0f) {
            killBossPair(pair, owner);
        }
    } else if (part == 3) {
        s.coreHp -= dmg;
        if (client) {
            if (s.coreHp < 0.0f) s.coreHp = 0.0f;
        } else if (s.coreHp <= 0.0f) {
            killBoss(owner);
        }
    }
}

void World::killBossPair(u8 pair, u8 owner) {
    BossState& s = boss_;
    if (!(s.alive & (1u << pair))) return;
    s.alive = static_cast<u8>(s.alive & ~(1u << pair));
    s.pairHp[pair] = 0.0f;
    if (owner < kMaxPlayers && players_[owner].present && isLocal(owner)) {
        PlayerState& pl = players_[owner];
        pl.score += static_cast<u64>(std::llround(static_cast<double>(kBossPairScore) * pl.chain * rules_.scoreMultiplier));
    }
    if (role_ == NetRole::Host && netLog_.bossCount < NetLog::kBoss) {
        NetLog::Boss& o = netLog_.boss[netLog_.bossCount++];
        o = NetLog::Boss{};
        o.kind = 1;
        o.pair = pair;
        o.killer = owner;
    }
    // Its Shards burst from both tips (each device its own players', M4).
    for (u32 m = 0; m < 2; ++m) {
        const Vec2 tip = s.pos + dirOf(s.angle + static_cast<float>(pair + 3 * m) * kArmStep) * species_->boss.tip.armRadius;
        for (u32 p = 0; p < playerCount_; ++p) {
            if (!players_[p].present || !isLocal(p)) continue;
            for (u32 k = 0; k < kBossPairShards / 2; ++k)
                spawnShard(tip, dirOf(rng_.range(0.0f, kTau)) * rng_.range(kShardBurstLo, kShardBurstHi * 1.5f), static_cast<u8>(p));
        }
    }
    // The wheel flinches: it stops for one beat and turns the other way, faster (the next phase).
    s.angle0 = wheelAt(s, species_->boss, m1_);
    s.phase = static_cast<u8>(3 - pairsIn(s.alive));
    s.phaseTick = m1_ + kTicksPerBeat;
    s.phaseBar0 = bossFightBar(s.phaseTick) + 1;
    Event ev;
    ev.type = EventType::BossPair;
    ev.a = pair;
    ev.b = static_cast<u8>(pairsIn(s.alive));
    ev.x = s.pos.x;
    ev.y = s.pos.y;
    ev.f = s.angle + static_cast<float>(pair) * kArmStep;
    emit(ev);
}

void World::killBoss(u8 owner) {
    BossState& s = boss_;
    if (s.dying) return;
    s.dying = true;
    s.deathTick = m1_;
    s.coreHp = 0.0f;
    if (owner < kMaxPlayers && players_[owner].present && isLocal(owner)) {
        PlayerState& pl = players_[owner];
        ++pl.kills;
        pl.score += static_cast<u64>(std::llround(static_cast<double>(kBossCoreScore) * pl.chain * rules_.scoreMultiplier));
    }
    ++counters_.kills;
    // Its bullets go with it; then the Shards (kaleidos.md: "then the Shards"): 200 per player (each device
    // its own players', M4).
    for (u32 i = 0; i < bullets_.count; ++i) bullets_.dead[i] = 1;
    for (u32 p = 0; p < playerCount_; ++p) {
        if (!players_[p].present || !isLocal(p)) continue;
        for (u32 k = 0; k < kBossShards; ++k)
            spawnShard(s.pos + dirOf(rng_.range(0.0f, kTau)) * rng_.range(0.0f, 60.0f),
                       dirOf(rng_.range(0.0f, kTau)) * rng_.range(120.0f, 520.0f), static_cast<u8>(p));
        // Play test 2 (GAME_DESIGN 6.1): a boss drops two Cells for each player, whatever its health.
        for (u32 k = 0; k < kBossCells; ++k)
            spawnCell(s.pos, dirOf(kTau * (static_cast<float>(k) + 0.25f) / static_cast<float>(kBossCells)) * 160.0f, static_cast<u8>(p));
    }
    // The Track ends: its Track-change Break begins on the first bar line at least half a bar away, so
    // the band, which hears of it now, can still start it on that line.
    DirectorState& d = director_;
    const double rel = (m1_ + 0.5 * kTicksPerBar - d.startTick) / kTicksPerBar;
    d.changeBar = static_cast<u32>(std::floor(rel)) + 1;
    if (role_ == NetRole::Host && netLog_.bossCount < NetLog::kBoss) {
        NetLog::Boss& o = netLog_.boss[netLog_.bossCount++];
        o = NetLog::Boss{};
        o.kind = 2;
        o.killer = owner;
        o.changeBar = d.changeBar;
        o.x = s.pos.x;
        o.y = s.pos.y;
    }
    Event ev;
    ev.type = EventType::BossDeath;
    ev.x = s.pos.x;
    ev.y = s.pos.y;
    ev.tick = tickOfBar(d.changeBar);
    emit(ev);
}

void World::bossFire(double t) {
    BossState& s = boss_;
    const BossData& b = species_->boss;
    const BossData::Phase& ph = b.phases[s.phase < 4 ? s.phase : 3];
    const float adv = tickAgeOf(t);
    const float wheel = wheelAt(s, b, t);
    const u8 mask = bossFiringMask(t);
    // Play test 1's lever on the bullet density (1 = kaleidos.json's numbers); the loop ramps it (GAME_DESIGN 8).
    const float density = std::fmin(kBossBulletScale * loopRamp(kLoopThreatRamp, director_.track), kBossBulletDensityMax);
    if (mask) {
        const float spread = ph.spreadDeg * kDeg;
        for (u32 k = 0; k < 6; ++k) {
            if (!(mask & (1u << (k % 3)))) continue;
            const float a = wheel + static_cast<float>(k) * kArmStep;
            const Vec2 tip = s.pos + dirOf(a) * b.tip.armRadius;
            const u32 fan = scaledCount(ph.fan ? ph.fan : 1u, density, 1u);
            for (u32 j = 0; j < fan; ++j) {
                const float da = (static_cast<float>(j) - 0.5f * static_cast<float>(fan - 1)) * spread;
                spawnBullet(tip, dirOf(a + da) * b.bulletSpeed, adv, b.bulletLife);
            }
        }
    }
    // The core: Spiral adds a ring of 24 on beat 1; the naked core fires 16 every beat (every eighth from
    // its ninth bar), alternate rings turned half a step.
    if (ph.coreBurst) {
        const double inBar = t - barStartOf(t);
        bool fire = false;
        if (s.phase == 2) {
            fire = inBar < 0.5;
        } else {
            const u32 bars = bossFightBar(t) >= s.phaseBar0 ? bossFightBar(t) - s.phaseBar0 : 0;
            const double every = bars >= 8 ? 2.0 * kTicksPerStep : kTicksPerBeat;
            const double in = std::fmod(t, every);
            fire = t >= s.phaseTick && (in < 0.5 || every - in < 0.5);
        }
        if (fire) {
            const u32 n = scaledCount(ph.coreBurst, density, 4u);
            const u32 index = static_cast<u32>(std::floor(t / (2.0 * kTicksPerStep) + 0.5));
            const float turn = (index & 1u) ? 0.5f * kTau / static_cast<float>(n) : 0.0f;
            for (u32 j = 0; j < n; ++j) {
                const float a = wheel + turn + kTau * static_cast<float>(j) / static_cast<float>(n);
                spawnBullet(s.pos + dirOf(a) * b.coreRingRadius[0], dirOf(a) * b.bulletSpeed, adv, b.bulletLife);
            }
            Event ev;
            ev.type = EventType::BossBurst;
            ev.b = static_cast<u8>(n);
            ev.x = s.pos.x;
            ev.y = s.pos.y;
            emit(ev);
        }
    }
}

void World::bossContact() {
    const BossState& s = boss_;
    const BossData& b = species_->boss;
    if (!s.active || s.dying || m1_ < s.fightTick || rules_.godMode) return;
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        if (!pl.alive || pl.invulnerable() || !isLocal(p) || sheltered(p)) continue;  // M4: what you see is what hits you
                                                                                      // (play test 2: a level-up orb shelters)
        const Vec2 rel = pl.pos - s.pos;
        bool hit = lengthSq(rel) <= (b.contactCore + kPlayerHitRadius) * (b.contactCore + kPlayerHitRadius);
        for (u32 k = 0; k < 6 && !hit; ++k) {
            const u32 pr = k % 3;
            if (!(s.alive & (1u << pr))) continue;
            const Vec2 d = dirOf(s.angle + static_cast<float>(k) * kArmStep);
            const float u = rel.x * d.x + rel.y * d.y;
            if (u < kBossArmInner || u > kBossArmOuter) continue;
            const float v = std::fabs(rel.x * d.y - rel.y * d.x);
            if (v <= armHalfWidth(b, u) * kContactShrink + kPlayerHitRadius) hit = true;
            // A firing beam along this arm, out to the arena wall.
            if (!hit && bossBeamState(pr, m1_) == 2 && u > b.tip.armRadius &&
                v <= 0.5f * b.phases[1].beamWidth + kPlayerHitRadius)
                hit = true;
        }
        if (!hit) {
            // A beam reaches far past the wheel's radius.
            for (u32 k = 0; k < 6 && !hit; ++k) {
                const u32 pr = k % 3;
                if (bossBeamState(pr, m1_) != 2) continue;
                const Vec2 d = dirOf(s.angle + static_cast<float>(k) * kArmStep);
                const float u = rel.x * d.x + rel.y * d.y;
                const float v = std::fabs(rel.x * d.y - rel.y * d.x);
                if (u > b.tip.armRadius && v <= 0.5f * b.phases[1].beamWidth + kPlayerHitRadius) hit = true;
            }
        }
        if (hit && !haloBlocks(p, s.pos)) hitPlayer(p, s.pos);
    }
}

void World::updateBoss(float dt) {
    BossState& s = boss_;
    if (!s.active) return;
    const BossData& b = species_->boss;
    s.prevPos = s.pos;
    s.prevAngle = s.angle;
    if (s.dying) {
        if (m1_ >= s.deathTick + kTicksPerBar) s.active = false;
        return;
    }
    s.angle = wheelAt(s, b, m1_);
    if (scriptKillPairs_ || scriptKillCore_) {
        for (u8 k = 0; k < 3; ++k)
            if (scriptKillPairs_ & (1u << k)) killBossPair(k, 0);
        if (scriptKillCore_) killBoss(0);
        scriptKillPairs_ = 0;
        scriptKillCore_ = false;
        if (s.dying) return;
    }
    // Drift at 30 wu/s to stay 450-650 wu from the nearest player.
    i32 near = nearestPlayer(s.pos.x, s.pos.y);
    if (near >= 0) {
        const Vec2 to = players_[near].pos - s.pos;
        const float d = length(to);
        if (d > 1e-3f) {
            const Vec2 u = to * (1.0f / d);
            if (d < b.keepNear) s.pos = s.pos - u * (b.driftSpeed * dt);
            if (d > b.keepFar) s.pos = s.pos + u * (b.driftSpeed * dt);
        }
        const float m = b.radius + 60.0f;
        s.pos.x = clampf(s.pos.x, -kArenaHalfW + m, kArenaHalfW - m);
        s.pos.y = clampf(s.pos.y, -kArenaHalfH + m, kArenaHalfH - m);
    }
    if (m1_ < s.fightTick) return;
    forCrossings(m0_, m1_, kTicksPerStep, [&](double t, i64) {
        if (t >= s.fightTick) bossFire(t);
    });
    // Beams: announced at the start of their telegraph bar and when they fire.
    forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
        for (u32 p = 0; p < 3; ++p) {
            const u8 st = bossBeamState(p, t + 1.0);
            if (!st) continue;
            Event ev;
            ev.type = EventType::BossBeam;
            ev.a = static_cast<u8>(p);
            ev.b = st == 2 ? 1 : 0;
            ev.tick = st == 2 ? t : t + kTicksPerBar;
            ev.x = s.pos.x;
            ev.y = s.pos.y;
            emit(ev);
        }
    });
}

}  // namespace rf::sim
