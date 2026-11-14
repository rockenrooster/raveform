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
// GAME_DESIGN 7.6 (b1d4a2f): "the Ram's horns, the Sentry Array's domed turrets, the Brood Mother's dome and whatever else of a
// mini-boss's own body took nothing take a quarter" (the Hive's decoy is light, not its body: it still stops a shot for nothing).
constexpr float kMiniArmour = 0.25f;
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
constexpr float kBsBeamWu = 10.0f;                                    // pattern[1]; look.md v2.5: 10 wu (part 4: 16)
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

// ---- Cadence (design/bosses/cadence.md, cadence.json's prose; M5 part 6)
constexpr float kCdFollow = 40.0f;            // numbers.follow: "the square's centre follows the players' centre at 40 wu/s"
constexpr float kCdModulationAt = 0.66f, kCdInversionAt = 0.33f;  // phases: "100 to 66%", "66 to 33%", "33 to 0%"
constexpr u32 kCdTurnEvery = 4;               // Modulation: "On every fourth bar line, as the song comes back to I, the square turns a quarter turn"
constexpr u32 kCdSnareFan = 7;                // "Each firing pillar adds a second fan of seven on the snare"
constexpr u32 kCdRosetteOne = 16, kCdRosetteThree = 12;  // Inversion: "a rosette of sixteen on the One and twelve on three"
constexpr u32 kCdShards = 300;                // reward: "300 Shards for each player"

// ---- Arpeggiator (design/bosses/arpeggiator.md, arpeggiator.json's prose; M5 part 6)
constexpr float kApPlaceWu = 380.0f, kApFollow = 120.0f;  // place: "380 wu from the players' centre on its side ... follows them along its own axis at 120 wu/s"
constexpr float kApEighthsAt = 0.66f, kApTwoHandsAt = 0.33f;  // phases: "100 to 66%", "66 to 33%", "33 to 0%"
constexpr float kApOne = 2.0f;                // "x 2 on the core for that beat" (the shutter open for the first beat of every bar)
constexpr u32 kApMoveEvery = 4;               // Eighth notes: "Every fourth bar it moves to the next side of the players"
constexpr u8 kApLanesG[16] = {1, 2, 3, 4, 3, 2, 3, 4, 1, 4, 3, 4, 2, 1, 4, 3};  // laneOfNote: "For G that is 1 2 3 4 3 2 3 4 1 4 3 4 2 1 4 3"
constexpr u8 kApStabSteps[6] = {0, 3, 6, 9, 12, 14};  // Two hands: "the chord stab's rhythm (steps 0, 3, 6, 9, 12, 14)"
constexpr float kApConsoleW = 900.0f, kApConsoleD = 120.0f;  // parts: "A console 900 wu wide and 120 deep"

// ---- Fata Morgana (design/bosses/morgana.md; M5 part 6)
constexpr float kMgFigureWu = 900.0f;     // drift: "the oasis drifts along a figure of eight 900 wu wide at 110 wu/s"
constexpr float kMgDriftSpeed = 110.0f;
constexpr float kMgOrbitOut = 70.0f;      // orbit: "Morgana circles the oasis 70 wu outside its edge, one turn every four bars"
constexpr u32 kMgOrbitBars = 4;
constexpr u32 kMgRestEvery = 4;           // Drift: "Every fourth bar the oasis rests for the bar and she crosses it from side to side"
constexpr float kMgOne = 2.0f;            // "x 2 while she is over the water"; Crossing: "x 2 for the first beat after each leap"
constexpr float kMgMirageAt = 0.66f;      // phases: Drift 100 to 66%, Mirage 66 to 33%, Crossing 33 to 0%
constexpr float kMgCrossingAt = 0.33f;
constexpr float kMgMirageWu = 500.0f;     // Mirage: "a second oasis appears 500 wu from the first"
constexpr u32 kMgMirageBars = 2;          // "after two bars it is gone"
constexpr float kMgLeapWu = 600.0f;       // Crossing: "the next one is drawn dashed 600 wu away for the bar before"
constexpr u32 kMgLeapEvery = 2;           // "Every second bar the oasis leaps"
constexpr float kMgCorridorWu = 160.0f;   // "a corridor of calm 160 wu wide joins the two"
constexpr u32 kMgKickSteps[3] = {0, 6, 10};  // the storm: "this Track's broken kick: steps 0, 6 and 10"
constexpr u32 kMgSnareSteps[2] = {4, 12};    // the mirage "goes out for an eighth on every snare (two and four)"
constexpr u32 kMgSandStreaks = 15;        // "fifteen streaks of sand in the danger colour at 0.8" (sixty at 0.5 until the
constexpr float kMgSandLevel = 0.8f;       // owner's red-line pass, 2026-11-14)
constexpr float kMgSandWu = 4.0f;          // "each drawn as a lethal line is (... a 4 wu capsule ...)"
constexpr float kMgEdgeLevel = 2.2f;      // the oasis: "a ring in the palette's accent at 2.2 with a second ring 12 wu inside it"
constexpr float kMgInnerRingWu = 12.0f;
constexpr float kMgLatticeWhipWu = 10.0f; // "the lattice whipped 10 wu along the wind"
constexpr float kMgStormLowPassHz = 2000.0f;  // sound: "outside it the music bus is also low-passed at 2 kHz"

