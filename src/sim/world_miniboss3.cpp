// M5 part 6: mini-bosses batch 3 (design/bosses/minibosses.md "Batch 3", minibosses.json): the space itself is the weapon.
// Mirror Mimic (it stands where the nearest player would be through a mirror point and fires when they fire; it copies their
// Dash a beat late, and that is when it can be caught), Twin Tether (two nodes and a lethal rope sweeping their circle; a Dash
// through the rope goes slack), Weaver Prime (a cage of lethal trail round where the player stood, a lap a bar, a knot on every
// One) and Deep Well (it swallows shots and gives them back on the One; overfed it chokes, and only then can it be hurt).
// From Track 9 the mini-boss carries elite traits (Armored, Swift, Regenerating; minibosses.json common.pick).
//
// Co-op: the Mimic mirrors through the point where the players' centre was as it arrived (the host's spawn message) and each
// device answers the shots of the player it mirrors. The Tether's middle follows the nearest player (the heartbeat's sync
// point); a cut strand is its cutter's device's (the slack and the x 2 there: decision taken alone). The Weaver's cage (its
// centre, where its laps begin) and the Well's bar line (what it was fed, whether it chokes) are the host's (bossDecide).

#include <cmath>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;
using namespace prose;

namespace {
double beatLine(double t) { return std::floor(t / kTicksPerBeat) * kTicksPerBeat; }
float wrap2Pi(float a) {
    a = std::fmod(a, kTau);
    return a < 0.0f ? a + kTau : a;
}
bool segmentsCross(Vec2 a0, Vec2 a1, Vec2 b0, Vec2 b1) {
    const auto side = [](Vec2 o, Vec2 d, Vec2 p) { return d.x * (p.y - o.y) - d.y * (p.x - o.x); };
    const Vec2 da = a1 - a0, db = b1 - b0;
    const float s0 = side(b0, db, a0), s1 = side(b0, db, a1), t0 = side(a0, da, b0), t1 = side(a0, da, b1);
    return ((s0 <= 0.0f && s1 >= 0.0f) || (s0 >= 0.0f && s1 <= 0.0f)) && ((t0 <= 0.0f && t1 >= 0.0f) || (t0 >= 0.0f && t1 <= 0.0f));
}
float segDist(Vec2 p, Vec2 a, Vec2 b, Vec2* at = nullptr) {
    const Vec2 d = b - a;
    const float u = clampf(dot(p - a, d) / std::fmax(lengthSq(d), 1e-6f), 0.0f, 1.0f);
    const Vec2 c = a + d * u;
    if (at) *at = c;
    return length(p - c);
}
}  // namespace

// ---------------------------------------------------------------------------------------------- common

void World::enterMini3(Vec2 penAt) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    switch (s.kind) {
        case BossKind::MirrorMimic: {
            // The mirror point: where the players' centre was as it arrived (a small white ring).
            const float l = 400.0f;
            m.ringAt = Vec2{clampf(penAt.x, -kArenaHalfW + l, kArenaHalfW - l), clampf(penAt.y, -kArenaHalfH + l, kArenaHalfH - l)};
            break;
        }
        case BossKind::TwinTether:
            // Two nodes, each half the health.
            s.alive = 0x3u;
            s.partMax = 0.5f * s.coreMax;
            s.partHp[0] = s.partHp[1] = s.partMax;
            for (u32 k = 0; k < 2; ++k) s.body[1 + k] = s.bodyPrev[1 + k] = s.body[0] + dirOf(kPi * static_cast<float>(k)) * (0.5f * kTtApart);
            break;
        default: break;
    }
}

float World::mini3Scale(u8 part, u8 index, double t) const {
    const BossState& s = boss_;
    switch (s.kind) {
        case BossKind::MirrorMimic: return mimicCopying(t) ? 2.0f : 1.0f;
        case BossKind::TwinTether: return part == 2 && tetherSlack(t) ? 2.0f : 1.0f;
        case BossKind::WeaverPrime: return weaverKnot(index, t) ? 2.0f : 1.0f;
        case BossKind::DeepWell: {
            const MiniData::Entry* e = species_->minis.find(s.kind);
            return wellChoked(t) ? (e ? e->dmgChoked : 2.0f) : (e ? e->dmgSwallowing : kDwArmour);
        }
        default: return 1.0f;
    }
}

