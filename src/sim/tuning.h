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
constexpr u32 kDirectorAliveCap = 900;       // no new bodies while this many enemies live (perf contract; play test 1,
                                             // second pass: 600 -> 900, measured on the Moto's densest part)
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
constexpr float kTrackBpm[10] = {120.0f, 126.0f, 132.0f, 138.0f, 144.0f, 150.0f, 156.0f, 162.0f, 168.0f, 174.0f};  // GAME_DESIGN 4.2 (quoted)

// ---- Trio (GAME_DESIGN 6.1, screens.md "The Trio": ring 170 wu, orbs 38 wu, Riffs x1.14 quoted) ----
constexpr float kTrioRingWu = 170.0f;
constexpr float kTrioOrbWu = 38.0f;
constexpr float kTrioRiffScale = 1.14f;
constexpr float kTrioTouchWu = 6.0f;          // (M3: taken on touch; play test 1 holds instead, below)
constexpr float kTrioHomeSpeed = 900.0f;      // wu/s: the orb that flies to the ship when the clock ends
// Play test 1 (GAME_DESIGN 6.1, screens.md "The Trio", 2026-11-12: a choice is never an accident): nothing can be
// taken for the Break's first two beats; then an orb is taken by staying inside its thin ring (1.42 x the icon,
// quoted) for two beats; leaving empties it at once.
constexpr double kTrioLockBeats = 2.0;
constexpr double kTrioHoldBeats = 2.0;
constexpr float kTrioHoldRing = 1.42f;
// How a Trio is dealt (GAME_DESIGN 6.1, M5: the lead's starting weights): a Mod not held, a Mod held under its cap, a new
// Riff (none once four are held), a level of a held Riff; an ingredient of a Fusion whose other ingredient is held, x2.
constexpr float kDealModNew = 1.0f, kDealModHeld = 3.0f, kDealRiffNew = 3.0f, kDealRiffLevel = 3.0f, kDealFusionFactor = 2.0f;
// M5 part 3 (measured; for the lead): the cells' Mods (Extra Cell, Regen) count as held before they are, so the halo can grow
// in the first pass as it did in the play tests' smaller pool (Regen grew back 107 cells for the careless bot then, 5 with 1).
constexpr float kDealCellsNew = 3.0f;

// ---- Mods (GAME_DESIGN 6.2 gives each effect per stack; how wide, how far is chosen) ----
// Play test 1 (2026-11-12) raised them: Split +2 lanes, Pierce +2, Ricochet +2 (and on to the nearest other enemy
// within 300 wu), Overcharge +50%, Magnet +100% of the base radius, On-Beat Crit x3 / x5 / x8 with a burst over 80
// wu (all quoted; sim/upgrades.h has the per-stack numbers).
constexpr float kSplitFanDeg = 5.0f;          // Split: lanes 5 degrees apart, centred on the aim
constexpr float kOverchargePerStack = 0.50f;  // GAME_DESIGN 6.2 (quoted): +50% damage
constexpr float kOverchargeRadius = 0.15f;    // and +15% shot radius per stack (look only)
constexpr float kMagnetPerStack = 1.00f;      // GAME_DESIGN 6.2 (quoted): +100% of the base pull radius
constexpr float kRicochetSeekWu = 300.0f;     // GAME_DESIGN 6.2 (quoted): a hit bounces on to the nearest enemy this near
constexpr float kCritBurstWu = 80.0f;         // GAME_DESIGN 6.2 (quoted): a beat shot's hit bursts this wide ...
constexpr float kCritBurstShare = 0.5f;       // ... dealing this share of the shot's damage to the others in it (chosen)
// The shot pool (TECH_SPEC 7.1: 1,024 a player) with Split 7 x Prism 5, Double-Time, Hyper and the endless tempo
// on top: above kShotsSoftCap live shots of a player (from a beat; until below kShotsThinEnd, on a beat: GAME_DESIGN 6.6)
// every lane still fires but those beyond the middle three fly half as far, and the multipliers' extras and Prism's
// mini-refractions wait; above kShotsMiddleOnly the middle lane alone fires; a player never has more than kShotsHardCap
// (a guard the rules above keep from being reached).
constexpr u32 kShotsSoftCap = 576;
constexpr u32 kShotsMiddleOnly = 800;
constexpr u32 kShotsHardCap = 1000;
constexpr u32 kShotsThinEnd = 432;            // GAME_DESIGN 6.6 (quoted): thinning ends below three quarters of the soft cap
constexpr float kThinReach = 0.5f;            // GAME_DESIGN 6.6 (quoted): the lanes beyond the middle three fly half as far
constexpr u32 kMaxShotsPerStep = 35;          // Split 7 x Prism 5
// The Shard pool (2,048): above this many out at once a loose Shard lasts one bar instead of GAME_DESIGN 5's four and a
// death drops one Shard; within kShardsReserve of the cap a death drops none.
constexpr u32 kShardsCrowded = 1536;
constexpr double kShardCrowdedBars = 1.0;
constexpr u32 kShardsReserve = 64;