// ---- Kaleidos, Reprise (design/bosses/reprise.md; M5 part 6)
constexpr float kRpScale = 1.25f;         // the wheel: "Kaleidos as built, a quarter larger"
constexpr u32 kRpTurnBars = 8;            // turn: "one revolution every eight bars"
constexpr float kRpMedleyAt = 0.70f;      // phases: Reprise 100 to 70%, Medley 70 to 20%, Last Light 20 to 0%
constexpr float kRpLastAt = 0.20f;
constexpr u32 kRpHitSteps[4] = {0, 4, 10, 12};  // Reprise: "on the two-step's hits (kick on steps 0 and 10, snare on 4 and 12)"
constexpr u32 kRpFan = 5;                 // "mirrored fans of five from each pair of arms"
constexpr float kRpOne = 2.0f;            // "the core is open, x 1, and x 2 on the One" (Reprise and Last Light)
constexpr u32 kRpSoloBars = 2;            // Medley: "The arms take turns round the wheel, two bars each"
constexpr u32 kRpWaveSegments = 8;        // Longwave's arm: "a wave of eight segments that crosses the frame once"
constexpr float kRpPullWu = 200.0f;       // Event Horizon's arm: "ships and bullets are drawn toward the wheel at 200 wu/s"
constexpr u32 kRpRing = 24;               // "a ring of twenty-four leaves the core on its second bar line"
constexpr float kRpArpDegPerSemi = 15.0f; // Last Light: "its direction 15 degrees round for every semitone the note stands above
constexpr float kRpArpBarDeg = 45.0f;     // the bar's lowest, the whole figure turning 45 degrees a bar, 260 wu/s"
constexpr float kRpArpSpeed = 260.0f;
constexpr u32 kRpBulletCap = 200;         // caps: "200 bullets"

// ---- Mini-bosses batch 3 (design/bosses/minibosses.md; M5 part 6)
constexpr float kMmBulletSpeed = 400.0f;  // Mirror Mimic: "one bullet leaves it along the player's aim turned half round, 400 wu/s"
constexpr u32 kMmLateBeats = 1;           // "it copies the player's Dash one beat late"
constexpr u32 kMmOpenBeats = 2;           // "for the beat of its late Dash and the beat after, it cannot fire and takes damage x 2"
constexpr u32 kMmCap = 60;                // cap: "at most 60 bullets"
constexpr float kTtApart = 520.0f;        // Twin Tether: "two nodes 520 wu apart turn about their middle, one turn every two
constexpr u32 kTtTurnBars = 2;            // bars, and the middle follows the nearest player at 60 wu/s"
constexpr float kTtFollow = 60.0f;
constexpr u32 kTtSlackBeats = 1, kTtPerfectBeats = 2;  // "one beat after any player dashes through the beam (two after a Perfect Dash)"
constexpr float kTtFlail = 420.0f;        // half: "a beam 420 wu long turning once a bar about itself"
constexpr float kWvRadii[4] = {300.0f, 250.0f, 200.0f, 150.0f};  // Weaver Prime: laps "50 wu tighter than the last (300, 250, 200, 150)"
constexpr u32 kWvLapBeats = 3;            // "each lap takes three beats"
constexpr u32 kWvTrailBars = 2;           // "a lethal trail that lasts two bars"
constexpr float kWvBurstWu = 40.0f;       // "every trail segment alive bursts 40 wu wide"
constexpr u32 kWvSegments = 160;          // cap: "160 trail segments"
constexpr float kDwDrift = 30.0f, kDwDriftHalf = 90.0f;  // Deep Well: drifts at 30 wu/s (half: 90)
constexpr float kDwBend = 400.0f;         // "shots within 400 wu bend into it and ships within 300 wu are drawn in at up to 120 wu/s"
constexpr float kDwPull = 300.0f, kDwPullMax = 120.0f;
constexpr float kDwPerBullet = 3.0f;      // "a ring of bullets, one for every 3 damage swallowed, at most 48, 220 wu/s"
constexpr u32 kDwRingMax = 48;
constexpr float kDwRingSpeed = 220.0f;
constexpr float kDwStomach = 0.05f;       // "its stomach holds 5% of the Track's mini-boss health in one bar"
constexpr float kDwStomachHalf = 1.5f;    // half: "it holds half as much again"
constexpr u32 kDwShardsMax = 300;         // death: "a ring of Shards, one for everything it swallowed in the fight, to 300"

