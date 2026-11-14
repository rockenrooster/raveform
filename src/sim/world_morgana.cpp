// M5 part 6: Fata Morgana, the boss of Track 9 (design/bosses/morgana.md, .json). The safe place moves: a storm closes the frame
// and hurts on the kick (steps 0, 6 and 10: a ship outside the calm loses a cell); the oasis (260 wu) drifts along a figure of
// eight 900 wu wide at 110 wu/s and rests every fourth bar while she crosses it (x 2 over the water). Mirage: at every fourth bar
// line a second oasis 500 wu away, one of the two real (the seed); the mirage blinks out on the snares and is gone after two bars.
// Crossing: every second bar the oasis leaps 600 wu (shown dashed the bar before), the old one shrinking over the bar while a
// corridor of calm 160 wu wide joins the two; she lands dazzled, x 2 for a beat.
//
// Co-op: the drift, the rests, her orbit and the storm's kicks are functions of the song's clock; the mirage's side and which one
// is real, and the leaps' ways, are drawn from the run's seed and the song's bar; the phase follows the health on every device and
// acts only on bar lines (the mirage on the fourth, the first leap on an even one a bar after it is shown). Each device hurts its
// own players. No host decision.

#include <cmath>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;
using namespace prose;

namespace {
constexpr float kDegR = kTau / 360.0f;
constexpr double kBar = kTicksPerBar;
constexpr double kRestPeriod = kMgRestEvery * kTicksPerBar;

// The figure of eight (a lemniscate of Gerono): x = a sin q, y = a sin q cos q, 900 wu wide.
Vec2 figureAt(float q) {
    const float a = 0.5f * kMgFigureWu;
    return Vec2{a * std::sin(q), a * std::sin(q) * std::cos(q)};
}

// The rest bars' ticks in [0, y) of the song's ticks (the last bar of every four rests).
double restBefore(double y) {
    const double k = std::floor(y / kRestPeriod);
    const double m = y - k * kRestPeriod;
    const double in = m - (kRestPeriod - kBar);
    return k * kBar + (in < 0.0 ? 0.0 : (in > kBar ? kBar : in));
}

Vec2 clampArena(Vec2 p, float mx, float my) { return Vec2{clampf(p.x, -mx, mx), clampf(p.y, -my, my)}; }

const Vec2 kDirs8[8] = {{1.0f, 0.0f},  {0.7071f, 0.7071f},  {0.0f, 1.0f},  {-0.7071f, 0.7071f},
                        {-1.0f, 0.0f}, {-0.7071f, -0.7071f}, {0.0f, -1.0f}, {0.7071f, -0.7071f}};
}  // namespace

double World::mgSong(double t) const { return t + static_cast<double>(songShift_) * kBar; }

double World::mgEnd(double t) const {
    // The drift and the rests stop at the first leap.
    const MorganaState& g = boss_.mg;
    return g.crossTick >= 0.0 && t > g.crossTick ? g.crossTick : t;
}

bool World::morganaResting(double t) const {
    // The oasis rests for the last bar of every four (before the Crossing), and she crosses it.
    const BossState& s = boss_;
    if (t < s.fightTick || (s.mg.crossTick >= 0.0 && t >= s.mg.crossTick)) return false;
    const double y = mgSong(t);
    return y - std::floor(y / kRestPeriod) * kRestPeriod >= kRestPeriod - kBar;
}

Vec2 World::morganaDrift(double t) const {
    // The oasis on its figure: 110 wu/s of arc for every tick that is not a rest, from the fight's start to the first leap.
    const BossState& s = boss_;
    const MorganaState& g = s.mg;
    const double f = s.fightTick, e = mgEnd(t);
    const double moving = e > f ? (e - f) - (restBefore(mgSong(e)) - restBefore(mgSong(f))) : 0.0;
    const float tps = ticksPerSecond();
    const float total = g.arc[MorganaState::kArc];
    float arc = total > 0.0f ? std::fmod(kMgDriftSpeed * static_cast<float>(moving) / tps, total) : 0.0f;
    // The figure's parameter at that arc length (the table's steps, linear between them).
    u32 i = 0;
    while (i + 1 < MorganaState::kArc && g.arc[i + 1] <= arc) ++i;
    const float span = g.arc[i + 1] - g.arc[i];
    const float u = span > 1e-4f ? (arc - g.arc[i]) / span : 0.0f;
    const float q = (static_cast<float>(i) + u) * kTau / static_cast<float>(MorganaState::kArc);
    return clampArena(g.home + g.offset + figureAt(q), kArenaHalfW - kMgMarginWu, kArenaHalfH - kMgMarginWu);
}

