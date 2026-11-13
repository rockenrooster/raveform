// The Choir, the boss of Track 3 (design/bosses/choir.md; numbers and shapes from choir.json). Eight Voices on the
// Round, each a note of the scale. Only a singing Voice can be hurt; every call gets its answer; a dead Voice is a
// rest. Who sings when is a function of song time and of which Voices live (set on each bar line); the Round's turn
// changes only when a Voice dies.

#include <cmath>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
constexpr float kDeg = kTau / 360.0f;
constexpr u32 kV = ChoirData::kVoices;
constexpr float kStep = kTau / static_cast<float>(kV);
u32 bitsIn(u32 m) {
    u32 n = 0;
    for (; m; m &= m - 1) ++n;
    return n;
}
i64 imod(i64 a, i64 m) { return ((a % m) + m) % m; }
}  // namespace

u8 World::choirPhaseOf(u32 alive) {
    // choir.json phases: Unison 8-7, Canon 6-5, Fugue 4-3, Solo 2-1.
    const u32 n = bitsIn(alive);
    return n >= 7 ? 0 : (n >= 5 ? 1 : (n >= 3 ? 2 : 3));
}

float World::choirRingAngle(double t) const {
    const ChoirState& c = boss_.ch;
    const double since = t > c.ringTick ? t - c.ringTick : 0.0;
    return c.ring0 + c.ringDir * kTau * static_cast<float>(since / (static_cast<double>(prose::kChTurnBars) * kTicksPerBar));
}

u32 World::choirPhaseBar(double t) const {
    const u32 fb = bossFightBar(t);
    return fb >= boss_.phaseBar0 ? fb - boss_.phaseBar0 : 0u;
}

bool World::choirSinging(u32 k, double t) const {
    const BossState& s = boss_;
    if (!(s.alive & (1u << k)) || t < s.fightTick) return false;
    return t < s.ch.singUntil[k] || choirHeld(k, t, nullptr) == 2;
}

u8 World::choirCage(double t) const {
    // The cage (from the Canon): the neighbour strings show danger dashes for a bar, then burn for two beats from the
    // next downbeat. Every fourth bar in the Canon, every second in the Fugue.
    const BossState& s = boss_;
    if (!s.active || s.dying || t < s.fightTick) return 0;
    const u32 per = s.ch.phase == 1 ? prose::kChCanonCageBars : (s.ch.phase == 2 ? prose::kChFugueCageBars : 0u);
    if (!per) return 0;
    const u32 j = choirPhaseBar(t);
    const double inBar = t - std::floor(t / kTicksPerBar) * kTicksPerBar;
    if (j % per == per - 1) return 1;
    if (j % per == 0 && j >= per && inBar < static_cast<double>(species_->choir.cageBurnBeats) * kTicksPerBeat) return 2;
    return 0;
}

u8 World::choirHeld(u32 k, double t, float* angle) const {
    // The held note (Solo, every fourth bar): a dashed line for a bar, then a beam from the open mouth sweeping 120
    // degrees over beats 1 and 2 of the next.
    const BossState& s = boss_;
    const ChoirData& d = species_->choir;
    if (!(s.alive & (1u << k)) || s.ch.phase != 3 || t < s.fightTick) return 0;
    const u32 j = choirPhaseBar(t);
    const double inBar = t - std::floor(t / kTicksPerBar) * kTicksPerBar;
    const u32 per = prose::kChHeldBars;
    const float aim = s.ch.heldAim[k];
    if (j % per == per - 1) {
        if (angle) *angle = aim - 0.5f * d.heldSweepDeg * kDeg;
        return 1;
    }
    const double sweep = static_cast<double>(d.heldSweepBeats) * kTicksPerBeat;
    if (j % per == 0 && j >= per && inBar < sweep) {
        if (angle) *angle = aim + (static_cast<float>(inBar / sweep) - 0.5f) * d.heldSweepDeg * kDeg;
        return 2;
    }
    return 0;
}