// ---- The arena (design/arena.md, arena.json's rules; M5 part 5)
constexpr float kArTrack1Scale = 300.0f;      // "the mini-boss health of that Track ... (Track 1 uses 300)"
constexpr float kArPlaceNearWu = 1000.0f, kArPlaceFarWu = 1800.0f;  // structures "1,000 to 1,800 wu from the players' centre"
constexpr float kArApartWu = 700.0f;          // "at least 700 wu from each other"
constexpr float kArWallKeepWu = 400.0f;       // "never within 400 wu of a wall"
constexpr float kArStayNearWu = 600.0f;       // "unless a player is within 600 wu, in which case they stay until that player leaves"
constexpr u32 kArMaxStructures = 6;           // caps: "at most 6 structures and 1 event alive"
constexpr float kArVaultHealth = 0.33f;       // Vault: "health: trackScale x 0.33"
constexpr u32 kArVaultShards = 40;            // "one Boost orb and 40 Shards for each player"
constexpr float kArPylonRingWu = 150.0f;      // Pylon: "its ring is 150 wu"
constexpr float kArPylonFillBeats = 4.0f;     // "stand inside for four beats"
constexpr float kArPylonEmpty = 2.0f;         // "leave and it empties twice as fast"
constexpr float kArPylonPulseWu = 520.0f, kArPylonPulseSpeed = 700.0f;  // "a ring that runs to 520 wu at 700 wu/s"
constexpr float kArPylonDamage = 6.0f;        // "deals 6 x the Track's number to each enemy once"
constexpr u32 kArPylonBars = 36, kArPylonBarsT1 = 24;  // "one cycle (36 bars; 24 in Track 1)"
constexpr float kArRiftHealth = 0.25f;        // Rift: "health: trackScale x 0.25"
constexpr u32 kArRiftPerBar = 4;              // "four of the Track's fodder come through it every bar"
constexpr u32 kArRiftShards = 30;             // "one Boost orb and 30 Shards for each player"
constexpr float kArBumperKick = 600.0f;       // Bumper: "a kick of 600 wu/s"
constexpr float kArBumperShotDamage = 1.5f;   // "a free bounce, and x 1.5 damage afterwards"
constexpr u32 kArBumperCharge = 8;            // "at eight it discharges on the next beat"
constexpr float kArBumperRingWu = 260.0f, kArBumperDamage = 10.0f;  // "a ring to 260 wu dealing 10 x the Track's number"
constexpr float kArBumperApart[2] = {300.0f, 500.0f};  // "300 to 500 wu apart: a cluster"
constexpr float kArCurrentWidth = 240.0f, kArCurrentLength = 1600.0f, kArCurrentPush = 260.0f;  // Current: "240 wu wide and 1,600 long ... 260 wu/s faster"
constexpr float kArCurrentWith = 1.3f, kArCurrentAgainst = 0.7f;  // "30% faster and deal 30% more; against it, 30% slower and no less"
constexpr float kArLensLength = 320.0f;       // Lens: "a line 320 wu long"
constexpr float kArLensFanDeg = 14.0f, kArLensSide = 0.5f;  // "the two beside it fan 14 degrees out at half damage"
constexpr float kArLensTurnBars = 8.0f;       // "one turn every eight bars"
constexpr float kArVaultDropNear = 1000.0f, kArVaultDropFar = 1400.0f;  // Vault Drop: "1,000 to 1,400 wu from the players' centre"
constexpr float kArVaultDropHealth = 0.4f;    // "health trackScale x 0.4; broken: two Boost orbs and 80 Shards"
constexpr u32 kArVaultDropBoosts = 2, kArVaultDropShards = 80;
constexpr float kArSurgeWu = 900.0f;          // Rift Surge: "three Rifts open on a triangle 900 wu from the players' centre"
constexpr u32 kArSurgePerBar = 6;             // "each letting six through a bar"
constexpr u32 kArSurgeAllShards = 100;        // "all three: a Cell for each hurt player, 100 Shards for the others"
constexpr float kArHuntWu = 1200.0f;          // Elite Hunt: "warps in 1,200 wu away"
constexpr u32 kArHuntTraits = 2, kArHuntTraits7 = 3;  // "carrying two traits (three from Track 7)"
constexpr float kArHuntFlee = 0.8f;           // "flees the nearest player at 0.8 of its speed"
constexpr u32 kArHuntShards = 150;            // "a Trio for each player (as a level's) and 150 Shards"
constexpr float kArCometPassWu = 300.0f, kArCometSpeed = 700.0f;  // Comet: "passing within 300 wu of the players' centre at 700 wu/s"
constexpr u32 kArCometShardsBeat = 5;         // "leaves five Shards a beat behind it"
constexpr float kArCometHealth = 0.1f;        // "health trackScale x 0.1; broken: 60 Shards and one Boost orb"
constexpr u32 kArCometShards = 60;
constexpr float kArCometWarnBeats = 2.0f;     // "a white chevron two beats ahead"
constexpr float kArEclipseScene = 0.15f, kArEclipseLattice = 0.05f;  // Eclipse: "scene light x 0.15 between beats ... the lattice at 0.05"
constexpr float kArEclipseCalm = 0.4f, kArEclipseCalmFlash = 0.3f;   // "reduce flashing: x 0.4 between beats and the flash adds 0.3"
constexpr float kArEclipseScore = 2.0f;       // "kills score double while it lasts"
constexpr u32 kArMeteorsBeat = 3;             // Meteor Rain: "on every beat, three dashed danger rings of 110 wu"
constexpr float kArMeteorWu = 110.0f, kArMeteorBeats = 2.0f, kArMeteorDamage = 20.0f;  // "two beats later ... 20 x the Track's number"
constexpr u32 kArConvoyCarriers = 3, kArConvoyHounds = 8;  // Convoy: "three carriers ... with eight Hounds of the Track's tier"
constexpr float kArConvoySpeed = 60.0f, kArConvoyFromWu = 1500.0f;  // "at 60 wu/s, starting 1,500 wu from the players' centre"
constexpr float kArCarrierHealth = 0.33f, kArCarrierRadius = 44.0f;  // "each carrier: health trackScale x 0.33", look: "radius 44"
constexpr u32 kArCarrierShards = 50;          // "one Boost orb and 50 Shards for each player; the last of the three also drops a Cell"
constexpr float kArCarrierApart = 120.0f;     // (chosen) the line's spacing
constexpr float kArStormWu = 320.0f, kArStormSpeed = 90.0f;  // Storm Cell: "a circle of 320 wu drifting toward the players' centre at 90 wu/s"
constexpr u32 kArStormBodies = 6;             // "lightning joins up to six bodies inside it"
constexpr float kArStormDamage = 15.0f;       // "15 x the Track's number to each enemy"
// Elites (design/traits.md; traits.json carries their numbers)
constexpr u32 kElFromTrack = 3;                // "From: Track 3, cycle 1" (d59062f; it was Track 2's second cycle)
constexpr float kElPaceBarsT3 = 16.0f, kElPaceBarsT6 = 8.0f;  // "about once in 16 bars in Track 3, falling evenly to once in 8 bars by Track 6"
constexpr float kElWarpWu = 90.0f;            // "warps in like any enemy but from 90 wu"

}  // namespace rf::sim::prose
