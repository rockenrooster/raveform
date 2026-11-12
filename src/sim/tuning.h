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
constexpr float kDrifterHomeTurn = 0.15f;     // rad/s: drifters' wander leans toward the nearest player
constexpr float kHoundSeparationWu = 26.0f;   // hounds keep this far apart (a pack, not a point)
constexpr float kHoundBurstSpread = 0.12f;    // rad: hound_B's three shots fan this far apart
constexpr float kShoalCohesion = 0.9f, kShoalAlignment = 1.1f, kShoalSeparation = 2.6f, kShoalSeek = 1.0f;
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
// Score = enemy value x Chain (GAME_DESIGN 5). The Chain starts at 0, so the multiplier is max(1, Chain).
constexpr u32 kChainFloor = 1;

// ---- The simple spawner (stage A; the director replaces it) ----
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

}  // namespace rf::sim::tuning
