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

// The careful bot's choice among a Trio's orbs (M5 part 3; M8: the kit's dealing, design/modes/README.md 5): a Fusion first,
// then by kBotUpgradeScore / kBotBoostScore, a Fusion's other ingredient raised; the first of the best.
u32 botPickOrb(const Loadout& l, const u8* orbs, u32 n);

class Bot {
public:
    // M8 TRACERS (design/modes/tracers.md "What to check"): every policy flies rings there; Child is the child-like one (half a
    // second late, no Dash), and the Novice flies as it does.
    // M8 SONGLINE: every policy rides the line (the Spark's gates in order); Slow is the rider at half a ship's speed (the line's
    // finding a lost ship, songline.md "What to check"), Child the child-like one.
    enum class Policy : u8 { Play, Stress, Reckless, Novice, Assist, Child, Slow };
    void setPolicy(Policy p) { policy_ = p; }  // --autoplay --reckless-after: the run ends on its own
    void reset(u64 seed, Policy policy);
    StickInput think(const World& world, u32 player, const PlayerController& controller, double musicTick);
    Policy policy() const { return policy_; }
    // The title's demo (the owner, 2026-11-14: "make the demo AI actually get powerups"): Boost orbs, Extra Lives and Shards
    // pull harder and from farther; precise() says the ship is closing on (or holding) a Trio's orb or a pickup, where the demo's
    // eased stick must answer at once (Play::attractInput) or it overshoots the orb and the hold empties.
    void setDemo(bool on) { demo_ = on; }
    bool precise() const { return precise_; }

private:
    Rng rng_{7};
    Policy policy_ = Policy::Play;
    float wander_ = 0.0f;
    bool demo_ = false, precise_ = false;
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
    // M8 TRACERS: the ring-flier circles the nearest twelve bodies (or a dark part of the floor) and starts the next circle when the
    // ring locks; the child does the same half a second late and never dashes.
    StickInput thinkTracers(const World& w, u32 player, const PlayerController& controller, StickInput in);
    StickInput thinkSongline(const World& w, u32 player, const PlayerController& controller, StickInput in);
    // M8 JAM's wanderer (jam.md "What to check"): crosses the room's pads at random, holds a hold pad its beats, takes from a shelf.
    StickInput thinkJam(const World& w, u32 player, const PlayerController& controller, StickInput in);
    i32 jamTarget_ = -1;     // a pad, or 100 + an orb of the open shelf
    u32 jamSince_ = 0;       // ticks at the target
    u32 jamStay_ = 0;        // ticks to stay there
    void pickCircle(const World& w, u32 player, Vec2 pos);
    Vec2 ringC_;
    float ringR_ = 0.0f, ringDir_ = 1.0f;
    bool ringOn_ = false;
    u32 ringCount_ = 0, ringTicks_ = 0;
    static constexpr u32 kChildDelayTicks = 30;  // half a second at 60 Hz
    Vec2 childMove_[kChildDelayTicks] = {};
    u32 childAt_ = 0;
};

}  // namespace rf::sim
