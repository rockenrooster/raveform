#include "sim/bot.h"

#include <cmath>

#include "sim/tuning.h"

namespace rf::sim {

namespace {
// Potential-field weights (bot v0; tuned so the headless soak survives a few minutes of Track 1).
constexpr float kBodyReachWu = 520.0f;
constexpr float kBodyScaleWu = 110.0f;
constexpr float kBulletReachWu = 360.0f;
constexpr float kBulletMissWu = 70.0f;
constexpr float kShardReachWu = 420.0f;
constexpr float kWallReachWu = 450.0f;
constexpr float kKeepNearWu = 260.0f, kKeepFarWu = 520.0f;
constexpr float kAimRangeWu = 860.0f;
// M3 stage C: the boss is a target only within a shot's flight (1,050 wu), and with the crowd around the
// ship comes back within a working distance of it (the swarm's pull alone led it away for minutes).
constexpr float kBossAimWu = 1000.0f;
constexpr float kBossWorkFarWu = 750.0f;
// M3 stage C2: with the crowd in the frame something is always within 250 wu; in a boss fight the bot shoots
// a body only when it is this close (about to touch), else the boss.
constexpr float kBossFodderWu = 140.0f;
// M4: a downed teammate's beacon this near is worth going to (two frames wide).
constexpr float kReviveReachWu = 1600.0f;
// M5 part 3: how much the bot wants each upgrade (GAME_DESIGN 6.2-6.3 order) and Boost (6.4): the stream's strength and
// what reaches a boss highest, cells next, the crowd's tools and the economy lower.
constexpr i32 kBotUpgradeScore[kUpgradeCount] = {
    // Split, Double-Time, Pierce, Ricochet, Fork, Seeker, Heavy Step, Accent, Echo, Wave, Overcharge, Rear Guard
    10, 10, 6, 5, 6, 3, 5, 8, 8, 3, 9, 2,
    // Arc, Burn, Shatter, Gravity Rounds, Extra Cell, Regen, Afterimage, Reserve, Shockstep, Aegis, Last Light, Slipstream
    5, 5, 4, 3, 9, 10, 3, 4, 3, 6, 6, 3,
    // Magnet, Prospector, Nova Stock, Encore, Lucky Break, Collector, On-Beat Crit, Syncopation, Bar Line, Crescendo,
    // Kick Pulse, Afterparty
    4, 2, 6, 2, 3, 3, 9, 7, 4, 7, 4, 3,
    // Orbitals, Pulse Ring, Sentinel, Lancer, Minelayer, Storm Coil, Swarm, Singularity, Mirror, Comet, Sweeper, Flak
    9, 9, 9, 7, 3, 7, 7, 4, 7, 5, 6, 6};
constexpr i32 kBotBoostScore[kBoostCount] = {
    // Prism, Hyper, Supernova, Colossus, Time Stretch, Freeze Frame, Halo, Magnet Storm, Black Hole, Chain Storm, Chorus,
    // Beam, Ricochet Fever, Confetti, Fractal, Carousel
    8, 9, 6, 7, 6, 6, 8, 2, 4, 5, 8, 7, 5, 3, 6, 4};
}  // namespace

void Bot::reset(u64 seed, Policy policy) {
    rng_.reseed(seed ^ 0xB07B07B07ull);
    policy_ = policy;
    wander_ = rng_.range(0.0f, kTau);
    ticks_ = 0;
    dashLast_ = novaLast_ = false;
    anchor_ = Vec2{};
}

StickInput Bot::think(const World& w, u32 player, const PlayerController& controller, double musicTick) {
    ++ticks_;
    StickInput in;
    in.assist = true;
    const PlayerState& me = w.player(player);
    const PlayerPose& pose = controller.pose();
    if (!me.alive) return in;
    const Vec2 pos = pose.pos;
    const Enemies& e = w.enemies();
    const Bullets& b = w.bullets();

    // Aim: the nearest solid enemy, led by the shot's flight time; fire while one is in range.
    i32 target = -1;
    float targetD = kAimRangeWu * kAimRangeWu;
    for (u32 i = 0; i < e.count; ++i) {
        if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping) continue;
        const float dx = e.x[i] - pos.x, dy = e.y[i] - pos.y;
        const float d = dx * dx + dy * dy;
        if (d < targetD) {
            targetD = d;
            target = static_cast<i32>(i);
        }
    }
    if (target >= 0) {
        const Vec2 at{e.x[target], e.y[target]};
        const float t = length(at - pos) / kShotSpeed;
        in.aim = (at + Vec2{e.vx[target], e.vy[target]} * t) - pos;
        in.aimActive = lengthSq(in.aim) > 1e-6f;
    }
    // The boss (M3 stage B): unless something is close, shoot where damage counts. Through an open sector
    // (a dead pair's direction) at the core; else at the resting pair's nearer arm; else at any arm.
    const BossState& boss = w.boss();
    const bool bossFight = boss.active && !boss.dying && musicTick >= boss.fightTick;
    const bool kaleidos = boss.kind == BossKind::Kaleidos;
    Vec2 bossAimPoint{};
    bool openSector = false;
    // M5: the other fights are circles with a multiplier each (sim/boss.h BossPart): aim at the one where damage will count
    // when the shot arrives (the crest coming, the Voice about to sing), nearest first; keep their bodies in mind below.
    BossPart parts[kBossBodies + 4];
    const u32 partCount = kaleidos ? 0u : w.bossParts(parts, kBossBodies + 4);
    Vec2 bossNear = boss.pos;  // the fight's nearest body (the working distance and the dodging)
    float bossNearD = 1e30f;
    for (u32 k = 0; k < partCount; ++k) {
        const float d = lengthSq(parts[k].pos - pos);
        if (d < bossNearD) {
            bossNearD = d;
            bossNear = parts[k].pos;
        }
    }
    if (bossFight && !kaleidos && (target < 0 || targetD > kBossFodderWu * kBossFodderWu)) {
        float best = -1.0f;
        for (u32 k = 0; k < partCount; ++k) {
            const BossPart& q = parts[k];
            if (q.part == 1) continue;
            const float d = length(q.pos - pos);
            if (d > kBossAimWu) continue;
            const float flight = d / kShotSpeed;
            const u32 b = q.part == 3 ? 0u : 1u + q.index;
            const Vec2 vel = (boss.body[b] - boss.bodyPrev[b]) * (1.0f / kDt);
            const Vec2 at = q.pos + vel * flight;
            const float scale = w.bossDamageScale(q.part, q.index, musicTick + static_cast<double>(flight) * w.ticksPerSecond(),
                                                  normalizeOr(at - pos, Vec2{1.0f, 0.0f}));
            const float score = scale * 1000.0f - d * 0.1f;
            if (scale > 0.0f && score > best) {
                best = score;
                bossAimPoint = at;
            }
        }
        if (best > 0.0f) {
            in.aim = bossAimPoint - pos;
            in.aimActive = lengthSq(in.aim) > 1e-6f;
        }
    }
    if (bossFight && kaleidos && lengthSq(pos - boss.pos) < kBossAimWu * kBossAimWu && (target < 0 || targetD > kBossFodderWu * kBossFodderWu)) {
        const Vec2 rel = pos - boss.pos;
        const float myAngle = angleOf(rel);
        for (u32 k = 0; k < 6; ++k) {
            if (boss.alive & (1u << (k % 3))) continue;
            if (std::fabs(wrapAngle(myAngle - w.bossArmAngle(k))) < 0.35f) openSector = true;
        }
        if (openSector || boss.alive == 0) {
            bossAimPoint = boss.pos;
        } else {
            float best = 1e30f;
            const float spin = wrapAngle(boss.angle - boss.prevAngle) / kDt;  // the wheel's turn, rad/s
            for (u32 k = 0; k < 6; ++k) {
                const u32 pr = k % 3;
                if (!(boss.alive & (1u << pr))) continue;
                // Lead the turning arm by the shot's flight time.
                const Vec2 now = boss.pos + dirOf(w.bossArmAngle(k)) * 170.0f;
                const float flight = length(now - pos) / kShotSpeed;
                const Vec2 mid = boss.pos + dirOf(w.bossArmAngle(k) + spin * flight) * 170.0f;
                float d = lengthSq(mid - pos);
                if (!w.bossResting(pr, musicTick)) d *= 4.0f;  // armoured: a last resort
                if (d < best) {
                    best = d;
                    bossAimPoint = mid;
                }
            }
        }
        in.aim = bossAimPoint - pos;
        in.aimActive = lengthSq(in.aim) > 1e-6f;
    }

