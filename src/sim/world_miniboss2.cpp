// M5 part 4: mini-bosses batch 2 (design/bosses/minibosses.md "Batch 2", minibosses.json): four ways of being surrounded,
// each making its safe place out of the beat. The Moth Queen's court (a ring of Moths spiralling in, bursting on the
// fourth bar while she is open), the Brood Mother's nest (eggs that hatch on beat 3; no broodling, no dome), the Blink
// Star's star (drawn a beat an edge, then fired; dazed where it began) and the Sentry Array's clock (eight turrets in a
// fenced ring; the one under the hand is the only one open).
//
// Co-op, as for batch 1: the host spawns what they call and decides their deaths and their health steps; the court's and
// the star's centres are the host's (bossDecide); everything else follows song time.

#include <cmath>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;
using namespace prose;

namespace {
constexpr float kDegR = kTau / 360.0f;
constexpr u32 kStarOrder[5] = {0, 2, 4, 1, 3};  // a five-point star drawn point to point: every second point
i64 imod(i64 a, i64 m) { return ((a % m) + m) % m; }

float wrap2Pi(float a) {
    a = std::fmod(a, kTau);
    return a < 0.0f ? a + kTau : a;
}
}  // namespace

// ---------------------------------------------------------------------------------------------- common

void World::enterMini2(Vec2 penAt) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    switch (s.kind) {
        case BossKind::SentryArray: {
            // Eight turrets warp in evenly on a ring round the players' centre (the spawner's pen point) and stand.
            const float l = kSaRingWu + 200.0f;
            const Vec2 c = penAt;
            m.ringAt = Vec2{clampf(c.x, -kArenaHalfW + l, kArenaHalfW - l), clampf(c.y, -kArenaHalfH + l, kArenaHalfH - l)};
            m.ringAng = -0.5f * kPi;  // the hand at twelve
            s.alive = (1u << kSaTurrets) - 1u;
            s.partMax = s.coreMax / static_cast<float>(kSaTurrets);
            for (u32 k = 0; k < kSaTurrets; ++k) {
                m.turretAng[k] = m.turretFrom[k] = m.turretTo[k] = -0.5f * kPi + kTau * static_cast<float>(k) / static_cast<float>(kSaTurrets);
                s.partHp[k] = s.partMax;
                s.body[1 + k] = s.bodyPrev[1 + k] = m.ringAt + dirOf(m.turretAng[k]) * kSaRingWu;
                s.bodyAng[1 + k] = s.bodyPrevAng[1 + k] = m.turretAng[k] + kPi;
            }
            s.body[0] = s.bodyPrev[0] = m.ringAt;
            break;
        }
        case BossKind::BlinkStar: m.ringTick = -1.0; break;
        case BossKind::MothQueen: m.ringTick = -1.0; break;
        default: break;
    }
}

float World::mini2Scale(u8 part, double t) const {
    const BossState& s = boss_;
    const MiniState& m = s.mini;
    switch (s.kind) {
        case BossKind::MothQueen: return mothOpen(t) ? 2.0f : 1.0f;
        case BossKind::BroodMother: return m.shut ? kBmShut : kBmExposed;
        case BossKind::BlinkStar: return blinkDazed(t) ? 2.0f : 1.0f;
        case BossKind::SentryArray: return part == 2 ? 2.0f : kMiniArmour;  // the turret under the hand (no dome); a dome a quarter (b1d4a2f)
        default: return 1.0f;
    }
}

u32 World::mini2Parts(BossPart* out, u32 cap) const {
    // The Sentry Array: each turret, the one under the hand open (part 2), the others domed (part 1: shots stop on them).
    const BossState& s = boss_;
    u32 n = 0;
    const i32 hand = sentryHand();
    const MiniData::Entry* e = species_->minis.find(s.kind);
    const float r = e ? e->radius : 44.0f;
    for (u32 k = 0; k < kSaTurrets; ++k) {
        if (!(s.alive & (1u << k)) || n >= cap) continue;
        out[n++] = BossPart{s.body[1 + k], static_cast<i32>(k) == hand ? r : r * 1.3f, static_cast<u8>(static_cast<i32>(k) == hand ? 2 : 1),
                            static_cast<u8>(k)};
    }
    return n;
}