// ---- Riffs (GAME_DESIGN 6.3) ----
// Play test 1: Orbitals 3, 5, 7, 9, 11 shards that also destroy the enemy bullets they touch; Pulse Ring on every
// beat from level 1 at twice the damage, then a second ring, wider, heavier, and every eighth at level 5; Sentinel a
// burst of 3, then 4, a second drone, 5, a third drone (quoted; sim/upgrades.h).
constexpr float kOrbitalRadiusWu = 64.0f;     // Orbitals: circle around the ship, one turn per bar (quoted)
constexpr float kOrbitalHitWu = 10.0f;
constexpr float kOrbitalDamage = 1.0f;        // per eighth note an enemy stays in contact
constexpr float kPulseRadiusWu = 230.0f;      // Pulse Ring: expands to this ...
constexpr float kPulseRadiusWide = 290.0f;    // ... (level 3 and up)
constexpr float kPulseGrowS = 0.25f;          // ... in this long
constexpr float kPulseDamage = 4.0f;          // per enemy the front passes: twice M3's 2.0 (quoted), level 4 and up x1.5
constexpr float kPulseSecondRingS = 0.10f;    // level 2: a second ring this much later (screens.md example)
constexpr float kSentinelRangeWu = 700.0f;    // Sentinel: shoots the nearest enemy within this
constexpr float kSentinelOffsetX = -34.0f, kSentinelOffsetY = -30.0f;  // the first drone's station beside the ship
// The second and third drones' stations (mirrored, and behind the ship).
constexpr float kSentinel2X = 34.0f, kSentinel2Y = -30.0f, kSentinel3X = 0.0f, kSentinel3Y = 42.0f;
constexpr float kSentinelFollowTau = 0.12f;   // s
constexpr float kSentinelSpreadDeg = 4.0f;    // a burst fans this far apart

// ---- Play test 2 (GAME_DESIGN 6.1, 2026-11-12): Shard levels, Cells, Regen ----
// Shard levels: level n takes kLevelShardsBase x kLevelShardsRatio^(n - 1) Shards (the design's 25 and 1.22 to start;
// tuned with the bot for a level every 20-30 s through the first three Tracks, docs/PROGRESS.md "Play test 2").
constexpr float kLevelShardsBase = 170.0f, kLevelShardsRatio = 1.17f;
constexpr u32 kLevelTrioBars = 4;           // GAME_DESIGN 6.1 (quoted): a level-up Trio's ring runs for four bars
constexpr u32 kLevelTrioClearBars = 4;      // chosen: one opens only when no Break begins in the next four bars, its ring's
                                            // span (the Break's own Trio keeps its place; the level waits, banked)
constexpr float kShelterShoveWu = 420.0f;   // chosen: wu/s of push on a body touching a level-up orb's thin ring
// Cells (health pickups): due about every 40 s with one cell missing and every 15 s on the last cell (quoted; linear in
// between), at most two loose a player (quoted), eight bars of life (quoted), two from a boss (quoted).
constexpr float kCellEveryOneMissingS = 40.0f, kCellEveryLastS = 15.0f;
constexpr u32 kCellsLooseMax = 2;
constexpr double kCellLifeBars = 8.0;
constexpr u32 kBossCells = 2;
constexpr float kCellRadiusWu = 16.0f;      // look.md 6 (quoted): the gold ring
constexpr float kCellPickupWu = 30.0f;      // chosen: taken when its ring touches the ship (16 + 14 wu)

