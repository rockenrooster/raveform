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
    charges_ = 1;
    targetHandle_ = 0;
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

    // ---- Dash (GAME_DESIGN 5): 220 wu in 0.12 s, invulnerable throughout; recharges in four beats; a
    // Dash within +-60 ms of a beat is Perfect (R7) and refunds half the recharge.
    const float ticksPerS = bpm * static_cast<float>(kTicksPerBeat) / 60.0f;
    // M5: Reserve (GAME_DESIGN 6.2) holds one more charge a stack; a charge comes back each recharge, one at a time.
    const u32 maxCharges = 1u + static_cast<u32>(state.loadout.mods[kReserve]);
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
        to.x = clampf(to.x, -kArenaHalfW + kPlayerVisualRadius, kArenaHalfW - kPlayerVisualRadius);
        to.y = clampf(to.y, -kArenaHalfH + kPlayerVisualRadius, kArenaHalfH - kPlayerVisualRadius);
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
    const float lx = kArenaHalfW - kPlayerVisualRadius, ly = kArenaHalfH - kPlayerVisualRadius;
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
    // the stick idle, the auto-aim assist fires at the nearest threat on a half-rate grid.
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
                if (use >= 0) {
                    targetHandle_ = e.handle[use];
                    // Lead the target by the shot's flight time.
                    const Vec2 at{e.x[use], e.y[use]};
                    const float t = length(at - p.pos) / kShotSpeed;
                    const Vec2 lead = at + Vec2{e.vx[use], e.vy[use]} * t;
                    p.aim = normalizeOr(lead - p.pos, lastAim_);
                    p.fire = 2;
                } else {
                    targetHandle_ = 0;
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