void World::mini2PartDied(u8 index) {
    // A turret: a tom and a gap in the scale; the others step round on the next bar line; the last is the Array's death.
    BossState& s = boss_;
    if (s.kind != BossKind::SentryArray) return;
    emitCue(BossCue::SaTurret, m1_, index, 0.0f, s.body[1 + index]);
    s.coreHp = 0.0f;
    for (u32 k = 0; k < kSaTurrets; ++k)
        if (s.alive & (1u << k)) s.coreHp += s.partHp[k];
}

void World::mini2Release(bool killed) {
    // What they called: the court flies straight out and falls as Shards; the Brood's young stay as ordinary broodlings;
    // the Array's bullets in flight fall to the players as Shards (killed).
    BossState& s = boss_;
    Enemies& e = enemies_;
    for (u32 i = 0; i < e.count; ++i) {
        if ((e.flags[i] & kFlagDead) || !(e.flags[i] & kFlagCalled)) continue;
        e.flags[i] &= static_cast<u8>(~kFlagCalled);
        if (e.state[i] == kCourt) {
            e.state[i] = kMoving;
            if (killed && role_ != NetRole::Client) killEnemy(i, 0xFF);
        } else if (e.state[i] == kEgg) {
            e.state[i] = kMoving;
        }
    }
    if (killed && s.kind == BossKind::SentryArray) {
        for (u32 i = 0; i < bullets_.count; ++i) {
            if (bullets_.dead[i]) continue;
            bullets_.dead[i] = 1;
            for (u32 p = 0; p < playerCount_; ++p)
                if (players_[p].present && isLocal(p)) spawnShard(Vec2{bullets_.x[i], bullets_.y[i]}, Vec2{bullets_.vx[i], bullets_.vy[i]} * 0.3f, static_cast<u8>(p));
        }
    }
    if (killed && s.kind == BossKind::BroodMother) {
        // Her last clutch: sixteen gold eggs across the frame, each ten Shards (here: the Shards, in clumps of ten).
        for (u32 p = 0; p < playerCount_; ++p) {
            if (!players_[p].present || !isLocal(p)) continue;
            for (u32 k = 0; k < kBmLastEggs; ++k) {
                const Vec2 at = s.body[0] + dirOf(kTau * static_cast<float>(k) / static_cast<float>(kBmLastEggs)) * (200.0f + 40.0f * static_cast<float>(k % 4));
                for (u32 j = 0; j < kBmEggShards; ++j) spawnShard(at, dirOf(rng_.range(0.0f, kTau)) * rng_.range(30.0f, 120.0f), static_cast<u8>(p));
            }
        }
    }
    if (killed && s.kind == BossKind::BlinkStar) {
        // Its last star's edges run to the players as Shards.
        for (u32 p = 0; p < playerCount_; ++p) {
            if (!players_[p].present || !isLocal(p)) continue;
            for (u32 k = 0; k < 5; ++k) {
                const Vec2 a = blinkPoint(kStarOrder[k]), b = blinkPoint(kStarOrder[(k + 1) % 5]);
                for (u32 j = 0; j < 12; ++j) spawnShard(a + (b - a) * (static_cast<float>(j) / 12.0f), Vec2{}, static_cast<u8>(p));
            }
        }
    }
}

