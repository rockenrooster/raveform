#pragma once
// The local player's controller (TECH_SPEC 4: the local player is an input to the sim, not part of it).
// It integrates the ship every rendered frame with the real frame time and the freshest input, so
// movement, Dash and Nova respond on the frame of the input (R1); each sim tick then reads its pose.
// Headless runs, the bot and the tests step it once per tick with the tick's time, which keeps them
// deterministic. Input comes from any source through StickInput: touch sticks, a gamepad, keyboard and
// mouse, or the bot (sim/bot.h).

#include "sim/world.h"

namespace rf::sim {

struct StickInput {
    Vec2 move;               // magnitude 0..1 (dead zone already applied)
    Vec2 aim;                // the aim stick's direction (any length when aimActive)
    bool aimActive = false;  // the aim stick is deflected / the fire button held: fire on the full grid
    bool nova = false;       // held (the press is the edge)
    bool dash = false;       // held
    bool assist = true;      // GAME_DESIGN 2: auto-aim assist while the aim stick is idle (on by default)
};

class PlayerController {
public:
    void reset(Vec2 pos);
    // dt: seconds since the last update (real frame time, or the tick in headless runs). musicTick: the
    // heard musical position now; bpm: its tempo.
    void update(const StickInput& in, float dt, double musicTick, float bpm, const World& world, u32 index);
    const PlayerPose& pose() const { return pose_; }
    void clearEdges() {
        pose_.novaPressed = false;
        pose_.dashStarted = false;
        pose_.perfectDash = false;
    }
    // Dash charge for the HUD: 1 when ready, else how far the recharge has come.
    float dashCharge(double musicTick) const;
    u32 autoTarget() const { return targetHandle_; }  // auto-aim target (0: none)
    bool firing() const { return pose_.fire != 0; }

private:
    PlayerPose pose_;
    Vec2 lastAim_{1.0f, 0.0f};
    bool aimWasActive_ = false;
    float fireHoldS_ = 0.0f;    // firing continues this long along the last aim (GAME_DESIGN 2)
    bool novaHeld_ = false, dashHeld_ = false;
    float dashLeftS_ = 0.0f;
    Vec2 dashDir_;
    double dashReadyAt_ = -1e18;  // musical tick at which the Dash has recharged
    double dashStartedAt_ = 0.0;
    u32 targetHandle_ = 0;
};

}  // namespace rf::sim
