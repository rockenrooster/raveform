#pragma once
// Upgrades of the first slice (GAME_DESIGN 6; M3 stage B): eight Mods, three Riffs and four Boosts, the
// Trio's draw rules (6.1) and what taking one does to a player's loadout. The effects themselves live in
// the world (sim/world_upgrades.cpp); the names and effect words are what the Trio and the HUD show.

#include "core/types.h"
#include "game/rng.h"

namespace rf::sim {

// Mods (GAME_DESIGN 6.2) then Riffs (6.3), in one list so a Trio orb is one number.
enum Upgrade : u8 {
    kSplit, kDoubleTime, kPierce, kRicochet, kOvercharge, kExtraCell, kMagnet, kOnBeatCrit,  // Mods
    kOrbitals, kPulseRing, kSentinel,                                                         // Riffs
    kUpgradeCount,
    kNoUpgrade = 0xFF
};
constexpr u32 kModCount = 8;
constexpr u32 kRiffCount = 3;
constexpr u8 kRiffMaxLevel = 5;  // GAME_DESIGN 6.1
constexpr u32 kRiffSlots = 4;
inline bool isRiff(u8 u) { return u >= kOrbitals && u < kUpgradeCount; }
inline u32 riffIndex(u8 u) { return static_cast<u32>(u - kOrbitals); }

enum Boost : u8 { kPrism, kHyper, kHalo, kMagnetStorm, kBoostCount, kNoBoost = 0xFF };
constexpr u32 kBoostSlots = 2;  // GAME_DESIGN 6.1: two at once; a third refreshes the oldest

struct UpgradeInfo {
    const char* name;  // as on the orb ("SPLIT")
    const char* icon;  // design/icons.json key
    u8 cap;            // Mods: stacks (GAME_DESIGN 6.2); Riffs: levels
    bool weapon;       // counts for the Trio's "affects weapons" guarantee
};
const UpgradeInfo& upgradeInfo(u8 u);
// The effect words for taking `u` to `level` (stacks or Riff level after the pick): two to four words.
const char* upgradeWords(u8 u, u32 level);

struct BoostInfo {
    const char* name;
    const char* icon;
    const char* words;
    u32 bars;  // duration (GAME_DESIGN 6.4: four bars unless noted; Halo at most eight)
};
const BoostInfo& boostInfo(u8 b);

struct Loadout {
    u8 mods[kModCount] = {};    // stacks
    u8 riffs[kRiffCount] = {};  // levels (0: not owned)
    u8 order[kUpgradeCount] = {};  // when each was first taken (1, 2, ...; 0 never): the HUD's band row
    u8 taken = 0;               // upgrades taken this run
    u32 riffsOwned() const {
        u32 n = 0;
        for (u8 r : riffs) n += r ? 1u : 0u;
        return n;
    }
    // Stacks of a Mod or level of a Riff.
    u32 level(u8 u) const { return isRiff(u) ? riffs[riffIndex(u)] : mods[u]; }
    bool canTake(u8 u) const;
};

// GAME_DESIGN 6.1: three options from the Mods that are not capped, the Riffs not yet owned (while a slot
// is free) and level-ups of owned Riffs, by weight (tuning::kTrioWeight*). Guarantees: at least one
// option affects weapons; with fewer than four Riffs and no new Riff offered in the last two Trios, one
// is. Returns how many options there are (fewer than three only when almost everything is maxed).
// `sinceRiff`: Trios in a row that offered no new Riff (updated).
u32 drawTrio(const Loadout& l, u8& sinceRiff, Rng& rng, u8 out[3]);

}  // namespace rf::sim
