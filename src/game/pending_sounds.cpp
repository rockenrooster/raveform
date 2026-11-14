// THE FIGHTS' SOUNDS AND THE PENDING ONES (M5): see pending_sounds.h. One row a cue; the engine's recipe where it can say
// what the page asks, else the nearest sound the band has and, in `pending`, what is missing.

#include "game/pending_sounds.h"

#include <cmath>

#include "audio/bake.h"
#include "audio/band.h"
#include "audio/cues.h"
#include "sim/world.h"

namespace rf {

using sim::BossCue;
using audio::Recipe;
using audio::Sfx;

namespace {
constexpr u8 sx(Sfx s) { return static_cast<u8>(s); }
constexpr u8 rc(Recipe r) { return static_cast<u8>(r); }
constexpr double kTicksPerBeatD = 96.0;

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
     nullptr, SoundPlay::PadVoice, rc(Recipe::PadVoice), true, 0.5f, 0.5f, 1},
    {BossCue::ChDeath, "choir.md", "the dying Voice's note falling a fourth over one beat into the reverb",
     nullptr, SoundPlay::PadVoice, rc(Recipe::PadVoice), true, 0.5f, 1, 1, NoteRule::Midi, 0, -5.0f, 0.0f, 1.0f},
    {BossCue::ChCageTick, "choir.md", "the telegraph tick on every beat of the cage's telegraph bar", nullptr, SoundPlay::Sfx, sx(Sfx::Tick), false, 0.6f,
     0, 0},
    {BossCue::ChCageBurn, "choir.md", "the impact as the cage burns", nullptr, SoundPlay::Recipe, rc(Recipe::Impact), false, 0.7f, 0, 0},
    {BossCue::ChHeld, "choir.md", "the last Voice's note held for the telegraph bar, rising an octave as the beam fires",
     nullptr, SoundPlay::PadVoice, rc(Recipe::PadVoice), true, 0.6f, 6, 1, NoteRule::Midi, 0, 12.0f, 4.0f, 0.25f},
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
    {BossCue::HzEat, "horizon.md", "each body eaten plays the next note of the Track's arp an octave down, bending down a fifth",
     "the arp patch has no one-shot that bends: the pad voice plays the note and glides four scale steps down; the drone (a sub sine "
     "on the root sinking a semitone for every three cells, springing back on the bar line) is not played: no sub sine voice",
     SoundPlay::PadVoice, rc(Recipe::PadVoice), true, 0.45f, 0.5f, 0.5f, NoteRule::ArpCount, 0, -4.0f, 0.0f, 0.5f},
    {BossCue::HzFed, "horizon.md", "fed: the impact and the gong on the bar line, louder the bigger the meal", nullptr, SoundPlay::Pair,
     rc(Recipe::Impact), false, 0.9f, 0, 0, NoteRule::None, rc(Recipe::Gong)},
    {BossCue::HzStarved, "horizon.md", "starved: the pads and the bass cut out for the first beat under a reversed cymbal",
     "the band cannot cut its pads and bass for a beat: the reversed cymbal plays alone, over the first beat", SoundPlay::Recipe,
     rc(Recipe::RevCymbal), false, 0.7f, 1, 0},
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
                                       "9 dB quieter and panned to the far side; the second Echo 15 dB down and darker",
     "the lead has no octave-down, low-passed copy: the player's lead note plays 9 dB down (the second Echo 15 dB), from where the Echo is",
     SoundPlay::Lead, 0xFF, false, 0.35f, 0, 0},
    {BossCue::FbOpen, "feedback.md", "open: the pad's filter opens and the Head holds the Track's root two octaves up, quietly; it rises a semitone "
                                       "for every 10% of health lost",
     "the band cannot open or duck its pad on cue: the held root plays as a pad voice for a bar each time the Head opens; in Howl the "
     "delay's feedback is not turned up",
     SoundPlay::PadVoice, rc(Recipe::PadVoice), false, 0.25f, 4, 1, NoteRule::KeyRoot, 0, 0.0f, 0.0f, 0.0f, 6},
    {BossCue::FbSwallow, "feedback.md", "a swallowed shot is a closed hat", nullptr, SoundPlay::Recipe, rc(Recipe::Hat), false, 0.4f, 0, 0},
    {BossCue::FbNull, "(not on the page; chosen)", "a Null bursts: the Nova's blast, quieter", nullptr, SoundPlay::Sfx, sx(Sfx::NovaBlast), false,
     0.6f, 0, 0},
    {BossCue::FbCut, "(not on the page; chosen)", "the tape is cut: a closing sweep over two beats", nullptr, SoundPlay::Recipe, rc(Recipe::Sweep), false,
     0.8f, -1, 2},
    // ---- Moth Queen (minibosses.md)
    {BossCue::MqStep, "minibosses.md", "the court is the open hat, stepping round the stereo field with the ring's turn",
     "the hat is panned to the court, not round it with its turn", SoundPlay::Recipe, rc(Recipe::Hat), false, 0.45f, 1, 0},
    {BossCue::MqRosette, "minibosses.md", "each rosette is a plucked note of the Track's arpeggio", nullptr, SoundPlay::Lead, 0xFF, false, 0.3f, 0, 0},
    {BossCue::MqRiser, "minibosses.md", "bar three is a riser", nullptr, SoundPlay::Recipe, rc(Recipe::RevCymbal), false, 0.6f, 4, 0},
    {BossCue::MqBurst, "minibosses.md", "the burst is the clap on the downbeat", "no clap: the impact, quiet", SoundPlay::Recipe, rc(Recipe::Impact),
     false, 0.5f, 0, 0},
    {BossCue::MqOpen, "minibosses.md", "while she is open the bed drops to kick and pad",
     "the band cannot drop to kick and pad: the bed is low-passed while she is open", SoundPlay::LowPass, 0, false, 1.0f, 500, 4},
    // ---- Brood Mother (minibosses.md)
    {BossCue::BmLay, "minibosses.md", "the laying is a low tom on beat 1", nullptr, SoundPlay::Recipe, rc(Recipe::Tom), false, 0.7f, 0, 0,
     NoteRule::KeyRoot, 0, 0.0f, 0.0f, 0.0f, 2},
    {BossCue::BmHatch, "minibosses.md", "the hatching is a chirp for each egg, arpeggiated up the chord on beat 3", nullptr, SoundPlay::Recipe,
     rc(Recipe::Chirp), false, 0.35f, 0, 0, NoteRule::ChordUp, 0, 0.0f, 0.0f, 0.0f, 5},
    {BossCue::BmBreak, "minibosses.md", "a bell marks the break",
     "the threads' pad (one voice for each living broodling) is not played: no voice a body", SoundPlay::Recipe, rc(Recipe::Bell), false, 0.6f, 0, 0},
    // ---- Blink Star (minibosses.md)
    {BossCue::BsHop, "minibosses.md", "each hop is the next note of a five-note pentatonic run on the Track's root", nullptr, SoundPlay::Recipe,
     rc(Recipe::Bell), false, 0.5f, 0, 0, NoteRule::PentaRun, 0, 0.0f, 0.0f, 0.0f, 5},
    {BossCue::BsFire, "minibosses.md", "the firing is all five notes at once as a stab with a zap", "the stab of five notes is not built: the zap alone",
     SoundPlay::Recipe, rc(Recipe::Zap), false, 0.8f, 0, 0},
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
    {BossCue::CdChord, "cadence.md", "each pillar is its chord: while it sounds, a pad voice holds its root and the band's stab plays that chord on the One at 1.2",
     "the band's stab on the One at 1.2 (a pad voice on the root plays a bar)", SoundPlay::PadVoice, rc(Recipe::PadVoice), false, 0.5f, 4, 0.5f,
     NoteRule::ChordUp, 0, 0, 0, 0, 3},
    {BossCue::CdFan, "cadence.md", "each firing pillar's fan is a tom on its own chord's root, on one and three", "its own chord's root (the Track's chord plays)",
     SoundPlay::Recipe, rc(Recipe::Tom), false, 0.5f, 0, 0},
    {BossCue::CdTurn, "cadence.md", "the turn of the square is a reversed cymbal into the bar line", nullptr, SoundPlay::Recipe, rc(Recipe::RevCymbal), false,
     0.6f, 1, 0},
    {BossCue::CdInversion, "cadence.md", "Inversion is the music bus high-passed at 800 Hz for one bar", "a high-pass on the music bus (a sweep plays)",
     SoundPlay::Recipe, rc(Recipe::Sweep), false, 0.6f, 1, 4},
    {BossCue::CdBurst, "cadence.md", "death: the four chords in turn, then the tonic with the gong", "the chords (a gong a burst plays)", SoundPlay::Recipe,
     rc(Recipe::Gong), false, 0.6f, 0, 0},
    // ---- Arpeggiator (design/bosses/arpeggiator.md "Sound"; M5 part 6)
    {BossCue::ApBlock, "arpeggiator.md", "each block sounds its own note, on the P3 bell at 0.3, as it passes the row the nearest player is in",
     "as it leaves its port (its chord tone; the row of the nearest player is not followed)", SoundPlay::Recipe, rc(Recipe::Bell), false, 0.3f, 0, 0,
     NoteRule::ChordUp, 0, 0, 0, 0, 5},
    {BossCue::ApShutter, "arpeggiator.md", "the shutter is a woodblock on the One with a sweep opening over the beat", nullptr, SoundPlay::Pair,
     rc(Recipe::Woodblock), false, 0.5f, 1, 1, NoteRule::None, rc(Recipe::Sweep)},
    {BossCue::ApMove, "arpeggiator.md", "a change of side is a whoosh over the bar before", nullptr, SoundPlay::Recipe, rc(Recipe::Whoosh), false, 0.5f, 4,
     0},
    {BossCue::ApSecond, "arpeggiator.md", "the second machine arrives with the chord's stab", "the chord's stab (the zap plays)", SoundPlay::Recipe,
     rc(Recipe::Zap), false, 0.8f, 0, 0},
    {BossCue::ApDeath, "arpeggiator.md", "death: Shard chirps running up the scale over two beats", nullptr, SoundPlay::Recipe, rc(Recipe::Chirp), false,
     0.5f, 0, 0, NoteRule::ScaleUp, 0, 0, 0, 0, 6},
    // ---- Fata Morgana (design/bosses/morgana.md "Sound"; M5 part 6)
    {BossCue::MgStorm, "morgana.md", "the storm is a crackle and a whoosh that are heard outside the oasis; outside it the music bus is also low-passed at 2 kHz",
     "the bed low-passed at 2 kHz while a ship of this device is outside (the crackle, a held voice, is not played)", SoundPlay::LowPass, 0, false, 1.0f,
     2000, 1.5f},
    {BossCue::MgWhoosh, "morgana.md", "the storm is a crackle and a whoosh that are heard outside the oasis", "a whoosh a bar while a ship is outside",
     SoundPlay::Recipe, rc(Recipe::Whoosh), false, 0.3f, 4, 0},
    {BossCue::MgBlink, "morgana.md", "a mirage's blink is a soft tick on the snare", "a soft tick (the glass tick, quiet)", SoundPlay::Recipe,
     rc(Recipe::Glass), false, 0.25f, 0, 0},
    {BossCue::MgLeap, "morgana.md", "the leap is the lead's arpeggio as a run of chirps, up", "the chord's tones up (the lead's own arpeggio is not read)",
     SoundPlay::Recipe, rc(Recipe::Chirp), false, 0.5f, 0, 0, NoteRule::ChordUp, 0, 0, 0, 0, 5},
    {BossCue::MgBell, "morgana.md", "while she is over the water, a bell on every beat", nullptr, SoundPlay::Recipe, rc(Recipe::Bell), false, 0.5f, 0, 0},
    // ---- Kaleidos, Reprise (design/bosses/reprise.md "Sound"; M5 part 6)
    {BossCue::RpBell, "reprise.md", "each solo is in its boss's own voice: Kaleidos's bells", nullptr, SoundPlay::Recipe, rc(Recipe::Bell), false, 0.3f, 0, 0},
    {BossCue::RpString, "reprise.md", "each solo is in its boss's own voice: Longwave's rubber string", nullptr, SoundPlay::Recipe, rc(Recipe::RubberString),
     false, 0.5f, 2, 0},
    {BossCue::RpPad, "reprise.md", "each solo is in its boss's own voice: a pad voice for The Choir", nullptr, SoundPlay::PadVoice, rc(Recipe::PadVoice),
     false, 0.5f, 2, 0.5f, NoteRule::ChordUp, 0, 0, 0, 0, 4},
    {BossCue::RpSub, "reprise.md", "each solo is in its boss's own voice: the sub drop for Event Horizon", nullptr, SoundPlay::Recipe, rc(Recipe::SubDrop),
     false, 0.8f, 0, 0},
    {BossCue::RpTom, "reprise.md", "each solo is in its boss's own voice: toms three against four for Polyrhythm", nullptr, SoundPlay::Recipe,
     rc(Recipe::Tom), false, 0.5f, 0, 0, NoteRule::KeyRoot, 0, 0, 0, 0, 3},
    {BossCue::RpEcho, "reprise.md", "each solo is in its boss's own voice: the player's lead reversed for Feedback",
     "the player's lead forward, 9 dB down (a reversed lead is not built)", SoundPlay::Lead, 0xFF, false, 0.35f, 0, 0},
    {BossCue::RpArp, "reprise.md", "the last pattern is First Light's arpeggio on the P3 bell, doubled by the band's lead",
     "the bell (the band's lead doubles it only while a player fires)", SoundPlay::Recipe, rc(Recipe::Bell), true, 0.3f, 0, 0},
    {BossCue::RpDeath, "reprise.md", "death: the tonic held, the ripple a reversed cymbal ending on the encore's first downbeat",
     "the reversed cymbal ends on the bar line the Track ends on (the encore comes after the Track-change Break); the tonic is not held",
     SoundPlay::Recipe, rc(Recipe::RevCymbal), false, 0.7f, 4, 0},
    // ---- Mini-bosses batch 3 (minibosses.md; M5 part 6)
    {BossCue::MmFire, "minibosses.md", "its fire is the player's lead upside down: each lead note answered a thirty-second later by the note as far below "
                                         "the chord's root as the lead's is above, at 0.4",
     "the player's own lead note a thirty-second later at 0.4 (its inversion below the root is not built)", SoundPlay::Lead, 0xFF, false, 0.4f, 0, 0},
    {BossCue::MmDash, "minibosses.md", "its Dash is the Dash's sweep reversed", "the sweep closing (a reversed sweep is not built)", SoundPlay::Recipe,
     rc(Recipe::Sweep), false, 0.6f, -1, 0},
    {BossCue::MmGlass, "minibosses.md", "death is glass", nullptr, SoundPlay::Recipe, rc(Recipe::Glass), false, 0.8f, 0, 0},
    {BossCue::TtCut, "minibosses.md", "a cut strand is a zap and the drone falls silent for its beat (silence is safe)",
     "the zap (the beam's drone, a voice whose filter follows the beam round, is not built)", SoundPlay::Recipe, rc(Recipe::Zap), false, 0.7f, 0, 0},
    {BossCue::TtTom, "minibosses.md", "each node is a tom as it passes twelve o'clock", nullptr, SoundPlay::Recipe, rc(Recipe::Tom), false, 0.5f, 0, 0},
    {BossCue::WvLap, "minibosses.md", "each lap is a saw glide rising over its three beats, a step higher each lap", nullptr, SoundPlay::Recipe,
     rc(Recipe::SawGlide), false, 0.45f, 2, 3, NoteRule::ScaleUp, 0, 0, 0, 0, 4},
    {BossCue::WvKnot, "minibosses.md", "the knot is a woodblock and a bell on the One", nullptr, SoundPlay::Pair, rc(Recipe::Woodblock), false, 0.5f, 0, 0,
     NoteRule::None, rc(Recipe::Bell)},
    {BossCue::WvBurst, "minibosses.md", "the cage going off is a crackle swelling into the chord's stab", "a zap (the crackle swelling into the stab is not built)",
     SoundPlay::Recipe, rc(Recipe::Zap), false, 0.6f, 0, 0},
    {BossCue::DwThup, "minibosses.md", "each swallowed shot is a soft low thup, a step higher for each ring lit, so the fill can be heard",
     "a quiet low tom (one a sixteenth at most)", SoundPlay::Recipe, rc(Recipe::Tom), false, 0.25f, 0, 0, NoteRule::ScaleUp, 0, 0, 0, 0, 2},
    {BossCue::DwChoke, "minibosses.md", "choking is the gong with the bed low-passed for the bar", nullptr, SoundPlay::Recipe, rc(Recipe::Gong), false, 0.6f, 0, 0},
    {BossCue::DwLow, "minibosses.md", "choking is the gong with the bed low-passed for the bar", nullptr, SoundPlay::LowPass, 0, false, 1.0f, 600, 4},
    {BossCue::DwRing, "minibosses.md", "a ring given back is a reversed cymbal into the One and a stab",
     "the impact on the One (the ring is decided on the One: no reversed cymbal leads into it; no stab)", SoundPlay::Recipe, rc(Recipe::Impact), false, 0.5f,
     0, 0},
    // ---- Elites (design/traits.md: "Heard as it arrives"; M5 part 5)
    {BossCue::ElArrive, "traits.md", "the elite's low fifth (root and fifth of the Track's key) as it warps in", nullptr, SoundPlay::Sfx, sx(Sfx::Elite), false,
     0.9f, 0, 0},
    {BossCue::ElArmored, "traits.md", "Armored: a gong, short", "a short gong (the recipe rings 2.5 s)", SoundPlay::Recipe, rc(Recipe::Gong), false, 0.5f, 0, 0},
    {BossCue::ElSwift, "traits.md", "Swift: an upward sweep", nullptr, SoundPlay::Recipe, rc(Recipe::Sweep), false, 0.6f, 1, 1},
    {BossCue::ElGiant, "traits.md", "Giant: the sub drop", nullptr, SoundPlay::Recipe, rc(Recipe::SubDrop), false, 0.8f, 0, 0},
    {BossCue::ElSplitting, "traits.md", "Splitting: a glass crack", "a crack (the glass tick stands in)", SoundPlay::Recipe, rc(Recipe::Glass), false, 0.7f, 0,
     0},
    {BossCue::ElVolatile, "traits.md", "Volatile: a crackle", "a one-shot crackle (the Crackle recipe is a held voice)", SoundPlay::Recipe, rc(Recipe::Zap),
     false, 0.5f, 0, 0},
    {BossCue::ElRegenerating, "traits.md", "Regenerating: two bells rising", "two bells (one plays, the chord's root)", SoundPlay::Recipe, rc(Recipe::Bell),
     false, 0.6f, 0, 0},
    {BossCue::ElShielded, "traits.md", "Shielded: two woodblocks", nullptr, SoundPlay::Recipe2, rc(Recipe::Woodblock), false, 0.7f, 0, 0},
    {BossCue::ElTwin, "traits.md", "Twin: the fifth again, a beat later", nullptr, SoundPlay::Sfx, sx(Sfx::Elite), false, 0.7f, 0, 0},
    {BossCue::ElMagnetic, "traits.md", "Magnetic: Shard chirps falling", "falling chirps (one chirp plays)", SoundPlay::Recipe, rc(Recipe::Chirp), false, 0.7f,
     0, 0},
    {BossCue::ElCommander, "traits.md", "Commander: a snare roll of one beat", nullptr, SoundPlay::Roll, rc(Recipe::Roll), false, 0.6f, 1, 0},
    {BossCue::ElCloaked, "traits.md", "Cloaked: a short reversed cymbal", nullptr, SoundPlay::Recipe, rc(Recipe::RevCymbal), false, 0.6f, 1, 0},
    {BossCue::ElPhasing, "traits.md", "Phasing: the fifth, gated on the eighths", "a gate on the eighths", SoundPlay::Sfx, sx(Sfx::Elite), false, 0.6f, 0, 0},
    {BossCue::ElShatter, "traits.md", "Armored: the outer hull shatters at half health", "(the page names no sound: glass)", SoundPlay::Recipe,
     rc(Recipe::Glass), false, 0.8f, 0, 0},
    {BossCue::ElSplit, "traits.md", "Splitting: two copies leave it", "(the page names no sound: glass)", SoundPlay::Recipe, rc(Recipe::Glass), false, 0.6f, 0,
     0},
    {BossCue::ElFuse, "traits.md", "Volatile: the danger ring fills over two beats", "(the page names no sound: a rising sweep)", SoundPlay::Recipe,
     rc(Recipe::Sweep), false, 0.5f, 1, 2},
    {BossCue::ElBurst, "traits.md", "Volatile: it bursts on the beat", "(the page names no sound: the impact)", SoundPlay::Recipe, rc(Recipe::Impact), false,
     0.7f, 0, 0},
    {BossCue::ElRegen, "traits.md", "Regenerating heals 8% on a beat", "(the page names no sound: a soft bell)", SoundPlay::Recipe, rc(Recipe::Bell), false,
     0.25f, 0, 0},
    {BossCue::ElStun, "traits.md", "Commander dies: those near it stunned for a beat", "(the page names no sound: the sub drop)", SoundPlay::Recipe,
     rc(Recipe::SubDrop), false, 0.5f, 0, 0},
    {BossCue::HuKill, "arena.md", "Elite Hunt: the kill is the upgrade chord", nullptr, SoundPlay::Sfx, sx(Sfx::Upgrade), false, 0.9f, 0, 0},
    // ---- The arena (design/arena.md "Sound")
    {BossCue::VaHit, "arena.md", "Vault: each hit a glass tick on a note of the scale, one step up for each quarter lost", nullptr, SoundPlay::Recipe,
     rc(Recipe::Glass), false, 0.35f, 0, 0, NoteRule::ScaleUp, 0, 0, 0, 0, 6},
    {BossCue::VaBreak, "arena.md", "Vault: the break an arpeggio of bells up the chord with a glass burst", "the arpeggio (one bell and the glass play)",
     SoundPlay::Pair, rc(Recipe::Bell), false, 0.8f, 0, 0, NoteRule::None, rc(Recipe::Glass)},
    {BossCue::PyFill, "arena.md", "Pylon: the fill is a woodblock on each of the four beats, rising", nullptr, SoundPlay::Recipe, rc(Recipe::Woodblock), false,
     0.6f, 0, 0, NoteRule::ScaleUp, 0, 0, 0, 0, 5},
    {BossCue::PyTuned, "arena.md", "Pylon: tuned is three bells up the chord", "three bells (one plays)", SoundPlay::Recipe, rc(Recipe::Bell), false, 0.8f, 0,
     0},
    {BossCue::PyPulse, "arena.md", "Pylon: each pulse a bell on the chord's root an octave up, on the One", nullptr, SoundPlay::Recipe, rc(Recipe::Bell), false,
     0.5f, 0, 0},
    {BossCue::RiOpen, "arena.md", "Rift: a drone on the chord's bass note while it is open, its cutoff rising through the Build",
     "a held drone per Rift (one-shot: a falling glide)", SoundPlay::Recipe, rc(Recipe::SawGlide), false, 0.4f, -12, 2},
    {BossCue::RiThrough, "arena.md", "Rift: a tom for each body that comes through", "a tom a body (one plays a bar)", SoundPlay::Recipe, rc(Recipe::Tom), false,
     0.5f, 0, 0},
    {BossCue::RiClose, "arena.md", "Rift destroyed: a reversed cymbal cut short, then glass", nullptr, SoundPlay::Pair, rc(Recipe::RevCymbal), false, 0.7f, 1, 0,
     NoteRule::None, rc(Recipe::Glass)},
    {BossCue::BuBounce, "arena.md", "Bumper: a bounce is a tom on its chord tone (root, third, fifth, octave)", nullptr, SoundPlay::Recipe, rc(Recipe::Tom),
     false, 0.45f, 0, 0, NoteRule::ChordUp, 0, 0, 0, 0, 3},
    {BossCue::BuDischarge, "arena.md", "Bumper: the discharge is the chord's stab with a zap", "the chord's stab (the zap plays)", SoundPlay::Recipe,
     rc(Recipe::Zap), false, 0.8f, 0, 0},
    {BossCue::LeSplit, "arena.md", "Lens: each split is a glass tick a fifth above the lead's note", "a fifth above the lead (the glass at the root plays)",
     SoundPlay::Recipe, rc(Recipe::Glass), false, 0.25f, 0, 0},
    {BossCue::EvAnnounce, "arena.md", "an event is announced one bar ahead", "(each event's own announcement: the Comet's rising glide plays for all)",
     SoundPlay::Recipe, rc(Recipe::SawGlide), false, 0.5f, 12, 2},
    {BossCue::VdLand, "arena.md", "Vault Drop: it lands with the gong", nullptr, SoundPlay::Recipe, rc(Recipe::Gong), false, 0.8f, 0, 0},
    {BossCue::CmShard, "arena.md", "Comet: a bell on each Shard it drops", "a bell a Shard (one a beat plays)", SoundPlay::Recipe, rc(Recipe::Bell), false, 0.35f,
     0, 0},
    {BossCue::EcOn, "arena.md", "Eclipse: the music bus low-passed at 1.2 kHz for the eight bars", nullptr, SoundPlay::LowPass, 0, false, 1.0f, 1200, 32},
    {BossCue::EcOff, "arena.md", "Eclipse: the low-pass lets go", "(the bed's low-pass ends by its own time)", SoundPlay::None, 0, false, 0.0f, 0, 0},
    {BossCue::EcFlash, "arena.md", "Eclipse: a soft tick on each flash", nullptr, SoundPlay::Recipe, rc(Recipe::Hat), false, 0.3f, 0, 0},
    {BossCue::MeWarn, "arena.md", "Meteor Rain: each warning a saw glide falling over its two beats", nullptr, SoundPlay::Recipe, rc(Recipe::SawGlide), false,
     0.4f, -12, 2},
    {BossCue::MeLand, "arena.md", "Meteor Rain: each impact the sub drop and a crash, on the beat", nullptr, SoundPlay::Pair, rc(Recipe::SubDrop), false, 0.7f,
     0, 0, NoteRule::None, rc(Recipe::Impact)},
    {BossCue::CoRoll, "arena.md", "Convoy: a low tom on every beat while they roll, one voice a carrier", "one voice a carrier (one tom plays)",
     SoundPlay::Recipe, rc(Recipe::Tom), false, 0.4f, 0, 0},
    {BossCue::CoBreak, "arena.md", "Convoy: each carrier broken is the Boost bells", nullptr, SoundPlay::Sfx, sx(Sfx::Boost), false, 0.9f, 0, 0},
    {BossCue::StStrike, "arena.md", "Storm Cell: each strike a zap on the chord's three notes", "the chord's three notes (one zap plays)", SoundPlay::Recipe,
     rc(Recipe::Zap), false, 0.5f, 0, 0},
    {BossCue::StWarn, "arena.md", "Storm Cell: a crackle that rises as it nears", "a held crackle voice (a short sweep plays)", SoundPlay::Recipe,
     rc(Recipe::Sweep), false, 0.3f, 1, 0},
};
static_assert(sizeof(kRows) / sizeof(kRows[0]) == static_cast<u32>(BossCue::Count), "one row per BossCue, in its order");
}  // namespace

