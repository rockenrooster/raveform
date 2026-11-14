// M5 part 6: Kaleidos, Reprise, the boss of Track 10 (design/bosses/reprise.md, .json). The first boss comes back as the first
// theme does: Kaleidos a quarter larger, six arms, each one playing a boss the player has already beaten (Kaleidos, Longwave,
// the Choir, Event Horizon, Polyrhythm, Feedback, in the wheel's order).
//
// Reprise (100-70%): Kaleidos's own fight on the two-step's hits (one pair resting each bar as Kaleidos's Bloom), the arms
// armoured, the core x 1 and x 2 on the One. Medley (70-20%): the core closes; the arms solo in turn round the wheel, two bars
// each; each arm has a sixth of the phase's health, the soloist alone open (the others and the closed core take a quarter, the
// core's on the soloist); a dead arm's solo is skipped. Last Light (20-0%): the bare core fires First Light's arpeggio, a
// bullet a sixteenth, 15 degrees round a semitone above the bar's lowest, the figure turning 45 degrees a bar.
//
// Co-op: the wheel's turn, the fans, the solos' order and timing and the last pattern follow the song's clock and the arms
// alive (the host decides an arm's death, as every part's); the wheel's place is the heartbeat's sync point. What reads where
// the players are (the wave's way, the volleys' aim, the Echo's player, the rings' first gap) is each device's own reading of
// the nearest player, as Kaleidos's and Cadence's aims are. No host decision.

#include <cmath>
#include <initializer_list>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;
using namespace prose;

namespace {
constexpr float kDegR = kTau / 360.0f;
constexpr float kArmStep = kTau / 6.0f;
constexpr u32 kWaveBody = 9;   // body[9 + j]: the wave's segments (part 2, index 8 + j: the bots read body[1 + index])
constexpr u32 kEchoBody = 17;  // body[17]: Feedback's Echo
constexpr u8 kLongwaveArm = 1, kHorizonArm = 3;
}  // namespace

float World::repriseArmAngle(u32 k, double t) const {
    const BossState& s = boss_;
    const double turns = t > s.fightTick ? (t - s.fightTick) / (kRpTurnBars * kTicksPerBar) : 0.0;
    return s.angle0 + static_cast<float>(kTau * turns) + static_cast<float>(k) * kArmStep;
}

Vec2 World::repriseArmTip(u32 k) const { return boss_.pos + dirOf(repriseArmAngle(k, m1_)) * species_->reprise.armLength; }

bool World::repriseWaveOut(double t) const {
    // Longwave's arm has let go: its two bars, while it lives.
    const BossState& s = boss_;
    const RepriseState& r = s.rp;
    return s.kind == BossKind::Reprise && r.phase == 1 && r.solo == kLongwaveArm && (s.alive & (1u << kLongwaveArm)) && r.soloTick >= 0.0 &&
           t >= r.soloTick && t < r.soloTick + kRpSoloBars * kTicksPerBar;
}

Vec2 World::repriseWaveSeg(u32 j, double t) const {
    // Segment j: across the wave's way, 150 wu apart; the line sets out from the wheel and travels 1,800 wu over the two bars;
    // each segment swings along the way, a swing a beat, the swing running across the line.
    const RepriseState& r = boss_.rp;
    const float u = clampf(static_cast<float>((t - r.soloTick) / (kRpSoloBars * kTicksPerBar)), 0.0f, 1.0f);
    const Vec2 side{-r.waveDir.y, r.waveDir.x};
    const float beats = static_cast<float>((t - r.soloTick) / kTicksPerBeat);
    const float swing = std::sin(kTau * beats - 0.8f * static_cast<float>(j));
    return r.waveFrom + r.waveDir * (u * kRpWaveTravel + kRpWaveAmp * swing) +
           side * ((static_cast<float>(j) - 0.5f * static_cast<float>(kRpWaveSegments - 1)) * kRpWaveSpacing);
}

bool World::repriseCrest(u32 j, double t) const {
    const RepriseState& r = boss_.rp;
    const float beats = static_cast<float>((t - r.soloTick) / kTicksPerBeat);
    return std::sin(kTau * beats - 0.8f * static_cast<float>(j)) > kRpCrestAt;
}