u8 World::choirSingers(double t) const {
    const BossState& s = boss_;
    const ChoirState& c = s.ch;
    if (!s.active || s.dying || t < s.fightTick || c.bar == 0xFFFFFFFFu) return 0;
    const u32 bar = bossFightBar(t + 1.0);
    u32 caller = c.caller;
    if (bar == c.bar + 1) {
        if (c.phase == 0) {
            caller = (caller + 1u) % kV;
        } else {
            for (u32 step = 1; step <= kV; ++step)
                if (s.alive & (1u << ((c.caller + step) % kV))) {
                    caller = (c.caller + step) % kV;
                    break;
                }
        }
    } else if (bar != c.bar) {
        return 0;
    }
    const i64 e = imod(static_cast<i64>(std::floor((t - std::floor(t / kTicksPerBar) * kTicksPerBar) / (kTicksPerBeat / 2.0) + 1e-6)), 8);
    u32 mask = 0;
    switch (c.phase) {
        case 0:
            if (e == 0 || e == 4) mask = 1u << caller;
            if (e == 2 || e == 6) mask = 1u << ((caller + kV / 2) % kV);
            break;
        case 1: {
            if (e == 0) {
                mask = 1u << caller;
                break;
            }
            u32 m = 0;
            for (u32 step = 1; step < kV; ++step) {
                const u32 k = (caller + step) % kV;
                if ((s.alive & (1u << k)) && ++m == static_cast<u32>(e)) mask = 1u << k;
            }
            break;
        }
        case 2: mask = (choirPhaseBar(t + 1.0) % 2u) == 0u ? c.groupA : c.groupB; break;
        default:
            if (e == 2 || e == 6) mask = s.alive;
            break;
    }
    return static_cast<u8>(mask & s.alive);
}

float World::choirScale(u8 part, u8 index, double t) const {
    const ChoirData& d = species_->choir;
    if (part != 2 || !choirSinging(index, t)) return d.dmgSilent;  // rule 2: shots ring off a closed hood
    return bitsIn(boss_.alive) == 1 ? d.dmgLast : d.dmgSinging;
}

void World::enterChoir(Vec2 centre) {
    BossState& s = boss_;
    const ChoirData& d = species_->choir;
    ChoirState& c = s.ch;
    s.alive = (1u << kV) - 1u;
    s.partMax = d.voiceHealth * s.scale;
    for (u32 k = 0; k < kV; ++k) s.partHp[k] = s.partMax;
    s.coreMax = s.coreHp = 0.0f;
    s.fightTick = s.enterTick + prose::kChEntranceBars * kTicksPerBar;  // firing starts on bar 3
    s.phaseTick = s.fightTick;
    c = ChoirState{};
    const float m = prose::kChWallKeepWu;
    c.centre = Vec2{clampf(centre.x, -kArenaHalfW + m, kArenaHalfW - m), clampf(centre.y, -kArenaHalfH + m, kArenaHalfH - m)};
    c.ring0 = -0.5f * kPi;  // Voice 0 (D) at the top, the scale clockwise
    c.ringTick = s.fightTick;
    c.ringDir = 1.0f;
    c.radius = d.ringRadius;
    for (double& u : c.singUntil) u = -1.0;
    s.pos = s.prevPos = c.centre;
    for (u32 k = 0; k < kV; ++k) {
        const float a = c.ring0 + static_cast<float>(k) * kStep;
        s.body[1 + k] = s.bodyPrev[1 + k] = c.centre + dirOf(a) * c.radius;
        s.bodyAng[1 + k] = s.bodyPrevAng[1 + k] = a + kPi;  // +x toward the centre
    }
    s.body[0] = s.bodyPrev[0] = c.centre;
}

void World::choirPartDied(u8 index, Event& ev) {
    BossState& s = boss_;
    ChoirState& c = s.ch;
    const Vec2 at = s.body[1 + index];
    ev.x = at.x;
    ev.y = at.y;
    ev.f = s.bodyAng[1 + index];
    // Rule 5: the Round stops for one beat and turns the other way.
    c.ring0 = choirRingAngle(m1_);
    c.ringTick = m1_ + kTicksPerBeat;
    c.ringDir = -c.ringDir;
    c.singUntil[index] = -1.0;
    for (u32 p = 0; p < playerCount_; ++p) {
        if (!players_[p].present || !isLocal(p)) continue;
        for (u32 k = 0; k < kChoirVoiceShards; ++k)
            spawnShard(at, dirOf(rng_.range(0.0f, kTau)) * rng_.range(kShardBurstLo, kShardBurstHi * 1.5f), static_cast<u8>(p));
    }
    emitCue(BossCue::ChDeath, m1_, index, static_cast<float>(species_->choir.midi[index]), at);
}

Vec2 World::choirAimFrom(Vec2 from) const {
    const i32 near = nearestPlayer(from.x, from.y);
    return near >= 0 ? players_[near].pos : boss_.ch.centre;
}