Vec2 World::morganaOasis(double t) const {
    const MorganaState& g = boss_.mg;
    if (g.crossTick < 0.0 || t < g.crossTick) return morganaDrift(t);
    if (g.leapTick < 0.0) return morganaDrift(g.crossTick);
    return t >= g.leapTick ? g.at : g.from;
}

bool World::morganaMirage(double t, Vec2& at, bool& lit) const {
    // Shown for two bars from its bar line; out for an eighth on every snare (steps 4-5 and 12-13).
    const MorganaState& g = boss_.mg;
    if (g.mirageTick < 0.0 || t < g.mirageTick || t >= g.mirageTick + kMgMirageBars * kBar) return false;
    at = morganaOasis(t) + g.mirage;
    const u32 step = static_cast<u32>(std::floor((t - std::floor(t / kBar) * kBar) / kTicksPerStep)) % 16u;
    lit = !(step == kMgSnareSteps[0] || step == kMgSnareSteps[0] + 1 || step == kMgSnareSteps[1] || step == kMgSnareSteps[1] + 1);
    return true;
}

bool World::morganaCalm(Vec2 at, double t) const {
    // The real oasis; the mirage while it is shown (a ship in it is in the storm once it is gone); after a leap, the old one
    // shrinking over the bar and the corridor between the two.
    const BossState& s = boss_;
    const MorganaState& g = s.mg;
    const float r = species_->morgana.oasisRadius;
    if (lengthSq(at - morganaOasis(t)) <= r * r) return true;
    Vec2 m;
    bool lit = false;
    if (morganaMirage(t, m, lit) && lengthSq(at - m) <= r * r) return true;
    if (g.leapTick >= 0.0 && t >= g.leapTick && t < g.leapTick + kBar) {
        const float u = static_cast<float>((t - g.leapTick) / kBar);
        const float ro = r * (1.0f - u);
        if (lengthSq(at - g.from) <= ro * ro) return true;
        const Vec2 d = g.at - g.from;
        const float l2 = lengthSq(d);
        const float w = l2 > 1e-6f ? clampf(dot(at - g.from, d) / l2, 0.0f, 1.0f) : 0.0f;
        const Vec2 near = g.from + d * w;
        if (lengthSq(at - near) <= 0.25f * kMgCorridorWu * kMgCorridorWu) return true;
    }
    return false;
}

Vec2 World::morganaBody(double t) const {
    // She circles the oasis 70 wu outside its edge, a turn every four bars (not counting the rests); in a rest bar she crosses
    // it from side to side through its middle, and comes out on the far side (half a turn on).
    const BossState& s = boss_;
    const MorganaState& g = s.mg;
    const double f = s.fightTick;
    const double tt = t > f ? t : f;
    const double e = mgEnd(tt);
    const double rests = restBefore(mgSong(e)) - restBefore(mgSong(f));
    const double turns = std::floor(mgSong(e) / kRestPeriod) - std::floor(mgSong(f) / kRestPeriod);
    const double orbit = (tt - f) - rests;
    const float phi = g.orbit0 + static_cast<float>(kTau * orbit / (kMgOrbitBars * kBar) + kPi * turns);
    const Vec2 c = morganaOasis(tt);
    const float rad = species_->morgana.oasisRadius + kMgOrbitOut;
    if (morganaResting(tt)) {
        const double y = mgSong(tt);
        const float u = static_cast<float>((y - std::floor(y / kRestPeriod) * kRestPeriod - (kRestPeriod - kBar)) / kBar);
        return c + dirOf(phi) * (rad * (1.0f - 2.0f * u));
    }
    return c + dirOf(phi) * rad;
}

float World::morganaScale(double t) const {
    // x 2 while she is over the resting water; x 2 for the beat after each leap; x 1 at other times.
    const BossState& s = boss_;
    const MorganaState& g = s.mg;
    if (g.leapTick >= 0.0 && t >= g.leapTick && t < g.leapTick + kTicksPerBeat) return kMgOne;
    if (morganaResting(t)) {
        const float r = species_->morgana.oasisRadius;
        if (lengthSq(morganaBody(t) - morganaOasis(t)) < r * r) return kMgOne;
    }
    return 1.0f;
}

