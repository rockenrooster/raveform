#pragma once
// M5 part 3: the game's side of the upgrades' sound (audio/cues.h, design/music/sound_m5.json). The band plays every
// patterned part (the lead's changes, the Riffs' instruments, the Boosts' and Fusions' layers, Kick Pulse's tom) from a
// player's loadout alone; the world's upgrade events (sim::EventType::Upgrade) become its cues here, counted over a
// frame and posted once a kind (the engine aggregates them again on its 1/32 steps).

#include "audio/cues.h"
#include "core/types.h"

namespace rf {
class Band;
namespace sim {
struct Event;
struct PlayerState;
class World;
}  // namespace sim

// Everything a player has, as the band numbers it (the same order: GAME_DESIGN 6.2 to 6.5).
audio::PlayerLoadout bandLoadout(const sim::PlayerState& pl);
u32 loadoutSignature(const audio::PlayerLoadout& l);

struct UpgradeCues {
    u32 count[audio::kCueCount] = {};
    float value[audio::kCueCount] = {};
    float pan[audio::kCueCount] = {};
    double tick[audio::kCueCount] = {};
    u32 burning = 0xFFFFFFFFu;  // Burn: the bodies burning last posted
    // Play test 3: the Orbitals' guard ring, as last seen (its shards lit, whether it was broken): each shard relit is a cue.
    u32 ringLit = 0;
    bool ringBroken = false;
    // One of this device's player's events (`songOffset`: the band's ticks minus the run's; `pan` -1..1).
    void add(const sim::Event& e, float pan, double songOffset);
    // Every cue counted this frame, once a kind; Burn's crackle follows the bodies this player has burning; play test 3: each
    // shard of a broken Orbitals ring relit (World::orbitalsLit) is a shaker tick a scale step higher.
    void flush(Band& band, const sim::World& w, u32 seat);
    void reset();
};

}  // namespace rf