// ---- The loop ramps (GAME_DESIGN 8, "The loop is where a run ends", the lead's review of play test 2): every Track after
// the first pass (Track 4 on) multiplies the threat budget and the bosses' bullet density by kLoopThreatRamp, enemy
// health (the boss's too) by kLoopHealthRamp and stretches the Cell clocks by kLoopCellRamp, compounding (sim/director.h
// loopRamp). Tuned with the bots within 1.15-1.5 (docs/PROGRESS.md, play test 2). The first pass is untouched.
constexpr float kLoopThreatRamp = 1.3f, kLoopHealthRamp = 1.25f, kLoopCellRamp = 1.25f;
// The lead's decision after the first ramp (GAME_DESIGN 8, `2fba23c`): from the loop's first Track the Breaks give back no
// cell and no Nova; Regen's periods stretch by kLoopRegenStretch a Track; enemies move kLoopEnemySpeedRamp faster a Track
// (at most kLoopEnemySpeedMax), their bullets kLoopBulletSpeedRamp (at most kLoopBulletSpeedMax, so a pattern can still be
// read), and every shooter's bullet count rises kLoopShooterRamp a Track (at most kLoopShooterMax, and only while the
// enemy bullet pool has a quarter free). Each compounding; tuned within a third of the design's step (docs/PROGRESS.md).
constexpr bool kLoopBreakRefills = false;  // the lead: the loop's Breaks give back no cell and no Nova
// The halo burns down (GAME_DESIGN 8, `daec945`, the last pass on the loop): on the first downbeat of every Track of the loop
// from the loop's kLoopHaloBurnFrom-th Track on (1: its first, the set list's length + 1; never a fixed Track number, M5),
// every ship loses kLoopHaloBurn cells of capacity (never below one); in the loop no kill
// drops a Cell and Regen stops (a boss's two Cells, a Halo, Extra Cell picks inside the burnt capacity and the Novas in
// stock remain). The knob the lead allows: one or two cells, from the loop's first or second Track (docs/PROGRESS.md).
constexpr u32 kLoopHaloBurn = 2;
constexpr u32 kLoopHaloBurnFrom = 1;  // the loop's first Track (Track 4 while the set list had three)
constexpr float kLoopRegenStretch = 1.5f;
constexpr float kLoopEnemySpeedRamp = 1.06f, kLoopEnemySpeedMax = 1.6f;
constexpr float kLoopBulletSpeedRamp = 1.06f, kLoopBulletSpeedMax = 1.5f;
constexpr float kLoopShooterRamp = 1.15f, kLoopShooterMax = 3.0f;
// The ramp holds a boss's bullet density at most this many times kaleidos.json's counts: the enemy bullet pool (2,048)
// then holds the naked core's bursts at 174 BPM (about 465 alive at 1x) with room for the other shooters (chosen; TECH_SPEC 7:
// what multiplies projectiles carries its own cap).
constexpr float kBossBulletDensityMax = 3.5f;

// ---- Difficulty levers (play test 1, GAME_DESIGN 8 "How hard": the threat budget and the bosses' bullet density,
// neither below 0.7 of the first slice's). 1.0 = the first slice's numbers.
constexpr float kThreatBudgetScale = 0.7f;
constexpr float kBossBulletScale = 0.7f;

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
constexpr float kFirstBossHealth = 0.7f;     // GAME_DESIGN 8 (quoted, play test 1 second pass): Track 1's Kaleidos at 0.7
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
// M5 PART 1: Longwave, the Choir, mini-bosses batch 1 and the Pen. The pages give the fights (sim/boss.cpp parses their
// json, sim/boss_prose.h quotes what they say in words); chosen here (docs/PROGRESS.md, M5 part 1, "Numbers chosen").
// ======================================================================================================
constexpr float kBossBulletScaleM5 = 1.0f;    // the new fights' bullet density lever (1 = the pages' counts; Kaleidos keeps 0.7)
constexpr float kBossNovaShareM5 = 1.0f;      // a Nova's blast on each part of theirs it can hurt (x kBossNovaDamage)
constexpr u32 kLwSegmentShards = 4;           // Shards a Longwave segment drops (24 of them)
constexpr u32 kChoirVoiceShards = 20;         // Shards a Voice drops (as Kaleidos's pair)
constexpr float kChoirMoveWu = 900.0f;        // wu/s: a Voice moves to its place at most this fast (the Fugue's shrink, Solo)
constexpr u32 kPrinceShards = 10;             // Shards a Carom Prince drops
constexpr float kPrinceDrift = 0.25f;         // the speed factor of a Prince that drifts on as a Carom after the King's death
constexpr float kRamTurnRate = 4.0f;          // rad/s: Battering Ram turns to face the nearest player
constexpr float kRamWallFanDeg = 160.0f;      // the wall's 16 bullets fan this wide from the point of impact, inward
constexpr float kRamWakeWu = 22.0f;           // the wake's half-width (contact)
constexpr float kHiveDartSpeed = 900.0f;      // wu/s: a Hive dart (and the Queen) moves to its place at most this fast

