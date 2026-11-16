// M5 part 6: Cadence, the boss of Track 7 (design/bosses/cadence.md, .json). The chord progression: four pillars on the corners
// of a square (I, V, vi, IV clockwise from the top left); the one whose chord the band is playing is silent and open, the
// other three fire on the half-time kick and snare. Modulation turns the square a quarter the other way every fourth bar line;
// Inversion turns the rule over (the sounding pillar alone fires; the other three are open).
//
// Co-op: everything follows the song's clock (the bar's chord, the beats, the turns); the square's centre follows the players'
// centre, kept where the host's is by the heartbeat (bossSyncPoint). The phase follows the health on every device. No new
// decision of the host's.

#include <cmath>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;
using namespace prose;

namespace {
constexpr float kDegR = kTau / 360.0f;
}  // namespace

u32 World::cadenceChord(double t) const {
    // The bar's chord (0 I, 1 V, 2 vi, 3 IV): the song's bar, one chord a bar.
    const i64 bar = static_cast<i64>(std::floor(t / kTicksPerBar)) + static_cast<i64>(songShift_);
    return static_cast<u32>(((bar % 4) + 4) % 4);
}

Vec2 World::cadencePillar(u32 k, double t) const {
    // Pillar k on its corner: clockwise from the top left, the square turned by the Modulation.
    const BossState& s = boss_;
    const CadenceData& d = species_->cadence;
    float rot = s.cd.rot;
    if (s.cd.turnTick >= 0.0) {
        const float u = clampf(static_cast<float>((t - s.cd.turnTick) / kTicksPerBeat), 0.0f, 1.0f);
        rot = s.cd.rotFrom + (s.cd.rotTo - s.cd.rotFrom) * u;
    }
    const float a = rot - 0.75f * kPi + 0.5f * kPi * static_cast<float>(k);
    return s.cd.centre + dirOf(a) * (0.5f * d.squareSide * 1.41421356f);
}

float World::cadenceScale(u32 k, double t) const {
    // Progression and Modulation: the bar's pillar is open (x 2 on its One), the others are closed (armour: a quarter, GAME_DESIGN
    // 7.6; the page says they cannot be hurt). Inversion: the sounding pillar is closed, the other three open.
    // (cadence.json numbers.damageScale since f32a441; Inversion: the quarter is the sounding pillar's, closed there)
    const BossState& s = boss_;
    const CadenceData& d = species_->cadence;
    const bool sounding = k == cadenceChord(t);
    const bool one = t - std::floor(t / kTicksPerBar) * kTicksPerBar < kTicksPerBeat;
    if (s.cd.phase < 2) return sounding ? (one ? d.dmgOne : d.dmgOpen) : d.dmgClosed;
    return sounding ? d.dmgClosed : d.dmgOpen;
}

void World::enterCadence(Vec2 centre) {
    BossState& s = boss_;
    const CadenceData& d = species_->cadence;
    s.fightTick = s.enterTick + kTicksPerBar;  // the square draws itself over its first bar
    s.phaseTick = s.fightTick;
    s.coreMax = d.health * s.scale;
    s.coreHp = s.coreMax;
    s.alive = 0xF;
    s.cd = CadenceState{};
    const float m = 0.5f * d.squareSide + d.pillarRadius + 300.0f;
    s.cd.centre = Vec2{clampf(centre.x, -kArenaHalfW + m, kArenaHalfW - m), clampf(centre.y, -kArenaHalfH + m, kArenaHalfH - m)};
    s.pos = s.prevPos = s.cd.centre;
    for (u32 k = 0; k < 4; ++k) s.body[1 + k] = s.bodyPrev[1 + k] = cadencePillar(k, s.enterTick);
}

