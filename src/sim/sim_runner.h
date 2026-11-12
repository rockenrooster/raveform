#pragma once
// Steps a world with the bot on the simulated musical clock (TECH_SPEC 4: "simulated clock derived from
// sim time (headless runs, tests, benchmarks)"): one tick = 1/60 s = bpm x 96 / 3600 musical ticks. The
// bot plays through the controller exactly as a person would; when it dies it taps retry and the run
// starts again on the next bar line. The inputs of every tick can be recorded and replayed into a fresh
// world (the determinism test).

#include "sim/bot.h"
#include "sim/player_controller.h"
#include "sim/world.h"

namespace rf::sim {

class SimRunner {
public:
    SimRunner() = default;
    SimRunner(const SimRunner&) = delete;
    SimRunner& operator=(const SimRunner&) = delete;

    // `world` is owned by the caller (it is large) and must outlive the runner.
    void start(World* world, u64 seed, Spawner::Mode mode, Bot::Policy policy, float bpm, bool godMode = false);
    // One tick with the bot's input. Returns the input the world read (for recording).
    const TickInput& step();
    // One tick with a recorded input (no bot, no controller).
    void replay(const TickInput& in);

    World& world() { return *world_; }
    const PlayerController& controller() const { return controller_; }
    double music() const { return music_; }
    u32 deaths() const { return deaths_; }
    bool autoRetry = true;

private:
    World* world_ = nullptr;
    PlayerController controller_;
    Bot bot_;
    TickInput in_;
    double music_ = 0.0;
    float bpm_ = 120.0f;
    u32 deaths_ = 0;
    bool retryNext_ = false;
};

}  // namespace rf::sim