// M5 part 2 (sim/world_families.cpp): what species.json's motion leaves open, chosen (docs/PROGRESS.md, M5 part 2).
constexpr float kFlankerArcRad = 0.9f;         // a Flanker lands heading this far off the line to the player (its arc)
constexpr float kFlankerApproachSlant = 0.8f;  // ... and comes in on a slant: the tangent's weight against the radial's 1
constexpr float kFlankerOrbitBand = 1.3f;      // within this x its orbit radius it circles
constexpr float kFlankerRadialWu = 120.0f;     // the radial correction's reach on the orbit
constexpr float kFlankerTurn = 3.0f;           // rad/s
constexpr float kSkitterTurn = 4.0f;           // rad/s
constexpr float kSkitterDodgeS = 0.18f;        // a sidestep of dodgeDistance takes this long
constexpr float kSkitterSeeWu = 900.0f;        // it reads a line of fire this far along the aim ...
constexpr float kSkitterLaneWu = 36.0f;        // ... passing within this of it (plus its radius)
constexpr float kMothCatchUp = 1.6f;           // a Moth flies to its place on the clock at up to this x its speed
// ======================================================================================================
// M5 PART 3: every upgrade (GAME_DESIGN 6.2-6.5 to 6.6's rules). Quoted where the tables give a number; the rest
// chosen to 6.6's "every pick is felt at once" (docs/PROGRESS.md, M5 part 3, "Numbers chosen").
// ======================================================================================================
constexpr float kSlipstreamPerStack = 0.15f;                // 6.2 (quoted; the lead's ruling after part 3): +15% speed, cap 2
constexpr float kForkDeg = 20.0f, kForkDamage = 0.5f;       // Fork: two half-damage shots (quoted) this far apart
constexpr float kSeekerTurn[4] = {0.0f, 3.0f, 6.0f, 10.0f}; // rad/s by stack
constexpr float kSeekerReachWu = 320.0f;
constexpr float kHeavyOrbSpeed = 0.45f, kHeavyOrbRadius = 3.0f, kHeavyOrbLifeS = 1.6f;  // x the shot's speed, radius
constexpr float kHeavyBlastWu[4] = {0.0f, 90.0f, 120.0f, 150.0f}, kHeavyBlastDamage[4] = {0.0f, 4.0f, 6.0f, 8.0f};
constexpr float kAccentDamage = 3.0f, kAccentLengthWu = 1500.0f, kAccentHalfWu = 14.0f;  // x the shot's damage
constexpr float kEchoDamage = 0.5f;                         // quoted: half damage
constexpr float kWaveAmpWu[3] = {0.0f, 28.0f, 40.0f}, kWaveRadius[3] = {1.0f, 1.6f, 2.0f};  // (GAME_DESIGN 6.2 row 10: one wave
                                                            // to a beat of flight, look.md v2.5)
