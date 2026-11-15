// THE FIGHTS' SOUNDS AND THE PENDING ONES (M5): see pending_sounds.h. One row a cue; the engine's recipe where it can say
// what the page asks, else the nearest sound the band has and, in `pending`, what is missing.
// M5 audio (docs/PROGRESS_m5_audio.md): every row plays what its page asks. The engine's commands go through an
// audio::CommandSink (the band in the game, an engine offline for the listening sheet), built here.

#include "game/pending_sounds.h"

#include <cmath>

#include "audio/bake.h"
#include "audio/command_sink.h"
#include "audio/cues.h"
#include "audio/engine.h"
#include "sim/arena.h"
#include "sim/world.h"

namespace rf {

using sim::BossCue;
using sim::BossKind;
using audio::CmdType;
using audio::Command;
using audio::NoteFrom;
using audio::Recipe;
using audio::Sfx;
using audio::Timing;
using audio::chordNote;

namespace {
constexpr u8 sx(Sfx s) { return static_cast<u8>(s); }
constexpr u8 rc(Recipe r) { return static_cast<u8>(r); }
constexpr double kTicksPerBeatD = 96.0;
constexpr double kTicksPerBarD = 384.0;

// M5 audio, engine choices where a page gives no number (starting values for the user's ear):
constexpr float kEchoDb9 = 0.355f, kEchoDb15 = 0.178f;  // feedback.md: 9 dB and 15 dB down (as written)
constexpr float kEchoCut = 1200.0f, kEchoCutDark = 700.0f;  // feedback.md: 1.2 kHz (as written); "darker": 700 Hz
constexpr float kEchoPan = 0.8f;                        // "panned to the far side"
constexpr float kFbOpenTone = 1.6f, kFbShutTone = 0.55f, kFbShutGain = 0.5f;  // the pad opened; ducked (-6 dB) and dull
constexpr float kFbHowlFeedback = 0.25f;                // Howl: the delay's feedback turned up by this (16 repeats)
constexpr float kFbRootGain = 0.25f;                    // "quietly"
constexpr float kHzDroneGain = 0.6f;                    // the drone under the band (the sub sine's own level x)
constexpr float kBroodGain = 0.3f;                      // each thread's pad voice
constexpr float kTetherGain = 0.8f, kTetherCutLo = 300.0f, kTetherCutHi = 2400.0f;  // the Sweeper's drone's sweep
constexpr float kRiftGain = 0.7f, kRiftCutLo = 300.0f, kRiftCutHi = 2400.0f;
constexpr float kStormGain = 1.0f, kStormDensityLo = 4.0f, kStormDensityHi = 80.0f;
constexpr float kMgCrackleGain = 0.8f, kMgCrackleDensity = 45.0f;
constexpr float kStabOne = 1.2f;                        // cadence.md: the stab at 1.2 (as written)
constexpr float kChoirSend = 0.5f;                      // choir.md: reverb send 0.5 (as written)
// The lead's rulings (cost): held pads and drones one voice each since the Moto measured 29.0% (the nearest open Rift's drone;
// the Brood Mother's threads one pad voice while any broodling lives).
constexpr u32 kRiftDrones = 1;
constexpr u8 kBroodVoices = 1;

// In sim::BossCue's order.
const FightSound kRows[] = {
    // ---- Longwave (design/bosses/longwave.md "Sound")
    {BossCue::LwRun, "longwave.md", "a rubber-string voice (the bass patch an octave up, decay x 2), one note on every crest step: the crest's place "
                                    "in its wave as a degree of the Track's scale (0 2 3 5 7 8 10 12 above the chord root)",
     nullptr, SoundPlay::Recipe, rc(Recipe::RubberString), false, 0.45f, 1, 0, NoteRule::RunDegree},
    {BossCue::LwTrough, "longwave.md", "the telegraph tick on every step a trough fires", nullptr, SoundPlay::Sfx, sx(Sfx::Tick), false, 0.45f, 0, 0},
    {BossCue::LwJaw, "longwave.md", "the elite-arrives patch, its low fifth, on beat 1 while the jaw is open", nullptr, SoundPlay::Sfx, sx(Sfx::Elite),
     false, 0.6f, 0, 0},
    {BossCue::LwLunge, "(not on the page; chosen)", "a closing sweep as the head dashes", nullptr, SoundPlay::Recipe, rc(Recipe::Sweep), false, 0.8f, -1,
     1},
    {BossCue::LwWhip, "longwave.md", "the impact a fifth up, on the whip's downbeat", nullptr, SoundPlay::Recipe, rc(Recipe::Impact), false, 0.8f, 7,
     0},
    // ---- The Choir (choir.md "Sound")
    {BossCue::ChVoice, "choir.md", "pad_voice: the pad patch as a voice (attack 20 ms, release one beat, reverb send 0.5), the Voice's own note every "
                                   "time it sings",
     nullptr, SoundPlay::PadVoice, rc(Recipe::PadVoice), true, 0.5f, 0.5f, 1, NoteRule::None, 0, 0.0f, 0.0f, 0.0f, 4, kChoirSend},
    {BossCue::ChDeath, "choir.md", "the dying Voice's note falling a fourth over one beat into the reverb",
     nullptr, SoundPlay::PadVoice, rc(Recipe::PadVoice), true, 0.5f, 1, 1, NoteRule::Midi, 0, -5.0f, 0.0f, 1.0f, 4, kChoirSend},
    {BossCue::ChCageTick, "choir.md", "the telegraph tick on every beat of the cage's telegraph bar", nullptr, SoundPlay::Sfx, sx(Sfx::Tick), false, 0.6f,
     0, 0},
    {BossCue::ChCageBurn, "choir.md", "the impact as the cage burns", nullptr, SoundPlay::Recipe, rc(Recipe::Impact), false, 0.7f, 0, 0},
    {BossCue::ChHeld, "choir.md", "the last Voice's note held for the telegraph bar, rising an octave as the beam fires",
     nullptr, SoundPlay::PadVoice, rc(Recipe::PadVoice), true, 0.6f, 6, 1, NoteRule::Midi, 0, 12.0f, 4.0f, 0.25f, 4, kChoirSend},
    // ---- Alpha Hound (minibosses.md)
    {BossCue::AlHowl, "minibosses.md", "saw_glide up a fifth over one beat (the howl, beat 1)", nullptr, SoundPlay::Recipe, rc(Recipe::SawGlide), false,
     0.7f, 7, 1},
    {BossCue::AlCharge, "minibosses.md", "roll: the Build's snare roll into beat 3 (the pack's charge)", nullptr, SoundPlay::Recipe, rc(Recipe::Roll),
     false, 0.7f, 2, 0},
    {BossCue::AlLunge, "minibosses.md", "sub_drop (the lunge)", nullptr, SoundPlay::Recipe, rc(Recipe::SubDrop), false, 0.9f, 0, 0},
    // ---- The Hive (minibosses.md)
    {BossCue::HvShape, "minibosses.md", "rev_cymbal into the bar line (a change of shape)", nullptr, SoundPlay::Recipe, rc(Recipe::RevCymbal), false,
     0.6f, 1, 0},
    {BossCue::HvRing, "minibosses.md", "closed hats that tighten as the Ring closes", nullptr, SoundPlay::Hats, rc(Recipe::Hat), false, 0.5f, 8, 0},
    {BossCue::HvSpear, "minibosses.md", "two toms for the Spear", nullptr, SoundPlay::Recipe2, rc(Recipe::Tom), false, 0.6f, 0, 0},
    {BossCue::HvWall, "minibosses.md", "a noise sweep for the Wall", nullptr, SoundPlay::Recipe, rc(Recipe::Sweep), false, 0.5f, 1, 2},
    // ---- Battering Ram (minibosses.md)
    {BossCue::RmLock, "minibosses.md", "a tom roll rising through beats 3 and 4 (the lock)", nullptr, SoundPlay::Roll, rc(Recipe::TomRoll), false, 0.6f,
     2, 12},
    {BossCue::RmCharge, "minibosses.md", "sub_drop and a driven kick (the charge)", nullptr, SoundPlay::Pair, rc(Recipe::SubDrop), false, 1.0f, 0, 0,
     NoteRule::None, rc(Recipe::DrivenKick)},
    {BossCue::RmWall, "minibosses.md", "gong (the wall rings)", nullptr, SoundPlay::Recipe, rc(Recipe::Gong), false, 1.0f, 0, 0},
    {BossCue::RmStun, "minibosses.md", "low_pass_bed while it is stunned (three beats, two from half health)", nullptr, SoundPlay::LowPass, 0, false, 1.0f,
     600, 3},
    // ---- Carom King (minibosses.md)
    {BossCue::CkBounce, "minibosses.md", "a rim shot at the wall's note of the Track's chord (left root, top third, right fifth, bottom seventh); "
                                         "Princes an octave up on the half beats",
     nullptr, SoundPlay::Recipe, rc(Recipe::RimShot), false, 0.6f, 0, 0, NoteRule::WallTone},
    // ---- Every mini-boss
    {BossCue::MiniEnter, "(not on the page; chosen)", "the elite-arrives patch under the name card", nullptr, SoundPlay::Sfx, sx(Sfx::Elite), false, 0.9f,
     0, 0},
    {BossCue::MiniDeath, "minibosses.md", "(its death is a large kill, Play)", nullptr, SoundPlay::None, 0, false, 0, 0, 0},
    {BossCue::MiniGone, "minibosses.md", "(it warps out without a sound)", nullptr, SoundPlay::None, 0, false, 0, 0, 0},
    // ---- Event Horizon (horizon.md "Sound")
    {BossCue::HzEat, "horizon.md", "each body eaten plays the next note of the Track's arp an octave down, bending down a fifth (the arp on the "
                                   "Track's lead patch, P1, gain 0.45 of the lead); the drone, a sub sine on the Track's root sinking a semitone "
                                   "for every three cells and springing back on the bar line, is held for the fight (FightScene)",
     nullptr, SoundPlay::Custom, 0, false, 0.45f, 0, 0},
    {BossCue::HzFed, "horizon.md", "fed: the impact and the gong on the bar line, louder the bigger the meal", nullptr, SoundPlay::Pair,
     rc(Recipe::Impact), false, 0.9f, 0, 0, NoteRule::None, rc(Recipe::Gong)},
    {BossCue::HzStarved, "horizon.md", "starved: the pads and the bass cut out for the first beat under a reversed cymbal (the cymbal swells "
                                       "through that beat)",
     nullptr, SoundPlay::Custom, rc(Recipe::RevCymbal), false, 0.7f, 1, 0},
    {BossCue::HzWave, "horizon.md", "the tidal wave rides in on the riser's last half-beat", nullptr, SoundPlay::Recipe, rc(Recipe::Whoosh), false,
     0.6f, 0.5f, 0},
    // ---- Polyrhythm (polyrhythm.md "Sound")
    {BossCue::PrThree, "polyrhythm.md", "Three is the tom, tuned to the Track's root, on twelfths 0, 4 and 8; it rises a semitone for every "
                                          "quarter of its health lost (Solo: the survivor's pitch)",
     nullptr, SoundPlay::Recipe, rc(Recipe::Tom), false, 0.7f, 0, 0, NoteRule::KeyRoot, 0, 0.0f, 0.0f, 0.0f, 3},
    {BossCue::PrFour, "polyrhythm.md", "Four is the rim, tuned to the flat second, on twelfths 0, 3, 6 and 9; it rises a semitone for every "
                                         "quarter of its health lost",
     nullptr, SoundPlay::Recipe, rc(Recipe::RimShot), false, 0.6f, 0, 0, NoteRule::KeyRoot, 0, 0.0f, 0.0f, 0.0f, 5},
    {BossCue::PrOne, "polyrhythm.md", "the One is both and the gong", nullptr, SoundPlay::Recipe, rc(Recipe::Gong), false, 0.6f, 0, 0},
    {BossCue::PrLinkTick, "polyrhythm.md", "the link burning: the telegraph tick on each beat of the telegraph bar", nullptr, SoundPlay::Sfx,
     sx(Sfx::Tick), false, 0.6f, 0, 0},
    {BossCue::PrLinkBurn, "polyrhythm.md", "the impact on beat 3", nullptr, SoundPlay::Recipe, rc(Recipe::Impact), false, 0.8f, 0, 0},
    // ---- Feedback (feedback.md "Sound")
    {BossCue::FbEcho, "feedback.md", "every echo shot plays its player's own lead note as it was recorded: an octave down, low-passed at 1.2 kHz, "
                                       "9 dB quieter and panned to the far side; the second Echo 15 dB down and darker (700 Hz)",
     nullptr, SoundPlay::Custom, 0, false, 1.0f, 0, 0},
    {BossCue::FbOpen, "feedback.md", "open: the pad's filter opens and the Head holds the Track's root two octaves up, quietly; it rises a semitone "
                                       "for every 10% of health lost; closed: the pad ducked and dull; Howl: the delay's feedback turned up "
                                       "(the held root, the pad's tone and the delay follow the Head: FightScene)",
     nullptr, SoundPlay::Custom, rc(Recipe::PadVoice), false, kFbRootGain, 4, 1},
    {BossCue::FbSwallow, "feedback.md", "a swallowed shot is a closed hat", nullptr, SoundPlay::Recipe, rc(Recipe::Hat), false, 0.4f, 0, 0},
    {BossCue::FbNull, "(not on the page; chosen)", "a Null bursts: the Nova's blast, quieter", nullptr, SoundPlay::Sfx, sx(Sfx::NovaBlast), false,
     0.6f, 0, 0},
    {BossCue::FbCut, "(not on the page; chosen)", "the tape is cut: a closing sweep over two beats", nullptr, SoundPlay::Recipe, rc(Recipe::Sweep), false,
     0.8f, -1, 2},
    // ---- Moth Queen (minibosses.md)
    {BossCue::MqStep, "minibosses.md", "the court is the open hat, stepping round the stereo field with the ring's turn (its pan the sine of the "
                                         "turn)",
     nullptr, SoundPlay::Custom, rc(Recipe::Hat), false, 0.45f, 1, 0},
    {BossCue::MqRosette, "minibosses.md", "each rosette is a plucked note of the Track's arpeggio", nullptr, SoundPlay::Lead, 0xFF, false, 0.3f, 0, 0},
    {BossCue::MqRiser, "minibosses.md", "bar three is a riser", nullptr, SoundPlay::Recipe, rc(Recipe::RevCymbal), false, 0.6f, 4, 0},
    {BossCue::MqBurst, "minibosses.md", "the burst is the clap on the downbeat", nullptr, SoundPlay::Recipe, rc(Recipe::Clap), false, 1.0f, 0, 0},
    {BossCue::MqOpen, "minibosses.md", "while she is open the bed drops to kick and pad (the hats, shaker, clap, snare, pulse, bass, stab and the "
                                         "band's effects cut for her open beats)",
     nullptr, SoundPlay::Custom, 0, false, 1.0f, 0, 4},
    // ---- Brood Mother (minibosses.md)
    {BossCue::BmLay, "minibosses.md", "the laying is a low tom on beat 1", nullptr, SoundPlay::Recipe, rc(Recipe::Tom), false, 0.7f, 0, 0,
     NoteRule::KeyRoot, 0, 0.0f, 0.0f, 0.0f, 2},
    {BossCue::BmHatch, "minibosses.md", "the hatching is a chirp for each egg, arpeggiated up the chord on beat 3", nullptr, SoundPlay::Recipe,
     rc(Recipe::Chirp), false, 0.35f, 0, 0, NoteRule::ChordUp, 0, 0.0f, 0.0f, 0.0f, 5},
    {BossCue::BmBreak, "minibosses.md", "a bell marks the break; the threads are a pad with one voice for each living broodling (the chord's root, "
                                        "third, fifth and seventh: four voices at most), silent when she is open (FightScene)",
     nullptr, SoundPlay::Recipe, rc(Recipe::Bell), false, 0.6f, 0, 0},
    // ---- Blink Star (minibosses.md)
    {BossCue::BsHop, "minibosses.md", "each hop is the next note of a five-note pentatonic run on the Track's root", nullptr, SoundPlay::Recipe,
     rc(Recipe::Bell), false, 0.5f, 0, 0, NoteRule::PentaRun, 0, 0.0f, 0.0f, 0.0f, 5},
    {BossCue::BsFire, "minibosses.md", "the firing is all five notes at once as a stab with a zap", nullptr, SoundPlay::Custom, rc(Recipe::Zap), false,
     0.8f, 0, 0, NoteRule::None, 0, 0.0f, 0.0f, 0.0f, 5},
    {BossCue::BsDazed, "minibosses.md", "dazed is the stab's tail with the bed low-passed", nullptr, SoundPlay::LowPass, 0, false, 1.0f, 600, 2},
    // ---- Sentry Array (minibosses.md)
    {BossCue::SaHand, "minibosses.md", "the hand is a woodblock on every beat, panned to where it points and one step up the scale for each step "
                                         "round (an octave a turn)",
     nullptr, SoundPlay::Recipe, rc(Recipe::Woodblock), false, 0.5f, 0, 0, NoteRule::ScaleUp, 0, 0.0f, 0.0f, 0.0f, 5},
    {BossCue::SaVolley, "minibosses.md", "the volleys are closed hats", nullptr, SoundPlay::Recipe, rc(Recipe::Hat), false, 0.35f, 0, 0},
    {BossCue::SaTurret, "minibosses.md", "a turret's death is a tom and a gap in the scale", nullptr, SoundPlay::Recipe, rc(Recipe::Tom), false, 0.7f, 0,
     0},
    {BossCue::SaBack, "minibosses.md", "the turn back is a reverse cymbal into its bar line", nullptr, SoundPlay::Recipe, rc(Recipe::RevCymbal), false,
     0.6f, 1, 0},
    // ---- Cadence (design/bosses/cadence.md "Sound"; M5 part 6)
    {BossCue::CdChord, "cadence.md", "each pillar is its chord: while it sounds, a pad voice holds its root and the band's stab plays that chord on the One at 1.2 "
                                     "(the pillar's chord: the Track's progression chord b, I V vi IV)",
     nullptr, SoundPlay::Custom, rc(Recipe::PadVoice), false, 0.5f, 4, 0.5f},
    {BossCue::CdFan, "cadence.md", "each firing pillar's fan is a tom on its own chord's root, on one and three", nullptr, SoundPlay::Custom, rc(Recipe::Tom),
     false, 0.5f, 0, 0, NoteRule::None, 0, 0.0f, 0.0f, 0.0f, 3},
    {BossCue::CdTurn, "cadence.md", "the turn of the square is a reversed cymbal into the bar line", nullptr, SoundPlay::Recipe, rc(Recipe::RevCymbal), false,
     0.6f, 1, 0},
    {BossCue::CdInversion, "cadence.md", "Inversion is the music bus high-passed at 800 Hz for one bar", nullptr, SoundPlay::Custom, 0, false, 1.0f, 800, 4},
    {BossCue::CdBurst, "cadence.md", "death: the four chords in turn (the stab at 1.2 on each pillar's burst), then the tonic with the gong on the "
                                     "bar line after them",
     nullptr, SoundPlay::Custom, rc(Recipe::Gong), false, 0.6f, 0, 0},
    // ---- Arpeggiator (design/bosses/arpeggiator.md "Sound"; M5 part 6)
    {BossCue::ApBlock, "arpeggiator.md", "each block sounds its own note, on the P3 bell at 0.3, as it passes the row the nearest player is in (this "
                                         "device's ship; the first machine's blocks the arpeggio's note of their step, the second's the chord's tone)",
     nullptr, SoundPlay::Custom, rc(Recipe::Bell), false, 0.3f, 0, 0},
    {BossCue::ApShutter, "arpeggiator.md", "the shutter is a woodblock on the One with a sweep opening over the beat", nullptr, SoundPlay::Pair,
     rc(Recipe::Woodblock), false, 0.5f, 1, 1, NoteRule::None, rc(Recipe::Sweep)},
    {BossCue::ApMove, "arpeggiator.md", "a change of side is a whoosh over the bar before", nullptr, SoundPlay::Recipe, rc(Recipe::Whoosh), false, 0.5f, 4,
     0},
    {BossCue::ApSecond, "arpeggiator.md", "the second machine arrives with the chord's stab", nullptr, SoundPlay::Recipe, rc(Recipe::Stab), false, 1.0f, 0,
     0},
    {BossCue::ApDeath, "arpeggiator.md", "death: Shard chirps running up the scale over two beats", nullptr, SoundPlay::Recipe, rc(Recipe::Chirp), false,
     0.5f, 0, 0, NoteRule::ScaleUp, 0, 0, 0, 0, 6},
    // ---- Fata Morgana (design/bosses/morgana.md "Sound"; M5 part 6)
    {BossCue::MgStorm, "morgana.md", "the storm is a crackle and a whoosh that are heard outside the oasis; outside it the music bus is also low-passed at "
                                     "2 kHz (the bed low-passed on each kick a ship of this device is outside; the crackle held while it is: FightScene)",
     nullptr, SoundPlay::LowPass, 0, false, 1.0f, 2000, 1.5f},
    {BossCue::MgWhoosh, "morgana.md", "the storm is a crackle and a whoosh that are heard outside the oasis (a whoosh a bar while a ship is outside)",
     nullptr, SoundPlay::Recipe, rc(Recipe::Whoosh), false, 0.3f, 4, 0},
    {BossCue::MgBlink, "morgana.md", "a mirage's blink is a soft tick on the snare (the band's snare as a ghost note, on the snare's steps)", nullptr,
     SoundPlay::Recipe, rc(Recipe::Snare), false, 0.3f, 0, 0},
    {BossCue::MgLeap, "morgana.md", "the leap is the lead's arpeggio as a run of chirps, up (its distinct notes from the lowest, an octave up)", nullptr,
     SoundPlay::Custom, rc(Recipe::Chirp), false, 0.5f, 0, 0},
    {BossCue::MgBell, "morgana.md", "while she is over the water, a bell on every beat", nullptr, SoundPlay::Recipe, rc(Recipe::Bell), false, 0.5f, 0, 0},
    // ---- Kaleidos, Reprise (design/bosses/reprise.md "Sound"; M5 part 6)
    {BossCue::RpBell, "reprise.md", "each solo is in its boss's own voice: Kaleidos's bells", nullptr, SoundPlay::Recipe, rc(Recipe::Bell), false, 0.3f, 0, 0},
    {BossCue::RpString, "reprise.md", "each solo is in its boss's own voice: Longwave's rubber string", nullptr, SoundPlay::Recipe, rc(Recipe::RubberString),
     false, 0.5f, 2, 0},
    {BossCue::RpPad, "reprise.md", "each solo is in its boss's own voice: a pad voice for The Choir", nullptr, SoundPlay::PadVoice, rc(Recipe::PadVoice),
     false, 0.5f, 2, 0.5f, NoteRule::ChordUp, 0, 0, 0, 0, 4, kChoirSend},
    {BossCue::RpSub, "reprise.md", "each solo is in its boss's own voice: the sub drop for Event Horizon", nullptr, SoundPlay::Recipe, rc(Recipe::SubDrop),
     false, 0.8f, 0, 0},
    {BossCue::RpTom, "reprise.md", "each solo is in its boss's own voice: toms three against four for Polyrhythm", nullptr, SoundPlay::Recipe,
     rc(Recipe::Tom), false, 0.5f, 0, 0, NoteRule::KeyRoot, 0, 0, 0, 0, 3},
    {BossCue::RpEcho, "reprise.md", "each solo is in its boss's own voice: the player's lead reversed for Feedback (its note of a bar before, the "
                                    "pluck's envelope backwards: a swell cut off)",
     nullptr, SoundPlay::Custom, 0, false, 0.35f, 0, 0},
    {BossCue::RpArp, "reprise.md", "the last pattern is First Light's arpeggio on the P3 bell, doubled by the band's lead (the local player's timbre, "
                                   "at 0.5 of the lead)",
     nullptr, SoundPlay::Custom, rc(Recipe::Bell), true, 0.3f, 0.5f, 0},
    {BossCue::RpDeath, "reprise.md", "death: the tonic held, the implosion a reversed cymbal ending on its burst (the beat the fight places it on, "
                                     "2026-11-14; its length in the cue's f); the tonic a pad voice from the death to a bar past it",
     nullptr, SoundPlay::Custom, rc(Recipe::RevCymbal), false, 0.7f, 4, 0},
    {BossCue::RpBoom, "reprise.md", "every bar's One booms under the band: the impact (sub, boom and crash) (the owner, 2026-11-14: \"more epic ... audio "
                                    "wise\")",
     nullptr, SoundPlay::Recipe, rc(Recipe::Impact), false, 0.45f, 0, 0},
    {BossCue::RpGong, "reprise.md", "every fourth bar's One rings the gong, as does each change of phase", nullptr, SoundPlay::Recipe, rc(Recipe::Gong), false,
     0.7f, 0, 0},
    // ---- Mini-bosses batch 3 (minibosses.md; M5 part 6)
    {BossCue::MmFire, "minibosses.md", "its fire is the player's lead upside down: each lead note answered a thirty-second later by the note as far below "
                                         "the chord's root as the lead's is above, at 0.4",
     nullptr, SoundPlay::Custom, 0, false, 0.4f, 0, 0},
    {BossCue::MmDash, "minibosses.md", "its Dash is the Dash's sweep reversed", nullptr, SoundPlay::Recipe, rc(Recipe::DashReverse), false, 0.6f, 0, 0},
    {BossCue::MmGlass, "minibosses.md", "death is glass", nullptr, SoundPlay::Recipe, rc(Recipe::Glass), false, 0.8f, 0, 0},
    {BossCue::TtCut, "minibosses.md", "a cut strand is a zap and the drone falls silent for its beat (silence is safe); the beam's drone, its filter "
                                      "opening as it comes round to this device's ship, is held for the fight (FightScene)",
     nullptr, SoundPlay::Recipe, rc(Recipe::Zap), false, 0.7f, 0, 0},
    {BossCue::TtTom, "minibosses.md", "each node is a tom as it passes twelve o'clock", nullptr, SoundPlay::Recipe, rc(Recipe::Tom), false, 0.5f, 0, 0},
    {BossCue::WvLap, "minibosses.md", "each lap is a saw glide rising over its three beats, a step higher each lap", nullptr, SoundPlay::Recipe,
     rc(Recipe::SawGlide), false, 0.45f, 2, 3, NoteRule::ScaleUp, 0, 0, 0, 0, 4},
    {BossCue::WvKnot, "minibosses.md", "the knot is a woodblock and a bell on the One", nullptr, SoundPlay::Pair, rc(Recipe::Woodblock), false, 0.5f, 0, 0,
     NoteRule::None, rc(Recipe::Bell)},
    {BossCue::WvBurst, "minibosses.md", "the cage going off is a crackle swelling into the chord's stab (the crackle over the beat before it: "
                                        "FightScene)",
     nullptr, SoundPlay::Recipe, rc(Recipe::Stab), false, 1.0f, 0, 0},
    {BossCue::DwThup, "minibosses.md", "each swallowed shot is a soft low thup, a step higher for each ring lit, so the fill can be heard (aggregated on "
                                       "the 1/32 steps)",
     nullptr, SoundPlay::Custom, rc(Recipe::Thup), false, 0.35f, 0, 0, NoteRule::None, 0, 0, 0, 0, 2},
    {BossCue::DwChoke, "minibosses.md", "choking is the gong with the bed low-passed for the bar", nullptr, SoundPlay::Recipe, rc(Recipe::Gong), false, 0.6f, 0, 0},
    {BossCue::DwLow, "minibosses.md", "choking is the gong with the bed low-passed for the bar", nullptr, SoundPlay::LowPass, 0, false, 1.0f, 600, 4},
    {BossCue::DwRing, "minibosses.md", "a ring given back is a reversed cymbal into the One and a stab (the cymbal sent a beat and a half ahead while the "
                                       "bar's fill says a ring will come: FightScene)",
     nullptr, SoundPlay::Recipe, rc(Recipe::Stab), false, 1.0f, 0, 0},
    // ---- Elites (design/traits.md: "Heard as it arrives"; M5 part 5)
    {BossCue::ElArrive, "traits.md", "the elite's low fifth (root and fifth of the Track's key) as it warps in", nullptr, SoundPlay::Sfx, sx(Sfx::Elite), false,
     0.9f, 0, 0},
    {BossCue::ElArmored, "traits.md", "Armored: a gong, short (0.7 s)", nullptr, SoundPlay::Recipe, rc(Recipe::Gong), false, 0.5f, 1, 0},
    {BossCue::ElSwift, "traits.md", "Swift: an upward sweep", nullptr, SoundPlay::Recipe, rc(Recipe::Sweep), false, 0.6f, 1, 1},
    {BossCue::ElGiant, "traits.md", "Giant: the sub drop", nullptr, SoundPlay::Recipe, rc(Recipe::SubDrop), false, 0.8f, 0, 0},
    {BossCue::ElSplitting, "traits.md", "Splitting: a glass crack", nullptr, SoundPlay::Recipe, rc(Recipe::Crack), false, 0.7f, 0, 0},
    {BossCue::ElVolatile, "traits.md", "Volatile: a crackle (a beat of it)", nullptr, SoundPlay::Recipe, rc(Recipe::CrackleShot), false, 1.0f, 1, 0},
    {BossCue::ElRegenerating, "traits.md", "Regenerating: two bells rising (the chord's root, then its fifth a sixteenth later)", nullptr, SoundPlay::Custom,
     rc(Recipe::Bell), false, 0.6f, 0, 0},
    {BossCue::ElShielded, "traits.md", "Shielded: two woodblocks", nullptr, SoundPlay::Recipe2, rc(Recipe::Woodblock), false, 0.7f, 0, 0},
    {BossCue::ElTwin, "traits.md", "Twin: the fifth again, a beat later", nullptr, SoundPlay::Sfx, sx(Sfx::Elite), false, 0.7f, 0, 0},
    {BossCue::ElMagnetic, "traits.md", "Magnetic: Shard chirps falling (five down the scale, a thirty-second apart)", nullptr, SoundPlay::Custom,
     rc(Recipe::Chirp), false, 0.7f, 0, 0},
    {BossCue::ElCommander, "traits.md", "Commander: a snare roll of one beat", nullptr, SoundPlay::Roll, rc(Recipe::Roll), false, 0.6f, 1, 0},
    {BossCue::ElCloaked, "traits.md", "Cloaked: a short reversed cymbal", nullptr, SoundPlay::Recipe, rc(Recipe::RevCymbal), false, 0.6f, 1, 0},
    {BossCue::ElPhasing, "traits.md", "Phasing: the fifth, gated on the eighths (sounding on each beat's first eighth)", nullptr, SoundPlay::Recipe,
     rc(Recipe::EliteGated), false, 0.6f, 0, 0},
    {BossCue::ElShatter, "traits.md (no sound named; chosen)", "Armored: the outer hull shatters at half health: glass", nullptr, SoundPlay::Recipe,
     rc(Recipe::Glass), false, 0.8f, 0, 0},
    {BossCue::ElSplit, "traits.md (no sound named; chosen)", "Splitting: two copies leave it: glass", nullptr, SoundPlay::Recipe, rc(Recipe::Glass), false,
     0.6f, 0, 0},
    {BossCue::ElFuse, "traits.md (no sound named; chosen)", "Volatile: the danger ring fills over two beats: a rising sweep", nullptr, SoundPlay::Recipe,
     rc(Recipe::Sweep), false, 0.5f, 1, 2},
    {BossCue::ElBurst, "traits.md (no sound named; chosen)", "Volatile: it bursts on the beat: the impact", nullptr, SoundPlay::Recipe, rc(Recipe::Impact),
     false, 0.7f, 0, 0},
    {BossCue::ElRegen, "traits.md (no sound named; chosen)", "Regenerating heals 8% on a beat: a soft bell", nullptr, SoundPlay::Recipe, rc(Recipe::Bell),
     false, 0.25f, 0, 0},
    {BossCue::ElStun, "traits.md (no sound named; chosen)", "Commander dies: those near it stunned for a beat: the sub drop", nullptr, SoundPlay::Recipe,
     rc(Recipe::SubDrop), false, 0.5f, 0, 0},
    {BossCue::HuKill, "arena.md", "Elite Hunt: the kill is the upgrade chord", nullptr, SoundPlay::Sfx, sx(Sfx::Upgrade), false, 0.9f, 0, 0},
    // ---- The arena (design/arena.md "Sound")
    {BossCue::VaHit, "arena.md", "Vault: each hit a glass tick on a note of the scale, one step up for each quarter lost", nullptr, SoundPlay::Recipe,
     rc(Recipe::Glass), false, 0.35f, 0, 0, NoteRule::ScaleUp, 0, 0, 0, 0, 6},
    {BossCue::VaBreak, "arena.md", "Vault: the break an arpeggio of bells up the chord (root, third, fifth, octave on the sixteenths) with a glass burst",
     nullptr, SoundPlay::Custom, rc(Recipe::Bell), false, 0.8f, 0, 0},
    {BossCue::PyFill, "arena.md", "Pylon: the fill is a woodblock on each of the four beats, rising", nullptr, SoundPlay::Recipe, rc(Recipe::Woodblock), false,
     0.6f, 0, 0, NoteRule::ScaleUp, 0, 0, 0, 0, 5},
    {BossCue::PyTuned, "arena.md", "Pylon: tuned is three bells up the chord (root, third, fifth on the sixteenths)", nullptr, SoundPlay::Custom,
     rc(Recipe::Bell), false, 0.8f, 0, 0},
    {BossCue::PyPulse, "arena.md", "Pylon: each pulse a bell on the chord's root an octave up, on the One", nullptr, SoundPlay::Recipe, rc(Recipe::Bell), false,
     0.5f, 0, 0},
    {BossCue::RiOpen, "arena.md", "Rift: a drone on the chord's bass note while it is open, its cutoff rising through the Build (each open Rift's drone "
                                  "follows the world: FightScene; the nearest one since the lead's ruling, a Rift Surge's a fifth up)",
     nullptr, SoundPlay::None, 0, false, 0.0f, 0, 0},
    {BossCue::RiThrough, "arena.md", "Rift: a tom for each body that comes through (the four come through together: aggregated as one, six for a Surge's)",
     nullptr, SoundPlay::Custom, rc(Recipe::Tom), false, 0.5f, 0, 0},
    {BossCue::RiClose, "arena.md", "Rift destroyed: a reversed cymbal cut short, then glass", nullptr, SoundPlay::Pair, rc(Recipe::RevCymbal), false, 0.7f, 1, 0,
     NoteRule::None, rc(Recipe::Glass)},
    {BossCue::BuBounce, "arena.md", "Bumper: a bounce is a tom on its chord tone (root, third, fifth, octave)", nullptr, SoundPlay::Recipe, rc(Recipe::Tom),
     false, 0.45f, 0, 0, NoteRule::ChordUp, 0, 0, 0, 0, 3},
    {BossCue::BuDischarge, "arena.md", "Bumper: the discharge is the chord's stab with a zap", nullptr, SoundPlay::Pair, rc(Recipe::Stab), false, 1.0f, 0, 0,
     NoteRule::None, rc(Recipe::Zap)},
    {BossCue::LeSplit, "arena.md", "Lens: each split is a glass tick a fifth above the lead's note (its player's; aggregated on the 1/32 steps)", nullptr,
     SoundPlay::Custom, rc(Recipe::Glass), false, 0.25f, 0, 0},
    {BossCue::EvAnnounce, "arena.md", "an event is announced one bar ahead (the Comet: a saw glide rising over the two beats of its warning; the other "
                                      "events' pages name no announcing sound: their own sound starts them)",
     nullptr, SoundPlay::Custom, rc(Recipe::SawGlide), false, 0.5f, 12, 2},
    {BossCue::VdLand, "arena.md", "Vault Drop: it lands with the gong", nullptr, SoundPlay::Recipe, rc(Recipe::Gong), false, 0.8f, 0, 0},
    {BossCue::CmShard, "arena.md", "Comet: a bell on each Shard it drops (the five of a beat drop together: aggregated as one)", nullptr, SoundPlay::Custom,
     rc(Recipe::Bell), false, 0.35f, 0, 0},
    {BossCue::EcOn, "arena.md", "Eclipse: the music bus low-passed at 1.2 kHz for the eight bars", nullptr, SoundPlay::LowPass, 0, false, 1.0f, 1200, 32},
    {BossCue::EcOff, "arena.md", "Eclipse: the low-pass lets go as the event ends", nullptr, SoundPlay::Custom, 0, false, 0.0f, 0, 0},
    {BossCue::EcFlash, "arena.md", "Eclipse: a soft tick on each flash", nullptr, SoundPlay::Recipe, rc(Recipe::Hat), false, 0.3f, 0, 0},
    {BossCue::MeWarn, "arena.md", "Meteor Rain: each warning a saw glide falling over its two beats", nullptr, SoundPlay::Recipe, rc(Recipe::SawGlide), false,
     0.4f, -12, 2},
    {BossCue::MeLand, "arena.md", "Meteor Rain: each impact the sub drop and a crash, on the beat", nullptr, SoundPlay::Pair, rc(Recipe::SubDrop), false, 0.7f,
     0, 0, NoteRule::None, rc(Recipe::Impact)},
    {BossCue::CoRoll, "arena.md", "Convoy: a low tom on every beat while they roll, one voice a carrier (each panned to its carrier; together as loud "
                                  "as the aggregation of as many)",
     nullptr, SoundPlay::Custom, rc(Recipe::Tom), false, 0.4f, 0, 0, NoteRule::None, 0, 0, 0, 0, 2},
    {BossCue::CoBreak, "arena.md", "Convoy: each carrier broken is the Boost bells", nullptr, SoundPlay::Sfx, sx(Sfx::Boost), false, 0.9f, 0, 0},
    {BossCue::StStrike, "arena.md", "Storm Cell: each strike a zap on the chord's three notes (a beat's strikes aggregated)", nullptr, SoundPlay::Custom,
     rc(Recipe::Zap), false, 0.5f, 0, 0},
    {BossCue::StWarn, "arena.md", "Storm Cell: a crackle that rises as it nears (held for the event, denser and louder as it nears: FightScene)", nullptr,
     SoundPlay::None, 0, false, 0.0f, 0, 0},
    // ---- Play test 3 (GAME_DESIGN 7.6, "Every hit on a boss is seen and heard"); the sim sends one of each a 1/32 step, f the
    // step's gain within its beat (1, then 0.7 of the step before, down to 0.3). Each plays on the band's next 1/32 step at the
    // row's gain times f.
    {BossCue::HitWeak, "GAME_DESIGN 7.6", "a shot on a weak point: a bright tick in the Track's key, counted on the 1/32 step like kills (the "
                                          "tick climbs the chord through a beat's steps: root, third, fifth, then the octave)",
     nullptr, SoundPlay::Custom, rc(Recipe::Tick), false, 1.0f, 0, 0, NoteRule::None, 0, 0, 0, 0, 6},
    {BossCue::HitArmour, "GAME_DESIGN 7.6", "a shot on armour: a dull low knock, counted on the 1/32 step like kills", nullptr, SoundPlay::Custom,
     rc(Recipe::Knock), false, 1.0f, 0, 0},
};
static_assert(sizeof(kRows) / sizeof(kRows[0]) == static_cast<u32>(BossCue::Count), "one row per BossCue, in its order");

// ---------------------------------------------------------------- the engine's commands

Command make(CmdType t) {
    Command k;
    k.type = t;
    k.when = Timing::Now;
    k.level = 0.0f;
    return k;
}
// A recipe's one-shot on `tick` (< 0: the next 1/32 step); agg > 0: aggregated, counting for that many.
void recipe(audio::CommandSink& s, Recipe r, i32 midi, float gain, float pan, double tick, float p1 = 0.0f, float p2 = 0.0f, u8 agg = 0) {
    Command k = make(CmdType::Recipe);
    k.when = tick >= 0.0 ? Timing::AtTick : Timing::Next32;
    k.a = static_cast<u8>(r);
    k.x = gain;
    k.y = pan;
    k.z = p1;
    k.level = p2;
    k.tick = tick;
    audio::encodeNote(k, midi);
    if (agg) {
        k.b = 1;
        k.mods[7] = agg;
    }
    s.send(k);
}
void sfxAt(audio::CommandSink& s, Sfx x, double tick, float gain, float pan) {
    Command k = make(CmdType::Sfx);
    k.when = Timing::AtTick;
    k.a = static_cast<u8>(x);
    k.x = gain;
    k.y = pan;
    k.tick = tick;
    s.send(k);
}
void leadNote(audio::CommandSink& s, u8 player, u8 degree, float gain, float pan, double tick, u8 timbre) {
    Command k = make(CmdType::LeadNote);
    k.a = player;
    k.b = degree;
    k.c = timbre;
    k.x = gain;
    k.y = pan;
    if (tick >= 0.0) {
        k.when = Timing::AtTick;
        k.tick = tick;
    }
    s.send(k);
}
void startVoice(audio::CommandSink& s, u8 slot, Recipe r, i32 midi, float gain, float pan, float param, float send = 0.0f, bool follow = false) {
    Command k = make(CmdType::VoiceStart);
    k.a = slot;
    k.b = static_cast<u8>(r);
    k.x = gain;
    k.y = pan;
    k.z = param;
    audio::encodeNote(k, midi);
    k.mods[5] = static_cast<u8>(send > 0.0f ? std::lround(send * 100.0f) : 0);
    k.mods[6] = follow ? 1 : 0;
    s.send(k);
}
// A held voice's parameter, its gain (x the gain it started with; < 0: unchanged), a note to glide to (-1: none), a new pan
// (outside -1..1: unchanged).
void setVoice(audio::CommandSink& s, u8 slot, float param, float gain, i32 midi = -1, float glideS = 0.0f, float pan = 9.0f) {
    Command k = make(CmdType::VoiceSet);
    k.a = slot;
    k.x = param;
    k.y = gain;
    k.z = glideS;
    audio::encodeNote(k, midi);
    if (midi < 0) k.c = 0;  // (no glide)
    if (pan >= -1.0f && pan <= 1.0f) {
        k.mods[5] = 1;
        k.level = pan;
    }
    s.send(k);
}
void stopVoice(audio::CommandSink& s, u8 slot, float release) {
    Command k = make(CmdType::VoiceStop);
    k.a = slot;
    k.x = release;
    s.send(k);
}
void lowPassBed(audio::CommandSink& s, float hz, float seconds) {
    Command k = make(CmdType::LowPassBed);
    k.x = hz;
    k.y = seconds;
    s.send(k);
}
void cutBed(audio::CommandSink& s, u16 buses, bool hats, double tick, float seconds) {
    Command k = make(CmdType::CutBed);
    k.when = tick >= 0.0 ? Timing::AtTick : Timing::Now;
    k.tick = tick;
    k.b = hats ? 1 : 0;
    k.mods[0] = static_cast<u8>(buses & 255u);
    k.mods[1] = static_cast<u8>(buses >> 8);
    k.y = seconds;
    s.send(k);
}
void highPassBed(audio::CommandSink& s, float hz, double tick, float seconds) {
    Command k = make(CmdType::HighPassBed);
    k.when = tick >= 0.0 ? Timing::AtTick : Timing::Now;
    k.tick = tick;
    k.x = hz;
    k.y = seconds;
    s.send(k);
}
void padTone(audio::CommandSink& s, float cutoff, float gain, float rampS) {
    Command k = make(CmdType::PadTone);
    k.x = cutoff;
    k.y = gain;
    k.z = rampS;
    s.send(k);
}
void delayBoost(audio::CommandSink& s, float feedback, u8 repeats) {
    Command k = make(CmdType::DelayBoost);
    k.x = feedback;
    k.a = repeats;
    s.send(k);
}
// A note of a player's lead played again (audio::LeadEchoMode), on `tick`.
void leadEcho(audio::CommandSink& s, u8 player, audio::LeadEchoMode mode, i32 midi, float gain, float pan, float cutoff, double back,
              double tick, u8 timbre = 0xFF, i8 octaves = 0, i8 bend = 0) {
    Command k = make(CmdType::LeadEcho);
    k.when = tick >= 0.0 ? Timing::AtTick : Timing::Now;
    k.tick = tick;
    k.a = player;
    k.b = static_cast<u8>(mode);
    audio::encodeNote(k, midi);
    k.x = gain;
    k.y = pan;
    k.z = cutoff;
    k.level = static_cast<float>(back);
    k.mods[5] = timbre;
    k.mods[6] = static_cast<u8>(octaves);
    k.mods[7] = static_cast<u8>(bend);
    s.send(k);
}

// The bus bits of audio::Bus.
constexpr u16 busBit(audio::Bus b) { return static_cast<u16>(1u << static_cast<u32>(b)); }

float aggGain(u32 n) {  // sound_m5.json: 1 - e^(-count / 2.5), one at its level, at most 1.6 x
    const float g = (1.0f - std::exp(-static_cast<float>(n) / 2.5f)) / (1.0f - std::exp(-1.0f / 2.5f));
    return g < 1.6f ? g : 1.6f;
}

// The Track the band plays now.
const audio::Track& trackOf(const audio::CommandSink& s) {
    const audio::MusicData& md = s.music();
    return md.tracks[s.track() < md.trackCount ? s.track() : 0u];
}
i32 tonicPc(const audio::Track& T) { return T.chordCount ? static_cast<i32>(T.chords[T.progressionCount ? T.progression[0] : 0].bass % 12u) : 0; }
// The Track's progression's chord k (Cadence's pillars: I V vi IV are Afterglow's progression).
const audio::Chord& progChord(const audio::Track& T, u32 k) { return T.chords[T.progressionCount ? T.progression[k % T.progressionCount] : 0]; }
i32 stabOf(const audio::Chord& ch, i32 shift = 0) {
    const auto n = [&](u32 i) { return i < ch.stabCount ? static_cast<i32>(ch.stab[i]) + shift : 0; };
    return audio::stabNotes(n(0), n(1), n(2), n(3));
}

void playCustom(audio::CommandSink& s, FightVoices& voices, const FightSound& r, const sim::Event& e, double tick, float pan, float beatS) {
    const audio::Track& T = trackOf(s);
    const FightScene& sc = voices.scene;
    switch (r.cue) {
        case BossCue::HzEat:
            // The next note of the Track's arp (the lead's own notes, in the bar it plays) an octave down, bending down a fifth.
            leadEcho(s, 0, audio::LeadEchoMode::Note, chordNote((e.b ? e.b - 1 : 0) % 16, 3, NoteFrom::Arp), r.gain, pan, 0.0f, 0.0, tick,
                     static_cast<u8>(audio::Timbre::P1Saw), 0, -7);
            break;
        case BossCue::HzStarved:
            // The pads and the bass out for the first beat, a reversed cymbal swelling through it.
            cutBed(s, busBit(audio::Bus::Pad) | busBit(audio::Bus::Bass), false, tick, beatS);
            recipe(s, Recipe::RevCymbal, -1, r.gain, pan, tick + kTicksPerBeatD, r.p1);
            break;
        case BossCue::FbEcho: {
            // The player's own note from a delay ago, an octave down, low-passed, quieter, far over (the second Echo further down
            // and darker). A teammate's at their lead's level on this device.
            // (cost, as a teammate's lead and its extra notes: a teammate's Echoes are its own device's, 18 dB and more under
            // the lead here)
            if (sc.valid && e.b != sc.seat) break;
            const bool second = e.f > 0.5f;
            const double back = sc.fbDelay[second ? 1 : 0];
            const float gain = (second ? kEchoDb15 : kEchoDb9) * r.gain;
            const float far = pan >= 0.0f ? kEchoPan : -kEchoPan;
            leadEcho(s, e.b, audio::LeadEchoMode::Recorded, -1, gain, far, second ? kEchoCutDark : kEchoCut, back, tick, 0xFF, -1);
            break;
        }
        case BossCue::FbOpen: {
            // The Head holds the root two octaves up, quietly, a semitone higher for every tenth of its health lost (the scene holds
            // it while the Head stays open; a bar when it is not followed).
            const i32 root = (T.chordCount ? static_cast<i32>(T.chords[T.progressionCount ? T.progression[0] : 0].bass) : 36) + 24 +
                             static_cast<i32>(std::lround(e.f));
            startVoice(s, 0, Recipe::PadVoice, root, r.gain, 0.0f, 0.0f);
            voices.stopAt[0] = tick + static_cast<double>(r.p1) * kTicksPerBeatD;
            voices.release[0] = r.p2 * beatS;
            voices.glideAt[0] = -1.0;
            voices.held[0] = 0;
            break;
        }
        case BossCue::MqStep:
            // The open hat stepping round the stereo field with the court's turn.
            recipe(s, Recipe::Hat, -1, r.gain, 0.75f * std::sin(e.f), tick, r.p1);
            break;
        case BossCue::MqOpen: {
            // The bed down to kick and pad for her open beats.
            if (!e.b) break;
            const u16 buses = busBit(audio::Bus::HatsBreak) | busBit(audio::Bus::Clap) | busBit(audio::Bus::Snare) | busBit(audio::Bus::Pulse) |
                              busBit(audio::Bus::Bass) | busBit(audio::Bus::Stab) | busBit(audio::Bus::Fx);
            cutBed(s, buses, true, tick, (e.f > 0.0f ? e.f : r.p2) * beatS);
            break;
        }
        case BossCue::BsFire: {
            // All five notes of the run at once as a stab, with a zap on the root.
            static const i32 kPenta[5] = {0, 3, 5, 7, 10};
            const i32 base = 12 * (r.octave + 1) + tonicPc(T);
            recipe(s, Recipe::Stab, audio::stabNotes(base + kPenta[0], base + kPenta[1], base + kPenta[2], base + kPenta[3], base + kPenta[4]), 1.0f,
                   pan, tick);
            recipe(s, Recipe::Zap, base, r.gain, pan, tick);
            break;
        }
        case BossCue::CdChord: {
            // The pillar's chord: a pad voice on its root for a bar, the band's stab on the One at 1.2.
            const audio::Chord& ch = progChord(T, e.b);
            const u8 slot = static_cast<u8>(e.b % 8u);
            startVoice(s, slot, Recipe::PadVoice, 60 + ch.bass % 12, r.gain, pan, 0.0f);
            voices.stopAt[slot] = tick + static_cast<double>(r.p1) * kTicksPerBeatD;
            voices.release[slot] = r.p2 * beatS;
            voices.glideAt[slot] = -1.0;
            recipe(s, Recipe::Stab, stabOf(ch), kStabOne, 0.0f, tick);
            break;
        }
        case BossCue::CdFan:
            recipe(s, Recipe::Tom, 12 * (r.octave + 1) + progChord(T, e.b).bass % 12, r.gain, pan, tick);
            break;
        case BossCue::CdInversion:
            highPassBed(s, r.p1, -1.0, r.p2 * beatS);
            break;
        case BossCue::CdBurst: {
            // Each pillar's burst its chord's stab; after the fourth, the tonic with the gong on the bar line.
            recipe(s, Recipe::Stab, stabOf(progChord(T, e.b)), kStabOne, pan, tick);
            if (e.b == 3) {
                recipe(s, Recipe::Stab, stabOf(progChord(T, 0)), kStabOne, 0.0f, tick + kTicksPerBeatD);
                recipe(s, Recipe::Gong, 12 * 6 + progChord(T, 0).bass % 12, r.gain, 0.0f, tick + kTicksPerBeatD);
            }
            break;
        }
        case BossCue::ApBlock: {
            // The block's own note as it passes the row of this device's ship: the first machine's the arpeggio's note of the step
            // it left on, the second's the chord's tone of its lane.
            const Vec2 from{e.x, e.y};
            u32 m = 0;
            if (sc.valid && length(from - sc.apMachine[1]) < length(from - sc.apMachine[0])) m = 1;
            double delay = 0.0;
            if (sc.valid) {
                const float d = clampf(dot(sc.ship - from, sc.apDir[m]), 0.0f, sc.apLength);
                delay = std::round(static_cast<double>(d / sc.apSpeed * sc.tps) / 12.0) * 12.0;
            }
            const i32 step = static_cast<i32>(std::floor(tick / 24.0 + 1e-6)) % 16;
            const i32 note = m == 0 ? audio::chordNoteAt(chordNote(step < 0 ? step + 16 : step, 4, NoteFrom::Arp), static_cast<i32>(delay))
                                    : audio::chordNoteAt(chordNote(e.b % 4u, 5, NoteFrom::ChordTone), static_cast<i32>(delay));
            recipe(s, Recipe::Bell, note, r.gain, pan, tick + delay);
            break;
        }
        case BossCue::MgLeap:
            recipe(s, Recipe::Chirp, chordNote(e.b, 5, NoteFrom::ArpRank), r.gain, pan, tick);
            break;
        case BossCue::RpEcho: {
            // The player's note of a bar before, its pluck backwards (on its own timbre).
            const float far = pan >= 0.0f ? kEchoPan : -kEchoPan;
            leadEcho(s, e.b, audio::LeadEchoMode::Reverse, -1, r.gain, far, 0.0f, kTicksPerBarD, tick);
            break;
        }
        case BossCue::RpArp:
            recipe(s, Recipe::Bell, static_cast<i32>(std::lround(e.f)), r.gain, pan, tick);
            leadEcho(s, 0, audio::LeadEchoMode::Note, static_cast<i32>(std::lround(e.f)), r.p1, 0.0f, 0.0f, 0.0, tick);
            break;
        case BossCue::RpDeath: {
            // The reversed cymbal over the implosion, ending on its burst (2026-11-14: as long as the implosion, e.f beats); the tonic
            // held from the death to a bar past it (it outlives the fight: slot 14 keeps its own time).
            recipe(s, Recipe::RevCymbal, -1, r.gain, pan, tick, e.f > 0.05f ? e.f : r.p1);
            startVoice(s, 14, Recipe::PadVoice, 60 + tonicPc(T), 0.5f, 0.0f, 0.0f);
            voices.stopAt[14] = tick + kTicksPerBarD;
            voices.release[14] = 4.0f * beatS;
            voices.glideAt[14] = -1.0;
            voices.keep[14] = true;
            break;
        }
        case BossCue::MmFire:
            // The lead note a thirty-second ago, mirrored about the chord's root.
            leadEcho(s, e.b, audio::LeadEchoMode::Mirror, -1, r.gain, pan, 0.0f, 12.0, tick);
            break;
        case BossCue::DwThup:
            recipe(s, Recipe::Thup, chordNote(e.b, r.octave), r.gain, pan, tick, 0.0f, 0.0f, 1);
            break;
        case BossCue::ElRegenerating:
            recipe(s, Recipe::Bell, chordNote(0, 5, NoteFrom::ChordTone), r.gain, pan, tick);
            recipe(s, Recipe::Bell, chordNote(2, 5, NoteFrom::ChordTone), r.gain, pan, tick + 24.0);
            break;
        case BossCue::ElMagnetic:
            for (i32 k = 0; k < 5; ++k) recipe(s, Recipe::Chirp, chordNote(4 - k, 6), r.gain, pan, tick + 12.0 * k);
            break;
        case BossCue::VaBreak:
            for (i32 k = 0; k < 4; ++k) recipe(s, Recipe::Bell, chordNote(k == 3 ? 4 : k, 5, NoteFrom::ChordTone), r.gain, pan, tick + 24.0 * k);
            recipe(s, Recipe::Glass, -1, r.gain, pan, tick);
            break;
        case BossCue::PyTuned:
            for (i32 k = 0; k < 3; ++k) recipe(s, Recipe::Bell, chordNote(k, 5, NoteFrom::ChordTone), r.gain, pan, tick + 24.0 * k);
            break;
        case BossCue::RiThrough: {
            u8 n = 4;  // arena.json: four through a Rift a bar, six through a Rift Surge's
            for (u32 k = 0; k < sc.rifts && sc.valid; ++k)
                if (sc.riftSurge[k] != 0xFF) n = 6;
            recipe(s, Recipe::Tom, -1, r.gain, pan, tick, 0.0f, 0.0f, n);
            break;
        }
        case BossCue::LeSplit:
            recipe(s, Recipe::Glass, chordNote(7, e.b < 4 ? e.b : 0, NoteFrom::Lead), r.gain, pan, tick, 0.0f, 0.0f, 1);
            break;
        case BossCue::EvAnnounce:
            if (e.b == static_cast<u8>(sim::EventKind::Comet)) recipe(s, Recipe::SawGlide, -1, r.gain, pan, tick, r.p1, r.p2);
            break;
        case BossCue::CmShard:
            recipe(s, Recipe::Bell, -1, r.gain, pan, tick, 0.0f, 0.0f, 5);
            break;
        case BossCue::EcOff: lowPassBed(s, 0.0f, 0.0f); break;
        case BossCue::CoRoll: {
            const u32 n = sc.valid && sc.carriers ? sc.carriers : 1u;
            const float each = r.gain * aggGain(n) / static_cast<float>(n);
            for (u32 k = 0; k < n; ++k) recipe(s, Recipe::Tom, chordNote(0, r.octave, NoteFrom::ChordTone), each, sc.valid && sc.carriers ? sc.carrierPan[k] : pan, tick);
            break;
        }
        case BossCue::StStrike:
            for (i32 k = 0; k < 3; ++k) recipe(s, Recipe::Zap, chordNote(k, 5, NoteFrom::ChordTone), r.gain, pan, tick, 0.0f, 0.0f, 1);
            break;
        case BossCue::HitWeak:
        case BossCue::HitArmour: {
            // Play test 3: on the band's next 1/32 step (as kills are), at the step's gain within its beat. The weak point's tick
            // climbs the chord with the steps (its gain says which: 1, 0.7, 0.49, then 0.343 and the floor 0.3).
            const float g = e.f > 0.0f ? std::fmin(e.f, 1.0f) : 1.0f;
            if (r.cue == BossCue::HitArmour) {
                recipe(s, Recipe::Knock, -1, r.gain * g, pan, -1.0);
                break;
            }
            static const i32 kTone[4] = {0, 1, 2, 4};
            const i32 step = g > 0.85f ? 0 : (g > 0.6f ? 1 : (g > 0.42f ? 2 : 3));
            recipe(s, Recipe::Tick, chordNote(kTone[step], r.octave, NoteFrom::ChordTone), r.gain * g, pan, -1.0);
            break;
        }
        default: break;
    }
}
}  // namespace

const FightSound* fightSounds(u32* count) {
    if (count) *count = static_cast<u32>(sizeof(kRows) / sizeof(kRows[0]));
    return kRows;
}

void playFightCue(audio::CommandSink& band, FightVoices& voices, const sim::Event& e, double tick, float pan, float bpm) {
    const u32 c = e.a;
    if (c >= static_cast<u32>(BossCue::Count)) return;
    const FightSound& r = kRows[c];
    const float beatS = 60.0f / std::fmax(bpm, 30.0f);
    i32 midi = r.pitched ? static_cast<i32>(std::lround(e.f)) : -1;
    if (r.note == NoteRule::RunDegree) {
        // Longwave's run (longwave.json music.rule): the crest's semitones above the root (0 2 3 5 7 8 10 12) are the
        // scale's degrees 0..7, as the band reads them against the Track's own scale and chord.
        static const i32 kSemis[8] = {0, 2, 3, 5, 7, 8, 10, 12};
        i32 degree = 0;
        for (i32 k = 0; k < 8; ++k)
            if (kSemis[k] == static_cast<i32>(std::lround(e.f))) degree = k;
        midi = chordNote(degree, 3);
    } else if (r.note == NoteRule::KeyRoot) {
        // The Track's root: its first chord's bass, in the row's octave.
        midi = 12 * (r.octave + 1) + tonicPc(trackOf(band)) + static_cast<i32>(std::lround(e.f));
    } else if (r.note == NoteRule::ChordUp) {
        midi = chordNote(static_cast<i32>(e.b % 4u), r.octave + static_cast<i32>(e.b / 4u), NoteFrom::ChordTone);
    } else if (r.note == NoteRule::PentaRun || r.note == NoteRule::ScaleUp) {
        static const i32 kPenta[5] = {0, 3, 5, 7, 10};
        if (r.note == NoteRule::ScaleUp) {
            midi = chordNote(static_cast<i32>(e.b % 8u), r.octave);
        } else {
            midi = 12 * (r.octave + 1) + tonicPc(trackOf(band)) + kPenta[e.b % 5u];
        }
    } else if (r.note == NoteRule::ArpCount) {
        midi = chordNote(static_cast<i32>(e.b % 4u) * 2, 3);  // root, third, fifth, seventh up the scale, an octave down
    } else if (r.note == NoteRule::WallTone) {
        // Carom King's walls: the chord tone of the wall (0 root, 1 third, 2 fifth, 3 seventh); Princes an octave up.
        midi = chordNote(static_cast<i32>(std::lround(e.f)), 5 + (e.b ? 1 : 0), NoteFrom::ChordTone);
    }
    switch (r.play) {
        case SoundPlay::Sfx: {
            // A Carom Prince's bounce: quieter (the page: an octave up).
            const float g = r.cue == BossCue::CkBounce && e.b ? 0.35f : r.gain;
            sfxAt(band, static_cast<Sfx>(r.id), tick, g, pan);
            break;
        }
        case SoundPlay::Lead: {
            const u8 timbre = r.id == 0xFF ? static_cast<u8>(e.b % 4u) : r.id;
            leadNote(band, 0, static_cast<u8>(e.b % 3u), r.gain, pan, tick, timbre);
            break;
        }
        case SoundPlay::Recipe: recipe(band, static_cast<Recipe>(r.id), midi, r.gain, pan, tick, r.p1, r.p2); break;
        case SoundPlay::Recipe2:
            recipe(band, static_cast<Recipe>(r.id), midi, r.gain, pan, tick, r.p1, r.p2);
            recipe(band, static_cast<Recipe>(r.id), midi, r.gain, pan, tick + 0.5 * kTicksPerBeatD, r.p1, r.p2);
            break;
        case SoundPlay::PadVoice: {
            // One voice a Voice (slots 0-7): a new note on the same slot replaces the last.
            const u8 slot = static_cast<u8>(e.b % 8u);
            startVoice(band, slot, static_cast<Recipe>(r.id), midi, r.gain, pan, 0.0f, r.send);
            voices.stopAt[slot] = tick + static_cast<double>(r.p1) * kTicksPerBeatD;
            voices.release[slot] = r.p2 * beatS;
            voices.glideAt[slot] = -1.0;
            voices.held[slot] = 0;
            if (r.glideSemis != 0.0f && midi >= 0) {
                // (a Voice's death falls a fourth over a beat; the held note rises an octave as the beam fires)
                voices.glideAt[slot] = tick + static_cast<double>(r.glideAfter) * kTicksPerBeatD;
                voices.glideTo[slot] = midi + static_cast<i32>(std::lround(r.glideSemis));
                voices.glideS[slot] = r.glideOver * beatS;
            }
            break;
        }
        case SoundPlay::Pair: {
            // Event Horizon fed: louder the bigger the meal (the cue's f, 0..1).
            const float g = r.cue == BossCue::HzFed ? r.gain * std::fmin(std::fmax(e.f, 0.3f), 1.0f) : r.gain;
            recipe(band, static_cast<Recipe>(r.id), midi, g, pan, tick, r.p1, r.p2);
            recipe(band, static_cast<Recipe>(r.id2), midi, g, pan, tick, r.p1, r.p2);
            break;
        }
        case SoundPlay::Hats: {
            // The Ring closing: quarters for half of it, eighths for a quarter, sixteenths for the last quarter.
            const double span = static_cast<double>(r.p1) * kTicksPerBeatD;
            double at = 0.0;
            while (at < span - 1e-6) {
                recipe(band, static_cast<Recipe>(r.id), -1, r.gain, pan, tick + at);
                at += at < 0.5 * span ? kTicksPerBeatD : (at < 0.75 * span ? 0.5 * kTicksPerBeatD : 0.25 * kTicksPerBeatD);
            }
            break;
        }
        case SoundPlay::Roll: {
            // The Ram's lock: the roll ends as it charges (two beats on, one for a lock straight after the last).
            const float beats = r.p1 - static_cast<float>(e.b);
            recipe(band, static_cast<Recipe>(r.id), midi, r.gain, pan, tick + static_cast<double>(beats) * kTicksPerBeatD, beats, r.p2);
            break;
        }
        case SoundPlay::LowPass:
            if (e.b) lowPassBed(band, r.p1, (e.f > 0.0f ? e.f : r.p2) * beatS);
            break;
        case SoundPlay::Custom: playCustom(band, voices, r, e, tick, pan, beatS); break;
        default: break;
    }
}

// ---------------------------------------------------------------- the scene: what the held voices follow

void readFightScene(const sim::World& w, u32 seat, double offset, FightScene& out) {
    using namespace sim;
    FightScene sc;
    sc.valid = true;
    sc.seat = static_cast<u8>(seat);
    sc.now = w.music() + offset;
    sc.tps = w.ticksPerSecond() > 1.0f ? w.ticksPerSecond() : 192.0f;
    const PlayerState& me = w.player(seat < w.playerCount() ? seat : 0u);
    sc.ship = me.pos;
    const auto panOf = [&](Vec2 at) { return clampf((at.x - sc.ship.x) / 900.0f, -1.0f, 1.0f) * 0.7f; };
    const BossState& b = w.boss();
    sc.fight = b.active && !b.dying && w.music() >= b.fightTick;
    sc.kind = b.kind;
    if (sc.fight) {
        switch (b.kind) {
            case BossKind::EventHorizon: sc.hzMeal = b.hz.meal; break;
            case BossKind::Feedback: {
                sc.fbOpen = b.fb.open >= 0.999f;
                sc.fbHowl = b.fb.phase == 2;
                const float share = b.coreMax > 0.0f ? b.coreHp / b.coreMax : 1.0f;
                sc.fbSemis = static_cast<u8>(clampf(std::floor((1.0f - share) * 10.0f), 0.0f, 10.0f));
                sc.fbDelay[0] = w.feedbackDelay(0);
                sc.fbDelay[1] = w.feedbackDelay(1);
                break;
            }
            case BossKind::BroodMother: {
                const Enemies& e = w.enemies();
                u32 n = 0;
                for (u32 i = 0; i < e.count; ++i)
                    if (!(e.flags[i] & kFlagDead) && (e.flags[i] & kFlagCalled) && e.state[i] != kEgg && e.state[i] != kWarping) ++n;
                sc.broodlings = static_cast<u8>(n < 255 ? n : 255);
                break;
            }
            case BossKind::TwinTether: {
                Vec2 a, c;
                sc.tetherBeam = w.tetherBeam(a, c);
                if (sc.tetherBeam) {
                    // How far the beam has still to turn to reach the ship (it turns counter-clockwise): the rope is a line
                    // through its middle (half a turn brings it round), the flail a ray from its node (a whole turn).
                    const bool rope = b.alive == 0x3u;
                    const Vec2 pivot = rope ? (a + c) * 0.5f : a;
                    const float beam = std::atan2(c.y - a.y, c.x - a.x), ship = std::atan2(sc.ship.y - pivot.y, sc.ship.x - pivot.x);
                    const float period = rope ? kPi : kTau;
                    float ahead = std::fmod(ship - beam, period);
                    if (ahead < 0.0f) ahead += period;
                    sc.tetherNear = 1.0f - ahead / period;
                    sc.tetherPan = panOf(pivot);
                }
                break;
            }
            case BossKind::DeepWell: {
                // The next ring: on the next bar line when the bar's fill makes one (a bullet for every 3, not choked), or on
                // the half's beat 3 (its second ring).
                const double t = w.music();
                const double bar = std::ceil(t / kTicksPerBarD) * kTicksPerBarD;
                const MiniState& m = b.mini;
                if (m.ringLeft > 0) {
                    const double three = std::floor(t / kTicksPerBarD) * kTicksPerBarD + 2.0 * kTicksPerBeatD;
                    if (three > t) sc.wellRing = three + offset;
                } else if (m.fill >= 3.0f && m.fill <= w.wellStomach() && !w.wellChoked(t)) {
                    sc.wellRing = bar + offset;
                }
                break;
            }
            case BossKind::WeaverPrime:
                if (b.mini.cageTick >= 0.0) sc.weaverBurst = b.mini.cageTick + 4.0 * kTicksPerBarD + offset;
                break;
            case BossKind::Morgana: sc.mgOutside = me.present && me.alive && !w.morganaCalm(me.pos, w.music()); break;
            case BossKind::Arpeggiator: {
                for (u32 m = 0; m < 2; ++m) {
                    sc.apMachine[m] = w.arpMachine(m);
                    Vec2 from, dir;
                    if (w.arpLane(m, 1, from, dir)) sc.apDir[m] = dir;
                }
                sc.apSpeed = w.species().arp.blockSpeed;
                sc.apLength = w.species().arp.laneLength;
                break;
            }
            default: break;
        }
    }
    // The arena: the open Rifts (the nearest two: the lead's ruling, cost), the Build's progress, the Storm Cell, the Convoy's
    // carriers.
    const ArenaBody* bodies = w.arenaBodies();
    float dist[3] = {1e30f, 1e30f, 1e30f};
    u8 surge = 0;
    for (u32 k = 0; k < World::kArenaBodies; ++k) {
        const ArenaBody& a = bodies[k];
        if (!a.alive) continue;
        if (a.kind == static_cast<u8>(StructKind::Rift) && a.from > 0.0) {
            const float d = length(a.pos - sc.ship);
            const u8 place = a.surge ? surge++ : 0xFF;
            u32 at = sc.rifts < kRiftDrones ? sc.rifts : kRiftDrones;
            while (at > 0 && dist[at - 1] > d) {
                if (at < kRiftDrones) {
                    dist[at] = dist[at - 1];
                    sc.riftPan[at] = sc.riftPan[at - 1];
                    sc.riftGain[at] = sc.riftGain[at - 1];
                    sc.riftSurge[at] = sc.riftSurge[at - 1];
                }
                --at;
            }
            if (at < kRiftDrones) {
                dist[at] = d;
                sc.riftPan[at] = panOf(a.pos);
                sc.riftGain[at] = clampf(1.0f - (d - 600.0f) / 2400.0f, 0.25f, 1.0f);  // heard from afar, louder near
                sc.riftSurge[at] = place;
                if (sc.rifts < kRiftDrones) ++sc.rifts;
            }
        }
        if (a.kind == kBodyCarrier && sc.carriers < 4) sc.carrierPan[sc.carriers++] = panOf(a.pos);
    }
    const Phrase& ph = w.phrase();
    if (ph.section == Section::Build && ph.sectionBars) {
        const double inBar = std::fmod(w.music(), kTicksPerBarD) / kTicksPerBarD;
        sc.build = clampf((static_cast<float>(ph.sectionBar) + static_cast<float>(inBar)) / static_cast<float>(ph.sectionBars), 0.0f, 1.0f);
    }
    const ArenaEventState& ev = w.arenaEvent();
    if (ev.kind == static_cast<u8>(EventKind::StormCell) && ev.stage == 1) {
        sc.storm = true;
        const float d = length(ev.pos - sc.ship);
        sc.stormNear = clampf(1.0f - (d - 320.0f) / 1680.0f, 0.0f, 1.0f);
        sc.stormPan = panOf(ev.pos);
    }
    out = sc;
}

namespace {
// A held voice the scene keeps at `base` x `mul` (mul 0..1 follows the scene; 0: silent, kept): started (or glided to its new
// note), its parameter, its share and its pan sent on a change of more than 3% (a pan: 0.05), or released. `id` names what it
// holds (a change of kind restarts it).
void hold(audio::CommandSink& s, FightVoices& v, u8 slot, u8 id, Recipe r, i32 midi, float base, float mul, float pan, float param,
          float glideS = 0.03f, bool follow = false) {
    if (v.held[slot] != id) {
        startVoice(s, slot, r, midi, base, pan, param, 0.0f, follow);
        if (mul != 1.0f) setVoice(s, slot, param, mul);
        v.held[slot] = id;
        v.heldNote[slot] = midi;
        v.heldParam[slot] = param;
        v.heldGain[slot] = mul;
        v.heldPan[slot] = pan;
        v.stopAt[slot] = -1.0;
        v.keep[slot] = false;
        return;
    }
    const bool note = midi != v.heldNote[slot];
    const bool moved = std::fabs(param - v.heldParam[slot]) > 0.03f * std::fmax(std::fabs(v.heldParam[slot]), 1.0f) ||
                       std::fabs(mul - v.heldGain[slot]) > 0.03f || (mul == 0.0f) != (v.heldGain[slot] == 0.0f);
    const bool panned = std::fabs(pan - v.heldPan[slot]) > 0.05f;
    if (!note && !moved && !panned) return;
    setVoice(s, slot, param, mul, note ? midi : -1, glideS, panned ? pan : 9.0f);
    v.heldNote[slot] = midi;
    v.heldParam[slot] = param;
    v.heldGain[slot] = mul;
    if (panned) v.heldPan[slot] = pan;
}
void letGo(audio::CommandSink& s, FightVoices& v, u8 slot, float release) {
    if (!v.held[slot]) return;
    stopVoice(s, slot, release);
    v.held[slot] = 0;
}

// The held voices the scene asks for (a frame's worth of changes).
void followScene(audio::CommandSink& s, FightVoices& v, bool all) {
    const FightScene& sc = v.scene;
    const audio::Track& T = trackOf(s);
    const float beatS = 60.0f / (T.bpm > 30.0f ? T.bpm : 120.0f);
    const bool fight = sc.valid && sc.fight && !all;
    const BossKind k = sc.kind;
    // Event Horizon's drone: a sub sine on the Track's root, a semitone down for every three cells of the meal (twelve).
    if (fight && k == BossKind::EventHorizon) {
        const i32 root = 24 + tonicPc(T) + (tonicPc(T) < 5 ? 12 : 0);  // (in the bass's octave: 29-40, 44-82 Hz)
        const i32 sink = static_cast<i32>((sc.hzMeal < 12 ? sc.hzMeal : 12) / 3);
        hold(s, v, 8, 1, Recipe::SubSine, root - sink, kHzDroneGain, 1.0f, 0.0f, 0.0f, 0.05f);
    } else if (fight && k == BossKind::TwinTether) {
        // Twin Tether's beam: the Sweeper's drone on the chord's bass an octave down, its cutoff opening as the beam comes round
        // to the ship; silent while it is slack (a cut strand).
        const float open = sc.tetherNear * sc.tetherNear * sc.tetherNear;
        const float cut = kTetherCutLo * std::pow(kTetherCutHi / kTetherCutLo, open);
        hold(s, v, 8, 2, Recipe::Drone, chordNote(0, 1, NoteFrom::ChordTone), kTetherGain, sc.tetherBeam ? 1.0f : 0.0f, sc.tetherPan, cut, 0.03f, true);
    } else {
        letGo(s, v, 8, 0.3f);
    }
    // Fata Morgana's storm: the crackle while this device's ship is outside the calm.
    if (fight && k == BossKind::Morgana && sc.mgOutside) hold(s, v, 9, 1, Recipe::Crackle, -1, kMgCrackleGain, 1.0f, 0.0f, kMgCrackleDensity);
    else letGo(s, v, 9, 0.3f);
    // The Brood Mother's threads: a pad voice for each living broodling (the chord's root, third, fifth, seventh), following the
    // chord; silent when none lives (she is open). (Cost, the lead's ruling: kBroodVoices of them, the root first.)
    for (u8 j = 0; j < 4; ++j) {
        if (fight && k == BossKind::BroodMother && sc.broodlings > j && j < kBroodVoices)
            hold(s, v, j, static_cast<u8>(10 + j), Recipe::PadVoice, chordNote(j, 4, NoteFrom::ChordTone), kBroodGain, 1.0f,
                 (static_cast<float>(j) - 1.5f) * 0.3f, 0.0f, 0.03f, true);
        else if (v.held[j] == 10 + j)
            letGo(s, v, j, beatS);
    }
    // Feedback: the pad's tone and the delay follow the Head; its held root lasts while it is open.
    {
        const i8 tone = fight && k == BossKind::Feedback ? (sc.fbOpen ? 1 : -1) : 0;
        if (tone != v.padTone) {
            padTone(s, tone > 0 ? kFbOpenTone : (tone < 0 ? kFbShutTone : 1.0f), tone < 0 ? kFbShutGain : 1.0f, 0.15f);
            v.padTone = tone;
        }
        const bool howl = fight && k == BossKind::Feedback && sc.fbHowl;
        if (howl != v.howl) {
            delayBoost(s, howl ? kFbHowlFeedback : 0.0f, howl ? 16 : 0);
            v.howl = howl;
        }
        if (fight && k == BossKind::Feedback && sc.fbOpen && v.stopAt[0] >= 0.0) v.stopAt[0] = std::fmax(v.stopAt[0], sc.now + kTicksPerBeatD);
        if (fight && k == BossKind::Feedback && !sc.fbOpen && v.stopAt[0] > sc.now) v.stopAt[0] = sc.now;
    }
    // Deep Well: a reversed cymbal into the ring it will give back (sent a beat and a half ahead, once a ring).
    if (fight && k == BossKind::DeepWell && sc.wellRing > sc.now && sc.wellRing - sc.now <= 1.5 * kTicksPerBeatD && v.wellPosted != sc.wellRing) {
        recipe(s, Recipe::RevCymbal, -1, 0.6f, 0.0f, sc.wellRing, 1.0f);
        v.wellPosted = sc.wellRing;
    }
    // Weaver Prime: the crackle swelling over the beat into the cage's going off (the stab on its cue).
    if (fight && k == BossKind::WeaverPrime && sc.weaverBurst > sc.now && sc.weaverBurst - sc.now <= 1.5 * kTicksPerBeatD &&
        v.weaverPosted != sc.weaverBurst) {
        recipe(s, Recipe::CrackleShot, -1, 1.0f, 0.0f, sc.weaverBurst, 1.0f, 1.0f);
        v.weaverPosted = sc.weaverBurst;
    }
    // The Rifts' drones (the nearest two): on the chord's bass note (a Rift Surge's a fifth apart: the bass, its fifth, the
    // ninth), the cutoff rising through the Build.
    const bool arena = sc.valid && !(sc.fight && !all);
    for (u8 j = 0; j < 3; ++j) {
        const u8 slot = static_cast<u8>(10 + j);
        if (arena && j < sc.rifts) {
            const u8 place = sc.riftSurge[j];
            const i32 note = place == 0xFF ? chordNote(0, 2, NoteFrom::ChordTone) : chordNote(place == 0 ? 0 : (place == 1 ? 4 : 8), 2);
            const float cut = kRiftCutLo * std::pow(kRiftCutHi / kRiftCutLo, sc.build);
            hold(s, v, slot, static_cast<u8>(1 + (place == 0xFF ? 0 : 1 + place)), Recipe::Drone, note, kRiftGain, sc.riftGain[j], sc.riftPan[j], cut,
                 0.03f, true);
        } else {
            letGo(s, v, slot, 0.5f);
        }
    }
    // The Storm Cell: a crackle that rises as it nears (denser and louder).
    if (arena && sc.storm) {
        const float n = sc.stormNear;
        hold(s, v, 13, 1, Recipe::Crackle, -1, kStormGain, 0.25f + 0.75f * n, sc.stormPan, kStormDensityLo + (kStormDensityHi - kStormDensityLo) * n * n);
    } else {
        letGo(s, v, 13, 0.3f);
    }
}
}  // namespace

void updateFightVoices(audio::CommandSink& band, FightVoices& voices, double heard, bool all, const sim::World* w, u32 seat, double offset) {
    if (w) readFightScene(*w, seat, offset, voices.scene);
    followScene(band, voices, all);
    for (u8 k = 0; k < 16; ++k) {
        const bool release = all && !voices.keep[k];
        if (voices.glideAt[k] >= 0.0 && heard >= voices.glideAt[k] && voices.stopAt[k] >= 0.0 && !release) {
            setVoice(band, k, 0.0f, -1.0f, voices.glideTo[k], voices.glideS[k]);
            voices.glideAt[k] = -1.0;
        }
        if (voices.stopAt[k] < 0.0 || (!release && heard < voices.stopAt[k])) continue;
        stopVoice(band, k, release ? 0.1f : std::fmax(voices.release[k], 0.02f));
        voices.stopAt[k] = -1.0;
        voices.glideAt[k] = -1.0;
        voices.keep[k] = false;
    }
}

}  // namespace rf
