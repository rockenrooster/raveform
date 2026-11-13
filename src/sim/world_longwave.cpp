// Longwave, the boss of Track 2 (design/bosses/longwave.md; numbers and shapes from longwave.json). A serpent whose
// body is a travelling sine wave: the head swims a figure of eight around its anchor, segment k sits k x spacing behind
// it along the path and swings sideways with the wave. Only the crest can be hurt, for its one grid step; troughs fire;
// the jaw opens on beat 1. The head's place, the wave and every bullet are functions of song time, of which segments
// are alive and of the moments the phase changed (on a beat line, so the step grid never slips).

#include <cmath>
#include <initializer_list>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
constexpr float kDeg = kTau / 360.0f;
u32 bitsIn(u32 m) {
    u32 n = 0;
    for (; m; m &= m - 1) ++n;
    return n;
}
u32 rankOf(u32 alive, u32 k) { return bitsIn(alive & ((1u << k) - 1u)); }  // live segments ahead of segment k
float smooth(float u) {
    u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
    return u * u * (3.0f - 2.0f * u);
}
i64 imod(i64 a, i64 m) { return ((a % m) + m) % m; }
constexpr u8 kRunSemis[8] = {0, 2, 3, 5, 7, 8, 10, 12};  // longwave.json music.rule (the degrees above the chord root)
}  // namespace

u32 World::bossSegmentsAlive() const { return bitsIn(boss_.alive); }

double World::longwaveSteps(double t) const {
    const LongwaveState& L = boss_.lw;
    return L.s0 + (t - L.sTick) / L.stepTicks;
}

Vec2 World::longwaveAnchor(double t) const {
    const LongwaveState& L = boss_.lw;
    if (L.dashTick >= 0.0 && t >= L.dashTick) {
        // The dash: the whole path slides along the lunge's line over its beat (L.anchor is where it ends).
        const LongwaveData& d = species_->longwave;
        const u32 ph = L.phase < 4 ? L.phase : 3u;
        const double beats = d.phases[ph].lungeDashBeats > 0.0f ? d.phases[ph].lungeDashBeats : 1.0;
        const float u = static_cast<float>((t - L.dashTick) / (beats * kTicksPerBeat));
        if (u < 1.0f) return L.lungeFrom + (L.anchor - L.lungeFrom) * smooth(u);
    }
    return L.anchor;
}

Vec2 World::longwavePoint(double t, float k, float shift, Vec2* tangentOut, float* waveOut) const {
    const BossState& s = boss_;
    const LongwaveState& L = s.lw;
    const LongwaveData& d = species_->longwave;
    const double p = L.p0 + L.pRate * (t - L.pTick);
    const double headArc = d.arcOf(p);
    const double pk = d.paramOf(headArc - static_cast<double>(k) * d.spacing);
    const Vec2 base = longwaveAnchor(t) + d.pathAt(pk);
    const Vec2 tan = d.tangentAt(pk);
    const double steps = longwaveSteps(t);
    const float lambda = static_cast<float>(d.wavelength);
    const float w = std::sin(kTau * (k / lambda - static_cast<float>(steps) / lambda - shift));
    const float taper = d.taper ? std::fmin(1.0f, k / static_cast<float>(d.taper)) : 1.0f;
    const float off = d.amplitude * taper * w;
    Vec2 at = base + Vec2{-tan.y, tan.x} * off;
    if (t < s.fightTick && s.fightTick > s.enterTick) {
        // The entrance: the head breaks in from the nearest wall and the body follows it onto the path over two bars.
        const float u = static_cast<float>((t - s.enterTick) / (s.fightTick - s.enterTick));
        const Vec2 headAt = L.anchor + d.pathAt(L.p0 + L.pRate * (s.enterTick - L.pTick));
        at = at + (L.enterFrom - headAt) * (1.0f - smooth(u));
    }
    if (tangentOut) *tangentOut = tan;
    if (waveOut) *waveOut = w;
    return at;
}

bool World::longwaveJawOpen(double t) const {
    const LongwaveState& L = boss_.lw;
    const double inBar = t - std::floor(t / kTicksPerBar) * kTicksPerBar;
    return inBar < prose::kLwJawBeats * kTicksPerBeat || t < L.jawUntil;  // rule 5, and the lunge's two beats
}

i32 World::longwaveCrest(u32 segment, double t) const {
    // Rule 2: the crest is the live segment whose place k (1 + its rank) satisfies k = n + 2 (mod the wavelength) for
    // step n: where sin(2 pi (k - steps) / wavelength) peaks during that step. Returns its place in the wave (0..7)
    // when `segment` is a crest now, else -1.
    const BossState& s = boss_;
    if (!(s.alive & (1u << segment)) || t < s.fightTick) return -1;
    const LongwaveData& d = species_->longwave;
    const i64 n = static_cast<i64>(std::floor(longwaveSteps(t) + 1e-9));
    const i64 k = static_cast<i64>(rankOf(s.alive, segment)) + 1;
    const i64 lambda = static_cast<i64>(d.wavelength);
    return imod(k - n - lambda / 4, lambda) == 0 ? static_cast<i32>(imod(n, lambda)) : -1;
}