u32 World::mini3Parts(BossPart* out, u32 cap) const {
    const BossState& s = boss_;
    const MiniData::Entry* e = species_->minis.find(s.kind);
    const float r = e ? e->radius : 56.0f;
    u32 n = 0;
    switch (s.kind) {
        case BossKind::MirrorMimic:
            if (n < cap) out[n++] = BossPart{s.body[0], r, 3, 0};
            if (s.mini.half && n < cap) out[n++] = BossPart{s.body[1], r, 3, 1};
            break;
        case BossKind::TwinTether:
            for (u32 k = 0; k < 2; ++k)
                if ((s.alive & (1u << k)) && n < cap) out[n++] = BossPart{s.body[1 + k], r, 2, static_cast<u8>(k)};
            break;
        case BossKind::WeaverPrime:
            if (n < cap) out[n++] = BossPart{s.body[0], r, 3, 0};
            if (s.mini.half && n < cap) out[n++] = BossPart{s.body[1], r, 3, 1};
            break;
        default:
            if (n < cap) out[n++] = BossPart{s.body[0], r, 3, 0};
            break;
    }
    return n;
}

void World::mini3PartDied(u8 index) {
    // A Tether node: the survivor swings what is left as a flail; the last is the Tether's death (killBossPart).
    BossState& s = boss_;
    MiniState& m = s.mini;
    if (s.kind != BossKind::TwinTether) return;
    if (s.alive) {
        m.flailTick = m1_;
        const u32 k = (s.alive & 1u) ? 0u : 1u;
        m.flail0 = angleOf(s.body[1 + k] - s.body[1 + (1u - k)]) + kPi;
    }
    (void)index;
    s.coreHp = 0.0f;
    for (u32 k = 0; k < 2; ++k)
        if (s.alive & (1u << k)) s.coreHp += s.partHp[k];
}

void World::mini3Release(bool killed) {
    // The Mimic's last volley and the Weaver's cage fall as Shards; the Well bursts into one Shard for each shot it swallowed
    // (to 300); the Tether's rope snaps (light only, then Shards).
    BossState& s = boss_;
    if (!killed) return;
    const auto shard = [&](Vec2 at, Vec2 v) {
        for (u32 p = 0; p < playerCount_; ++p)
            if (players_[p].present && isLocal(p)) spawnShard(at, v, static_cast<u8>(p));
    };
    switch (s.kind) {
        case BossKind::MirrorMimic:
            emitCue(BossCue::MmGlass, m1_, 0, 0.0f, s.body[0]);
            for (u32 i = 0; i < bullets_.count; ++i) {
                if (bullets_.dead[i]) continue;
                bullets_.dead[i] = 1;
                shard(Vec2{bullets_.x[i], bullets_.y[i]}, Vec2{bullets_.vx[i], bullets_.vy[i]} * 0.3f);
            }
            break;
        case BossKind::TwinTether:
            for (u32 j = 0; j < 24; ++j) shard(s.body[0] + dirOf(kTau * static_cast<float>(j) / 24.0f) * 260.0f, Vec2{});
            break;
        case BossKind::WeaverPrime: {
            const MiniState& m = s.mini;
            if (m.cageTick >= 0.0)
                for (u32 j = 0; j < 32; ++j) shard(m.cage + dirOf(m.cageAng + kTau * static_cast<float>(j) / 32.0f) * kWvRadii[j % 4u], Vec2{});
            break;
        }
        case BossKind::DeepWell: {
            const u32 n = s.mini.swallowed < kDwShardsMax ? s.mini.swallowed : kDwShardsMax;
            for (u32 j = 0; j < n; ++j) shard(s.body[0], dirOf(kTau * static_cast<float>(j) / static_cast<float>(n ? n : 1u)) * rng_.range(160.0f, 420.0f));
            break;
        }
        default: break;
    }
}