constexpr float kHelixAmpWu = 80.0f;                        // GAME_DESIGN 6.5 (quoted): the outer pair 80 wu to each side, the
constexpr float kHelixWaveBeats = 2.0f;                     // pairs inside at 2/3 and 1/3; one wave to two beats of flight
constexpr float kArcReachWu = 220.0f, kArcDamage = 2.0f;    // Arc: 2, 3, 4 more (quoted 2 at the first stack)
constexpr float kBurnDamage = 1.0f, kBurnSpreadWu = 90.0f;  // a beat's fire, x the stacks; neighbours caught on a death
constexpr double kBurnBeats = 2.0;                          // quoted
constexpr u32 kShatterShards[4] = {0, 3, 5, 7};
constexpr float kPelletSpeed = 700.0f, kPelletLifeS = 0.3f;
constexpr float kGravityReachWu[3] = {0.0f, 110.0f, 150.0f}, kGravityPull[3] = {0.0f, 70.0f, 110.0f};  // wu, wu/s
constexpr double kDecoyBeats[3] = {0.0, 2.0, 4.0};          // Afterimage (quoted 2 beats)
constexpr float kDecoyReachWu = 600.0f;
constexpr float kShockRingWu[4] = {0.0f, 140.0f, 180.0f, 220.0f}, kShockDamage[4] = {0.0f, 3.0f, 5.0f, 8.0f};
constexpr float kAegisArcDeg[4] = {0.0f, 60.0f, 90.0f, 120.0f}, kAegisInWu = 30.0f, kAegisOutWu = 60.0f;
constexpr float kLastLightWu = 420.0f;                      // Last Light: the mini-Nova's reach (the Nova's front: 1,400)
constexpr float kProspectorChance = 0.25f;                  // quoted
constexpr u32 kCollectorEvery[3] = {0, 250, 150};           // quoted
constexpr float kSyncopationDamage[3] = {1.0f, 1.5f, 2.0f}; // quoted +50%, then chosen +100%
constexpr u8 kSyncopationPierce = 2;
constexpr float kBarLineWu[4] = {0.0f, 900.0f, 900.0f, 1800.0f}, kBarLineHalfWu = 16.0f, kBarLineDamage = 5.0f;
constexpr float kCrescendoStep = 0.05f, kCrescendoMax[3] = {0.0f, 0.5f, 1.0f};  // quoted
constexpr float kKickRingWu[4] = {0.0f, 160.0f, 160.0f, 240.0f}, kKickPush = 380.0f, kKickDamage = 2.0f;
constexpr u32 kAfterpartyBars = 2;                          // quoted
// Riffs 4-12 (by level 1-5).
constexpr float kLancerDamage = 6.0f, kLancerHalfWu[6] = {0.0f, 10.0f, 16.0f, 16.0f, 22.0f, 22.0f}, kLancerLengthWu = 1600.0f;
constexpr float kMineBlastWu = 110.0f, kMineDamage = 8.0f;
constexpr float kCoilReachWu[6] = {0.0f, 220.0f, 220.0f, 260.0f, 260.0f, 300.0f}, kCoilDamage = 1.5f;
constexpr u32 kSwarmCount[6] = {0, 6, 8, 10, 12, 16};
constexpr float kMissileSpeed = 900.0f, kMissileDamage = 2.0f, kMissileLifeS = 1.4f, kMurmurationLifeS = 7.0f;
constexpr float kWellAheadWu = 300.0f, kWellPullWu[6] = {0.0f, 260.0f, 300.0f, 300.0f, 340.0f, 340.0f}, kWellPull = 260.0f;
constexpr float kWellBlastWu[6] = {0.0f, 200.0f, 230.0f, 230.0f, 260.0f, 260.0f}, kWellDamage[6] = {0.0f, 12.0f, 12.0f, 18.0f, 18.0f, 24.0f};
constexpr float kMirrorShare[6] = {0.0f, 0.5f, 0.6f, 0.7f, 0.8f, 1.0f}, kWingWu = 90.0f;  // quoted shares
constexpr float kCometOutWu = 520.0f, kCometRadiusWu = 22.0f, kCometDamage = 3.0f;
constexpr float kCometSize[6] = {0.0f, 1.0f, 1.5f, 1.5f, 2.0f, 2.0f};
constexpr u32 kCometCount[6] = {0, 1, 1, 2, 2, 3};
constexpr float kSweeperLengthWu[6] = {0.0f, 240.0f, 300.0f, 300.0f, 360.0f, 360.0f}, kSweeperDamage = 6.0f;  // a pass
constexpr u32 kFlakPellets[6] = {0, 12, 16, 20, 24, 24};
constexpr float kFlakBlastWu = 40.0f;
// Boosts 3-6, 9-16.
constexpr float kSupernovaWu = 70.0f;
constexpr float kColossusScale = 3.0f, kColossusRamDamage = 20.0f;  // quoted x3
constexpr float kTimeStretchSpeed = 0.5f;                           // quoted: half speed
constexpr float kFreezeChainWu = 90.0f;
constexpr float kHolePullWu = 700.0f, kHolePull = 300.0f, kHoleBlastWu = 500.0f, kHoleDamage = 50.0f;
constexpr double kHoleBeats = 3.0;                                  // quoted
constexpr float kChainStormWu = 360.0f, kChainStormDamage = 2.0f;
constexpr u32 kChainStormMax = 24;
constexpr float kChorusWu = 80.0f;
constexpr float kBeamLengthWu = 1400.0f, kBeamHalfWu = 10.0f;
constexpr u8 kFeverBounces = 5;                                     // quoted
constexpr float kFeverSpeedUp = 1.1f;
constexpr u32 kConfettiSparks = 6;                                  // quoted
constexpr float kCarouselWu = 70.0f, kBombSpeed = 500.0f, kBombLifeS = 1.0f, kBombBlastWu = 90.0f, kBombDamage = 6.0f;
// Fusions.
constexpr float kDownbeatHalfWu = 32.0f, kDownbeatLengthWu = 2400.0f;  // look.md v2.5: no beam hurts wider than 64 wu (was 60)
constexpr float kCollapsePullWu = 200.0f, kCollapsePull = 200.0f;
constexpr float kStandingDamage = 1.5f;
// The pages leave the health to the bot ("tune the segment health with the bot once it can surf", "tune the Voice health
// with the bot", minibosses.md "25 to 35 seconds of fire ... tune with the bot"): the careful bot's time to kill each at its
// home Track (8 seeds) brought into the page's window by these factors on the page's health (docs/PROGRESS.md, M5 part 1).
constexpr float kLongwaveHealthTune = 2.2f;  // 70 s with the careful bot (M5 part 1; part 3, the Trio dealt: 1.4 -> 2.2; 2.7 gave 86 s but the careless bot 3 of 20 in the first pass)
constexpr float kChoirHealthTune = 4.2f;      // 65 s with the careful bot (part 3, the Trio dealt: 4.5 -> 4.2; 4.8 gave 70 s, with the above)
constexpr float kMiniHealthTune[8] = {6.5f, 5.0f, 2.0f, 4.8f,    // Alpha Hound, The Hive, Battering Ram, Carom King: 25-35 s
                                     12.0f, 10.0f, 5.0f, 4.0f};  // M5 part 4: Moth Queen, Brood Mother, Blink Star, Sentry Array (x 1: 4, 5, 7, 11 s)