float World::longwaveScale(u8 part, u8 index, double t) const {
    const LongwaveData& d = species_->longwave;
    if (part == 3) {
        if (!longwaveJawOpen(t)) return d.dmgJawClosed;
        return boss_.alive == 0 ? d.dmgJawBare : d.dmgJaw;
    }
    if (part == 2) return longwaveCrest(index, t) >= 0 ? d.dmgCrest : d.dmgElsewhere;
    return 0.0f;
}

void World::enterLongwave(Vec2 centre) {
    BossState& s = boss_;
    const LongwaveData& d = species_->longwave;
    LongwaveState& L = s.lw;
    s.alive = d.segments >= 32 ? 0xFFFFFFFFu : ((1u << d.segments) - 1u);
    s.partMax = d.segHealth * s.scale;
    s.coreMax = d.headHealth * s.scale;
    for (u32 k = 0; k < d.segments; ++k) s.partHp[k] = s.partMax;
    s.coreHp = s.coreMax;
    s.fightTick = s.enterTick + prose::kLwEntranceBars * kTicksPerBar;
    s.phaseTick = s.fightTick;
    // The anchor: the centre of the players when it arrives, kept 1,100 wu from every wall (longwave.json path.anchor),
    // and so that the whole path with its wave stays inside the arena.
    const float mx = std::fmin(kArenaHalfW - prose::kLwWallKeepWu, kArenaHalfW - d.pathHalfW - d.amplitude - d.segRadius - 60.0f);
    const float my = std::fmin(kArenaHalfH - prose::kLwWallKeepWu, kArenaHalfH - d.pathHalfH - d.amplitude - d.segRadius - 60.0f);
    L = LongwaveState{};
    L.anchor = Vec2{clampf(centre.x, -mx, mx), clampf(centre.y, -my, my)};
    // The head starts at the end of the figure of eight nearer the nearest side wall, swimming.
    L.p0 = L.anchor.x >= 0.0f ? 0.0 : kPi;
    L.pTick = s.enterTick;
    L.pRate = kTau / (static_cast<double>(d.phases[0].lapBars) * kTicksPerBar);
    L.s0 = 0.0;
    L.sTick = s.fightTick;
    L.stepTicks = d.phases[0].stepTicks;
    L.phase = 0;
    const Vec2 head = L.anchor + d.pathAt(L.p0);
    // The nearest wall's point.
    const float dl = head.x + kArenaHalfW, dr = kArenaHalfW - head.x, dt = head.y + kArenaHalfH, db = kArenaHalfH - head.y;
    L.enterFrom = head;
    if (dl <= dr && dl <= dt && dl <= db) L.enterFrom.x = -kArenaHalfW;
    else if (dr <= dt && dr <= db) L.enterFrom.x = kArenaHalfW;
    else if (dt <= db) L.enterFrom.y = -kArenaHalfH;
    else L.enterFrom.y = kArenaHalfH;
    s.pos = s.prevPos = L.enterFrom;
    placeLongwave(s.enterTick);
    for (u32 k = 0; k < kBossBodies; ++k) {
        s.bodyPrev[k] = s.body[k];
        s.bodyPrevAng[k] = s.bodyAng[k];
    }
}

void World::placeLongwave(double t) {
    BossState& s = boss_;
    const LongwaveData& d = species_->longwave;
    Vec2 tan;
    s.body[0] = longwavePoint(t, 0.0f, 0.0f, &tan);
    s.bodyAng[0] = angleOf(tan);
    u32 rank = 0;
    for (u32 k = 0; k < d.segments; ++k) {
        if (!(s.alive & (1u << k))) continue;
        ++rank;
        s.body[1 + k] = longwavePoint(t, static_cast<float>(rank), 0.0f, &tan);
        s.bodyAng[1 + k] = angleOf(tan);
    }
    // The tail's tip, a little past the last live segment.
    s.body[kBossBodies - 1] = longwavePoint(t, static_cast<float>(rank) + 0.9f, 0.0f, &tan);
    s.bodyAng[kBossBodies - 1] = angleOf(tan);
    s.pos = s.body[0];
    s.angle = s.bodyAng[0];
}