bool World::mini3Hits(Vec2 at, Vec2& from) const {
    // What batch 3 adds to its bodies' contact: the mirror point, the Tether's beam, the Weaver's trails and their burst.
    const BossState& s = boss_;
    const MiniState& m = s.mini;
    if (s.dying || m1_ < s.fightTick) return false;
    switch (s.kind) {
        case BossKind::MirrorMimic:
            if (lengthSq(at - m.ringAt) <= (kMmPointWu + kPlayerHitRadius) * (kMmPointWu + kPlayerHitRadius)) {
                from = m.ringAt;
                return true;
            }
            return false;
        case BossKind::TwinTether: {
            Vec2 a, b;
            if (!tetherBeam(a, b)) return false;
            return segDist(at, a, b, &from) <= 0.5f * kTtBeamWu + kPlayerHitRadius;
        }
        case BossKind::WeaverPrime: {
            for (u32 w = 0; w < (m.half ? 2u : 1u); ++w) {
                for (u32 lap = 0; lap < 4; ++lap) {
                    Vec2 c;
                    float r = 0.0f, a0 = 0.0f, dir = 1.0f, u0 = 0.0f, u1 = 0.0f;
                    if (!weaverLap(w, lap, m1_, c, r, a0, dir, u0, u1)) continue;
                    const Vec2 rel = at - c;
                    const float rho = length(rel);
                    if (std::fabs(rho - r) > 0.5f * kWvTrailWu + kPlayerHitRadius) continue;
                    const float u = wrap2Pi((angleOf(rel) - a0) * dir) / kTau;
                    if (u >= u0 && u <= u1) {
                        from = c + rel * (r / std::fmax(rho, 1e-3f));
                        return true;
                    }
                }
            }
            // The last cage going off: its living laps (the third and the fourth) burst 40 wu wide for half a beat.
            if (m.cageTick >= 0.0 && m1_ >= m.cageTick && m1_ < m.cageTick + kWvBurstBeats * kTicksPerBeat && lengthSq(m.prevCage) > 0.0f) {
                const float rho = length(at - m.prevCage);
                for (u32 lap = 2; lap < 4; ++lap)
                    for (u32 w = 0; w < (m.half ? 2u : 1u); ++w) {
                        const float r = kWvRadii[lap] - (w ? 25.0f : 0.0f);
                        if (std::fabs(rho - r) <= 0.5f * kWvBurstWu + kPlayerHitRadius) {
                            from = m.prevCage + (at - m.prevCage) * (r / std::fmax(rho, 1e-3f));
                            return true;
                        }
                    }
            }
            return false;
        }
        default: return false;
    }
}

void World::mini3Feed(float dmg) {
    // Deep Well: a shot it swallowed (not choked): the bar's fill counts what it would have done (the quarter it takes is
    // armour's, GAME_DESIGN 7.6).
    BossState& s = boss_;
    MiniState& m = s.mini;
    if (s.kind != BossKind::DeepWell || wellChoked(m1_)) return;
    const MiniData::Entry* e = species_->minis.find(s.kind);
    m.fill += dmg / (e && e->dmgSwallowing > 0.0f ? e->dmgSwallowing : kDwArmour);
    ++m.swallowed;
    if (m1_ - m.lastThup >= kTicksPerStep) {
        m.lastThup = m1_;
        const float st = wellStomach();
        const u32 lit = st > 0.0f ? static_cast<u32>(clampf(4.0f * m.fill / st, 0.0f, 3.0f)) : 0u;
        emitCue(BossCue::DwThup, m1_, static_cast<u8>(lit), 0.0f, s.body[0]);
    }
}

void World::wellFeedNow(float dmg) {
    const MiniData::Entry* e = species_->minis.find(BossKind::DeepWell);
    mini3Feed(dmg * (e ? e->dmgSwallowing : kDwArmour));
}

void World::miniTraitsTick() {
    // Regenerating (traits.json): after two quiet beats it heals 8% of its health a beat (the host's: its health is the host's).
    BossState& s = boss_;
    MiniState& m = s.mini;
    if (!(m.traits & traitBit(kRegenerating)) || role_ == NetRole::Client || s.dying) return;
    const TraitData& td = species_->traits;
    forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64) {
        if (t - m.lastHurt < td.regenQuiet * kTicksPerBeat || s.coreHp >= s.coreMax) return;
        if (s.partMax > 0.0f && s.alive) {
            for (u32 k = 0; k < kBossParts; ++k)
                if (s.alive & (1u << k)) s.partHp[k] = std::fmin(s.partHp[k] + td.regenHeal * s.partMax, s.partMax);
        }
        s.coreHp = std::fmin(s.coreHp + td.regenHeal * s.coreMax, s.coreMax);
        emitCue(BossCue::ElRegen, t, 0, 0.0f, s.body[0]);
    });
}