    if (policy_ == Policy::Stress) {
        // A slow circle (radius 250 wu, once every 12 s) around where it started: the load stays in view.
        if (ticks_ == 1) anchor_ = pos;
        const float a = kTau * static_cast<float>(ticks_) * kDt / 12.0f;
        const Vec2 goal = anchor_ + Vec2{std::cos(a) * 250.0f, std::sin(a) * 120.0f};
        const Vec2 d = goal - pos;
        const float l = length(d);
        in.move = l > 4.0f ? d * (std::fmin(l / 120.0f, 0.6f) / l) : Vec2{};
        return in;
    }

    // ---- Movement: a potential field.
    const bool careful = policy_ != Policy::Reckless;
    Vec2 force{};
    Vec2 swarm{};
    u32 swarmN = 0, close = 0;
    bool contactSoon = false;
    Vec2 escape{};
    for (u32 i = 0; i < e.count; ++i) {
        if (e.flags[i] & kFlagDead) continue;
        const Vec2 d = pos - Vec2{e.x[i], e.y[i]};
        const float dist = length(d);
        if (dist > 900.0f) continue;
        swarm = swarm + Vec2{e.x[i], e.y[i]};
        ++swarmN;
        if (!careful || dist > kBodyReachWu || dist < 1e-3f) continue;
        if (e.state[i] == kWarping) {
            // Do not stand where something is about to land.
            if (dist < 90.0f) force = force + d * (1.2f / dist);
            continue;
        }
        float weight = 1.0f;
        if (e.state[i] == kTelegraph || e.state[i] == kDash || e.state[i] == kCharge) {
            // A Ram's path: step sideways off its line.
            const Vec2 line{e.a[i], e.b[i]};
            const float along = -dot(d, line);
            if (along > 0.0f) {
                const Vec2 side = d + line * along;  // from the line to me
                const float off = length(side);
                if (off < 90.0f) force = force + normalizeOr(side, perpOf(line)) * (3.0f * (90.0f - off) / 90.0f);
            }
            weight = 2.0f;
        }
        const float k = kBodyScaleWu / dist;
        force = force + d * (weight * k * k / dist);
        if (dist < e.radius[i] + 45.0f) {
            ++close;
            contactSoon = true;
            escape = escape + d * (1.0f / dist);
        } else if (dist < 200.0f) {
            ++close;
        }
    }
    u32 bulletsNear = 0;
    for (u32 k = 0; k < b.count && careful; ++k) {
        const Vec2 rel = pos - Vec2{b.x[k], b.y[k]};
        if (std::fabs(rel.x) > kBulletReachWu || std::fabs(rel.y) > kBulletReachWu) continue;
        const Vec2 v{b.vx[k], b.vy[k]};
        const float v2 = lengthSq(v);
        if (v2 < 1.0f) continue;
        const float tca = dot(rel, v) / v2;  // seconds until closest approach
        if (lengthSq(rel) < 150.0f * 150.0f) ++bulletsNear;
        if (tca < 0.0f || tca > 1.4f) continue;
        const Vec2 miss = rel - v * tca;
        const float m = length(miss);
        if (m > kBulletMissWu) continue;
        const Vec2 away = m > 1e-3f ? miss * (1.0f / m) : normalizeOr(perpOf(v), Vec2{1.0f, 0.0f});
        force = force + away * (2.5f * (kBulletMissWu - m) / kBulletMissWu / (tca + 0.25f));
        if (tca < 0.16f && m < 16.0f) {
            contactSoon = true;
            escape = escape + away;
        }
    }
    // Shards: the nearest few pull.
    const Shards& s = w.shards();
    for (u32 k = 0; k < s.count; ++k) {
        const Vec2 d = Vec2{s.x[k], s.y[k]} - pos;
        const float dist = length(d);
        if (dist > kShardReachWu || dist < 1e-3f) continue;
        force = force + d * (0.35f * (1.0f - dist / kShardReachWu) / dist);
    }
    // The swarm: keep it at a working distance.
    if (swarmN) {
        const Vec2 c = swarm * (1.0f / static_cast<float>(swarmN));
        const Vec2 d = c - pos;
        const float dist = length(d);
        if (dist > kKeepFarWu) force = force + d * (0.6f / dist);
        if (dist < kKeepNearWu && dist > 1e-3f) force = force - d * (0.8f / dist);
    }
    // The boss: within a working distance during the fight (both bots).
    if (bossFight) {
        const Vec2 rel = pos - (kaleidos ? boss.pos : bossNear);
        const float dist = length(rel);
        if (dist > kBossWorkFarWu) force = force - rel * (0.9f * std::fmin((dist - kBossWorkFarWu) / 300.0f, 1.0f) / dist);
    }
    // M5: the other fights: keep off their bodies, out of every line they have shown, inside the Choir's Round when its cage
    // shows, and beside Longwave's crest.
    if (boss.active && !boss.dying && careful && !kaleidos) {
        // The bodies, where they are and where they will be in a fifth and two fifths of a second against the ship's own
        // course (Longwave's swing and the Voices' moves are fast); one about to touch is dashed away from, like a body.
        for (u32 k = 0; k < partCount; ++k) {
            if (boss.kind == BossKind::Hive && parts[k].part == 1) continue;  // the decoy is light
            const u32 bi = parts[k].part == 3 ? 0u : (boss.kind == BossKind::Hive ? 1u : 1u + parts[k].index);
            const Vec2 vel = bi < kBossBodies ? (boss.body[bi] - boss.bodyPrev[bi]) * (1.0f / kDt) : Vec2{};
            const float keep = parts[k].radius + 130.0f;
            for (u32 j = 0; j < 3; ++j) {
                const float ahead = 0.2f * static_cast<float>(j);
                const Vec2 d = pos - parts[k].pos + (pose.vel - vel) * ahead;
                const float dist = length(d);
                if (dist < keep && dist > 1e-3f) force = force + d * ((j == 0 ? 3.0f : 2.0f) * (keep - dist) / keep / dist);
            }
            const Vec2 d = pos - parts[k].pos;
            const float dist = length(d);
            if (dist < parts[k].radius + 50.0f && dist > 1e-3f) {
                contactSoon = true;
                escape = escape + d * (1.0f / dist);
            }
        }
        const auto offLine = [&](Vec2 from, Vec2 dir, float width) {
            const Vec2 rel = pos - from;
            const float along = dot(rel, dir);
            const Vec2 side = rel - dir * along;
            const float off = length(side);
            if (along > -60.0f && off < width) force = force + normalizeOr(side, perpOf(dir)) * (4.0f * (width - off) / width);
        };
        const MiniState& m = boss.mini;
        const LongwaveState& L = boss.lw;
        switch (boss.kind) {
            case BossKind::Longwave: {
                if (L.lungeArmed && musicTick >= L.lungeTelegraph - 96.0) offLine(boss.body[0], L.lungeDir, 140.0f);
                // Ride beside the nearest crest coming (rule 3: fly along with it), on the side the bot is on.
                float bestD = 1e30f;
                Vec2 goal{};
                bool any = false;
                const double ahead = musicTick + 0.5 * kTicksPerBeat;
                for (u32 k = 0; k < kBossParts; ++k) {
                    if (!(boss.alive & (1u << k)) || w.longwaveCrest(k, ahead) < 0) continue;
                    const Vec2 c = boss.body[1 + k];
                    const Vec2 n = perpOf(dirOf(boss.bodyAng[1 + k]));
                    const Vec2 g = c + n * (dot(pos - c, n) >= 0.0f ? 240.0f : -240.0f);
                    const float d = lengthSq(g - pos);
                    if (d < bestD) {
                        bestD = d;
                        goal = g;
                        any = true;
                    }
                }
                if (!any && boss.alive == 0) goal = boss.body[0] + normalizeOr(pos - boss.body[0], Vec2{1.0f, 0.0f}) * 380.0f;
                if (any || boss.alive == 0) {
                    const Vec2 d = goal - pos;
                    const float l = length(d);
                    if (l > 30.0f) force = force + d * (std::fmin(l / 200.0f, 1.2f) / l);
                }
                break;
            }
            case BossKind::Choir: {
                // Inside the Round, near its centre: every Voice in reach and none in the way (choir.md: the Round surrounds
                // the players). Outside it (a wall, the crowd), back in through the gap between the two nearest Voices.
                const Vec2 c = boss.ch.centre;
                const float dc = length(pos - c);
                const bool cage = w.choirCage(musicTick + kTicksPerBeat) != 0;
                const float want = cage ? 110.0f : 150.0f;
                if (boss.ch.phase < 3 && dc > want) {
                    Vec2 goal = c;
                    if (dc > boss.ch.radius - 40.0f) {
                        // The gap: the middle of the two live Voices nearest the line to the centre.
                        const float a = angleOf(pos - c);
                        float best = 10.0f;
                        for (u32 k = 0; k < ChoirData::kVoices; ++k) {
                            const u32 n = (k + 1) % ChoirData::kVoices;
                            if (!(boss.alive & (1u << k)) && !(boss.alive & (1u << n))) continue;
                            const Vec2 mid = (boss.body[1 + k] + boss.body[1 + n]) * 0.5f;
                            const float da = std::fabs(wrapAngle(angleOf(mid - c) - a));
                            if (da < best) {
                                best = da;
                                goal = c + normalizeOr(mid - c, Vec2{1.0f, 0.0f}) * (boss.ch.radius - 120.0f);
                            }
                        }
                    }
                    const Vec2 to = goal - pos;
                    const float l = length(to);
                    if (l > 1e-3f) force = force + to * ((cage ? 2.2f : 1.6f) * std::fmin((dc - want) / 120.0f, 1.0f) / l);
                }
                for (u32 k = 0; k < ChoirData::kVoices; ++k) {
                    float a = 0.0f;
                    if (w.choirHeld(k, musicTick + kTicksPerBeat, &a) == 0) continue;
                    // The held note sweeps 120 degrees: keep out of its whole fan.
                    const Vec2 from = boss.body[1 + k];
                    const float mid = boss.ch.heldAim[k];
                    const float off = wrapAngle(angleOf(pos - from) - mid);
                    if (std::fabs(off) < 1.25f) force = force + perpOf(dirOf(mid)) * (off >= 0.0f ? 2.5f : -2.5f);
                }
                break;
            }
            case BossKind::AlphaHound:
                if (m.state == 1 || m.state == 2) offLine(boss.body[0], m.lockDir, 140.0f);
                break;
            case BossKind::BatteringRam:
                if (m.state == 1 || m.state == 2) offLine(boss.body[0], m.lockDir, 170.0f);
                break;
            case BossKind::Hive:
                for (u32 h = 0; h < m.hives; ++h)
                    if (m.shape[h] == 1) offLine(m.shapeAt[h], m.shapeDir[h], 110.0f);
                break;
            default: break;
        }
    }
    // The boss: keep off its arms and core, step out of a telegraphed beam's line, and circle with the
    // wheel toward an open sector.
    if (boss.active && !boss.dying && careful && kaleidos) {
        const Vec2 rel = pos - boss.pos;
        const float dist = length(rel);
        if (dist > 1e-3f) {
            if (dist < 330.0f) force = force + rel * (2.5f * (330.0f - dist) / 330.0f / dist);
            for (u32 k = 0; k < 6; ++k) {
                const u32 pr = k % 3;
                if (!(boss.alive & (1u << pr))) continue;
                const Vec2 d = dirOf(w.bossArmAngle(k));
                const float along = dot(rel, d);
                const Vec2 side = rel - d * along;
                const float off = length(side);
                if (w.bossBeamState(pr, musicTick + 96.0) != 0 && along > 0.0f && off < 120.0f)
                    force = force + normalizeOr(side, perpOf(d)) * (4.0f * (120.0f - off) / 120.0f);
            }
            Vec2 tangent = perpOf(rel * (1.0f / dist));
            if (bossFight && !openSector && boss.alive != 0x7 && boss.alive != 0) {
                // Toward the nearest dead pair's direction.
                float best = 10.0f;
                for (u32 k = 0; k < 6; ++k) {
                    if (boss.alive & (1u << (k % 3))) continue;
                    const float da = wrapAngle(w.bossArmAngle(k) - angleOf(rel));
                    if (std::fabs(da) < std::fabs(best)) best = da;
                }
                if (best < 0.0f) tangent = tangent * -1.0f;
                force = force + tangent * 0.9f;
            } else {
                force = force + tangent * 0.3f;
            }
        }
    }
    // A Trio: go to the orb the bot likes best (a new Riff, then a weapon, then the rest) and stay on it until it
    // is taken (play test 1: an orb is held for two beats); a Boost lying near: fetch it.
    const TrioState& tr = w.trio(player);
    bool holding = false;
    Vec2 holdPull{};
    if (tr.open && tr.homing < 0 && tr.orbs) {
        u32 pick = 0;
        i32 bestScore = -1;
        for (u32 k = 0; k < tr.orbs; ++k) {
            const u8 u = tr.upgrade[k];
            // M5 part 3: with the whole pool (36 Mods, 12 Riffs) the bot picks as a player who knows them: a Fusion's gold
            // orb first, then by what each does for the fights (the stream's strength, a Riff that reaches the boss,
            // cells), a new Riff a little above its level-ups. Boosts in the orbs by the same idea.
            i32 score = isFusionOrb(u) ? 100 : (isBoostOrb(u) ? kBotBoostScore[boostOfOrb(u)] : kBotUpgradeScore[u]);
            // (GAME_DESIGN 6.1: the careful bot takes a Fusion's ingredient when the other is held)
            if (u < kUpgradeCount)
                for (u8 f = 0; f < kFusionCount; ++f) {
                    const FusionInfo& fi = fusionInfo(f);
                    if (!me.loadout.fused(f) && ((u == fi.a && me.loadout.level(fi.b) > 0) || (u == fi.b && me.loadout.level(fi.a) > 0))) score += 6;
                }
            if (score > bestScore) {
                bestScore = score;
                pick = k;
            }
        }
        const Vec2 d = tr.orbPos[pick] - pos;
        const float dist = length(d);
        const float ring = tuning::kTrioHoldRing * tuning::kTrioOrbWu * (isRiff(tr.upgrade[pick]) ? tuning::kTrioRiffScale : 1.0f);
        if (dist > 0.5f * ring) {
            if (dist > 1e-3f) force = force + d * (2.2f / dist);
        } else {
            // On it: hold still near its centre; the field's other pulls only nudge (the hold empties if it leaves).
            holding = true;
            holdPull = d * (3.0f / ring);
        }
    }
    // M4 co-op (GAME_DESIGN 10): a teammate's beacon within reach: go and stand in its ring until it revives.
    if (!holding) {
        for (u32 q = 0; q < w.playerCount(); ++q) {
            const PlayerState& o = w.player(q);
            if (q == player || !o.present || !o.down) continue;
            const Vec2 d = o.beacon - pos;
            const float dist = length(d);
            if (dist > kReviveReachWu) continue;
            if (dist > 0.5f * tuning::kReviveRingWu) {
                if (dist > 1e-3f) force = force + d * (2.4f / dist);
            } else {
                holding = true;
                holdPull = d * (3.0f / tuning::kReviveRingWu);
            }
            break;
        }
    }
    const BoostPickups& bp = w.boostPickups();
    for (u32 k = 0; k < bp.count; ++k) {
        const Vec2 d = Vec2{bp.x[k], bp.y[k]} - pos;
        const float dist = length(d);
        if (dist < 650.0f && dist > 1e-3f) force = force + d * (1.2f / dist);
    }
    // Play test 2: its own Cells while it is hurt (harder on the last cell).
    if (me.cells < me.cellCap) {
        const CellPickups& cp = w.cellPickups();
        for (u32 k = 0; k < cp.count; ++k) {
            if (cp.dead[k] || cp.owner[k] != player) continue;
            const Vec2 d = Vec2{cp.x[k], cp.y[k]} - pos;
            const float dist = length(d);
            if (dist < 650.0f && dist > 1e-3f) force = force + d * ((me.cells <= 1 ? 2.0f : 1.2f) / dist);
        }
    }
    // Walls.
    const auto wall = [&](float distToWall, Vec2 inward) {
        if (distToWall < kWallReachWu) force = force + inward * (2.0f * (kWallReachWu - distToWall) / kWallReachWu);
    };
    wall(pos.x + kArenaHalfW, Vec2{1.0f, 0.0f});
    wall(kArenaHalfW - pos.x, Vec2{-1.0f, 0.0f});
    wall(pos.y + kArenaHalfH, Vec2{0.0f, 1.0f});
    // M5: the Pen's walls, from inside (a shorter reach: the Pen is small).
    const PenState& pen = w.pen();
    if (pen.up && w.penInside(pos, 0.0f)) {
        const auto penWall = [&](float d, Vec2 inward) {
            if (d < 220.0f) force = force + inward * (1.6f * (220.0f - d) / 220.0f);
        };
        penWall(pos.x - (pen.centre.x - pen.half), Vec2{1.0f, 0.0f});
        penWall((pen.centre.x + pen.half) - pos.x, Vec2{-1.0f, 0.0f});
        penWall(pos.y - (pen.centre.y - pen.half), Vec2{0.0f, 1.0f});
        penWall((pen.centre.y + pen.half) - pos.y, Vec2{0.0f, -1.0f});
    }
    wall(kArenaHalfH - pos.y, Vec2{0.0f, -1.0f});
    // A slow wander so an empty arena still gets crossed.
    wander_ += rng_.range(-0.35f, 0.35f);
    force = force + dirOf(wander_) * 0.25f;
    // M5, the Choir (Unison to Fugue): inside the Round, nothing (the crowd, Shards, a dodge) takes the ship out across the
    // ring of Voices; the outward part of the pull fades out before the ring.
    if (careful && boss.active && !boss.dying && boss.kind == BossKind::Choir && boss.ch.phase < 3) {
        const Vec2 rel = pos - boss.ch.centre;
        const float dc = length(rel);
        const float room = boss.ch.radius - w.species().choir.voiceRadius - 60.0f - dc;
        if (dc > 1e-3f && room < 140.0f && dc < boss.ch.radius - 30.0f) {
            const Vec2 out = rel * (1.0f / dc);
            const float outward = dot(force, out);
            if (outward > 0.0f) force = force - out * (outward * (room <= 0.0f ? 1.3f : std::fmin(1.0f, 1.0f - room / 140.0f)));
        }
    }
    if (holding) force = force * 0.25f + holdPull;
    const float f = length(force);
    in.move = f > 0.05f ? force * (std::fmin(f, 1.0f) / f) : Vec2{};

    // ---- Dash out of an imminent hit; Nova when surrounded.
    bool dash = false;
    if (careful && contactSoon && controller.dashCharge(musicTick) >= 1.0f && !pose.dashing) {
        dash = true;
        in.move = normalizeOr(escape, normalizeOr(force, dirOf(wander_)));
    }
    in.dash = dash && !dashLast_;
    dashLast_ = dash;
    const bool nova = careful && me.novas > 0 && !me.novaPending &&
                      ((close >= 7 && me.novas >= 2) || bulletsNear >= 10 || (me.cells == 0 && contactSoon && !dash));
    in.nova = nova && !novaLast_;
    novaLast_ = nova;
    return in;
}

}  // namespace rf::sim