void World::choirSing(u32 k, double t, u32 bullets, float spreadDeg, float aim) {
    BossState& s = boss_;
    const ChoirData& d = species_->choir;
    if (!(s.alive & (1u << k))) return;  // rule 4: a dead Voice is a rest
    const Vec2 at = s.body[1 + k];
    const u32 n = bossScaledCount(bullets, bossBulletDensity(), 1u);
    const float adv = tickAgeOf(t);
    for (u32 j = 0; j < n; ++j) {
        const float da = (static_cast<float>(j) - 0.5f * static_cast<float>(n - 1)) * spreadDeg * kDeg;
        spawnBullet(at, dirOf(aim + da) * d.bulletSpeed, adv, d.bulletLife);
    }
    const double until = t + prose::kChSingBeats * kTicksPerBeat;
    if (until > s.ch.singUntil[k]) s.ch.singUntil[k] = until;
    emitCue(BossCue::ChVoice, t, static_cast<u8>(k), static_cast<float>(d.midi[k]), at);
}

void World::choirBar(double t) {
    // The bar line: its phase, its caller, the Fugue's groups and aims, the held note's aim.
    BossState& s = boss_;
    ChoirState& c = s.ch;
    const ChoirData& d = species_->choir;
    const u8 ph = choirPhaseOf(s.alive);
    if (ph != c.phase || c.bar == 0xFFFFFFFFu) {
        if (c.bar != 0xFFFFFFFFu && ph == 2 && c.phase < 2) c.shrinkTick = t;  // the Round shrinks into the Fugue
        c.phase = ph;
        s.phase = ph;
        s.phaseTick = t;
        s.phaseBar0 = bossFightBar(t + 1.0);
    }
    c.bar = bossFightBar(t + 1.0);
    // The caller: Unison moves one Voice clockwise every bar (a dead one is a rest); the Canon one live Voice.
    if (c.bar == 0) {
        c.caller = 0;
    } else if (ph == 0) {
        c.caller = static_cast<u8>((c.caller + 1u) % kV);
    } else {
        for (u32 step = 1; step <= kV; ++step) {
            const u32 k = (c.caller + step) % kV;
            if (s.alive & (1u << k)) {
                c.caller = static_cast<u8>(k);
                break;
            }
        }
    }
    if (ph == 2) {
        // Two callers as far apart as the live Voices allow; the others answer.
        u32 bi = 0, bj = 0, best = 0;
        for (u32 i = 0; i < kV; ++i)
            for (u32 j = i + 1; j < kV; ++j) {
                if (!(s.alive & (1u << i)) || !(s.alive & (1u << j))) continue;
                const u32 gap = j - i < kV - (j - i) ? j - i : kV - (j - i);
                if (gap > best) {
                    best = gap;
                    bi = i;
                    bj = j;
                }
            }
        c.groupA = static_cast<u8>(best ? ((1u << bi) | (1u << bj)) : 0u);
        c.groupB = static_cast<u8>(s.alive & ~static_cast<u32>(c.groupA));
        for (u32 k = 0; k < kV; ++k)
            if (s.alive & (1u << k)) c.heldAim[k] = angleOf(choirAimFrom(s.body[1 + k]) - s.body[1 + k]);  // each stream's centre
    }
    if (ph == 3) {
        const u32 j = choirPhaseBar(t + 1.0);
        if (j % prose::kChHeldBars == prose::kChHeldBars - 1)
            for (u32 k = 0; k < kV; ++k)
                if (s.alive & (1u << k)) {
                    c.heldAim[k] = angleOf(choirAimFrom(s.body[1 + k]) - s.body[1 + k]);
                    emitCue(BossCue::ChHeld, t, static_cast<u8>(k), static_cast<float>(d.midi[k]), s.body[1 + k]);
                }
    }
    // The cage's tick on every beat of its telegraph bar, its burn on the downbeat after.
    const u8 cage = choirCage(t + 1.0);
    if (cage == 1)
        for (u32 b = 0; b < 4; ++b) emitCue(BossCue::ChCageTick, t + b * kTicksPerBeat, 0, 0.0f, c.centre);
    if (cage == 2) emitCue(BossCue::ChCageBurn, t, 0, 0.0f, c.centre);
}