// ---------------------------------------------------------------------------------------------- Mirror Mimic

bool World::mimicCopying(double t) const {
    const MiniState& m = boss_.mini;
    return boss_.kind == BossKind::MirrorMimic && m.dashCopy >= 0.0 && t >= m.dashCopy && t < m.dashCopy + kMmOpenBeats * kTicksPerBeat;
}

Vec2 World::mimicImage(u32 k) const {
    // Image 0 through the point, image 1 (half health) across the level line through it; the Dash not yet copied is taken off
    // the player's place, then copied over the Dash's own time.
    const BossState& s = boss_;
    const MiniState& m = s.mini;
    const u32 p = m.mirrorOf < kMaxPlayers ? m.mirrorOf : 0u;
    Vec2 q = players_[p].pos;
    if (m.dashCopy >= 0.0) {
        const float u = clampf(static_cast<float>((m1_ - m.dashCopy) / (kDashS * ticksPerSecond())), 0.0f, 1.0f);
        q = q - m.dashVec * (1.0f - u);
    }
    const Vec2 c = m.ringAt;
    return k == 0 ? c * 2.0f - q : Vec2{q.x, 2.0f * c.y - q.y};
}

void World::updateMimic(float /*dt*/) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    // The player it mirrors: the nearest to the mirror point.
    float best = 1e30f;
    for (u32 p = 0; p < playerCount_; ++p) {
        if (!players_[p].present || !players_[p].alive) continue;
        const float d = lengthSq(players_[p].pos - m.ringAt);
        if (d < best) {
            best = d;
            m.mirrorOf = static_cast<u8>(p);
        }
    }
    // Its Dash, copied a beat late.
    for (u32 k = 0; k < eventCount_; ++k) {
        const Event& ev = events_[k];
        if (ev.type != EventType::Dash || ev.a != m.mirrorOf) continue;
        const Vec2 v{ev.x2 - ev.x, ev.y2 - ev.y};
        m.dashVec = (m.dashCopy > m1_ ? m.dashVec : Vec2{}) + v;
        m.dashCopy = beatLine(m1_) + kMmLateBeats * kTicksPerBeat;
        emitCue(BossCue::MmDash, m.dashCopy, 0, 0.0f, s.body[0]);
    }
    if (m.dashCopy >= 0.0 && m1_ >= m.dashCopy + kMmOpenBeats * kTicksPerBeat) {
        m.dashCopy = -1.0;
        m.dashVec = Vec2{};
    }
    s.body[0] = mimicImage(0);
    s.body[1] = m.half ? mimicImage(1) : s.body[0];
    const Vec2 pl = players_[m.mirrorOf].pos;
    s.bodyAng[0] = angleOf(pl - s.body[0]);
    s.bodyAng[1] = angleOf(pl - s.body[1]);
    if (m1_ < s.fightTick || mimicCopying(m1_)) return;
    // It answers each step the player's lead sounds: one bullet from each image along the aim turned half round (image 1: the
    // aim turned across the level line).
    for (u32 k = 0; k < eventCount_; ++k) {
        const Event& ev = events_[k];
        if (ev.type != EventType::Shot || ev.a != m.mirrorOf) continue;
        const i64 step = static_cast<i64>(std::floor(m1_ / kTicksPerStep));
        if (step == m.fireStep) continue;
        m.fireStep = step;
        if (!trackCount(1u, 1u, static_cast<i64>(m.answers++), 0u)) continue;  // (0449eb3: every second answer, or three in four)
        const Vec2 dir = normalizeOr(Vec2{ev.x2, ev.y2}, Vec2{1.0f, 0.0f});
        for (u32 i = 0; i < (m.half ? 2u : 1u); ++i) {
            if (bullets_.count >= kMmCap) break;
            const Vec2 d = i == 0 ? dir * -1.0f : Vec2{dir.x, -dir.y};
            const MiniData::Entry* e = species_->minis.find(s.kind);
            spawnBullet(s.body[i] + d * (e ? e->radius : 60.0f), d * kMmBulletSpeed, 0.0f, kMmBulletLife);
        }
        emitCue(BossCue::MmFire, m1_ + 0.5 * kTicksPerStep, m.mirrorOf, 0.0f, s.body[0]);
    }
}

// ---------------------------------------------------------------------------------------------- Twin Tether

bool World::tetherSlack(double t) const { return boss_.kind == BossKind::TwinTether && t < boss_.mini.slackUntil; }

