#include "sim/player_controller.h"

#include <cmath>

#include "sim/tuning.h"

namespace rf::sim {

using namespace tuning;

void PlayerController::reset(Vec2 pos) {
    pose_ = PlayerPose{};
    pose_.present = true;
    pose_.pos = pos;
    lastAim_ = Vec2{1.0f, 0.0f};
    aimWasActive_ = false;
    fireHoldS_ = 0.0f;
    novaHeld_ = dashHeld_ = false;
    dashLeftS_ = 0.0f;
    dashReadyAt_ = -1e18;
    charges_ = static_cast<u8>(kDashCharges);
    shoveKnown_ = false;
    targetHandle_ = 0;
}

float shoveProgress(const PlayerState::Shove& s, double tick) {
    // How far through a shove: eased out (1 - (1 - u)^2), so its velocity starts at twice the mean and dies away to nothing.
    const double u = (tick - s.tick) / (s.ticks > 1.0 ? s.ticks : 1.0);
    if (u <= 0.0) return 0.0f;
    if (u >= 1.0) return 1.0f;
    const float v = 1.0f - static_cast<float>(u);
    return 1.0f - v * v;
}

void PlayerController::takeShoves(const PlayerState& state, double musicTick, PlayerPose& p, bool move) {
    // 3283249 (horizon.md rule 7): the ship rides each shove its whole way over its time, on top of its own steering (what
    // the shove has carried by musicTick, less what it had already). Not moving: only noted (a dead ship, a new controller).
    for (u32 k = 0; k < PlayerState::kShoves; ++k) {
        const PlayerState::Shove& sh = state.shoves[k];
        const float at = shoveProgress(sh, musicTick);
        if (!shoveKnown_ || sh.serial != shoveSerial_[k]) {
            shoveTaken_[k] = (shoveKnown_ && move) ? 0.0f : at;
            shoveSerial_[k] = sh.serial;
        }
        if (move && at > shoveTaken_[k]) p.pos = p.pos + sh.by * (at - shoveTaken_[k]);
        if (at > shoveTaken_[k] || !move) shoveTaken_[k] = at;
    }
    shoveKnown_ = true;
}

float PlayerController::dashCharge(double musicTick) const {
    if (charges_ > 0) return 1.0f;
    const double span = dashReadyAt_ - dashStartedAt_;
    return span > 0.0 ? static_cast<float>(clampf(static_cast<float>((musicTick - dashStartedAt_) / span), 0.0f, 1.0f)) : 1.0f;
}

void PlayerController::update(const StickInput& in, float dt, double musicTick, float bpm, const World& world, u32 index) {
    const PlayerState& state = world.player(index);
    PlayerPose& p = pose_;
    p.present = true;
    if (!state.alive) {
        takeShoves(state, musicTick, p, false);
        if (state.knocked && world.onStage()) p.pos = Vec2{};  // (M8 README.md 7: on the Stage a knocked out ship comes back at its centre)
        p.vel = Vec2{};
        p.fire = 0;
        p.dashing = false;
        dashLeftS_ = 0.0f;
        novaHeld_ = in.nova;
        dashHeld_ = in.dash;
        aimWasActive_ = false;
        return;
    }

    // ---- Nova: the press is the edge; the world decides whether there is one to spend (R6).
    if (in.nova && !novaHeld_) p.novaPressed = true;
    novaHeld_ = in.nova;

    // ---- Dash (GAME_DESIGN 5): 220 wu in 0.12 s, invulnerable throughout; recharges in two beats; a
    // Dash within +-60 ms of a beat is Perfect (R7) and refunds half the recharge.
    const float ticksPerS = bpm * static_cast<float>(kTicksPerBeat) / 60.0f;
    // Two charges (the owner, 2026-11-15: Reserve's, now the Dash's own); a charge comes back each recharge, one at a time.
    const u32 maxCharges = kDashCharges;
    if (musicTick >= dashReadyAt_ && charges_ < maxCharges) {
        ++charges_;
        if (charges_ < maxCharges) {
            dashStartedAt_ = dashReadyAt_ > 0.0 ? dashReadyAt_ : musicTick;
            dashReadyAt_ = dashStartedAt_ + kDashRechargeBeats * kTicksPerBeat;
        }
    }
    if (charges_ > maxCharges) charges_ = static_cast<u8>(maxCharges);
    if (in.dash && !dashHeld_ && dashLeftS_ <= 0.0f && charges_ > 0) {
        const Vec2 dir = lengthSq(in.move) > 0.04f ? normalizeOr(in.move, dirOf(p.heading)) : dirOf(p.heading);
        const double inBeat = std::fmod(musicTick, kTicksPerBeat);
        const double off = inBeat < kTicksPerBeat * 0.5 ? inBeat : kTicksPerBeat - inBeat;
        const bool perfect = static_cast<float>(off) / ticksPerS <= kPerfectDashS;
        dashDir_ = dir;
        dashLeftS_ = kDashS;
        if (charges_ >= maxCharges || musicTick >= dashReadyAt_) {  // no recharge under way: this one starts it
            dashStartedAt_ = musicTick;
            dashReadyAt_ = musicTick + (perfect ? 0.5 : 1.0) * kDashRechargeBeats * kTicksPerBeat;
        }
        --charges_;
        p.dashStarted = true;
        p.perfectDash = perfect;
        p.dashFrom = p.pos;
        Vec2 to = p.pos + dir * kDashWu;
        to.x = clampf(to.x, -world.halfW() + kPlayerVisualRadius, world.halfW() - kPlayerVisualRadius);
        to.y = clampf(to.y, -world.halfH() + kPlayerVisualRadius, world.halfH() - kPlayerVisualRadius);
        p.dashTo = to;
    }
    dashHeld_ = in.dash;

    // ---- Movement (GAME_DESIGN 5): 520 wu/s, near-instant acceleration, a slight glide on release.
    const Vec2 was = p.pos;
    if (dashLeftS_ > 0.0f) {
        const float step = dashLeftS_ < dt ? dashLeftS_ : dt;
        p.pos = p.pos + dashDir_ * (kDashWu / kDashS * step);
        p.vel = dashDir_ * (kDashWu / kDashS);
        dashLeftS_ -= dt;
        p.dashing = dashLeftS_ > 0.0f;
        if (!p.dashing) p.vel = dashDir_ * kPlayerSpeed;
    } else {
        p.dashing = false;
        float m = length(in.move);
        const Vec2 dir = m > 1e-4f ? in.move * (1.0f / m) : Vec2{};
        if (m > 1.0f) m = 1.0f;
        if (m > 1e-4f) {
            const float speed = kPlayerSpeed * (1.0f + kSlipstreamPerStack * static_cast<float>(state.loadout.mods[kSlipstream]));  // M5: Slipstream
            const Vec2 target = dir * (speed * m);
            const float k = 1.0f - std::exp(-dt / kAccelTau);
            p.vel = p.vel + (target - p.vel) * k;
        } else {
            p.vel = p.vel * std::exp(-dt / kGlideTau);
        }
        p.pos = p.pos + p.vel * dt;
    }
    // M5 part 4 (GAME_DESIGN 7.2): Wells pull the ship toward them (Event Horizon lets go of a ship it throws: 3283249).
    if (!p.dashing) p.pos = p.pos + world.wellPull(p.pos, !world.thrown(index, musicTick)) * dt;
    // M5 part 5 (design/arena.md): a Current carries the ship; a Bumper kicks it off unhurt.
    if (!p.dashing) p.pos = p.pos + world.currentFlow(p.pos) * dt;
    world.bumperBounce(p.pos, p.vel, kPlayerVisualRadius);
    // 3283249: Event Horizon's wave and throw carry the ship on top of its steering (pushes, never jumps).
    takeShoves(state, musicTick, p, true);
    // M5 (minibosses.md): the Pen's walls stop ships without hurting (inside stays inside, outside stays outside).
    {
        Vec2 to = p.pos;
        const u8 hit = world.penKeep(was, to, kPlayerVisualRadius);
        if (hit) {
            p.pos = to;
            if (hit & 1u) p.vel.x = 0.0f;
            if (hit & 2u) p.vel.y = 0.0f;
        }
    }
    // The walls (GAME_DESIGN 9).
    const float lx = world.halfW() - kPlayerVisualRadius, ly = world.halfH() - kPlayerVisualRadius;
    if (p.pos.x < -lx || p.pos.x > lx) {
        p.pos.x = clampf(p.pos.x, -lx, lx);
        p.vel.x = 0.0f;
    }
    if (p.pos.y < -ly || p.pos.y > ly) {
        p.pos.y = clampf(p.pos.y, -ly, ly);
        p.vel.y = 0.0f;
    }

    // ---- Fire (GAME_DESIGN 2, R2): the aim stick fires along its direction on every grid step; when the
    // right thumb leaves it (to press Nova or Dash) firing continues along the last aim for 400 ms; with
    // the stick idle, the auto-aim assist fires at the nearest threat at half rate (fire 2: every second step of the grid,
    // Double-Time included; the owner, 2026-11-14, 27077b6: play test 3 had put it on the full grid for a day), and in a fight
    // at the boss.
    if (in.aimActive && lengthSq(in.aim) > 1e-8f) {
        lastAim_ = normalizeOr(in.aim, lastAim_);
        p.aim = lastAim_;
        p.fire = 1;
        fireHoldS_ = kFireContinueS;
        aimWasActive_ = true;
        targetHandle_ = 0;
    } else {
        if (aimWasActive_) aimWasActive_ = false;
        fireHoldS_ -= dt;
        if (fireHoldS_ > 0.0f) {
            p.aim = lastAim_;
            p.fire = 1;
        } else {
            p.fire = 0;
            if (in.assist) {
                // The nearest solid enemy within range; the current target is kept while it stays within
                // kAutoAimKeep x the nearest one's distance (no flicker between neighbours).
                const Enemies& e = world.enemies();
                const float range2 = kAutoAimRangeWu * kAutoAimRangeWu;
                i32 nearest = -1;
                float nearestD = range2;
                for (u32 i = 0; i < e.count; ++i) {
                    if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping) continue;
                    const float dx = e.x[i] - p.pos.x, dy = e.y[i] - p.pos.y;
                    const float d = dx * dx + dy * dy;
                    if (d < nearestD) {
                        nearestD = d;
                        nearest = static_cast<i32>(i);
                    }
                }
                i32 use = nearest;
                const i32 held = targetHandle_ ? world.indexOf(targetHandle_) : -1;
                if (held >= 0 && nearest >= 0 && e.state[held] != kWarping) {
                    const float dx = e.x[held] - p.pos.x, dy = e.y[held] - p.pos.y;
                    const float d = dx * dx + dy * dy;
                    if (d < range2 && d <= nearestD * kAutoAimKeep * kAutoAimKeep) use = held;
                }
                // GAME_DESIGN 2 (play test 3): in a boss or mini-boss fight the boss comes first (its open weak point, else its
                // nearest part), unless an ordinary enemy is within 250 wu of the ship.
                Vec2 bossAt{};
                const bool enemyNear = nearest >= 0 && nearestD <= kAssistNearWu * kAssistNearWu;
                if (!enemyNear && world.assistBossAim(p.pos, musicTick, bossAt)) {
                    targetHandle_ = 0;
                    p.aim = normalizeOr(bossAt - p.pos, lastAim_);
                    p.fire = 2;  // (the assist's half rate)
                } else if (use >= 0) {
                    targetHandle_ = e.handle[use];
                    // Lead the target by the shot's flight time.
                    const Vec2 at{e.x[use], e.y[use]};
                    const float t = length(at - p.pos) / kShotSpeed;
                    const Vec2 lead = at + Vec2{e.vx[use], e.vy[use]} * t;
                    p.aim = normalizeOr(lead - p.pos, lastAim_);
                    p.fire = 2;  // the half rate (GAME_DESIGN 2, 27077b6)
                } else {
                    targetHandle_ = 0;
                }
                // Feedback (GAME_DESIGN 7.6, play test 3): the assist rests on every second bar, so the Echoes fall silent and
                // the Head opens for a player who never lifts a thumb. In Regeneration the second Echo plays two bars behind:
                // there it rests two bars of three (else the Head would never open for it).
                const BossState& bs = world.boss();
                if (p.fire && bs.active && !bs.dying && bs.kind == BossKind::Feedback && musicTick >= bs.fightTick) {
                    const i64 bar = static_cast<i64>(std::floor(musicTick / kTicksPerBar));
                    const bool rest = bs.fb.phase == 1 ? (bar % 3) != 0 : (bar & 1) != 0;
                    if (rest) p.fire = 0;
                }
            }
        }
    }

    // ---- The hull faces its course; standing still it turns to its aim.
    float want = p.heading;
    if (lengthSq(p.vel) > 40.0f * 40.0f) {
        want = angleOf(p.vel);
    } else if (p.fire) {
        want = angleOf(p.aim);
    }
    p.heading = turnToward(p.heading, want, kHeadingTurn * dt);
}

}  // namespace rf::sim
