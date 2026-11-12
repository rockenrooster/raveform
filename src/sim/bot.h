#pragma once
// The bot player v0 (TECH_SPEC 7.3): it plays through the same input interface as a person (StickInput),
// headless (soak tests, the determinism test, benchmarks) and on screen (--bot). A potential field
// dodges bodies and bullets and drifts toward Shards; it aims at the nearest threat with a lead, dashes
// out of an imminent hit and spends a Nova when surrounded. Deterministic: it reads the world and its own
// random stream only. The Stress policy circles slowly and shoots (bench_stress keeps the ship alive);
// the Reckless one ignores danger (no dodging, Dash or Nova), so tests see hits, deaths and retries.

#include "game/rng.h"
#include "sim/player_controller.h"

namespace rf::sim {

class Bot {
public:
    enum class Policy : u8 { Play, Stress, Reckless };
    void setPolicy(Policy p) { policy_ = p; }  // --autoplay --reckless-after: the run ends on its own
    void reset(u64 seed, Policy policy);
    StickInput think(const World& world, u32 player, const PlayerController& controller, double musicTick);
    Policy policy() const { return policy_; }

private:
    Rng rng_{7};
    Policy policy_ = Policy::Play;
    float wander_ = 0.0f;
    u32 ticks_ = 0;
    bool dashLast_ = false, novaLast_ = false;
    Vec2 anchor_;
};

}  // namespace rf::sim