bool World::tetherBeam(Vec2& a, Vec2& b) const {
    const BossState& s = boss_;
    const MiniState& m = s.mini;
    if (s.kind != BossKind::TwinTether || !s.alive || s.dying || m1_ < s.fightTick || tetherSlack(m1_)) return false;
    if (s.alive == 0x3u) {
        a = s.body[1];
        b = s.body[2];
        return true;
    }
    // The flail: what is left of the rope, turning once a bar about the survivor.
    const u32 k = (s.alive & 1u) ? 0u : 1u;
    a = s.body[1 + k];
    const float ang = m.flail0 + static_cast<float>(kTau * (m1_ - m.flailTick) / kTicksPerBar);
    b = a + dirOf(ang) * kTtFlail;
    return true;
}

void World::updateTether(float dt) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    // The middle follows the nearest player at 60 wu/s (Swift: x 1.6); the nodes turn about it once in two bars.
    const float speed = kTtFollow * ((m.traits & traitBit(kSwift)) ? species_->traits.swiftSpeed : 1.0f);
    const i32 near = nearestPlayer(s.body[0].x, s.body[0].y);
    if (near >= 0) {
        const Vec2 to = players_[near].pos - s.body[0];
        const float l = length(to);
        if (l > 1.0f) s.body[0] = s.body[0] + to * (std::fmin(speed * dt, l) / l);
    }
    const float l = 0.5f * kTtApart + 200.0f;
    s.body[0] = Vec2{clampf(s.body[0].x, -kArenaHalfW + l, kArenaHalfW - l), clampf(s.body[0].y, -kArenaHalfH + l, kArenaHalfH - l)};
    const float th0 = static_cast<float>(kTau * (m0_ - s.fightTick) / (kTtTurnBars * kTicksPerBar));
    const float th = static_cast<float>(kTau * (m1_ - s.fightTick) / (kTtTurnBars * kTicksPerBar));
    for (u32 k = 0; k < 2; ++k) {
        s.body[1 + k] = s.body[0] + dirOf(th + kPi * static_cast<float>(k)) * (0.5f * kTtApart);
        s.bodyAng[1 + k] = th + kPi * static_cast<float>(k);
        // A node passing twelve o'clock: a tom.
        if ((s.alive & (1u << k)) && m1_ >= s.fightTick) {
            const float a0 = wrap2Pi(th0 + kPi * static_cast<float>(k) + 0.5f * kPi), a1 = wrap2Pi(th + kPi * static_cast<float>(k) + 0.5f * kPi);
            if (a1 < a0) emitCue(BossCue::TtTom, m1_, static_cast<u8>(k), 0.0f, s.body[1 + k]);
        }
    }
    s.coreHp = 0.0f;
    for (u32 k = 0; k < 2; ++k)
        if (s.alive & (1u << k)) s.coreHp += s.partHp[k];
    // Each player's last Dash: Perfect or not.
    for (u32 k = 0; k < eventCount_; ++k) {
        const Event& ev = events_[k];
        if (ev.type != EventType::Dash || ev.a >= kMaxPlayers) continue;
        if (ev.b) m.perfect = static_cast<u8>(m.perfect | (1u << ev.a));
        else m.perfect = static_cast<u8>(m.perfect & ~(1u << ev.a));
    }
    // A Dash through the beam cuts a strand: dark for a beat (two after a Perfect Dash), both nodes x 2 (this device's own
    // players: decision taken alone).
    Vec2 a, b;
    if (m1_ >= s.fightTick && tetherBeam(a, b)) {
        for (u32 p = 0; p < playerCount_; ++p) {
            const PlayerState& pl = players_[p];
            if (!pl.present || !pl.alive || !pl.dashing || !isLocal(p)) continue;
            if (!segmentsCross(pl.prevPos, pl.pos, a, b)) continue;
            const double beats = (m.perfect & (1u << p)) ? kTtPerfectBeats : kTtSlackBeats;
            m.slackUntil = std::fmax(m.slackUntil, m1_ + beats * kTicksPerBeat);
            emitCue(BossCue::TtCut, m1_, static_cast<u8>(p), 0.0f, pl.pos);
            break;
        }
    }
}

// ---------------------------------------------------------------------------------------------- Weaver Prime

