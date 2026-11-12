#include "sim/sim_runner.h"

namespace rf::sim {

void SimRunner::start(World* world, u64 seed, Spawner::Mode mode, Bot::Policy policy, float bpm, bool godMode) {
    world_ = world;
    bpm_ = bpm;
    music_ = 0.0;
    deaths_ = 0;
    retryNext_ = false;
    world_->spawner().mode = mode;
    world_->rules() = Rules{};
    world_->rules().godMode = godMode;
    world_->rules().novas = policy != Bot::Policy::Stress;
    world_->resetRun(seed, music_, 1);
    controller_.reset(world_->player(0).pos);
    bot_.reset(seed, policy);
}

const TickInput& SimRunner::step() {
    World& w = *world_;
    const double ticksPerTick = static_cast<double>(bpm_) * kTicksPerBeat / 60.0 * static_cast<double>(kDt);
    const double m0 = music_, m1 = music_ + ticksPerTick;
    const StickInput stick = bot_.think(w, 0, controller_, m0);
    controller_.update(stick, kDt, m1, bpm_, w, 0);
    in_ = TickInput{};
    in_.music0 = m0;
    in_.music1 = m1;
    in_.bpm = bpm_;
    in_.players[0] = controller_.pose();
    in_.retry = retryNext_;
    retryNext_ = false;
    w.tick(in_);
    controller_.clearEdges();
    for (u32 k = 0; k < w.eventCount(); ++k) {
        const Event& ev = w.events()[k];
        if (ev.type == EventType::Death) {
            ++deaths_;
            if (autoRetry) retryNext_ = true;  // the bot taps retry at once; the run restarts on the next bar
        } else if (ev.type == EventType::Restart) {
            controller_.reset(w.player(0).pos);
        }
    }
    music_ = m1;
    return in_;
}

void SimRunner::replay(const TickInput& in) {
    world_->tick(in);
    music_ = in.music1;
}

}  // namespace rf::sim