void World::choirEighth(double t) {
    // Who sings on this eighth (choir.json phases).
    BossState& s = boss_;
    const ChoirState& c = s.ch;
    const ChoirData& d = species_->choir;
    const i64 e = imod(static_cast<i64>(std::floor((t - std::floor(t / kTicksPerBar) * kTicksPerBar) / (kTicksPerBeat / 2.0) + 0.5)), 8);
    const auto aimAt = [&](u32 k) { return angleOf(choirAimFrom(s.body[1 + k]) - s.body[1 + k]); };
    switch (c.phase) {
        case 0: {  // Unison: the caller on beats 1 and 3, the Voice opposite on 2 and 4
            if (e == 0 || e == 4) choirSing(c.caller, t, d.unisonCall, prose::kChFanCallDeg, aimAt(c.caller));
            const u32 o = (c.caller + kV / 2) % kV;
            if (e == 2 || e == 6) choirSing(o, t, d.unisonAnswer, prose::kChFanCallDeg, aimAt(o));
            break;
        }
        case 1: {  // Canon: the caller on beat 1, then every other live Voice clockwise, an eighth apart
            if (e == 0) {
                choirSing(c.caller, t, d.canonCall, prose::kChFanCallDeg, aimAt(c.caller));
                break;
            }
            u32 m = 0;
            for (u32 step = 1; step < kV; ++step) {
                const u32 k = (c.caller + step) % kV;
                if (!(s.alive & (1u << k))) continue;
                if (++m == static_cast<u32>(e)) choirSing(k, t, d.canonAnswer, prose::kChFanAnswerDeg, aimAt(k));
            }
            break;
        }
        case 2: {  // Fugue: two callers stream across the nearest player; the others play it back mirrored next bar
            const bool callBar = (choirPhaseBar(t + 1.0) % 2u) == 0u;
            const u8 group = callBar ? c.groupA : c.groupB;
            u32 m = 0;
            for (u32 k = 0; k < kV; ++k) {
                if (!(group & (1u << k)) || !(s.alive & (1u << k))) continue;
                const float dir = ((m++ & 1u) == 0u ? 1.0f : -1.0f) * (callBar ? 1.0f : -1.0f);
                const float u = static_cast<float>(e) / 7.0f - 0.5f;
                choirSing(k, t, d.fugueStream, 0.0f, c.heldAim[k] + dir * u * prose::kChFugueSweepDeg * kDeg);
            }
            break;
        }
        default: {  // Solo: each live Voice a ring of 16 on beats 2 and 4, alternate rings turned half a step
            if (e != 2 && e != 6) break;
            for (u32 k = 0; k < kV; ++k) {
                if (!(s.alive & (1u << k))) continue;
                const u32 n = bossScaledCount(d.soloRing, bossBulletDensity(), 4u);
                const float turn = e == 6 ? 0.5f * kTau / static_cast<float>(n) : 0.0f;
                const Vec2 at = s.body[1 + k];
                for (u32 j = 0; j < n; ++j)
                    spawnBullet(at, dirOf(turn + kTau * static_cast<float>(j) / static_cast<float>(n)) * d.bulletSpeed, tickAgeOf(t), d.bulletLife);
                const double until = t + prose::kChSingBeats * kTicksPerBeat;
                if (until > s.ch.singUntil[k]) s.ch.singUntil[k] = until;
                emitCue(BossCue::ChVoice, t, static_cast<u8>(k), static_cast<float>(d.midi[k]), at);
            }
            break;
        }
    }
}

bool World::choirBeamsHit(Vec2 at) const {
    // The burning cage (a string between neighbouring live Voices) and the held note's beam.
    const BossState& s = boss_;
    const float half = 0.5f * species_->boss.phases[1].beamWidth + kPlayerHitRadius;  // as Kaleidos's beam
    const auto nearSeg = [&](Vec2 a, Vec2 b) {
        const Vec2 d = b - a;
        const float l2 = lengthSq(d);
        const float t = l2 > 1e-6f ? clampf(dot(at - a, d) / l2, 0.0f, 1.0f) : 0.0f;
        return lengthSq(a + d * t - at) <= half * half;
    };
    if (choirCage(m1_) == 2) {
        for (u32 k = 0; k < kV; ++k) {
            const u32 n = (k + 1) % kV;
            if ((s.alive & (1u << k)) && (s.alive & (1u << n)) && nearSeg(s.body[1 + k], s.body[1 + n])) return true;
        }
    }
    for (u32 k = 0; k < kV; ++k) {
        float a = 0.0f;
        if (choirHeld(k, m1_, &a) != 2) continue;
        const Vec2 from = s.body[1 + k];
        if (nearSeg(from, from + dirOf(a) * 12000.0f)) return true;
    }
    return false;
}