double World::weaverStart(u32 w) const {
    // A Weaver's cage begins on its first knot: the first on the cage's bar line, the second (half health) two beats later.
    return boss_.mini.cageTick + (w ? 2.0 * kTicksPerBeat : 0.0);
}

bool World::weaverLap(u32 w, u32 lap, double t, Vec2& centre, float& radius, float& ang0, float& dir, float& u0, float& u1) const {
    // Lap `lap` of the current cage: drawn over beats 2-4 of its bar (the second Weaver: beats 4, 1, 2), each point alive for
    // two bars from when it was drawn.
    const MiniState& m = boss_.mini;
    if (m.cageTick < 0.0 || lap > 3) return false;
    const double from = weaverStart(w) + static_cast<double>(lap) * kTicksPerBar + kTicksPerBeat;
    if (t < from) return false;
    const double span = kWvLapBeats * kTicksPerBeat;
    u1 = clampf(static_cast<float>((t - from) / span), 0.0f, 1.0f);
    u0 = clampf(static_cast<float>((t - kWvTrailBars * kTicksPerBar - from) / span), 0.0f, 1.0f);
    if (u1 <= u0) return false;
    centre = m.cage;
    radius = kWvRadii[lap] - (w ? 25.0f : 0.0f);
    dir = w ? -1.0f : 1.0f;
    ang0 = m.cageAng + (w ? kPi : 0.0f);
    return true;
}

bool World::weaverKnot(u32 w, double t) const {
    const MiniState& m = boss_.mini;
    if (boss_.kind != BossKind::WeaverPrime || m.cageTick < 0.0) return false;
    const double rel = t - weaverStart(w);
    if (rel < 0.0) return false;
    return rel - std::floor(rel / kTicksPerBar) * kTicksPerBar < kTicksPerBeat;
}

Vec2 World::weaverPos(u32 w, double t) const {
    // The knot for the beat where the last lap began (the first: where this lap begins); then round the lap in three beats.
    const BossState& s = boss_;
    const MiniState& m = s.mini;
    if (m.cageTick < 0.0) return s.body[w];
    double rel = t - weaverStart(w);
    if (rel < 0.0) rel += 4.0 * kTicksPerBar;  // (the second, before its first knot of this cage: the last cage's last lap)
    const u32 bar = static_cast<u32>(std::floor(rel / kTicksPerBar)) % 4u;
    const double inBar = rel - std::floor(rel / kTicksPerBar) * kTicksPerBar;
    const float a0 = m.cageAng + (w ? kPi : 0.0f), dir = w ? -1.0f : 1.0f, inner = w ? 25.0f : 0.0f;
    if (inBar < kTicksPerBeat) return m.cage + dirOf(a0) * (kWvRadii[bar ? bar - 1u : 0u] - inner);
    const float u = static_cast<float>((inBar - kTicksPerBeat) / (kWvLapBeats * kTicksPerBeat));
    return m.cage + dirOf(a0 + dir * kTau * u) * (kWvRadii[bar] - inner);
}

void World::updateWeaver(float /*dt*/) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    // Bar lines: a new cage every four bars round where the nearest player is (the host's), the last one's laps going off.
    forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
        if (t < s.fightTick - 0.5) return;
        if (m.cageTick < 0.0 || t >= m.cageTick + 4.0 * kTicksPerBar - 1.0) {
            const Vec2 w0 = m.cageTick < 0.0 ? s.body[0] : weaverPos(0, t - 1.0);
            const i32 near = nearestPlayer(w0.x, w0.y);
            const Vec2 c = near >= 0 ? players_[near].pos : w0;
            bossDecide(kGeomWeaverCage, c, Vec2{angleOf(w0 - c), 0.0f}, t);
            if (lengthSq(m.prevCage) > 0.0f) emitCue(BossCue::WvBurst, t, 0, 0.0f, m.prevCage);
        }
        emitCue(BossCue::WvKnot, t, 0, 0.0f, s.body[0]);
    });
    // Beats: each lap's saw glide as it begins; the second Weaver's knot on beat 3.
    forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64) {
        if (m.cageTick < 0.0 || t < m.cageTick) return;
        for (u32 w = 0; w < (m.half ? 2u : 1u); ++w) {
            const double rel = t - weaverStart(w);
            if (rel < -0.5) continue;
            const double inBar = rel - std::floor(rel / kTicksPerBar + 1e-9) * kTicksPerBar;
            const u32 bar = static_cast<u32>(std::floor(rel / kTicksPerBar + 1e-9)) % 4u;
            if (std::fabs(inBar - kTicksPerBeat) < 0.5) emitCue(BossCue::WvLap, t, static_cast<u8>(bar), 0.0f, weaverPos(w, t));
            if (w == 1 && inBar < 0.5) emitCue(BossCue::WvKnot, t, 1, 0.0f, weaverPos(w, t));
        }
    });
    for (u32 w = 0; w < 2; ++w) {
        const Vec2 p = weaverPos(w < (m.half ? 2u : 1u) ? w : 0u, m1_);
        s.bodyAng[w] = angleOf(p - s.body[w]);
        s.body[w] = p;
    }
}