void World::enterMorgana(Vec2 centre) {
    BossState& s = boss_;
    const MorganaData& d = species_->morgana;
    s.fightTick = s.enterTick + kTicksPerBar;  // the storm closes in over a bar; the oasis is where the players are
    s.phaseTick = s.fightTick;
    s.coreMax = d.health * s.scale;
    s.coreHp = s.coreMax;
    s.alive = 0;
    s.mg = MorganaState{};
    MorganaState& g = s.mg;
    // The figure's arc length, kArc steps of its parameter (eight sub-steps each).
    g.arc[0] = 0.0f;
    Vec2 last = figureAt(0.0f);
    for (u32 i = 1; i <= MorganaState::kArc; ++i) {
        float l = 0.0f;
        for (u32 k = 1; k <= 8; ++k) {
            const Vec2 p = figureAt(kTau * (static_cast<float>(i - 1) + static_cast<float>(k) / 8.0f) / static_cast<float>(MorganaState::kArc));
            l += length(p - last);
            last = p;
        }
        g.arc[i] = g.arc[i - 1] + l;
    }
    const float a = 0.5f * kMgFigureWu;
    g.home = clampArena(centre, kArenaHalfW - kMgMarginWu - a, kArenaHalfH - kMgMarginWu - 0.5f * a);
    g.orbit0 = -0.5f * kPi;  // (she starts above the oasis)
    s.pos = s.prevPos = morganaBody(s.enterTick);
    s.body[0] = s.bodyPrev[0] = s.pos;
}

Vec2 World::mgLeapTo(Vec2 from, i64 songBar) const {
    // The next oasis: 600 wu from this one, the way drawn from the seed and the song's bar (the first of eight that keeps it clear
    // of the walls).
    const u32 h = hash32(hash32(static_cast<u32>(seed_), static_cast<u32>(seed_ >> 32) ^ 0x3A6Au), static_cast<u32>(songBar) * 0x9E37u + 0x1EAFu);
    for (u32 k = 0; k < 8; ++k) {
        const Vec2 p = from + kDirs8[(h + k * 3u) % 8u] * kMgLeapWu;
        if (std::fabs(p.x) <= kArenaHalfW - kMgMarginWu && std::fabs(p.y) <= kArenaHalfH - kMgMarginWu) return p;
    }
    return from - normalizeOr(from, Vec2{1.0f, 0.0f}) * kMgLeapWu;
}

