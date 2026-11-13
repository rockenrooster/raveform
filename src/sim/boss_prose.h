#pragma once
// ======================================================================================================
// NUMBERS THE BOSS PAGES GIVE ONLY IN PROSE (M5 part 1).
//
// Every number a design/bosses/*.json gives as a value is parsed from it (sim/boss.cpp). These are the ones
// the pages give only in words ("a fan of three to each side, 16 degrees apart"), quoted here with the page
// and the place. They are design numbers, not chosen ones (those are in sim/tuning.h).
// ======================================================================================================

#include "core/types.h"

namespace rf::sim::prose {

// ---- Longwave (longwave.md, longwave.json) ----
constexpr float kLwChopSpreadDeg = 16.0f;    // phases[1].fire.pattern: "a fan of three to each side, 16 degrees apart"
constexpr u32 kLwWhitecapLungeBars = 2;      // phases[2].lunge.every: "second bar"
constexpr u32 kLwBareLungeBars = 1;          // phases[3].lunge.every: "bar"
constexpr float kLwWallKeepWu = 1100.0f;     // path.anchor: "kept at least 1100 wu from every wall"
constexpr double kLwEntranceBars = 2.0;      // entrance: "the body follows it onto the figure of eight over two bars"
constexpr double kLwJawBeats = 1.0;          // jaw.open: "the whole of beat 1 of every bar"
constexpr double kLwLungeJawBeats = 2.0;     // phases[2].lunge.note: "its jaw stays open for two beats"
constexpr float kLwCrowdShare = 0.3f;        // longwave.md Numbers: "the director's budget drops to 30% and only fodder
                                             // families spawn" (the crowd's share of its Track's target in the fight)

// ---- The Choir (choir.md, choir.json) ----
constexpr float kChFanCallDeg = 11.0f;       // phases[0].call.pattern: "a fan at the nearest player, 11 degrees apart"
constexpr float kChFanAnswerDeg = 14.0f;     // phases[1].response.pattern: "14 degrees apart"
constexpr float kChFugueSweepDeg = 80.0f;    // phases[2].call.pattern: "a stream that sweeps 80 degrees"
constexpr float kChTurnBars = 16.0f;         // ring.turn: "one revolution every 16 bars"
constexpr float kChFollowWu = 60.0f;         // ring.centre: "it follows the centroid at 60 wu/s"
constexpr float kChWallKeepWu = 700.0f;      // ring.centre: "kept 700 wu from every wall"
constexpr u32 kChCanonCageBars = 4;          // phases[1].cage.every: "fourth bar"
constexpr u32 kChFugueCageBars = 2;          // phases[2].cage.every: "second bar"
constexpr u32 kChHeldBars = 4;               // phases[3].heldNote.every: "fourth bar"
constexpr double kChEntranceBars = 2.0;      // entrance: "firing starts on bar 3"
constexpr double kChSingBeats = 1.0;         // singing: "open from the step it fires until one beat later"
constexpr u32 kChoirShards = 220;            // death: "then 220 Shards" (choir.md Numbers: 220 Shards per player)

// ---- Mini-bosses (minibosses.md, minibosses.json) ----
constexpr double kMiniHoldBars = 8.0;        // common.drop: "for at most 8 bars past its 16"
constexpr double kMiniCardBars = 1.0;        // common.arrives: "a small name card holds for one bar"
constexpr float kMiniScoreTier5 = 25.0f;     // common.reward: "25 times a tier-5 body of its family"
// Alpha Hound
constexpr float kAlphaCircleWu = 420.0f, kAlphaCircleSpeed = 170.0f;  // move
constexpr u32 kAlphaPack = 6, kAlphaHalfPack = 5;                     // pattern[0], half
constexpr float kAlphaChevronWu = 500.0f;                             // pattern[0]
constexpr float kAlphaChargeSpeed = 520.0f;                           // pattern[1]
constexpr double kAlphaChargeBeats = 2.0;                             // pattern[1]
constexpr u32 kAlphaLungeBars = 4, kAlphaHalfLungeBars = 3;           // pattern[2], half
constexpr float kAlphaLungeSpeed = 900.0f;                            // pattern[2]
constexpr double kAlphaWindedBeats = 2.0;                             // weakPoint
constexpr double kAlphaScatterBars = 2.0;                             // death
constexpr u32 kAlphaCap = 30;                                         // cap
// The Hive
constexpr u32 kHiveDarts = 120, kHiveHatch = 12, kHiveMax = 160, kHiveHalfDarts = 60;  // move, cap, half
constexpr u32 kHiveShapeBars = 2;                                     // pattern[0]
constexpr float kHiveRingFrom = 300.0f, kHiveRingTo = 120.0f, kHiveRingGapDeg = 60.0f;  // pattern[1]
constexpr float kHiveSpearSpeed = 700.0f;                             // pattern[2]
constexpr float kHiveWallLen = 900.0f, kHiveWallSpeed = 160.0f, kHiveWallGap = 90.0f;  // pattern[3]
// Battering Ram
constexpr float kRamFrontDeg = 120.0f;                                // move: "its front 120 degrees sheds every shot"
constexpr float kRamChargeSpeed = 1400.0f;                            // pattern[1]
constexpr float kRamWallWu = 300.0f;                                  // pattern[2]
constexpr u32 kRamWallBullets = 16;                                   // pattern[2]
constexpr double kRamStunBeats = 3.0, kRamHalfStunBeats = 2.0;        // weakPoint, half
constexpr double kRamWakeBars = 1.0;                                  // half
// Carom King
constexpr float kCaromSpeed = 300.0f;                                 // move
constexpr float kCaromBounceGain = 0.08f, kCaromMaxFactor = 1.6f;     // move
constexpr u32 kCaromBounceBullets = 5;                                // pattern[0]
constexpr float kCaromBounceFanDeg = 40.0f;                           // pattern[0]: "fanned 40 degrees either side"
constexpr float kPrinceRadius = 40.0f, kPrinceHealth = 0.12f, kPrinceSpeed = 1.3f;  // pattern[1]
constexpr float kKingRadii[4] = {80.0f, 68.0f, 56.0f, 44.0f};         // pattern[1]
constexpr float kKingSplitSpeed = 1.15f;                              // pattern[1]: "15% faster"
constexpr u32 kPrinceCaroms = 2;                                      // pattern[2]
constexpr double kCaromHotBeats = 1.0;                                // weakPoint
constexpr u32 kCaromCap = 6, kCaromBulletCap = 60;                    // cap

}  // namespace rf::sim::prose