// ---------------------------------------------------------------------------------------------- Deep Well

float World::wellStomach() const {
    // 5% of its health a bar (its own: the Track's mini-boss health with its factor); half as much again from half health.
    const BossState& s = boss_;
    const float armour = (s.mini.traits & traitBit(kArmored)) ? species_->traits.armoredHealth : 1.0f;
    return kDwStomach * s.coreMax / armour * (s.mini.half ? kDwStomachHalf : 1.0f);
}

bool World::wellChoked(double t) const {
    const MiniState& m = boss_.mini;
    return boss_.kind == BossKind::DeepWell && m.chokeTick >= 0.0 && t >= m.chokeTick && t < m.chokeTick + kTicksPerBar;
}

void World::updateWell(float dt) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    // It drifts toward the nearest player (30 wu/s; half health 90; Swift x 1.6).
    const float speed = (m.half ? kDwDriftHalf : kDwDrift) * ((m.traits & traitBit(kSwift)) ? species_->traits.swiftSpeed : 1.0f);
    const i32 near = nearestPlayer(s.body[0].x, s.body[0].y);
    if (near >= 0) {
        const Vec2 to = players_[near].pos - s.body[0];
        const float l = length(to);
        if (l > 1.0f) s.body[0] = s.body[0] + to * (std::fmin(speed * dt, l) / l);
        s.bodyAng[0] = angleOf(to);
    }
    // Shots within 400 wu bend into it (not while it is choked: it cannot swallow).
    if (m1_ >= s.fightTick && !wellChoked(m1_)) {
        for (u32 k = 0; k < shots_.count; ++k) {
            if (shots_.dead[k]) continue;
            const Vec2 to = s.body[0] - Vec2{shots_.x[k], shots_.y[k]};
            const float l = length(to);
            if (l > kDwBend || l < 1.0f) continue;
            const Vec2 vel = Vec2{shots_.dx[k], shots_.dy[k]} * shots_.speed[k] + to * (shots_.speed[k] * 6.0f * dt * (1.0f - l / kDwBend) / l);
            const float sp = length(vel);
            if (sp < 1.0f) continue;
            shots_.dx[k] = vel.x / sp;
            shots_.dy[k] = vel.y / sp;
        }
    }
    if (m1_ < s.fightTick) return;
    // Bar lines (the host's): what the bar fed it comes back as a ring (one bullet a 3 damage, at most 48; from half health in
    // two rings, beats 1 and 3), unless it was overfed: then it chokes for the bar.
    forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
        if (t < s.fightTick + 0.5) return;
        if (role_ != NetRole::Client) bossDecide(kGeomWellBar, Vec2{m.fill, m.fill > wellStomach() ? 1.0f : 0.0f}, Vec2{}, t);
        m.fill = 0.0f;
    });
    forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64 beatIndex) {
        if (m.ringLeft == 0 || ((beatIndex % 4) + 4) % 4 != 2) return;
        const u32 n = m.ringLeft;
        m.ringLeft = 0;
        const float a0 = static_cast<float>(hash32(static_cast<u32>(t), 0xD3E1u) % 360u) * kTau / 360.0f;
        for (u32 j = 0; j < n; ++j) {
            const float a = a0 + kTau * static_cast<float>(j) / static_cast<float>(n);
            spawnBullet(s.body[0] + dirOf(a) * 70.0f, dirOf(a) * kDwRingSpeed, tickAgeOf(t), 0.0f);
        }
        emitCue(BossCue::DwRing, t, 1, 0.0f, s.body[0]);
    });
}

}  // namespace rf::sim