void World::updateCadence(float dt) {
    BossState& s = boss_;
    CadenceState& c = s.cd;
    const CadenceData& d = species_->cadence;
    for (u32 k = 0; k < 4; ++k) s.bodyPrev[1 + k] = s.body[1 + k];
    s.prevPos = s.pos;
    if (s.dying) {
        // The progression resolves: the four burst in the song's order on the four beats of the last bar.
        forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64) {
            if (s.payoutTick < 0.0 || t < s.payoutTick - kTicksPerBar - 1.0 || t > s.payoutTick - 1.0) return;
            const u32 k = static_cast<u32>(std::floor((t - (s.payoutTick - kTicksPerBar)) / kTicksPerBeat + 0.5)) % 4u;
            emitCue(BossCue::CdBurst, t, static_cast<u8>(k), 0.0f, s.body[1 + k]);
        });
        return;
    }
    // The centre follows the players' centre at 40 wu/s (kept inside the arena).
    const Vec2 to = playersCentre() - c.centre;
    const float l = length(to);
    if (l > 1.0f) c.centre = c.centre + to * (std::fmin(kCdFollow * dt, l) / l);
    const float m = 0.5f * d.squareSide + d.pillarRadius + 300.0f;
    c.centre = Vec2{clampf(c.centre.x, -kArenaHalfW + m, kArenaHalfW - m), clampf(c.centre.y, -kArenaHalfH + m, kArenaHalfH - m)};
    s.pos = c.centre;
    // The phase by health.
    const float share = s.coreMax > 0.0f ? s.coreHp / s.coreMax : 0.0f;
    const u8 phase = share > kCdModulationAt ? 0 : (share > kCdInversionAt ? 1 : 2);
    if (phase != c.phase && m1_ >= s.fightTick) {
        c.phase = phase;
        s.phase = phase;
        s.phaseTick = m1_;
        if (phase == 2) emitCue(BossCue::CdInversion, m1_, 0, 0.0f, c.centre);
    }
    // The turn ends.
    if (c.turnTick >= 0.0 && m1_ >= c.turnTick + kTicksPerBeat) {
        c.rot = c.rotTo;
        c.turnTick = -1.0;
    }
    for (u32 k = 0; k < 4; ++k) s.body[1 + k] = cadencePillar(k, m1_);
    if (m1_ < s.fightTick) return;
    // Bar lines: the bar's chord sounds; in the Modulation, every fourth bar line (as the song comes back to I) the square
    // turns a quarter the other way over a beat. The review's fixes (H9, 2026-11-15): the turn is announced a bar ahead, on the
    // bar line before (where the song plays IV): its reversed cymbal is sent then and ends on the turn, and the view draws each
    // pillar's sweep as a dashed tell over that bar. An announced turn happens even if the phase has changed since (the warning
    // is kept); a turn is never made unannounced.
    forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
        const u32 chord = cadenceChord(t + 1.0);
        emitCue(BossCue::CdChord, t, static_cast<u8>(chord), 0.0f, cadencePillar(chord, t));
        if (c.turnWarn >= 0.0 && std::fabs(c.turnWarn - t) < 0.5) {
            c.rotFrom = c.rot;
            c.rotTo = c.rot - 0.5f * kPi;
            c.turnTick = t;
            c.turnWarn = -1.0;
        }
        if (c.phase == 1 && c.turnWarn < 0.0 && cadenceChord(t + kTicksPerBar + 1.0) == 0) {
            c.turnWarn = t + kTicksPerBar;
            emitCue(BossCue::CdTurn, c.turnWarn, 0, 0.0f, c.centre);
        }
    });
    // Beats 1 and 3 (the half-time kick and snare).
    forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64 beatIndex) {
        const u32 beat = static_cast<u32>(((beatIndex % 4) + 4) % 4);
        if (beat != 0 && beat != 2) return;
        const u32 chord = cadenceChord(t + 1.0);
        const float age = tickAgeOf(t);
        if (c.phase < 2) {
            // The three that are not sounding: a fan of five at the nearest player (and, in the Modulation, a second of seven on
            // the snare).
            for (u32 k = 0; k < 4; ++k) {
                if (k == chord) continue;
                const Vec2 p = cadencePillar(k, t);
                const i32 np = nearestPlayer(p.x, p.y);
                const float aim = np >= 0 ? angleOf(players_[np].pos - p) : 0.0f;
                const auto fan = [&](u32 n, float spreadDeg) {
                    const u32 m = bossCount(n, 2u, beatIndex / 2, k, loopShooters_);  // (0449eb3: the Track's count, the fan thinned evenly)
                    for (u32 j = 0; j < m; ++j) {
                        const float off = m > 1 ? (static_cast<float>(j) / static_cast<float>(m - 1) - 0.5f) * spreadDeg * kDegR : 0.0f;
                        spawnBullet(p + dirOf(aim + off) * d.pillarRadius, dirOf(aim + off) * d.bulletSpeed, age, d.bulletLife);
                    }
                };
                fan(d.fanBullets, d.fanSpreadDeg);
                if (c.phase == 1 && beat == 2) fan(kCdSnareFan, d.fanSpreadDeg * static_cast<float>(kCdSnareFan) / static_cast<float>(d.fanBullets));
                emitCue(BossCue::CdFan, t, static_cast<u8>(k), 0.0f, p);
            }
        } else {
            // Inversion: the sounding pillar alone fires, a rosette of sixteen on the One and twelve on three.
            const Vec2 p = cadencePillar(chord, t);
            const u32 n = bossCount(beat == 0 ? kCdRosetteOne : kCdRosetteThree, 4u, beatIndex / 2, 0u, loopShooters_);  // (0449eb3)
            const float a0 = static_cast<float>(hash32(static_cast<u32>(t), 0xCADu) % 360u) * kDegR;
            for (u32 j = 0; j < n; ++j) {
                const float a = a0 + kTau * static_cast<float>(j) / static_cast<float>(n);
                spawnBullet(p + dirOf(a) * d.pillarRadius, dirOf(a) * d.bulletSpeed, age, d.bulletLife);
            }
            emitCue(BossCue::CdFan, t, static_cast<u8>(chord), 0.0f, p);
        }
    });
}

}  // namespace rf::sim