void World::updateMorgana(float dt) {
    (void)dt;
    BossState& s = boss_;
    MorganaState& g = s.mg;
    const MorganaData& d = species_->morgana;
    s.prevPos = s.pos;
    s.bodyPrev[0] = s.body[0];
    if (s.dying) return;  // the storm drops at once
    // The phase by health (the Crossing is never undone: its leaps are scheduled).
    const float share = s.coreMax > 0.0f ? s.coreHp / s.coreMax : 0.0f;
    u8 phase = share > kMgMirageAt ? 0 : (share > kMgCrossingAt ? 1 : 2);
    if (g.phase == 2) phase = 2;
    if (phase != g.phase && m1_ >= s.fightTick) {
        g.phase = phase;
        s.phase = phase;
        s.phaseTick = m1_;
        if (phase == 2 && g.crossTick < 0.0) {
            // The first leap: the first even bar line of the song a bar or more after the next bar line (its place is shown for
            // the bar before).
            double leap = (std::floor(m1_ / kBar) + 2.0) * kBar;
            const i64 sb = static_cast<i64>(std::floor(mgSong(leap) / kBar + 0.5));
            if (sb % static_cast<i64>(kMgLeapEvery)) leap += kBar;
            g.crossTick = g.nextTick = leap;
            g.nextShown = false;
        }
    }
    s.pos = morganaBody(m1_);
    s.body[0] = s.pos;
    if (m1_ < s.fightTick) return;
    const float r = d.oasisRadius;
    // Bar lines: a leap; the next one shown; the mirage; the storm's whoosh for a ship outside.
    forCrossings(m0_, m1_, kBar, [&](double t, i64) {
        const i64 sb = static_cast<i64>(std::floor(mgSong(t) / kBar + 0.5));
        if (g.nextTick >= 0.0 && t >= g.nextTick - 1.0) {
            g.from = morganaOasis(t - 1.0);
            if (!g.nextShown) g.next = mgLeapTo(g.from, sb);
            g.at = g.next;
            g.leapTick = t;
            g.nextTick = t + kMgLeapEvery * kBar;
            g.nextShown = false;
            emitCue(BossCue::MgLeap, t, 0, 0.0f, g.at);
        }
        if (g.nextTick >= 0.0 && !g.nextShown && t >= g.nextTick - kBar - 1.0) {
            const Vec2 base = g.leapTick >= 0.0 ? g.at : morganaDrift(g.nextTick);
            g.next = mgLeapTo(base, sb + 1);
            g.nextShown = true;
        }
        if (g.phase == 1 && g.crossTick < 0.0 && ((sb % static_cast<i64>(kMgRestEvery)) + kMgRestEvery) % kMgRestEvery == 0) {
            // A second oasis 500 wu from the first, one of them real: the way and which from the seed and the song's bar.
            const u32 h = hash32(hash32(static_cast<u32>(seed_), static_cast<u32>(seed_ >> 32) ^ 0x3171u), static_cast<u32>(sb) * 0x9E37u + 0x6A11u);
            const float a = 0.5f * kMgFigureWu;
            Vec2 dir = kDirs8[h % 8u];
            for (u32 k = 0; k < 8; ++k) {
                const Vec2 cand = kDirs8[(h + k * 3u) % 8u];
                const Vec2 home = g.home + g.offset + cand * kMgMirageWu;
                if (std::fabs(home.x) <= kArenaHalfW - kMgMarginWu - a && std::fabs(home.y) <= kArenaHalfH - kMgMarginWu - 0.5f * a) {
                    dir = cand;
                    break;
                }
            }
            if ((h >> 8) & 1u) {
                g.offset = g.offset + dir * kMgMirageWu;  // the new one is real: the old place is the mirage
                g.mirage = dir * -kMgMirageWu;
            } else {
                g.mirage = dir * kMgMirageWu;
            }
            g.mirageTick = t;
        }
        bool outside = false;
        for (u32 p = 0; p < playerCount_; ++p) {
            const PlayerState& pl = players_[p];
            if (pl.present && pl.alive && isLocal(p) && !morganaCalm(pl.pos, t)) outside = true;
        }
        if (outside) emitCue(BossCue::MgWhoosh, t, 1, 0.0f, s.pos);
    });
    // Sixteenths: the storm and her fans on the kick; the mirage's blink on the snare; the leap's run of chirps.
    forCrossings(m0_, m1_, kTicksPerStep, [&](double t, i64 stepIndex) {
        const u32 step = static_cast<u32>(((stepIndex % 16) + 16) % 16);
        if (step == kMgKickSteps[0] || step == kMgKickSteps[1] || step == kMgKickSteps[2]) {
            bool outside = false;
            for (u32 p = 0; p < playerCount_; ++p) {
                PlayerState& pl = players_[p];
                if (!pl.present || !pl.alive || !isLocal(p) || morganaCalm(pl.pos, t)) continue;
                outside = true;
                if (!pl.invulnerable() && !rules_.godMode && !sheltered(p)) bossHitPlayer(p, pl.pos, true);
            }
            emitCue(BossCue::MgStorm, t, outside ? 1 : 0, 0.0f, s.pos);
            if (!morganaResting(t)) {
                // A fan of five into the calm at the nearest player.
                const Vec2 at = morganaBody(t);
                const i32 np = nearestPlayer(at.x, at.y);
                const float aim = np >= 0 ? angleOf(players_[np].pos - at) : 0.0f;
                const float age = tickAgeOf(t);
                const u32 m = bossScaledCount(d.fanBullets, bossBulletDensity() * loopShooters_, 2);
                for (u32 j = 0; j < m; ++j) {
                    const float off = m > 1 ? (static_cast<float>(j) / static_cast<float>(m - 1) - 0.5f) * kMgFanSpreadDeg * kDegR : 0.0f;
                    spawnBullet(at + dirOf(aim + off) * d.bodyRadius, dirOf(aim + off) * d.bulletSpeed, age, kMgBulletLife);
                }
            }
        }
        Vec2 mp;
        bool lit = false;
        if ((step == kMgSnareSteps[0] || step == kMgSnareSteps[1]) && morganaMirage(t, mp, lit)) emitCue(BossCue::MgBlink, t, 0, 0.0f, mp);
        if (g.leapTick >= 0.0) {
            const double k = std::floor((t - g.leapTick) / kTicksPerStep + 0.5);
            if (k >= 1.0 && k < 5.0) emitCue(BossCue::MgLeap, t, static_cast<u8>(k), 0.0f, g.at);
        }
    });
    // Beats: a bell while she is over the water.
    forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64) {
        if (morganaResting(t) && lengthSq(morganaBody(t) - morganaOasis(t)) < r * r) emitCue(BossCue::MgBell, t, 0, 0.0f, morganaBody(t));
    });
}

}  // namespace rf::sim