void World::longwavePhaseAt(double t) {
    // A phase change lands on a beat line: the head's rate and the wave's step change, never their place.
    BossState& s = boss_;
    LongwaveState& L = s.lw;
    const LongwaveData& d = species_->longwave;
    const u32 ph = d.phaseOf(bitsIn(s.alive));
    if (ph == L.phase) return;
    L.p0 = L.p0 + L.pRate * (t - L.pTick);
    L.pTick = t;
    L.s0 = std::floor(longwaveSteps(t) + 0.5);  // integer on a beat line: the step grids all divide the beat
    L.sTick = t;
    L.phase = static_cast<u8>(ph);
    if (ph < 3) {
        L.pRate = kTau / (static_cast<double>(d.phases[ph].lapBars) * kTicksPerBar);
        L.stepTicks = d.phases[ph].stepTicks;
    } else {
        L.pRate = kTau / (static_cast<double>(d.phases[2].lapBars) * kTicksPerBar);  // the bare head keeps swimming
    }
    L.whipTick = (std::floor(t / kTicksPerBar) + 1.0) * kTicksPerBar;  // the downbeat after each phase change
    L.lungeArmed = false;
    L.lungeTelegraph = L.lungeTick = -1.0;
    s.phase = static_cast<u8>(ph);
    s.phaseTick = t;
    s.phaseBar0 = bossFightBar(t) + 1;
}

void World::longwavePartDied(u8 index, Event& ev) {
    BossState& s = boss_;
    LongwaveState& L = s.lw;
    const Vec2 at = s.body[1 + index];
    ev.x = at.x;
    ev.y = at.y;
    ev.f = s.bodyAng[1 + index];
    // Its Shards (each device its own players').
    for (u32 p = 0; p < playerCount_; ++p) {
        if (!players_[p].present || !isLocal(p)) continue;
        for (u32 k = 0; k < kLwSegmentShards; ++k)
            spawnShard(at, dirOf(rng_.range(0.0f, kTau)) * rng_.range(kShardBurstLo, kShardBurstHi), static_cast<u8>(p));
    }
    // The phase is how many segments are left: a change waits for the next beat line.
    const u32 ph = species_->longwave.phaseOf(bitsIn(s.alive));
    if (ph != L.phase && L.pendingPhaseTick < 0.0) L.pendingPhaseTick = (std::floor(m1_ / kTicksPerBeat) + 1.0) * kTicksPerBeat;
}

void World::longwaveFire(double t, i64 n) {
    // Rule 4: every trough fires on its step, square to the body.
    BossState& s = boss_;
    const LongwaveData& d = species_->longwave;
    const LongwaveState& L = s.lw;
    const u32 ph = L.phase < 3 ? L.phase : 2u;
    const float adv = tickAgeOf(t);
    const float density = bossBulletDensity();
    const u32 per = bossScaledCount(d.phases[ph].perSide ? d.phases[ph].perSide : 1u, density, 1u);
    const float spread = (ph == 1 ? prose::kLwChopSpreadDeg : 0.0f) * kDeg;
    const i64 lambda = static_cast<i64>(d.wavelength);
    u32 rank = 0;
    bool fired = false;
    for (u32 k = 0; k < d.segments; ++k) {
        if (!(s.alive & (1u << k))) continue;
        ++rank;
        if (imod(static_cast<i64>(rank) - n - 3 * lambda / 4, lambda) != 0) continue;
        Vec2 tan;
        const Vec2 at = longwavePoint(t, static_cast<float>(rank), 0.0f, &tan);
        const float a = angleOf(Vec2{-tan.y, tan.x});
        for (float side : {0.0f, kPi}) {
            for (u32 j = 0; j < per; ++j) {
                const float da = (static_cast<float>(j) - 0.5f * static_cast<float>(per - 1)) * spread;
                spawnBullet(at, dirOf(a + side + da) * d.bulletSpeed, adv, d.bulletLife);
            }
        }
        fired = true;
    }
    if (fired) emitCue(BossCue::LwTrough, t, 0, 0.0f, s.body[0]);
    // The run: one note on every crest step, the crest's place in its wave as a degree of the scale (music.rule).
    if (s.alive) emitCue(BossCue::LwRun, t, static_cast<u8>(imod(n, lambda)), static_cast<float>(kRunSemis[imod(n, 8)]), s.body[0]);
}

