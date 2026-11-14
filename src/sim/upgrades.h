#pragma once
// Upgrades (GAME_DESIGN 6): the 36 Mods, 12 Riffs, 16 Boosts and 8 Fusions (M5 part 3; the first slice had eight Mods,
// three Riffs and four Boosts, play test 2 Regen), the Trio's draw rules (6.1) and what taking one does to a loadout.
// The effects live in the world (sim/world_upgrades.cpp, world_upgrades_m5.cpp); the names and effect words are what the
// Trio and the HUD show. Numbering: GAME_DESIGN's tables, as src/audio/cues.h has them (id = the table's number - 1).

#include "core/types.h"
#include "game/rng.h"

namespace rf::sim {

// Mods (GAME_DESIGN 6.2, 1-36) then Riffs (6.3, 1-12), in one list so a Trio orb is one number.
enum Upgrade : u8 {
    // Weapon
    kSplit, kDoubleTime, kPierce, kRicochet, kFork, kSeeker, kHeavyStep, kAccent, kEcho, kWave, kOvercharge, kRearGuard,
    kArc, kBurn, kShatter, kGravityRounds,
    // Defence and movement
    kExtraCell, kRegen, kAfterimage, kReserve, kShockstep, kAegis, kLastLight, kSlipstream,
    // Economy
    kMagnet, kProspector, kNovaStock, kEncore, kLuckyBreak, kCollector,
    // Rhythm
    kOnBeatCrit, kSyncopation, kBarLine, kCrescendo, kKickPulse, kAfterparty,
    // Riffs
    kOrbitals, kPulseRing, kSentinel, kLancer, kMinelayer, kStormCoil, kSwarm, kSingularity, kMirror, kComet, kSweeper, kFlak,
    kUpgradeCount,
    kNoUpgrade = 0xFF
};
constexpr u32 kModCount = 36;
constexpr u32 kRiffCount = 12;
constexpr u8 kRiffMaxLevel = 5;  // GAME_DESIGN 6.1
constexpr u32 kRiffSlots = 4;
inline bool isRiff(u8 u) { return u >= kOrbitals && u < kUpgradeCount; }
inline u32 riffIndex(u8 u) { return static_cast<u32>(u - kOrbitals); }

// GAME_DESIGN 6.4 (1-16).
enum Boost : u8 {
    kPrism, kHyper, kSupernova, kColossus, kTimeStretch, kFreezeFrame, kHalo, kMagnetStorm, kBlackHole, kChainStorm, kChorus,
    kBeam, kRicochetFever, kConfetti, kFractal, kCarousel,
    kBoostCount,
    kNoBoost = 0xFF
};
constexpr u32 kBoostSlots = 2;  // GAME_DESIGN 6.1: two at once; a third refreshes the oldest

// GAME_DESIGN 6.5 (1-8): both ingredients maxed brings the Fusion as a gold fourth orb.
enum Fusion : u8 { kHelixArray, kStandingWave, kTeslaBloom, kShardHalo, kDownbeatCannon, kCollapseField, kMurmuration, kQuartet, kFusionCount };
struct FusionInfo {
    const char* name;
    const char* icon;
    const char* words;
    u8 a, b;  // its ingredients (Upgrade)
};
const FusionInfo& fusionInfo(u8 f);

// Play test 2 (GAME_DESIGN 6.1): with every Mod and Riff at its cap a Trio's orbs offer Boosts. Such an orb's number is
// kBoostOrb + the Boost; a Fusion's kFusionOrb + the Fusion (neither collides with an upgrade).
constexpr u8 kBoostOrb = 0x40;
constexpr u8 kFusionOrb = 0x60;
inline bool isBoostOrb(u8 u) { return u >= kBoostOrb && u < kBoostOrb + kBoostCount; }
inline u8 boostOfOrb(u8 u) { return static_cast<u8>(u - kBoostOrb); }
inline bool isFusionOrb(u8 u) { return u >= kFusionOrb && u < kFusionOrb + kFusionCount; }
inline u8 fusionOfOrb(u8 u) { return static_cast<u8>(u - kFusionOrb); }
// A Trio orb: an upgrade, a Boost or a Fusion.
inline bool validOrb(u8 u) { return u < kUpgradeCount || isBoostOrb(u) || isFusionOrb(u); }
constexpr u32 kMaxOrbs = 6;  // three, + Lucky Break's two, + a Fusion's gold orb

struct UpgradeInfo {
    const char* name;  // as on the orb ("SPLIT")
    const char* icon;  // design/icons.json key
    u8 cap;            // Mods: stacks (GAME_DESIGN 6.2); Riffs: levels
    bool weapon;       // counts for the Trio's "affects weapons" guarantee
};
const UpgradeInfo& upgradeInfo(u8 u);
// The effect words for taking `u` to `level` (stacks or Riff level after the pick): two to four words.
const char* upgradeWords(u8 u, u32 level);

// What each stack or level gives (GAME_DESIGN 6.2, 6.3 as raised after play test 1, 2026-11-12: every pick is felt
// at once). The world, the view and the effect words read these.
inline u32 splitLanes(u32 stacks) { return 1u + stacks; }             // 1, 2, 3, 4, 5 lanes (eb10554: a lane a rank, cap 4)
inline u32 pierceCount(u32 stacks) { return 2u * stacks; }            // enemies a shot passes through
inline u32 ricochetBounces(u32 stacks) { return 2u * stacks; }        // off walls, and on to the nearest other enemy
inline float onBeatCritMultiplier(u32 stacks) {                      // x3, x5, x8 on the beat
    constexpr float k[4] = {1.0f, 3.0f, 5.0f, 8.0f};
    return k[stacks < 3 ? stacks : 3];
}
inline u32 orbitalCount(u32 level) { return level ? 1u + 2u * level : 0u; }  // 3, 5, 7, 9, 11 shards
inline u32 sentinelBurst(u32 level) {                                // shots a drone fires on each off-beat
    constexpr u32 k[6] = {0, 3, 4, 4, 5, 5};
    return k[level < 5 ? level : 5];
}
inline u32 sentinelDrones(u32 level) {                               // a second drone at level 3, a third at 5
    constexpr u32 k[6] = {0, 1, 1, 2, 2, 3};
    return k[level < 5 ? level : 5];
}
constexpr u32 kMaxDrones = 3;
inline u32 regenEveryBars(u32 stacks) {                              // Regen (play test 2): a cell grows back every
    constexpr u32 k[4] = {0, 16, 8, 4};                              // 16, 8, then 4 bars
    return k[stacks < 3 ? stacks : 3];
}

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
    u8 fusions = 0;             // bit f: Fusion f taken (GAME_DESIGN 6.5)
    u32 riffsOwned() const {
        u32 n = 0;
        for (u8 r : riffs) n += r ? 1u : 0u;
        return n;
    }
    // Stacks of a Mod or level of a Riff.
    u32 level(u8 u) const { return u >= kUpgradeCount ? 0u : (isRiff(u) ? riffs[riffIndex(u)] : mods[u]); }
    bool canTake(u8 u) const;
    bool fused(u8 f) const { return (fusions >> f) & 1u; }
    // A Fusion is due: both its ingredients at their caps and it not taken (the Trio's gold orb).
    bool fusionDue(u8 f) const;
};