bool World::repriseHorizonOn() const {
    const BossState& s = boss_;
    const RepriseState& r = s.rp;
    return s.active && !s.dying && s.kind == BossKind::Reprise && r.phase == 1 && r.solo == kHorizonArm && (s.alive & (1u << kHorizonArm)) &&
           r.soloTick >= 0.0 && m1_ >= r.soloTick && m1_ < r.soloTick + kRpSoloBars * kTicksPerBar;
}

float World::repriseScale(u8 part, u8 index, double t) const {
    // The core: x 1, x 2 on the One (Reprise and Last Light); closed in the Medley (a quarter, on the soloist's health). The
    // arms: white-edged in the Reprise (a quarter); in the Medley the soloist x 1, the others a quarter; the wave's crests x 1,
    // the rest of it a quarter.
    const RepriseState& r = boss_.rp;
    const RepriseData& d = species_->reprise;
    const bool one = t - std::floor(t / kTicksPerBar) * kTicksPerBar < kTicksPerBeat;
    if (part == 3) return r.phase == 1 ? d.dmgClosedCore : (one ? kRpOne : 1.0f);
    if (part != 2) return 0.0f;
    if (index >= 8) return repriseCrest(index - 8u, t) ? 1.0f : d.dmgWaveOff;
    if (r.phase == 1 && index == r.solo) return 1.0f;
    return r.phase == 0 ? d.dmgArmEarly : d.dmgArmNotSolo;
}

void World::enterReprise(Vec2 centre) {
    BossState& s = boss_;
    const RepriseData& d = species_->reprise;
    const BossData& k = species_->boss;
    s.fightTick = s.enterTick + kTicksPerBar;  // the wheel assembles over a bar, as Kaleidos's
    s.phaseTick = s.fightTick;
    s.coreMax = d.health * s.scale;
    s.coreHp = s.coreMax;
    s.alive = 0x3Fu;
    for (u32 j = 0; j < 6; ++j) s.partHp[j] = 0.0f;  // (the arms' shares are dealt as the Medley begins)
    s.rp = RepriseState{};
    s.angle0 = -0.5f * kPi;
    const float m = d.armLength * kRpScale + 300.0f;
    s.pos = s.prevPos = Vec2{clampf(centre.x, -kArenaHalfW + m, kArenaHalfW - m), clampf(centre.y, -kArenaHalfH + m, kArenaHalfH - m)};
    s.angle = s.prevAngle = s.angle0;
    s.body[0] = s.bodyPrev[0] = s.pos;
    for (u32 j = 0; j < 6; ++j) s.body[1 + j] = s.bodyPrev[1 + j] = s.pos + dirOf(repriseArmAngle(j, s.enterTick)) * (0.7f * d.armLength);
    (void)k;
}

void World::repriseDamage(u8 part, u8 index, float dmg, u8 owner) {
    // Where a hit's damage goes (damageBossFinal; the claimant's multiplier applied). Reprise and Last Light: the core's
    // health. Medley: an arm's sixth (a wave segment's: Longwave's arm; the closed core's: the soloist's); the core's health is
    // the floor of Last Light plus the arms' left.
    BossState& s = boss_;
    RepriseState& r = s.rp;
    const bool client = role_ == NetRole::Client;
    if (r.phase != 1) {
        s.coreHp -= dmg;
        if (s.coreHp <= 0.0f) {
            if (client) s.coreHp = 0.0f;
            else killBoss(owner);
        }
        return;
    }
    u8 arm = index >= 8 ? kLongwaveArm : index;
    if (part == 3) arm = r.solo;
    if (arm >= 6 || !(s.alive & (1u << arm))) return;
    s.partHp[arm] -= dmg;
    if (s.partHp[arm] < 0.0f) s.partHp[arm] = 0.0f;
    float left = 0.0f;
    for (u32 k = 0; k < 6; ++k)
        if (s.alive & (1u << k)) left += s.partHp[k];
    s.coreHp = kRpLastAt * s.coreMax + left;
    if (!client && s.partHp[arm] <= 0.0f) killBossPart(arm, owner);
}

