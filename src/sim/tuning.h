#pragma once
// ======================================================================================================
// NUMBERS CHOSEN FOR M3 STAGE A ("the fight") WHERE THE DESIGN DOCUMENTS GIVE NONE.
//
// Every other gameplay number is read from design data (design/species.json, design/look.json,
// design/music/conductor.json) or is quoted from docs/GAME_DESIGN.md / docs/TECH_SPEC.md / design/look.md
// with the section named next to it. Each value below is a first guess, listed in docs/PROGRESS.md
// (M3, "numbers chosen") so the lead can retune it. Nothing here is retyped design data.
// ======================================================================================================

#include "core/types.h"

namespace rf::sim::tuning {

// ---- Player (GAME_DESIGN 5 gives 520 wu/s, "near-instant acceleration, slight glide on release") ----
constexpr float kAccelTau = 0.030f;       // s: velocity follows the stick with this time constant
constexpr float kGlideTau = 0.090f;       // s: velocity decays with this time constant after release
constexpr float kShotLifeS = 0.75f;       // s: a shot flies 1,050 wu (the view's half-diagonal is ~880)
constexpr float kShotRadius = 4.0f;       // wu: a shot's collision radius against enemy circles
constexpr float kMuzzleWu = 10.0f;        // wu: shots leave this far along the aim from the ship's centre
constexpr float kAutoAimRangeWu = 900.0f; // wu: auto-aim assist picks the nearest enemy within this
constexpr float kAutoAimKeep = 1.25f;     // a held target stays while within this x the nearest one's distance
constexpr float kHeadingTurn = 18.0f;     // rad/s: the hull turns toward its course at most this fast
constexpr float kNovaGraceS = 0.30f;      // s: Nova invulnerability lasts this long after the blast
constexpr float kNovaFrontS = 0.35f;      // s: the front's growth time (look.md 5) during which it clears
constexpr float kNovaFrontWu = 1400.0f;   //    ... out to this radius (look.md 5)
constexpr float kPanicShoveWu = 600.0f;   // wu/s: the panic pulse's push on enemies within 220 wu
constexpr float kKnockbackDecay = 6.0f;   // 1/s: how fast that push dies away
constexpr float kHitDipScale = 0.85f;     // look.md 5: "time dips to 85% for 200 ms" (quoted, not chosen)
constexpr float kHitDipS = 0.20f;

// ---- Fairness and engagement (GAME_DESIGN 8, 9) ----
// Ranged enemies only telegraph and fire while inside this rectangle around a player (the 20:9 view
// at zoom 1 is +-800 x +-360 wu); an engaged shooter outside the camera's view gets an edge indicator.
constexpr float kEngageHalfW = 900.0f, kEngageHalfH = 430.0f;
// The Nova clears what is "in view": the widest view (zoom 0.85) around the player.
constexpr float kNovaViewHalfW = 941.0f, kNovaViewHalfH = 424.0f;

// ---- Enemies (species.json gives speeds, health, radii and the motion keys) ----
constexpr float kBulletLifeS = 4.0f;          // s: enemy bullets live this long (or until a wall)
constexpr float kDrifterHomeTurn = 1.0f;      // rad/s: drifters' wander leans toward the nearest player (C2: was 0.15)
constexpr float kHoundSeparationWu = 26.0f;   // hounds keep this far apart (a pack, not a point)
constexpr float kHoundBurstSpread = 0.12f;    // rad: hound_B's three shots fan this far apart
constexpr float kShoalCohesion = 0.9f, kShoalAlignment = 1.1f, kShoalSeparation = 2.6f, kShoalSeek = 0.5f;  // C2: seek was 1.0
constexpr float kShoalSpeedScale = 0.7f;      // C2: species.json's shoal speed x this (the -30% the lead allows)
constexpr double kCrowdArrivingBars = 0.0;   // C2: crowd fodder counts as arriving until it lands plus this many bars
constexpr float kCrowdDebtGain = 0.5f;      // C2: a bar's shortfall x this is owed on top of the next bars' orders (at most half the target)
constexpr float kCrowdBossRoomWu = 450.0f;   // C2: while a boss lives drifters and shoals keep this far from a ship
constexpr float kShoalTurn = 5.0f;            // rad/s
constexpr float kRamEngageWu = 650.0f;        // a Ram starts its telegraph within this distance
constexpr float kRamPathWu = 310.0f;          // drawn chevrons reach this far (620 wu/s x one beat at 120 BPM)
constexpr float kCaromAimSpread = 0.35f;      // rad: caroms leave their warp-in toward the player +- this
constexpr float kSentryTurnsPerBar = 0.25f;   // the three arms turn once every four bars
constexpr float kContactShrink = 0.85f;       // contact damage uses 85% of an enemy's radius (forgiving)

// ---- Shards and Chain (GAME_DESIGN 5: magnet 90 wu, fade after four bars, +1 Chain per Shard) ----
constexpr float kShardBurstLo = 60.0f, kShardBurstHi = 160.0f;  // wu/s at the kill
constexpr float kShardDrag = 4.0f;                              // 1/s while loose
constexpr float kShardHomeAccel = 3000.0f;                      // wu/s^2 once inside the magnet
constexpr float kShardHomeMax = 900.0f;                         // wu/s
constexpr float kShardPickupWu = 20.0f;                         // collected within this of the ship

// ---- Formations (the director's spending; stage A's spawner numbers) ----
constexpr float kBankBars = 3.0f;            // unspent budget carries over, at most this many bars' worth
constexpr float kSpawnNearWu = 450.0f;       // formations land 450-780 wu out horizontally ...
constexpr float kSpawnFarWu = 780.0f;
constexpr float kSpawnSquash = 0.45f;        // ... and 0.45 of that vertically (on the 20:9 screen)
constexpr float kSpawnMinPlayerWu = 260.0f;  // GAME_DESIGN 8 (quoted)
constexpr u32 kShoalGroupCapStageA = 40;     // species.json groupSize max (quoted)

// ---- Stress scenario (bench_stress) ----
constexpr u32 kStressEnemies = 700;
constexpr u32 kStressSentries = 32;          // 32 x 3 bullets every 1/8 note x 4 s = about 1,500 alive
constexpr float kStressHalfW = 780.0f, kStressHalfH = 340.0f;
constexpr float kStressRecycleWu = 1300.0f;  // enemies left this far behind are recycled

// ---- Band intensity follows the threat on screen (stage A keeps the band in Groove) ----
constexpr float kIntensityFloor = 0.15f;
constexpr float kIntensityPerThreat = 1.0f / 16.0f;
constexpr float kIntensityTau = 2.0f;        // s

// ======================================================================================================
// M3 STAGE B: the director, upgrades and Kaleidos. Chosen where GAME_DESIGN 4, 6, 8 and kaleidos.md give
// no number (each listed in docs/PROGRESS.md, M3 stage B, "numbers chosen").
// ======================================================================================================

// ---- Director (GAME_DESIGN 4.1, 8) ----
constexpr float kBuildLastBarSpeed = 0.85f;  // GAME_DESIGN 4.1 (quoted): the Build's last bar at 85%
constexpr float kBuildScaleEnd = 1.6f;       // GAME_DESIGN 8 (quoted): Build ramps 1.0 -> 1.6
constexpr float kDropScaleStart = 2.0f;      // GAME_DESIGN 8 (quoted): Drop 2.0, decaying to 1.2
constexpr float kDropScaleEnd = 1.2f;
constexpr float kBossBudgetScale = 0.3f;     // kaleidos.md (quoted): 30%, fodder families only
constexpr float kMegaWaveBars = 3.0f;        // the Drop's mega-wave spends 3 bars of Drop budget at once
constexpr float kEndlessBudgetStep = 2.0f;   // per cycle past species.json's last budget (12/bar)
constexpr u32 kDirectorAliveCap = 600;       // no new bodies while this many enemies live (perf contract)
// A player's camera frame in the simulation (M3 stage C2: the crowd is counted in it, species.json
// firstSlice.crowd): the reference view, 2400 x 1080 px at 1.5 px a wu and zoom 1 (look.md), centred on the ship.
constexpr float kFrameHalfWWu = 800.0f, kFrameHalfHWu = 360.0f;
// The crowd's formations (GAME_DESIGN 8 names floods along a frame edge, rings and corner swarms; where
// they land is species.json's rrive; the shares and the spacing are chosen): a flood lands in clusters
// of up to 8, at most 220 wu apart along the edge.
constexpr float kCrowdFloodShare = 0.5f, kCrowdRingShare = 0.25f;
constexpr float kCrowdFloodSpacingWu = 220.0f;
constexpr float kCrowdEdgeBase = 0.25f;      // C2: a flood's edge: this, plus the ship's heading toward it (0-1)
constexpr float kAssistStep = 0.1f, kAssistMin = 0.8f, kAssistMax = 1.3f;  // GAME_DESIGN 8 (quoted)
constexpr float kBoostEveryLoS = 20.0f, kBoostEveryHiS = 30.0f;            // GAME_DESIGN 6.1 (quoted)
constexpr float kSpotlightWu = 560.0f;       // a debut's group lands this far from the ship
constexpr float kEndlessBpmStep = 4.0f, kEndlessBpmCap = 174.0f;           // GAME_DESIGN 4.2 (quoted)
constexpr float kTrackBpm[3] = {120.0f, 126.0f, 132.0f};                   // GAME_DESIGN 4.2 (quoted)

// ---- Trio (GAME_DESIGN 6.1, screens.md "The Trio": ring 170 wu, orbs 38 wu, Riffs x1.14 quoted) ----
constexpr float kTrioRingWu = 170.0f;
constexpr float kTrioOrbWu = 38.0f;
constexpr float kTrioRiffScale = 1.14f;
constexpr float kTrioTouchWu = 6.0f;          // taken when the ship's hit circle is this far inside the orb
constexpr float kTrioHomeSpeed = 900.0f;      // wu/s: the orb that flies to the ship when the clock ends
constexpr float kTrioWeightMod = 1.0f;        // draw weights: an uncapped Mod,
constexpr float kTrioWeightNewRiff = 1.2f;    // a Riff not yet owned,
constexpr float kTrioWeightRiffLevel = 1.0f;  // a level for an owned Riff

// ---- Mods (GAME_DESIGN 6.2 gives each effect per stack; how wide, how far is chosen) ----
constexpr float kSplitFanDeg = 5.0f;          // Split: lanes 5 degrees apart, centred on the aim
constexpr float kOverchargePerStack = 0.25f;  // GAME_DESIGN 6.2 (quoted): +25% damage
constexpr float kOverchargeRadius = 0.15f;    // and +15% shot radius per stack (look only)
constexpr float kMagnetPerStack = 0.40f;      // GAME_DESIGN 6.2 (quoted): +40% Shard pull radius
constexpr u32 kMaxShotsPerStep = 15;          // Split x Prism never spawns more than this per step

// ---- Riffs (GAME_DESIGN 6.3) ----
constexpr float kOrbitalRadiusWu = 64.0f;     // Orbitals: circle around the ship, one turn per bar (quoted)
constexpr float kOrbitalHitWu = 10.0f;
constexpr float kOrbitalDamage = 1.0f;        // per eighth note an enemy stays in contact
constexpr float kPulseRadiusWu = 230.0f;      // Pulse Ring: expands to this ...
constexpr float kPulseRadiusL4 = 290.0f;      // ... (level 4 and up)
constexpr float kPulseGrowS = 0.25f;          // ... in this long
constexpr float kPulseDamage = 2.0f;          // per enemy the front passes (level 5: x1.5)
constexpr float kPulseSecondRingS = 0.10f;    // level 3: a second ring this much later (screens.md example)
constexpr float kSentinelRangeWu = 700.0f;    // Sentinel: shoots the nearest enemy within this
constexpr float kSentinelOffsetX = -34.0f, kSentinelOffsetY = -30.0f;  // its station beside the ship
constexpr float kSentinelFollowTau = 0.12f;   // s
constexpr float kSentinelSpreadDeg = 4.0f;    // a burst fans this far apart

// ---- Boosts (GAME_DESIGN 6.4) ----
constexpr float kBoostPickupWu = 26.0f;
constexpr double kBoostLifeBars = 8.0;        // an uncollected Boost fades after eight bars
constexpr float kPrismFanDeg = 9.0f;          // Prism: five lanes 9 degrees apart
constexpr float kPrismMiniDeg = 35.0f;        // a hit splits into two mini shots +-35 degrees ...
constexpr float kPrismMiniLifeS = 0.22f;      // ... that fly this long ...
constexpr float kPrismMiniDamage = 0.5f;      // ... for half damage
constexpr u32 kHaloHits = 3;                  // GAME_DESIGN 6.4 (quoted)
constexpr float kHaloInvulS = 0.6f;           // after a hit the Halo absorbed

// ---- Kaleidos (kaleidos.md / kaleidos.json give the fight; the rest is chosen) ----
constexpr float kBossTrackHealth[3] = {1.0f, 1.5f, 2.0f};  // the lead: 1.5x in Track 2, 2x in Track 3 (stage B, C briefs)
constexpr float kBossEndlessHealth = 1.06f;   // GAME_DESIGN 8 (quoted, from Track 7): +6% per Track after Track 3 (stage C)
constexpr float kBossPairScore = 500.0f;      // score per pair (x Chain)
constexpr float kBossCoreScore = 2000.0f;     // score for the core (x Chain)
constexpr u32 kBossPairShards = 20;           // Shards a pair drops (an elite's, GAME_DESIGN 5)
constexpr u32 kBossShards = 200;              // GAME_DESIGN 5 (quoted): 200 for bosses
constexpr float kBossNovaDamage = 60.0f;      // a Nova's blast: to each pair, and the core when open
constexpr float kBossArmInner = 78.0f;        // the arms' hit shape: from the first facet's inner point ...
constexpr float kBossArmOuter = 274.0f;       // ... to past the tip star
constexpr float kBossEnterWu = 500.0f;        // kaleidos.md (quoted): enters 500 wu from the nearest player

// ======================================================================================================
// M4 STAGE A: co-op (GAME_DESIGN 10, TECH_SPEC 8). Quoted numbers are marked; the rest are chosen where the
// documents give no number for scaling with the player count (listed in docs/PROGRESS.md, M4 stage A).
// ======================================================================================================
constexpr float kCoopBudgetPerPlayer = 0.6f;      // GAME_DESIGN 8 (quoted): threat budget x (1 + 0.6 x (players - 1))
constexpr float kCoopBossHealthPerPlayer = 0.5f;  // GAME_DESIGN 8 (quoted): boss health x (1 + 0.5 x (players - 1))
constexpr u32 kCoopAliveCapPerPlayer = 150;       // the director's alive cap grows this much a seat (600 -> 1,050 with four):
                                                  // each frame keeps its own crowd, the cap is the phone's CPU
constexpr u32 kTargetRethinkSteps = 4;            // an enemy picks its player again every fourth sixteenth (staggered by
                                                  // spawn number, so on the same musical steps on every device) ...
constexpr float kTargetStick = 0.7f;              // ... and changes only to a player this much nearer (stickiness)
constexpr float kCoopShardReachW = 1600.0f, kCoopShardReachH = 720.0f;  // a death drops a player's Shards within two
                                                  // frames of it (every death drops for every player, GAME_DESIGN 10)
constexpr float kReviveRingWu = 90.0f;            // the beacon's ring a teammate stands in (GAME_DESIGN 10: two beats)
constexpr double kReviveBeats = 2.0;              // GAME_DESIGN 10 (quoted)
constexpr float kPredictUndoS = 0.30f;            // TECH_SPEC 8.1 (quoted): a predicted death not confirmed in 300 ms is undone
constexpr float kCorrectionTau = 0.150f;          // TECH_SPEC 8.1 (quoted): a correction's offset decays over about 150 ms
constexpr float kCorrectionSnapWu = 96.0f;        // TECH_SPEC 8.1 (quoted): above this error it snaps
}  // namespace rf::sim::tuning