bool World::minion2Motion(u32 i, float dt) {
    Enemies& e = enemies_;
    const BossState& s = boss_;
    const MiniState& m = s.mini;
    (void)dt;
    switch (e.state[i]) {
        case kCourt: {
            // Its place round the court: a quarter turn a bar (the outer ring of two the other way), stepping inward on every
            // beat from 420 to 140 wu over three bars (the inner ring of two over two).
            if (!s.active || s.dying || s.kind != BossKind::MothQueen) {
                e.state[i] = kMoving;
                e.flags[i] &= static_cast<u8>(~kFlagCalled);
                return false;
            }
            const bool inner = e.b[i] > 0.5f;
            const double since = m1_ - m.ringTick;
            const double steps = inner ? 8.0 : 12.0;
            const double beat = std::fmin(std::floor(since / kTicksPerBeat) + 1.0, steps);
            const float r = kMqCourtFrom - (kMqCourtFrom - kMqCourtTo) * static_cast<float>(beat / steps);
            const float turn = static_cast<float>(since / kTicksPerBar) * 0.25f * kTau * (m.half && !inner ? -1.0f : 1.0f);
            const Vec2 want = m.ringAt + dirOf(e.a[i] + turn) * r;
            const Vec2 to = want - Vec2{e.x[i], e.y[i]};
            const float l = length(to);
            const float sp = kMqCourtSpeed / loopSpeed_;
            const Vec2 v = l <= sp * kDt ? to * (1.0f / (kDt * loopSpeed_)) : to * (sp / l);
            e.vx[i] = v.x;
            e.vy[i] = v.y;
            if (l > 1e-3f) e.heading[i] = angleOf(to);
            return true;
        }
        case kEgg: {
            // An egg rides its place round her until it hatches (then a broodling: its family's chase).
            if (!s.active || s.dying || s.kind != BossKind::BroodMother || m1_ >= e.until[i]) {
                e.state[i] = kMoving;
                if (s.active && !s.dying && s.kind == BossKind::BroodMother && m1_ >= e.until[i]) {
                    // (M7, UBSan: the place's angle can be negative; its eighth of the turn from the fraction of a turn)
                    const float turn = e.a[i] / kTau - std::floor(e.a[i] / kTau);
                    emitCue(BossCue::BmHatch, e.until[i], static_cast<u8>(static_cast<u32>(turn * 8.0f) % 8u), 0.0f, Vec2{e.x[i], e.y[i]});
                }
                if (!s.active || s.dying || s.kind != BossKind::BroodMother) e.flags[i] &= static_cast<u8>(~kFlagCalled);
                return false;
            }
            const Vec2 want = s.body[0] + dirOf(e.a[i]) * kBmClutchWu;
            const Vec2 to = want - Vec2{e.x[i], e.y[i]};
            const Vec2 v = to * (1.0f / (kDt * loopSpeed_));
            e.vx[i] = v.x;
            e.vy[i] = v.y;
            return true;
        }
        default: return false;
    }
}

void World::minion2Land(u32 i) {
    // A court Moth takes its place (its angle round the court); an egg its place round the Mother and its hatch.
    Enemies& e = enemies_;
    const BossState& s = boss_;
    const Species& sp = species_->get(e.species[i]);
    if (s.kind == BossKind::MothQueen && sp.family == Family::Moth) {
        e.state[i] = kCourt;
        const Vec2 rel = Vec2{e.x[i], e.y[i]} - s.mini.ringAt;
        e.a[i] = angleOf(rel) - static_cast<float>((m1_ - s.mini.ringTick) / kTicksPerBar) * 0.25f * kTau;
    } else if (s.kind == BossKind::BroodMother && e.species[i] == SpeciesTable::kBroodling) {
        e.state[i] = kEgg;
        e.a[i] = angleOf(Vec2{e.x[i], e.y[i]} - s.body[0]);
        e.until[i] = std::floor((m1_ + 1.0) / kTicksPerBeat) * kTicksPerBeat + kBmHatchBeats * kTicksPerBeat;
    }
}

// ---------------------------------------------------------------------------------------------- Moth Queen

bool World::mothOpen(double t) const {
    // Open for the whole fourth bar of each court (from half health its second half only).
    const BossState& s = boss_;
    if (s.kind != BossKind::MothQueen || s.mini.ringTick < 0.0 || t < s.fightTick) return false;
    const double since = t - s.mini.ringTick;
    const double from = (kMqCycleBars - 1) * kTicksPerBar + (s.mini.half ? 0.5 * kTicksPerBar : 0.0);
    return since >= from && since < kMqCycleBars * kTicksPerBar;
}

void World::mothCourt(double t) {
    // Bar one, beat 1: twelve Moths of the Track's tier warp in as a ring of 420 wu round the nearest player (from half
    // health two courts of eight, the inner and the outer).
    BossState& s = boss_;
    const i32 near = nearestPlayer(s.body[0].x, s.body[0].y);
    if (near < 0) return;
    const Vec2 c = players_[near].pos;
    bossDecide(kGeomMothCourt, c, Vec2{}, t);
    const u16 sp = miniTierSpecies(Family::Moth);
    const u32 rings = s.mini.half ? 2u : 1u, each = s.mini.half ? kMqHalfCourt : kMqCourt;
    for (u32 r = 0; r < rings; ++r)
        for (u32 k = 0; k < each; ++k) {
            const float a = kTau * (static_cast<float>(k) + 0.5f * static_cast<float>(r)) / static_cast<float>(each);
            const i32 i = spawnEnemy(sp, c + dirOf(a) * kMqCourtFrom, a + kPi, t + kWarpBeats * kTicksPerBeat);
            if (i < 0) break;
            enemies_.flags[i] |= kFlagCalled;
            enemies_.b[i] = r == 1 ? 1.0f : 0.0f;  // the inner ring (closing a bar sooner, turning the other way)
            ++s.mini.called;
        }
}