void World::reprisePartDied(u8 index, Event& ev) {
    // An arm shot off: its tune is gone for good (its solo skipped from then on); the last one opens Last Light.
    BossState& s = boss_;
    ev.f = repriseArmAngle(index, m1_);
    ev.x = repriseArmTip(index).x;
    ev.y = repriseArmTip(index).y;
    float left = 0.0f;
    for (u32 k = 0; k < 6; ++k)
        if (s.alive & (1u << k)) left += s.partHp[k];
    s.coreHp = kRpLastAt * s.coreMax + left;
}

bool World::repriseHits(Vec2 at, Vec2& from) const {
    // Feedback's Echo is a body (it is not a target).
    const BossState& s = boss_;
    if (!s.rp.echoOn) return false;
    const float r = species_->feedback.echoRadius + kPlayerHitRadius;
    if (lengthSq(at - s.body[kEchoBody]) > r * r) return false;
    from = s.body[kEchoBody];
    return true;
}

u32 World::repriseParts(BossPart* out, u32 cap) const {
    const BossState& s = boss_;
    const RepriseData& d = species_->reprise;
    u32 n = 0;
    const auto add = [&](Vec2 p, float r, u8 part, u8 index) {
        if (n < cap) out[n++] = BossPart{p, r, part, index};
    };
    if (m1_ < s.fightTick) return 0;
    add(s.pos, d.coreRadius, 3, 0);
    const bool wave = repriseWaveOut(m1_);
    for (u32 k = 0; k < 6; ++k) {
        if (!(s.alive & (1u << k)) || (wave && k == kLongwaveArm)) continue;
        const Vec2 dv = dirOf(s.angle + static_cast<float>(k) * kArmStep);
        for (float u : {0.45f, 0.7f, 0.95f}) add(s.pos + dv * (u * d.armLength), kRpArmCircleR, 2, static_cast<u8>(k));
    }
    if (wave)
        for (u32 j = 0; j < kRpWaveSegments; ++j) add(s.body[kWaveBody + j], species_->longwave.segRadius, 2, static_cast<u8>(8 + j));
    return n;
}

