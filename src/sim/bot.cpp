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
    Vec2 bossAimPoint{};
    bool openSector = false;
    if (bossFight && lengthSq(pos - boss.pos) < kBossAimWu * kBossAimWu && (target < 0 || targetD > kBossFodderWu * kBossFodderWu)) {
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
        if (e.state[i] == kTelegraph || e.state[i] == kDash) {
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
        const Vec2 rel = pos - boss.pos;
        const float dist = length(rel);
        if (dist > kBossWorkFarWu) force = force - rel * (0.9f * std::fmin((dist - kBossWorkFarWu) / 300.0f, 1.0f) / dist);
    }
    // The boss: keep off its arms and core, step out of a telegraphed beam's line, and circle with the
    // wheel toward an open sector.
    if (boss.active && !boss.dying && careful) {
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
            const i32 score = (isRiff(u) && me.loadout.level(u) == 0) ? 3 : (upgradeInfo(u).weapon ? 2 : 1);
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
    const BoostPickups& bp = w.boostPickups();
    for (u32 k = 0; k < bp.count; ++k) {
        const Vec2 d = Vec2{bp.x[k], bp.y[k]} - pos;
        const float dist = length(d);
        if (dist < 650.0f && dist > 1e-3f) force = force + d * (1.2f / dist);
    }
    // Walls.
    const auto wall = [&](float distToWall, Vec2 inward) {
        if (distToWall < kWallReachWu) force = force + inward * (2.0f * (kWallReachWu - distToWall) / kWallReachWu);
    };
    wall(pos.x + kArenaHalfW, Vec2{1.0f, 0.0f});
    wall(kArenaHalfW - pos.x, Vec2{-1.0f, 0.0f});
    wall(pos.y + kArenaHalfH, Vec2{0.0f, 1.0f});
    wall(kArenaHalfH - pos.y, Vec2{0.0f, -1.0f});
    // A slow wander so an empty arena still gets crossed.
    wander_ += rng_.range(-0.35f, 0.35f);
    force = force + dirOf(wander_) * 0.25f;
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