// GAME_DESIGN 6.1 (M5: how a Trio is dealt): three orbs (four, five with Lucky Break). The first is firepower: the weapon
// and rhythm Mods held, the Riffs held and the four staples (Split, Double-Time, Overcharge, On-Beat Crit), what is at its
// cap left out (none: drawn as the others). The others are the whole pool by weight (dealWeight, tuning::kDeal*). No two
// alike. With fewer than four Riffs and no new Riff offered in the last two Trios, one of the others is. Play test 2: when
// fewer upgrades can still be taken, the orbs left offer different Boosts. A Fusion that is due comes as one more (gold)
// orb. Returns the orbs (<= kMaxOrbs). `sinceRiff`: Trios in a row that offered no new Riff (updated).
// a667116: the first Trio of a run (each player's own) offers Split and Double-Time, the other orbs drawn as ever.
u32 drawTrio(const Loadout& l, u8& sinceRiff, Rng& rng, u8 out[kMaxOrbs], bool firstOfRun = false);
// The dealing's weight of an upgrade for a loadout (0: it cannot be offered); a weapon or rhythm Mod (firepower).
float dealWeight(const Loadout& l, u8 u);
bool firepower(u8 u);

// Shard levels (GAME_DESIGN 6.1, play test 2): the Shards level n takes from level n - 1, base x ratio^(n - 1) rounded
// (tuning::kLevelShardsBase and kLevelShardsRatio; n >= 1), the first tuning::kLevelShardsFirst.
u32 levelShards(u32 n);

}  // namespace rf::sim