void World::updateChoir(float dt) {
    BossState& s = boss_;
    ChoirState& c = s.ch;
    const ChoirData& d = species_->choir;
    // The entrance: the Voices warp in clockwise, one every eighth, each singing its note; on bar 2 all eight sing.
    if (m1_ < s.fightTick) {
        forCrossings(m0_, m1_, kTicksPerBeat / 2.0, [&](double t, i64) {
            const double since = t - s.enterTick;
            if (since < -0.5) return;
            const u32 e = static_cast<u32>(std::floor(since / (kTicksPerBeat / 2.0) + 0.5));
            if (e < kV) emitCue(BossCue::ChVoice, t, static_cast<u8>(e), static_cast<float>(d.midi[e]), s.body[1 + e]);
            if (e == 8)
                for (u32 k = 0; k < kV; ++k) emitCue(BossCue::ChVoice, t, static_cast<u8>(k), static_cast<float>(d.midi[k]), s.body[1 + k]);
        });
    }
    // The Round follows the players' centre at 60 wu/s, kept 700 wu from every wall.
    Vec2 sum{};
    u32 n = 0;
    for (u32 p = 0; p < playerCount_; ++p)
        if (players_[p].alive) {
            sum = sum + players_[p].pos;
            ++n;
        }
    if (n) {
        const Vec2 to = sum * (1.0f / static_cast<float>(n)) - c.centre;
        const float l = length(to);
        const float step = prose::kChFollowWu * dt;
        if (l > 1e-3f) c.centre = c.centre + to * (l < step ? 1.0f : step / l);
        const float m = prose::kChWallKeepWu;
        c.centre = Vec2{clampf(c.centre.x, -kArenaHalfW + m, kArenaHalfW - m), clampf(c.centre.y, -kArenaHalfH + m, kArenaHalfH - m)};
    }
    // The Round's radius: 380, shrinking to 300 over two bars into the Fugue.
    if (c.shrinkTick >= 0.0) {
        const float u = clampf(static_cast<float>((m1_ - c.shrinkTick) / (static_cast<double>(d.shrinkBars) * kTicksPerBar)), 0.0f, 1.0f);
        c.radius = d.ringRadius + (d.ringFugue - d.ringRadius) * u;
    }
    if (m1_ >= s.fightTick) {
        forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
            if (t >= s.fightTick - 0.5) choirBar(t);
        });
        if (c.bar == 0xFFFFFFFFu) choirBar(s.fightTick);
        forCrossings(m0_, m1_, kTicksPerBeat / 2.0, [&](double t, i64) {
            if (t >= s.fightTick - 0.5) choirEighth(t);
        });
    }
    // Where each Voice stands: on the Round, or (Solo) circling a player at 280 wu, a lap every four bars.
    const float ring = choirRingAngle(m1_);
    u32 m = 0;
    const u32 live = bitsIn(s.alive);
    for (u32 k = 0; k < kV; ++k) {
        if (!(s.alive & (1u << k))) continue;
        Vec2 want;
        float face;
        if (c.phase == 3 && m1_ >= s.fightTick) {
            // In co-op each Voice circles a different player while two live.
            i32 who = nearestPlayer(s.body[1 + k].x, s.body[1 + k].y);
            if (coop() && live == 2) {
                u32 seen = 0;
                for (u32 p = 0; p < playerCount_; ++p)
                    if (players_[p].alive && seen++ == m % 2u) who = static_cast<i32>(p);
            }
            const Vec2 around = who >= 0 ? players_[who].pos : c.centre;
            const float a = kTau * static_cast<float>((m1_ - s.phaseTick) / (static_cast<double>(d.orbitLapBars) * kTicksPerBar)) +
                            static_cast<float>(m) * kPi + static_cast<float>(k) * kStep;
            want = around + dirOf(a) * d.orbitRadius;
            face = angleOf(around - want);
        } else {
            const float a = ring + static_cast<float>(k) * kStep;
            want = c.centre + dirOf(a) * c.radius;
            face = a + kPi;
        }
        ++m;
        const Vec2 to = want - s.body[1 + k];
        const float l = length(to);
        const float step = kChoirMoveWu * dt;
        s.body[1 + k] = l <= step ? want : s.body[1 + k] + to * (step / l);
        s.bodyAng[1 + k] = face;
    }
    s.body[0] = c.centre;
    s.pos = c.centre;
}

}  // namespace rf::sim