void World::updateMoth(float dt) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    Vec2& q = s.body[0];
    // She holds 520 wu from the nearest player, drifting round them one turn every eight bars.
    const i32 near = nearestPlayer(q.x, q.y);
    if (near >= 0) {
        const Vec2 P = players_[near].pos;
        const float a = angleOf(q - P) + kTau * dt * ticksPerSecond() / static_cast<float>(kMqOrbitBars * kTicksPerBar);
        const Vec2 want = P + dirOf(a) * kMqHoldWu;
        const Vec2 to = want - q;
        const float l = length(to);
        if (l > 1e-3f) q = q + to * (std::fmin(l, kMqMoveSpeed * dt) / l);
        m.face = normalizeOr(P - q, m.face);
    }
    s.bodyAng[0] = angleOf(m.face);
    if (m1_ < s.fightTick) return;
    forCrossings(m0_, m1_, kTicksPerBeat / 2.0, [&](double t, i64 eighth) {
        if (t < s.fightTick - 0.5) return;
        const u32 bar = bossFightBar(t + 1.0) % kMqCycleBars;
        const i64 inBar = imod(eighth, 8);
        if (bar == 0 && inBar == 0) {
            m.ringTick = t;
            if (role_ != NetRole::Client) mothCourt(t);
            emitCue(BossCue::MqRiser, t + (kMqCycleBars - 1) * kTicksPerBar, 0, 0.0f, q);  // (it ends on the close)
        }
        if (m.ringTick < 0.0) return;
        const double since = t - m.ringTick;
        // The close: every court Moth still alive bursts into a ring of six and is gone (the inner ring a bar sooner).
        for (u32 r = 0; r < (m.half ? 2u : 1u); ++r) {
            const double closeAt = (r == 1 ? 2.0 : 3.0) * kTicksPerBar;
            if (std::fabs(since - closeAt) > 0.5) continue;
            Enemies& e = enemies_;
            for (u32 i = 0; i < e.count; ++i) {
                if ((e.flags[i] & kFlagDead) || e.state[i] != kCourt || (e.b[i] > 0.5f) != (r == 1)) continue;
                for (u32 j = 0; j < kMqBurst; ++j)
                    spawnBullet(Vec2{e.x[i], e.y[i]}, dirOf(kTau * static_cast<float>(j) / kMqBurst + e.a[i]) * kMqBurstSpeed, tickAgeOf(t), 0.0f);
                if (role_ == NetRole::Client) continue;
                e.flags[i] |= kFlagDead;
                Event ev;
                ev.type = EventType::Kill;
                ev.species = e.species[i];
                ev.b = 0xFF;
                ev.c = 1;
                ev.x = e.x[i];
                ev.y = e.y[i];
                emit(ev);
                if (role_ == NetRole::Host) logDeath(i, 0xFF, kDeathDissolve);
            }
            emitCue(BossCue::MqBurst, t, static_cast<u8>(r), 0.0f, m.ringAt);
        }
        if (since < (kMqCycleBars - 1) * kTicksPerBar) {
            // Bars one to three: a rosette on every eighth (from half health in pairs), each turned 12 degrees from the last.
            const u32 sets = m.half ? 2u : 1u;
            for (u32 k = 0; k < sets; ++k)
                for (u32 j = 0; j < kMqPetals; ++j) {
                    const float a = m.rosette + kTau * (static_cast<float>(j) + 0.5f * static_cast<float>(k)) / kMqPetals;
                    spawnBullet(q + dirOf(a) * 40.0f, dirOf(a) * kMqPetalSpeed, tickAgeOf(t), kMqPetalReach / kMqPetalSpeed);
                }
            m.rosette += kMqRosetteTurnDeg * kDegR;
            emitCue(BossCue::MqRosette, t, static_cast<u8>(imod(eighth, 16)), 0.0f, q);
            if ((inBar & 1) == 0) emitCue(BossCue::MqStep, t, 0, static_cast<float>(since / kTicksPerBar) * 0.25f * kTau, m.ringAt);
        } else if (std::fabs(since - (kMqCycleBars - 1) * kTicksPerBar - (m.half ? 0.5 * kTicksPerBar : 0.0)) < 0.5) {
            emitCue(BossCue::MqOpen, t, 1, m.half ? 2.0f : 4.0f, q);
        }
    });
}