void World::updateReprise(float dt) {
    BossState& s = boss_;
    RepriseState& r = s.rp;
    const RepriseData& d = species_->reprise;
    const BossData& kd = species_->boss;
    if (s.dying) return;
    // The wheel turns once in 5 1/3 bars and keeps 675-975 wu from the nearest player at 45 wu/s (the owner's 1.5x, 2026-11-14:
    // once in eight bars, 450-650 at 30, as Kaleidos does, until then).
    s.angle = repriseArmAngle(0, m1_);
    const i32 near = nearestPlayer(s.pos.x, s.pos.y);
    if (near >= 0) {
        const Vec2 to = players_[near].pos - s.pos;
        const float l = length(to);
        if (l > 1e-3f) {
            const Vec2 u = to * (1.0f / l);
            if (l < kRpKeepNear) s.pos = s.pos - u * (kRpDriftSpeed * dt);
            if (l > kRpKeepFar) s.pos = s.pos + u * (kRpDriftSpeed * dt);
        }
        const float m = d.armLength * kRpScale + 100.0f;
        s.pos = Vec2{clampf(s.pos.x, -kArenaHalfW + m, kArenaHalfW - m), clampf(s.pos.y, -kArenaHalfH + m, kArenaHalfH - m)};
    }
    s.body[0] = s.pos;
    s.bodyAng[0] = s.angle;
    for (u32 k = 0; k < 6; ++k) s.body[1 + k] = s.pos + dirOf(s.angle + static_cast<float>(k) * kArmStep) * (0.7f * d.armLength);
    if (m1_ < s.fightTick) return;
    // reprise.md "Sound" (the owner, 2026-11-14: "more epic ... audio wise"): every bar's One booms; every fourth rings the gong.
    forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64 bar) {
        if (t < s.fightTick) return;
        emitCue(BossCue::RpBoom, t, 0, 0.0f, s.pos);
        if (bar % static_cast<i64>(kRpGongBars) == 0) emitCue(BossCue::RpGong, t, 0, 0.0f, s.pos);
    });
    // The phase: the Medley once the core is down to 70% (each arm a sixth of the phase's health); Last Light once every arm is
    // gone. Each change rings the gong.
    if (r.phase == 0 && s.coreHp <= kRpMedleyAt * s.coreMax) {
        r.phase = 1;
        s.phase = 1;
        s.phaseTick = m1_;
        const float share = (s.coreHp - kRpLastAt * s.coreMax) / 6.0f;
        s.partMax = share > 0.0f ? share : 1.0f;
        for (u32 k = 0; k < 6; ++k) s.partHp[k] = (s.alive & (1u << k)) ? s.partMax : 0.0f;
        emitCue(BossCue::RpGong, m1_, 1, 0.0f, s.pos);
    }
    if (r.phase == 1 && (s.alive & 0x3Fu) == 0) {
        r.phase = 2;
        s.phase = 2;
        s.phaseTick = m1_;
        r.lastTick = (std::floor(m1_ / kTicksPerBar) + 1.0) * kTicksPerBar;
        r.figure0 = s.angle;
        r.echoOn = false;
        s.coreHp = std::fmin(s.coreHp, kRpLastAt * s.coreMax);
        emitCue(BossCue::RpGong, m1_, 2, 0.0f, s.pos);
    }
    const float density = bossBulletDensity() * loopShooters_;
    const auto room = [&]() { return bullets_.count < kRpBulletCap; };
    const auto fan = [&](Vec2 from, float aim, u32 count, float spreadDeg, float speed, float life, double t) {
        const u32 n = bossScaledCount(count, density, 1u);
        const float age = tickAgeOf(t);
        for (u32 j = 0; j < n && room(); ++j) {
            const float off = n > 1 ? (static_cast<float>(j) / static_cast<float>(n - 1) - 0.5f) * spreadDeg * kDegR : 0.0f;
            spawnBullet(from, dirOf(aim + off) * speed, age, life);
        }
    };
    const auto tipAt = [&](u32 k, double t) { return s.pos + dirOf(repriseArmAngle(k, t)) * d.armLength; };
    const auto aimAt = [&](Vec2 from) {
        const i32 np = nearestPlayer(from.x, from.y);
        return np >= 0 ? angleOf(players_[np].pos - from) : 0.0f;
    };
    // ---- Medley: the solos, two bars each round the wheel (a dead arm skipped; its solo ends with it).
    if (r.phase == 1) {
        forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
            const bool over = r.solo == 0xFF || t >= r.soloTick + kRpSoloBars * kTicksPerBar - 1.0 || !(s.alive & (1u << r.solo));
            if (!over || (s.alive & 0x3Fu) == 0) return;
            u8 next = r.solo == 0xFF ? 0 : static_cast<u8>((r.solo + 1u) % 6u);
            for (u32 k = 0; k < 6 && !(s.alive & (1u << next)); ++k) next = static_cast<u8>((next + 1u) % 6u);
            r.solo = next;
            r.soloTick = t;
            r.echoOn = false;
            const Vec2 tip = tipAt(next, t);
            const i32 np = nearestPlayer(s.pos.x, s.pos.y);
            const Vec2 toP = np >= 0 ? players_[np].pos - s.pos : dirOf(repriseArmAngle(next, t));
            switch (next) {
                case kLongwaveArm:
                    r.waveFrom = s.pos;
                    r.waveDir = normalizeOr(toP, Vec2{1.0f, 0.0f});
                    break;
                case kHorizonArm: emitCue(BossCue::RpSub, t, 0, 0.0f, s.pos); break;
                case 4: r.polyGap0 = angleOf(toP); break;
                case 5:
                    r.echoPlayer = static_cast<u8>(np >= 0 ? np : 0);
                    r.echoOn = true;
                    break;
                default: break;
            }
            (void)tip;
        });
    }
    // ---- Each sixteenth: the Reprise's fans on the two-step's hits; the solos' work; Last Light's arpeggio.
    forCrossings(m0_, m1_, kTicksPerStep, [&](double t, i64 stepIndex) {
        const u32 step = static_cast<u32>(((stepIndex % 16) + 16) % 16);
        if (r.phase == 0) {
            // Kaleidos's own fight: mirrored fans of five from each pair's two arms (one pair resting each bar, A B C in turn).
            if (step != kRpHitSteps[0] && step != kRpHitSteps[1] && step != kRpHitSteps[2] && step != kRpHitSteps[3]) return;
            const u32 rest = bossFightBar(t) % 3u;
            for (u32 k = 0; k < 6; ++k) {
                if (k % 3u == rest) continue;
                const float a = repriseArmAngle(k, t);
                fan(s.pos + dirOf(a) * d.armLength, a, kRpFan, kd.phases[0].spreadDeg * static_cast<float>(kRpFan - 1), kd.bulletSpeed, kd.bulletLife, t);
            }
            return;
        }
        if (r.phase == 2) {
            // First Light's arpeggio from the bare core: the bar's chord (the song's bar), a bullet each sixteenth.
            if (r.lastTick < 0.0 || t < r.lastTick - 0.5) return;
            const i64 bar = static_cast<i64>(std::floor(t / kTicksPerBar + 1e-9));
            const u32 chord = static_cast<u32>((((bar + static_cast<i64>(songShift_)) % 4) + 4) % 4);
            const double bars = std::floor((t - r.lastTick) / kTicksPerBar + 1e-9);
            const float a = r.figure0 + static_cast<float>(bars) * kRpArpBarDeg * kDegR + static_cast<float>(d.arpSemis[chord][step]) * kRpArpDegPerSemi * kDegR;
            if (room()) spawnBullet(s.pos + dirOf(a) * d.coreRadius, dirOf(a) * kRpArpSpeed, tickAgeOf(t), kd.bulletLife);
            emitCue(BossCue::RpArp, t, 0, static_cast<float>(d.arpMidi[chord][step]), s.pos);
            return;
        }
        if (r.solo >= 6 || !(s.alive & (1u << r.solo)) || t < r.soloTick) return;
        const double inSolo = t - r.soloTick;
        const u32 soloBar = static_cast<u32>(std::floor(inSolo / kTicksPerBar + 1e-9));
        if (soloBar >= kRpSoloBars) return;
        switch (r.solo) {
            case 0: {
                // Kaleidos's arm: its mirrored fans on the eighths (the soloist and its opposite arm).
                if (step % 2u) break;
                for (u32 k : {0u, 3u}) {
                    const float a = repriseArmAngle(k, t);
                    fan(tipAt(k, t), a, kRpFan, kd.phases[0].spreadDeg * static_cast<float>(kRpFan - 1), kd.bulletSpeed, kd.bulletLife, t);
                }
                emitCue(BossCue::RpBell, t, 0, 0.0f, tipAt(0, t));
                break;
            }
            case kLongwaveArm:
                if (step % 4u == 0) emitCue(BossCue::RpString, t, static_cast<u8>(step / 4u), 0.0f, repriseWaveSeg(3, t));
                break;
            case 2: {
                // The Choir's arm: a volley on the One, answered on three by the arm opposite.
                if (step != 0 && step != 8) break;
                const u32 k = step == 0 ? 2u : 5u;
                const Vec2 from = tipAt(k, t);
                fan(from, aimAt(from), kRpVolley, kRpVolleySpreadDeg, kRpVolleySpeed, kd.bulletLife, t);
                emitCue(BossCue::RpPad, t, step == 0 ? 0 : 1, 0.0f, from);
                break;
            }
            case kHorizonArm:
                // Event Horizon's arm: the pull (below and wellPull); a ring of twenty-four on its second bar line.
                if (step == 0 && soloBar == 1) {
                    const u32 n = bossScaledCount(kRpRing, density, 4u);
                    for (u32 j = 0; j < n && room(); ++j) {
                        const float a = s.angle + kTau * static_cast<float>(j) / static_cast<float>(n);
                        spawnBullet(s.pos + dirOf(a) * d.coreRadius, dirOf(a) * kd.bulletSpeed, tickAgeOf(t), kd.bulletLife);
                    }
                    emitCue(BossCue::RpSub, t, 1, 0.0f, s.pos);
                }
                break;
            case 4: {
                // Polyrhythm's arm: two rings each One, a gap each; the "three" ring's gap 120 degrees on a bar, the "four"
                // ring's 90, both on the way to the nearest player on its solo's first One (the gaps meet there).
                if (step == 0) {
                    for (u32 q = 0; q < 2; ++q) {
                        const float gap = r.polyGap0 + static_cast<float>(soloBar) * (q == 0 ? 120.0f : 90.0f) * kDegR;
                        const float speed = q == 0 ? kRpThreeSpeed : kRpFourSpeed;
                        const u32 n = bossScaledCount(kRpRing, density, 8u);
                        for (u32 j = 0; j < n && room(); ++j) {
                            const float a = gap + kTau * (static_cast<float>(j) + 0.5f) / static_cast<float>(n);
                            if (std::fabs(wrapAngle(a - gap)) < 0.5f * kRpRingGapDeg * kDegR) continue;
                            spawnBullet(s.pos + dirOf(a) * d.coreRadius, dirOf(a) * speed, tickAgeOf(t), kd.bulletLife);
                        }
                    }
                }
                if (step % 4u == 0) emitCue(BossCue::RpTom, t, 1, 7.0f, s.pos);  // (the four pulses: a fifth up)
                break;
            }
            default: break;
        }
    });
    // Polyrhythm's arm: its three pulses a bar against the four (the root).
    if (r.phase == 1 && r.solo == 4 && (s.alive & (1u << 4)))
        forCrossings(m0_, m1_, kTicksPerBar / 3.0, [&](double t, i64) {
            if (t >= r.soloTick - 0.5 && t < r.soloTick + kRpSoloBars * kTicksPerBar - 0.5) emitCue(BossCue::RpTom, t, 0, 0.0f, s.pos);
        });
    // ---- The wave's segments (Longwave's arm let go).
    if (repriseWaveOut(m1_))
        for (u32 j = 0; j < kRpWaveSegments; ++j) s.body[kWaveBody + j] = repriseWaveSeg(j, m1_);
    // ---- Event Horizon's arm: bullets drawn toward the wheel at 200 wu/s (the ships: wellPull); those at the core are gone.
    if (repriseHorizonOn()) {
        for (u32 b = 0; b < bullets_.count; ++b) {
            if (bullets_.dead[b]) continue;
            const Vec2 to = s.pos - Vec2{bullets_.x[b], bullets_.y[b]};
            const float l = length(to);
            if (l < d.coreRadius * 0.5f) {
                bullets_.dead[b] = 1;
                continue;
            }
            bullets_.x[b] += to.x / l * kRpPullWu * dt;
            bullets_.y[b] += to.y / l * kRpPullWu * dt;
        }
    }
    // ---- Feedback's arm: an Echo of the nearest player plays back that player's last bar from the far side of the wheel.
    if (r.echoOn && r.solo == 5 && (s.alive & (1u << 5)) && m1_ < r.soloTick + kRpSoloBars * kTicksPerBar) {
        const FeedbackData& fd = species_->feedback;
        const u32 p = r.echoPlayer < playerCount_ ? r.echoPlayer : 0u;
        Vec2 rel, aim;
        bool alive = false;
        if (players_[p].present && feedbackPose(p, m1_ - kTicksPerBar, rel, aim, alive) && alive) {
            const Vec2 at = s.pos - rel;  // (mirrored through the wheel)
            s.body[kEchoBody] = at;
            s.bodyAng[kEchoBody] = angleOf(aim * -1.0f);
            const FeedbackTape::Lane& l = tape_.lanes[p];
            const float tps = ticksPerSecond();
            bool sounded = false;
            for (u32 k = 0; k < l.shotCount; ++k) {
                const FeedbackTape::Shot& q = l.shots[(l.shotHead + FeedbackTape::kShots - 1 - k) % FeedbackTape::kShots];
                const double when = q.tick + kTicksPerBar;
                if (when <= m0_ - kTicksPerBar) break;
                if (when <= m0_ || when > m1_ || bullets_.count >= kRpBulletCap) continue;
                spawnEchoShot(at - q.off, q.dir * -fd.shotSpeed, static_cast<float>((m1_ - when) / tps), fd.shotLife, static_cast<u8>(p));
                if (!sounded) {
                    sounded = true;
                    emitCue(BossCue::RpEcho, when, static_cast<u8>(p), 0.0f, at);
                }
            }
        } else {
            s.body[kEchoBody] = s.pos + dirOf(s.angle + 0.5f * kPi) * (2.0f * d.armLength);
        }
    } else {
        r.echoOn = false;
    }
}

}  // namespace rf::sim