// M5 part 4, mini-bosses batch 2 (minibosses.md gives no number for these).
constexpr float kMqBurstSpeed = 200.0f;      // a court Moth's burst (as a Moth's own bullets)
constexpr float kMqCourtSpeed = 600.0f;      // a court Moth's step inward (it reaches its place within the beat)
constexpr float kMqMoveSpeed = 300.0f;       // the Queen keeps her distance at up to this
constexpr float kBsDecoyBulletSpeed = 220.0f;  // a decoy's fan (half health)
constexpr float kBsDecoyFanDeg = 15.0f;
constexpr float kSaStepBeats = 1.0f;         // turrets step round to stand evenly over a beat after a death

// M5 part 4, Event Horizon (horizon.md gives no number for these).
constexpr float kHzJetLengthWu = 1800.0f;  // the jets reach past the frame's edge at the fight's zoom (0.8)
constexpr float kHzJetHalfWu = 22.0f;      // a beam as wide as the Choir's held note
constexpr float kHorizonHealthTune = 16.0f; // the careful bot at home: x 1 9 s, x 6 25 s (the page: about 80 s)
// Polyrhythm (polyrhythm.md gives no number for these).
constexpr float kPrLinkBurnHalfWu = 16.0f;  // the burning link: a beam as wide as the Choir's cage strings
constexpr float kPolyHealthTune = 3.0f;     // the careful bot at home: x 1 9 s, x 8 35 s with 7 wall hits a fight and half lost (the page: 100-120 s)
constexpr float kFeedbackHealthTune = 2.5f; // the careful bot at home: x 1 34 s (the page: about two minutes)

// ======================================================================================================
// M4 STAGE A: co-op (GAME_DESIGN 10, TECH_SPEC 8). Quoted numbers are marked; the rest are chosen where the
// documents give no number for scaling with the player count (listed in docs/PROGRESS.md, M4 stage A).
// ======================================================================================================
constexpr float kCoopBudgetPerPlayer = 0.6f;      // GAME_DESIGN 8 (quoted): threat budget x (1 + 0.6 x (players - 1))
constexpr float kCoopBossHealthPerPlayer = 0.5f;  // GAME_DESIGN 8 (quoted): boss health x (1 + 0.5 x (players - 1))
constexpr u32 kCoopAliveCapPerPlayer = 150;       // the director's alive cap grows this much a seat (900 -> 1,350 with four):
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
