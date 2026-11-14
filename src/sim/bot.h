#pragma once
// The bot player v0 (TECH_SPEC 7.3): it plays through the same input interface as a person (StickInput),
// headless (soak tests, the determinism test, benchmarks) and on screen (--bot). A potential field
// dodges bodies and bullets and drifts toward Shards; it aims at the nearest threat with a lead, dashes
// out of an imminent hit and spends a Nova when surrounded. Deterministic: it reads the world and its own
// random stream only. The Stress policy circles slowly and shoots (bench_stress keeps the ship alive);
// the Reckless one ignores danger (no dodging, Dash or Nova), so tests see hits, deaths and retries. The Novice
// (GAME_DESIGN 7.6, M5) stands in for a player's first week: since play test 3 it never aims and fires only through
// the auto-aim assist (GAME_DESIGN 2), reacts to threats 250 ms late, never rides a wave or threads a gap, dashes out
// of half the hits it sees coming, Novas at one cell, and takes a Trio's first orb half the time. The Assist policy
// (play test 3) is the careful bot's movement with the assist's fire: aim is never what ends its run.

#include "game/rng.h"
#include "sim/player_controller.h"

namespace rf::sim {

class Bot {
public:
    enum class Policy : u8 { Play, Stress, Reckless, Novice, Assist };
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
    // The novice (GAME_DESIGN 7.6).
    bool novContact_ = false, novDash_ = false;  // a hit seen coming, and whether it dashes out of this one
    double novTrio_ = -1.0;                       // the Trio it chose for (its open tick) and the orb
    u32 novPick_ = 0;
    static constexpr u32 kNoviceDelayTicks = 15;  // tuning::kNoviceReactS at 60 Hz
    Vec2 novMove_[kNoviceDelayTicks] = {};        // its steering, a quarter of a second behind
    u32 novMoveAt_ = 0;
};

}  // namespace rf::sim