void World::updateLongwave(float /*dt*/) {
    BossState& s = boss_;
    LongwaveState& L = s.lw;
    const LongwaveData& d = species_->longwave;
    // A phase change waiting for its beat line.
    if (L.pendingPhaseTick >= 0.0 && m1_ >= L.pendingPhaseTick) {
        longwavePhaseAt(L.pendingPhaseTick);
        L.pendingPhaseTick = -1.0;
    }
    if (m1_ >= s.fightTick) {
        // The wave's steps crossed this tick (the troughs and the run).
        const double s0 = longwaveSteps(m0_ > s.fightTick ? m0_ : s.fightTick - 1e-6), s1 = longwaveSteps(m1_);
        for (i64 n = static_cast<i64>(std::floor(s0)) + 1; n <= static_cast<i64>(std::floor(s1 + 1e-9)) && s.alive; ++n) {
            const double t = L.sTick + (static_cast<double>(n) - L.s0) * L.stepTicks;
            if (t >= s.fightTick && t <= m1_ + 1e-6) longwaveFire(t, n);
        }
        // The whip: a ring from the tail's tip on the downbeat after each phase change.
        if (L.whipTick >= 0.0 && m1_ >= L.whipTick) {
            const double t = L.whipTick;
            L.whipTick = -1.0;
            const u32 n = bossScaledCount(d.whipCount, bossBulletDensity(), 8u);
            const Vec2 tip = s.alive ? s.body[kBossBodies - 1] : s.body[0];
            for (u32 j = 0; j < n; ++j)
                spawnBullet(tip, dirOf(kTau * static_cast<float>(j) / static_cast<float>(n)) * d.bulletSpeed, tickAgeOf(t), d.bulletLife);
            emitCue(BossCue::LwWhip, t, 0, 0.0f, tip);
        }
        if (L.lungeArmed && m1_ >= L.lungeTick) {
            const LongwaveData::Phase& P = d.phases[L.phase < 4 ? L.phase : 3];
            L.lungeArmed = false;
            L.lungeFrom = longwaveAnchor(L.lungeTick);
            L.dashTick = L.lungeTick;
            const float mx = std::fmin(kArenaHalfW - prose::kLwWallKeepWu, kArenaHalfW - d.pathHalfW - d.amplitude - d.segRadius - 60.0f);
            const float my = std::fmin(kArenaHalfH - prose::kLwWallKeepWu, kArenaHalfH - d.pathHalfH - d.amplitude - d.segRadius - 60.0f);
            const Vec2 to = L.anchor + L.lungeDir * (P.lungeDashWu > 0.0f ? P.lungeDashWu : 700.0f);
            L.anchor = Vec2{clampf(to.x, -mx, mx), clampf(to.y, -my, my)};
            L.jawUntil = L.lungeTick + prose::kLwLungeJawBeats * kTicksPerBeat;
            emitCue(BossCue::LwLunge, L.lungeTick, 0, 0.0f, s.body[0]);
        }
        const u32 ph = L.phase;
        // The lunge (Whitecap every second bar, the bare head every bar): the line shows, then the dash on the downbeat.
        forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
            if (t < s.fightTick) return;
            // The jaw's horn on beat 1.
            emitCue(BossCue::LwJaw, t, 0, 0.0f, s.body[0]);
            if (ph < 2 || L.lungeArmed) return;
            const u32 j = bossFightBar(t + 1.0) >= s.phaseBar0 ? bossFightBar(t + 1.0) - s.phaseBar0 : 0u;
            const LongwaveData::Phase& P = d.phases[ph];
            const bool bare = ph == 3;
            if (!bare && (j % prose::kLwWhitecapLungeBars) != 1u) return;
            L.lungeArmed = true;
            L.lungeTick = t + static_cast<double>(kTicksPerBar);
            L.lungeTelegraph = L.lungeTick - static_cast<double>(P.lungeTelegraphBeats) * kTicksPerBeat;
            L.lungeDir = dirOf(s.bodyAng[0]);
        });
        if (L.lungeArmed && L.lungeTelegraph >= 0.0 && m0_ < L.lungeTelegraph + 1e-6 && m1_ >= L.lungeTelegraph) {
            // The line runs from the head through where the nearest player stands as it shows (the host's call).
            const i32 near = nearestPlayer(s.body[0].x, s.body[0].y);
            if (near >= 0) bossDecide(kGeomLunge, s.body[0], normalizeOr(players_[near].pos - s.body[0], L.lungeDir), L.lungeTelegraph);
        }
        // The bare head: a ring of 16 on beats 2 and 4, alternate rings turned half a step.
        if (ph == 3 && d.phases[3].headBurst) {
            forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64 beat) {
                if (t < s.phaseTick || (imod(beat, 4) != 1 && imod(beat, 4) != 3)) return;
                const u32 n = bossScaledCount(d.phases[3].headBurst, bossBulletDensity(), 4u);
                const float turn = (imod(beat / 2, 2) == 1) ? 0.5f * kTau / static_cast<float>(n) : 0.0f;
                for (u32 j = 0; j < n; ++j)
                    spawnBullet(s.body[0], dirOf(turn + kTau * static_cast<float>(j) / static_cast<float>(n)) * d.bulletSpeed, tickAgeOf(t),
                                d.bulletLife);
                Event ev;
                ev.type = EventType::BossBurst;
                ev.b = static_cast<u8>(n);
                ev.x = s.body[0].x;
                ev.y = s.body[0].y;
                emit(ev);
            });
        }
    }
    placeLongwave(m1_);
}

}  // namespace rf::sim