// ---------------------------------------------------------------------------------------------- Brood Mother

void World::broodClutch(double t, u32 n) {
    // A clutch: eggs in a ring 170 wu round her (a clutch that would pass the cap lays fewer).
    BossState& s = boss_;
    u32 alive = 0;
    for (u32 i = 0; i < enemies_.count; ++i)
        if (!(enemies_.flags[i] & kFlagDead) && (enemies_.flags[i] & kFlagCalled)) ++alive;
    const float a0 = static_cast<float>(hash32(static_cast<u32>(t), 0xB0Du) % 360u) * kDegR;
    for (u32 k = 0; k < n && alive < kBmCap; ++k, ++alive) {
        const float a = a0 + kTau * static_cast<float>(k) / static_cast<float>(n);
        // (it lands on the next tick: an egg can be shot at once)
        const i32 i = spawnEnemy(SpeciesTable::kBroodling, s.body[0] + dirOf(a) * kBmClutchWu, a, std::fmax(t, m1_) + 0.5);
        if (i < 0) break;
        enemies_.flags[i] |= kFlagCalled;
        ++s.mini.called;
    }
    emitCue(BossCue::BmLay, t, static_cast<u8>(n), 0.0f, s.body[0]);
}

void World::updateBrood(float dt) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    Vec2& q = s.body[0];
    // She drifts toward the players' centre at 40 wu/s.
    const Vec2 to = playersCentre() - q;
    const float l = length(to);
    if (l > 1.0f) q = q + to * (std::fmin(l, kBmDrift * dt) / l);
    if (l > 1.0f) m.face = to * (1.0f / l);
    s.bodyAng[0] = angleOf(m.face);
    if (m1_ < s.fightTick) return;
    // The clutch on beat 1 (from half health two of five, on beats 1 and 3), hatching two beats after it is laid.
    if (role_ != NetRole::Client)
        forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64 beat) {
            if (t < s.fightTick - 0.5) return;
            const i64 b = imod(beat, 4);
            if (b == 0) broodClutch(t, m.half ? kBmHalfClutch : kBmClutch);
            if (b == 2 && m.half) broodClutch(t, kBmHalfClutch);
        });
    // The dome: shut while one broodling lives (a thread from each to her); with none it breaks (x 2).
    bool any = false;
    const Enemies& e = enemies_;
    for (u32 i = 0; i < e.count && !any; ++i)
        any = !(e.flags[i] & kFlagDead) && (e.flags[i] & kFlagCalled) && e.state[i] != kEgg && e.state[i] != kWarping;
    if (m.shut && !any) emitCue(BossCue::BmBreak, m1_, 0, 0.0f, q);
    m.shut = any;
}

// ---------------------------------------------------------------------------------------------- Blink Star

Vec2 World::blinkPoint(u32 k) const {
    const MiniState& m = boss_.mini;
    const float r = kBsStarWu * (m.half ? kBsHalfScale : 1.0f);
    return m.ringAt + dirOf(m.ringAng + kTau * static_cast<float>(k % 5u) / 5.0f) * r;
}

u32 World::blinkEdges(double t) const {
    // The edges drawn of the star under way: one a beat from beat 2 (the fifth on bar two's beat 2, as it fires).
    const MiniState& m = boss_.mini;
    if (m.ringTick < 0.0 || t < m.ringTick) return 0;
    const double q = std::floor((t - m.ringTick) / kTicksPerBeat);
    return q >= 5.0 ? (q < 6.0 ? 5u : 0u) : static_cast<u32>(q);
}

