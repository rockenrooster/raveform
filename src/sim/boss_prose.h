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
constexpr u32 kHzShards = 300;               // death: "a fountain of 300 Shards" (horizon.md Numbers: 300 Shards per player)
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
// ---- Batch 2 (minibosses.json; M5 part 4)
// Moth Queen
constexpr float kMqHoldWu = 520.0f;                                   // move: "holds 520 wu from the nearest player"
constexpr double kMqOrbitBars = 8.0;                                  // move: "one turn every eight bars"
constexpr u32 kMqCourt = 12, kMqHalfCourt = 8;                        // pattern[0], half
constexpr float kMqCourtFrom = 420.0f, kMqCourtTo = 140.0f;           // pattern[0], [1]
constexpr u32 kMqCycleBars = 4;                                       // pattern[0]: "a court is four bars"
constexpr u32 kMqPetals = 5;                                          // pattern[1]
constexpr float kMqRosetteTurnDeg = 12.0f, kMqPetalSpeed = 240.0f, kMqPetalReach = 700.0f;  // pattern[1]
constexpr u32 kMqBurst = 6;                                           // pattern[2]: "a ring of six bullets"
// Brood Mother
constexpr float kBmDrift = 40.0f;                                     // move
constexpr u32 kBmClutch = 6, kBmHalfClutch = 5;                       // pattern[0], half
constexpr float kBmClutchWu = 170.0f;                                 // pattern[0]
constexpr double kBmHatchBeats = 2.0;                                 // pattern[1] (beat 1 to beat 3), half
constexpr float kBmShut = 0.25f, kBmExposed = 2.0f;                   // pattern[2], weakPoint
constexpr u32 kBmCap = 36;                                            // cap
constexpr u32 kBmLastEggs = 16, kBmEggShards = 10;                    // death
// Blink Star
constexpr float kBsStarWu = 420.0f, kBsHalfScale = 1.25f;             // move, half
constexpr float kBsBeamWu = 16.0f;                                    // pattern[1]
constexpr u32 kBsDecoyFan = 3;                                        // half
// Sentry Array
constexpr u32 kSaTurrets = 8;                                         // move
constexpr float kSaRingWu = 340.0f;                                   // move
constexpr float kSaHandStepDeg = 45.0f;                               // pattern[0]
constexpr u32 kSaFan = 5;                                             // pattern[1]
constexpr float kSaBulletSpeed = 220.0f;                              // pattern[1]
constexpr u32 kSaHalfAt = 4;                                          // half: "with four left"

// ---- Event Horizon (design/bosses/horizon.md, horizon.json's prose; M5 part 4)
constexpr float kHzLaneWu = 1300.0f;          // Lanes: "spiral arms 1,300 wu long ..."
constexpr float kHzLaneWindDeg = 270.0f;      // "... that wind 270 degrees into the Well"
constexpr float kHzWaveSpeed = 600.0f;        // Tidal: "a ring leaves the Well at 600 wu/s ..."
constexpr float kHzWaveShove = 140.0f;        // "... and shoves ships and Shards 140 wu outward"
constexpr float kHzThrowWu = 300.0f;          // contact: "the ship is thrown 300 wu out"
constexpr float kHzArmTurnDeg = 12.0f;        // eruption: "each arm turning 12 degrees per bullet"
constexpr u32 kHzEruptBase = 8, kHzEruptPer = 4, kHzEruptMax = 96;  // eruption: "8 + 4 per body eaten, at most 96"
constexpr float kHzAxisTurnDeg = 45.0f;       // jets: "the axis turns 45 degrees every bar"
constexpr u32 kHzShardsPerBody = 5;           // "five uncollected Shards that fall in count as one body"
constexpr float kHzLatticeImpulse = 400.0f;   // Look: "a standing impulse of 400 at the horizon"

// ---- Polyrhythm (design/bosses/polyrhythm.md, polyrhythm.json's prose; M5 part 4)
constexpr float kPrHemiolaShare = 0.6f;       // phases: "60% and below" of their joint health
constexpr float kPrFourHemiolaSpeed = 300.0f; // Hemiola: "Four's walls fly at 300"
constexpr float kPrPivotSpeed = 50.0f;        // "the pivot follows the players' centre at 50 wu/s"
constexpr double kPrLinkTurnBars = 8.0;       // "the link turns once every 8 bars"
constexpr float kPrSoloWalk = 70.0f;          // Solo: "walks toward the nearest player at 70 wu/s"
constexpr u32 kPrShards = 240;                // Reward: "240 Shards per player"
constexpr float kPrLatticeImpulse = 160.0f;   // Look: "an impulse of 160 under each rotor on each of its pulses"
constexpr float kPrWallThinS = 0.5f;          // Look: walls "thinning to 2 px in their last half second"

// ---- Feedback (design/bosses/feedback.md, feedback.json's prose; M5 part 4)
constexpr float kFbRegenAt = 0.6f, kFbHowlAt = 0.3f;  // phases: "above 60%", "60% to 30%", "30% and below"
constexpr float kFbMinWu = 220.0f, kFbMaxWu = 900.0f; // record: "the distance from the Head is clamped to 220-900 wu"
constexpr double kFbPlayingSteps = 2.0;       // "playing from each echoed primary shot until two sixteenths after it"
constexpr u32 kFbShotCap = 500;               // cap: "at most 500 echo shots alive"
constexpr u32 kFbBarCap = 120;                // "a player's bar holding more than 120 shots is played back with every second shot left out"
constexpr u32 kFbShards = 320;                // reward: "320 Shards for each player"
constexpr float kFbLatticePullPx = 20.0f;     // Howl: "the lattice inside it is pulled 20 px toward the Head on every bar line"
constexpr float kFbConePush = 0.15f;          // Look: "the cone pushes out 15% on every kick"
constexpr float kFbNullBand = 30.0f;          // (chosen) a Null's front: as thick as the Nova's

}  // namespace rf::sim::prose