const FightSound* fightSounds(u32* count) {
    if (count) *count = static_cast<u32>(sizeof(kRows) / sizeof(kRows[0]));
    return kRows;
}

void playFightCue(Band& band, FightVoices& voices, const sim::Event& e, double tick, float pan, float bpm) {
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
        midi = audio::chordNote(degree, 3);
    } else if (r.note == NoteRule::KeyRoot) {
        // The Track's root: its first chord's bass, in the row's octave.
        const audio::MusicData& md = band.music();
        const u32 tr = band.track() < md.trackCount ? band.track() : 0u;
        const audio::Track& T = md.tracks[tr];
        const i32 root = T.chordCount ? static_cast<i32>(T.chords[T.progressionCount ? T.progression[0] : 0].bass % 12u) : 0;
        midi = 12 * (r.octave + 1) + root + static_cast<i32>(std::lround(e.f));
    } else if (r.note == NoteRule::ChordUp) {
        midi = audio::chordNote(static_cast<i32>(e.b % 4u), r.octave + static_cast<i32>(e.b / 4u), audio::NoteFrom::ChordTone);
    } else if (r.note == NoteRule::PentaRun || r.note == NoteRule::ScaleUp) {
        static const i32 kPenta[5] = {0, 3, 5, 7, 10};
        if (r.note == NoteRule::ScaleUp) {
            midi = audio::chordNote(static_cast<i32>(e.b % 8u), r.octave);
        } else {
            const audio::MusicData& md = band.music();
            const u32 tr = band.track() < md.trackCount ? band.track() : 0u;
            const audio::Track& T = md.tracks[tr];
            const i32 root = T.chordCount ? static_cast<i32>(T.chords[T.progressionCount ? T.progression[0] : 0].bass % 12u) : 0;
            midi = 12 * (r.octave + 1) + root + kPenta[e.b % 5u];
        }
    } else if (r.note == NoteRule::ArpCount) {
        midi = audio::chordNote(static_cast<i32>(e.b % 4u) * 2, 3);  // root, third, fifth, seventh up the scale, an octave down
    } else if (r.note == NoteRule::WallTone) {
        // Carom King's walls: the chord tone of the wall (0 root, 1 third, 2 fifth, 3 seventh); Princes an octave up.
        midi = audio::chordNote(static_cast<i32>(std::lround(e.f)), 5 + (e.b ? 1 : 0), audio::NoteFrom::ChordTone);
    }
    switch (r.play) {
        case SoundPlay::Sfx: {
            // A Carom Prince's bounce: quieter (the page: an octave up).
            const float g = r.cue == BossCue::CkBounce && e.b ? 0.35f : r.gain;
            band.sfxAt(static_cast<Sfx>(r.id), tick, g, pan);
            break;
        }
        case SoundPlay::Lead: {
            const u8 timbre = r.id == 0xFF ? static_cast<u8>(e.b % 4u) : r.id;
            if (r.cue == BossCue::FbEcho || r.cue == BossCue::RpEcho || r.cue == BossCue::MmFire) {
                // Feedback: the player's own lead (its voice), the step's degree; the second Echo 6 dB further down.
                const u8 degree = static_cast<u8>(static_cast<u64>(std::floor(e.tick / 24.0)) % 3u);
                band.leadNote(static_cast<u8>(e.b % 4u), degree, e.f > 0.5f ? 0.5f * r.gain : r.gain, pan, tick, 0xFF);  // (the Reprise's Echo, the Mimic too)
                break;
            }
            band.leadNote(0, static_cast<u8>(e.b % 3u), r.gain, pan, tick, timbre);
            break;
        }
        case SoundPlay::Recipe: band.playRecipe(static_cast<Recipe>(r.id), midi, r.gain, pan, tick, r.p1, r.p2); break;
        case SoundPlay::Recipe2:
            band.playRecipe(static_cast<Recipe>(r.id), midi, r.gain, pan, tick, r.p1, r.p2);
            band.playRecipe(static_cast<Recipe>(r.id), midi, r.gain, pan, tick + 0.5 * kTicksPerBeatD, r.p1, r.p2);
            break;
        case SoundPlay::PadVoice: {
            // One voice a Voice (slots 0-7): a new note on the same slot replaces the last.
            const u8 slot = static_cast<u8>(e.b % 8u);
            band.startVoice(slot, static_cast<Recipe>(r.id), midi, r.gain, pan, 0.0f);
            voices.stopAt[slot] = tick + static_cast<double>(r.p1) * kTicksPerBeatD;
            voices.release[slot] = r.p2 * beatS;
            voices.glideAt[slot] = -1.0;
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
            band.playRecipe(static_cast<Recipe>(r.id), midi, g, pan, tick, r.p1, r.p2);
            band.playRecipe(static_cast<Recipe>(r.id2), midi, g, pan, tick, r.p1, r.p2);
            break;
        }
        case SoundPlay::Hats: {
            // The Ring closing: quarters for half of it, eighths for a quarter, sixteenths for the last quarter.
            const double span = static_cast<double>(r.p1) * kTicksPerBeatD;
            double at = 0.0;
            while (at < span - 1e-6) {
                band.playRecipe(static_cast<Recipe>(r.id), -1, r.gain, pan, tick + at, 0.0f, 0.0f);
                at += at < 0.5 * span ? kTicksPerBeatD : (at < 0.75 * span ? 0.5 * kTicksPerBeatD : 0.25 * kTicksPerBeatD);
            }
            break;
        }
        case SoundPlay::Roll: {
            // The Ram's lock: the roll ends as it charges (two beats on, one for a lock straight after the last).
            const float beats = r.p1 - static_cast<float>(e.b);
            band.playRecipe(static_cast<Recipe>(r.id), midi, r.gain, pan, tick + static_cast<double>(beats) * kTicksPerBeatD, beats, r.p2);
            break;
        }
        case SoundPlay::LowPass:
            if (e.b) band.lowPassBed(r.p1, (e.f > 0.0f ? e.f : r.p2) * beatS);
            break;
        default: break;
    }
}

void updateFightVoices(Band& band, FightVoices& voices, double heard, bool all) {
    for (u8 k = 0; k < 16; ++k) {
        if (voices.glideAt[k] >= 0.0 && heard >= voices.glideAt[k] && voices.stopAt[k] >= 0.0 && !all) {
            band.setVoice(k, 0.0f, -1.0f, voices.glideTo[k], voices.glideS[k]);
            voices.glideAt[k] = -1.0;
        }
        if (voices.stopAt[k] < 0.0 || (!all && heard < voices.stopAt[k])) continue;
        band.stopVoice(k, all ? 0.1f : std::fmax(voices.release[k], 0.02f));
        voices.stopAt[k] = -1.0;
        voices.glideAt[k] = -1.0;
    }
}

}  // namespace rf