bool World::blinkFiring(double t) const {
    const MiniState& m = boss_.mini;
    if (boss_.kind != BossKind::BlinkStar || m.ringTick < 0.0) return false;
    const double q = (t - m.ringTick) / kTicksPerBeat;
    return q >= 5.0 && q < 6.0;
}

bool World::blinkDazed(double t) const {
    const MiniState& m = boss_.mini;
    if (boss_.kind != BossKind::BlinkStar || m.ringTick < 0.0) return false;
    const double q = (t - m.ringTick) / kTicksPerBeat;
    return q >= 6.0 && q < 8.0;
}

bool World::blinkBeamHit(Vec2 at, Vec2& from) const {
    // The star fires: all five edges are lethal for a beat, 16 wu wide.
    if (!blinkFiring(m1_)) return false;
    for (u32 k = 0; k < 5; ++k) {
        const Vec2 a = blinkPoint(kStarOrder[k]), b = blinkPoint(kStarOrder[(k + 1) % 5]);
        const Vec2 d = b - a;
        const float u = clampf(dot(at - a, d) / std::fmax(lengthSq(d), 1e-6f), 0.0f, 1.0f);
        const Vec2 c = a + d * u;
        if (lengthSq(at - c) <= (0.5f * kBsBeamWu + kPlayerHitRadius) * (0.5f * kBsBeamWu + kPlayerHitRadius)) {
            from = c;
            return true;
        }
    }
    return false;
}

void World::updateBlink(float /*dt*/) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    if (m1_ < s.fightTick) return;
    forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64) {
        if (t < s.fightTick - 0.5) return;
        const u32 bar = bossFightBar(t + 1.0);
        const double inBar = t - std::floor((t + 1.0) / kTicksPerBar) * kTicksPerBar;
        if ((bar & 1u) == 0 && inBar < 1.0) {
            // A new star, centred where the nearest player stands now, its first point on the side the Star is on.
            m.ringTick = t;
            if (role_ != NetRole::Client) {
                const i32 near = nearestPlayer(s.body[0].x, s.body[0].y);
                const Vec2 c = near >= 0 ? players_[near].pos : s.body[0];
                bossDecide(kGeomBlinkStar, c, Vec2{angleOf(s.body[0] - c), 0.0f}, t);
            }
        }
        if (m.ringTick < 0.0) return;
        const u32 q = static_cast<u32>(std::floor((t - m.ringTick) / kTicksPerBeat + 0.5));
        if (q <= 5) {
            // Beat 1 it appears at the first point; each beat after it blinks to the next in star order; the fifth hop
            // brings it home as the star fires.
            s.body[0] = s.bodyPrev[0] = blinkPoint(kStarOrder[q % 5]);
            if (q < 5) emitCue(BossCue::BsHop, t, static_cast<u8>(q), 0.0f, s.body[0]);
        }
        if (q == 5) {
            emitCue(BossCue::BsFire, t, 0, 0.0f, m.ringAt);
            if (m.half) {
                // Every point it left keeps a decoy, and each throws a fan of three at the nearest player.
                for (u32 k = 0; k < 5; ++k) {
                    const Vec2 p = blinkPoint(k);
                    const i32 near = nearestPlayer(p.x, p.y);
                    if (near < 0) break;
                    const float a = angleOf(players_[near].pos - p);
                    for (u32 j = 0; j < kBsDecoyFan; ++j)
                        spawnBullet(p, dirOf(a + (static_cast<float>(j) - 1.0f) * kBsDecoyFanDeg * kDegR) * kBsDecoyBulletSpeed, tickAgeOf(t), 0.0f);
                }
            }
        }
        if (q == 6) emitCue(BossCue::BsDazed, t, 1, 2.0f, s.body[0]);
    });
    const i32 near = nearestPlayer(s.body[0].x, s.body[0].y);
    if (near >= 0) m.face = normalizeOr(players_[near].pos - s.body[0], m.face);
    s.bodyAng[0] = angleOf(m.face);
}

// ---------------------------------------------------------------------------------------------- Sentry Array

i32 World::sentryHand() const {
    // The turret the hand points at (the nearest alive by angle).
    const BossState& s = boss_;
    if (s.kind != BossKind::SentryArray || !s.alive) return -1;
    i32 best = -1;
    float bd = 10.0f;
    for (u32 k = 0; k < kSaTurrets; ++k) {
        if (!(s.alive & (1u << k))) continue;
        const float d = std::fabs(wrapAngle(s.mini.turretAng[k] - s.mini.ringAng));
        if (d < bd) {
            bd = d;
            best = static_cast<i32>(k);
        }
    }
    return best;
}

bool World::fenceUp() const {
    const BossState& s = boss_;
    return s.active && !s.dying && s.kind == BossKind::SentryArray && m1_ >= s.enterTick + kWarpBeats * kTicksPerBeat;
}

float World::fenceRadius() const {
    u32 n = 0;
    for (u32 k = 0; k < kSaTurrets; ++k) n += (boss_.alive >> k) & 1u;
    return kSaRingWu * std::cos(kPi / static_cast<float>(n < 3 ? 3u : n));
}

u8 World::fenceKeep(Vec2 from, Vec2& to, float r) const {
    // The fence stops ships and bodies: inside stays inside, outside stays outside (the ring's polygon, as its circles).
    const Vec2 c = boss_.mini.ringAt;
    const float in = fenceRadius() - r, out = kSaRingWu + r;
    const float df = length(from - c), dt = length(to - c);
    if (df < fenceRadius()) {
        if (dt <= in) return 0;
        to = c + normalizeOr(to - c, Vec2{1.0f, 0.0f}) * in;
        return 3;
    }
    if (df >= kSaRingWu && dt < out) {
        to = c + normalizeOr(to - c, Vec2{1.0f, 0.0f}) * out;
        return 3;
    }
    return 0;
}

void World::fenceShots() {
    // Shots stop on the fence, except at the turret under the hand (it has no dome).
    if (!fenceUp()) return;
    const BossState& s = boss_;
    const Vec2 c = s.mini.ringAt;
    const float R = fenceRadius();
    const i32 hand = sentryHand();
    const float open = hand >= 0 ? s.mini.turretAng[hand] : 0.0f;
    const MiniData::Entry* me = species_->minis.find(s.kind);
    const float openHalf = ((me ? me->radius : 44.0f) + 10.0f) / kSaRingWu;
    Shots& sh = shots_;
    for (u32 i = 0; i < sh.count; ++i) {
        if (sh.dead[i]) continue;
        const float d0 = length(Vec2{sh.px[i], sh.py[i]} - c), d1 = length(Vec2{sh.x[i], sh.y[i]} - c);
        if ((d0 < R) == (d1 < R)) continue;
        if (hand >= 0 && std::fabs(wrapAngle(angleOf(Vec2{sh.x[i], sh.y[i]} - c) - open)) < openHalf) continue;
        sh.dead[i] = 1;
    }
}

void World::sentryVolley(double t, u32 fan) {
    // Every turret but the one under the hand fires a fan inward across its own share of the ring; the bullets end at the
    // centre. Together: a closing ring with one notch.
    BossState& s = boss_;
    const i32 hand = sentryHand();
    u32 n = 0;
    for (u32 k = 0; k < kSaTurrets; ++k) n += (s.alive >> k) & 1u;
    if (!n) return;
    const float share = kTau / static_cast<float>(n);
    for (u32 k = 0; k < kSaTurrets; ++k) {
        if (!(s.alive & (1u << k)) || static_cast<i32>(k) == hand) continue;
        const Vec2 p = s.body[1 + k];
        const float a0 = s.mini.turretAng[k];
        for (u32 j = 0; j < fan; ++j) {
            // Aimed at points of its share of the ring's far side, so the fan sweeps its share as it closes.
            const float off = fan > 1 ? (static_cast<float>(j) / static_cast<float>(fan - 1) - 0.5f) * share : 0.0f;
            const Vec2 aim = s.mini.ringAt + dirOf(a0 + kPi + off) * (kSaRingWu * 0.15f);
            const Vec2 d = normalizeOr(aim - p, dirOf(a0 + kPi));
            spawnBullet(p + d * 30.0f, d * kSaBulletSpeed, tickAgeOf(t), (kSaRingWu - 30.0f) / kSaBulletSpeed);
        }
    }
    emitCue(BossCue::SaVolley, t, static_cast<u8>(fan), 0.0f, s.mini.ringAt);
}

void World::sentryRespace() {
    // After a death the others step round to stand evenly again (keeping their order from the hand's turret).
    BossState& s = boss_;
    MiniState& m = s.mini;
    u32 order[kSaTurrets];
    u32 n = 0;
    for (u32 k = 0; k < kSaTurrets; ++k)
        if (s.alive & (1u << k)) order[n++] = k;
    if (!n) return;
    const float base = m.turretAng[order[0]];
    for (u32 j = 0; j < n; ++j) {
        const u32 k = order[j];
        m.turretFrom[k] = m.turretAng[k];
        float to = base + kTau * static_cast<float>(j) / static_cast<float>(n);
        to = m.turretFrom[k] + wrapAngle(to - m.turretFrom[k]);
        m.turretTo[k] = to;
    }
    m.stepTick = m1_;
}

void World::updateSentry(float /*dt*/) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    u32 n = 0;
    for (u32 k = 0; k < kSaTurrets; ++k) n += (s.alive >> k) & 1u;
    // Half (the host's step): with four left.
    if (!m.half && n <= kSaHalfAt && role_ != NetRole::Client) bossStep(0, 0xFF);
    if (m.half && m.halfBar0 == 0) m.halfBar0 = static_cast<i64>(std::floor(m1_ / kTicksPerBar)) + 1;
    // Stepping round after a death.
    if (m.stepTick >= 0.0) {
        const float u = clampf(static_cast<float>((m1_ - m.stepTick) / (kSaStepBeats * kTicksPerBeat)), 0.0f, 1.0f);
        for (u32 k = 0; k < kSaTurrets; ++k)
            if (s.alive & (1u << k)) m.turretAng[k] = m.turretFrom[k] + (m.turretTo[k] - m.turretFrom[k]) * u;
        if (u >= 1.0f) m.stepTick = -1.0;
    }
    for (u32 k = 0; k < kSaTurrets; ++k) {
        s.body[1 + k] = m.ringAt + dirOf(m.turretAng[k]) * kSaRingWu;
        s.bodyAng[1 + k] = m.turretAng[k] + kPi;
    }
    s.body[0] = m.ringAt;
    if (m1_ < s.fightTick) return;
    forCrossings(m0_, m1_, kTicksPerBeat / 2.0, [&](double t, i64 eighth) {
        if (t < s.fightTick - 0.5) return;
        const bool onBeat = (eighth & 1) == 0;
        const i64 bar = static_cast<i64>(std::floor((t + 1.0) / kTicksPerBar));
        const bool barLine = imod(eighth, 8) == 0;
        // A death: the others step round on the next bar line.
        if (barLine) {
            u32 placed = 0;
            for (u32 k = 0; k < kSaTurrets; ++k) placed += (s.alive >> k) & 1u;
            bool even = true;
            if (placed > 1) {
                u32 first = 0;
                while (!(s.alive & (1u << first))) ++first;
                u32 j = 0;
                for (u32 k = 0; k < kSaTurrets; ++k) {
                    if (!(s.alive & (1u << k))) continue;
                    const float want = m.turretAng[first] + kTau * static_cast<float>(j++) / static_cast<float>(placed);
                    if (std::fabs(wrapAngle(m.turretAng[k] - want)) > 0.01f) even = false;
                }
            }
            if (!even) sentryRespace();
        }
        if (onBeat) {
            // The hand moves on 45 degrees a beat (from half health backwards on every second bar, flashing a beat ahead).
            const bool back = m.half && bar >= m.halfBar0 && ((bar - m.halfBar0) & 1) == 1;
            if (back && barLine) emitCue(BossCue::SaBack, t, 0, 0.0f, m.ringAt);
            m.ringAng += (back ? -1.0f : 1.0f) * kSaHandStepDeg * kDegR;
            const u32 steps = static_cast<u32>(std::lround(wrap2Pi(m.ringAng + 0.5f * kPi) / (kSaHandStepDeg * kDegR))) % 8u;
            emitCue(BossCue::SaHand, t, static_cast<u8>(steps), m.ringAng, m.ringAt + dirOf(m.ringAng) * kSaRingWu);
        }
        // The volleys: on every beat a fan of five; from half health on the eighths, three then two.
        if (!m.half && onBeat) sentryVolley(t, kSaFan);
        if (m.half) sentryVolley(t, onBeat ? 3u : 2u);
    });
}

}  // namespace rf::sim
