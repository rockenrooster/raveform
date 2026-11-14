#pragma once
// ======================================================================================================
// THE FIGHTS' SOUNDS AND THE PENDING ONES (M5). Every sound a design page asks for goes through this one table:
// the fights' cues (design/bosses/*.md "Sound", in the words of design/music/sound_m5.json "recipes"), and later
// the structures', events' and traits'. Each row says what the page asks for and what plays: the engine's recipe
// (src/audio/cues.h, Band::playRecipe / startVoice / lowPassBed) where it can say it, else the nearest sound the
// band has, marked PENDING with what is missing. docs/PROGRESS.md ("Pending sounds") lists the PENDING rows, and the
// lead passes them to the audio engineer; when the engine can say one, its row changes here and nowhere else.
// M5 audio (docs/PROGRESS_m5_audio.md): the engine says every row now. What a page asks of a sound held while something
// lasts (a drone, a crackle, a pad of voices, the pad's tone) follows the world frame by frame (FightScene, read in
// updateFightVoices); the rows say what plays on each cue.
// ======================================================================================================

#include "core/math.h"
#include "core/types.h"
#include "sim/boss.h"

namespace rf {
namespace audio {
class CommandSink;
}
namespace sim {
struct Event;
class World;
}

// What a row plays.
enum class SoundPlay : u8 {
    None,      // nothing
    Sfx,       // a baked effect (audio::Sfx `id`) at `gain` on the cue's tick
    Lead,      // a lead note (the cue's place as a degree of the arpeggio; timbre `id`, 0xFF: the cue's Voice)
    Recipe,    // Band::playRecipe(audio::Recipe `id`, midi, gain, pan, tick, p1, p2); midi: the cue's f when `pitched`, else -1
    Recipe2,   // the same recipe twice, an eighth apart (the Hive's two toms)
    PadVoice,  // a held pad voice at the cue's note (f) in slot (cue.b % 8) for p1 beats, then released over p2 beats; with
               // glideSemis, it glides that far over glideOver beats from glideAfter beats on (setVoice's note)
    LowPass,   // Band::lowPassBed(p1 Hz, p2 beats) when the cue's b is 1
    Pair,      // recipe id and recipe id2 together on the cue's tick (the Ram's charge: the sub drop and the driven kick)
    Hats,      // the closed hat on a grid that tightens over p1 beats from the cue's tick (quarters, eighths, sixteenths)
    Roll,      // a roll recipe ending p1 beats after the cue's tick (p1 - b beats for the cue's b), p1 beats long; p2 its rise
    Custom,    // M5 audio: the row's own lines in pending_sounds.cpp (playCustom), as `asks` says; gain, p1, p2 as noted there
};
// How a row's note is found: none (-1: the recipe's default), the cue's f as a MIDI note, or against the Track's chord
// (audio::chordNote, resolved by the band at the tick it plays).
// ArpCount (Event Horizon's count): the meal's next note of the arpeggio (chord tones up the scale: 0 2 4 6 steps) an
// octave down, cue b = the meal.
// KeyRoot (Polyrhythm's drums): the Track's root (its first chord's bass) in MIDI octave octave, plus the cue's f semitones.
// ChordUp (the Brood Mother's hatching): chord tone b up the chord in octave; PentaRun (the Blink Star's hops): the b-th
// note of a minor pentatonic run on the Track's root in octave; ScaleUp (the Sentry Array's hand): b scale steps up.
enum class NoteRule : u8 { None, Midi, RunDegree, WallTone, ArpCount, KeyRoot, ChordUp, PentaRun, ScaleUp };

struct FightSound {
    sim::BossCue cue;
    const char* page;     // where the design asks for it
    const char* asks;     // what it asks for (sound_m5.json "recipes" words)
    const char* pending;  // nullptr: plays as asked. Else what the engine lacks (the row plays the nearest it has)
    SoundPlay play;
    u8 id;                // audio::Sfx, audio::Recipe or a timbre, by `play`
    bool pitched;         // the cue's f is a MIDI note for the recipe
    float gain, p1, p2;
    NoteRule note = NoteRule::None;  // (pitched rows: Midi)
    u8 id2 = 0;                      // Pair's second recipe
    float glideSemis = 0.0f, glideAfter = 0.0f, glideOver = 0.0f;  // PadVoice's glide (beats)
    i8 octave = 4;                   // KeyRoot's octave
    float send = 0.0f;               // M5 audio: a PadVoice's reverb send (choir.md: 0.5; 0 the effects' own)
};

// The table, one row per sim::BossCue (in its order).
const FightSound* fightSounds(u32* count);

// M5 audio: what the held voices follow, read from the world each frame (readFightScene), or written by a listening sheet.
// Ticks are the band's (the simulation's plus the band's offset).
struct FightScene {
    bool valid = false;
    u8 seat = 0;                 // this device's player
    double now = 0.0;            // the band tick now
    float tps = 192.0f;          // ticks a second
    Vec2 ship;                   // this device's ship
    bool fight = false;          // a boss or mini-boss is on (past its entrance, not dying)
    sim::BossKind kind = sim::BossKind::Kaleidos;
    u16 hzMeal = 0;              // Event Horizon: bodies eaten this bar (the meal meter: twelve cells)
    bool fbOpen = false, fbHowl = false;  // Feedback: the Head open (every Echo silent); Howl
    u8 fbSemis = 0;              // ... its held root's rise (a semitone for every tenth of its health lost)
    double fbDelay[2] = {384.0, 768.0};  // ... each Echo's delay (ticks)
    u8 broodlings = 0;           // Brood Mother: broodlings alive (hatched)
    bool tetherBeam = false;     // Twin Tether: its beam (not slack)
    float tetherNear = 0.0f, tetherPan = 0.0f;  // ... how near it has come round to the ship (0..1), where it is
    double wellRing = -1.0;      // Deep Well: the next ring given back (band tick; < 0 none foreseen)
    double weaverBurst = -1.0;   // Weaver Prime: the cage going off (band tick; < 0 none)
    bool mgOutside = false;      // Fata Morgana: this device's ship is outside the calm
    Vec2 apMachine[2], apDir[2];  // Arpeggiator: the machines' cores and their lanes' way
    float apSpeed = 420.0f, apLength = 900.0f;
    // The arena: the open Rifts (the nearest three: pan, gain by distance, a Rift Surge's place 0-2 or 0xFF), the Build's
    // progress (0..1), the Storm Cell (on, how near, pan), the Convoy's carriers alive (pans).
    u8 rifts = 0;
    float riftPan[3] = {}, riftGain[3] = {};
    u8 riftSurge[3] = {};
    float build = 0.0f;
    bool storm = false;
    float stormNear = 0.0f, stormPan = 0.0f;
    u8 carriers = 0;
    float carrierPan[4] = {};
};
void readFightScene(const sim::World& w, u32 seat, double offset, FightScene& out);

// The held voices a fight starts (the Choir's pads): released when their time is up. Play owns one.
// Slots (the band's held recipe voices): 0-7 a fight's pad voices (the Choir's Voices, Cadence's pillars, the Brood Mother's
// threads, Feedback's held root), 8 a fight's drone (Event Horizon's sub, Twin Tether's beam), 9 Fata Morgana's storm crackle,
// 10-12 the Rifts' drones, 13 the Storm Cell's crackle, 14 the Reprise's held tonic (it outlives the fight).
struct FightVoices {
    double stopAt[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};  // band ticks; -1 idle
    float release[16] = {};
    double glideAt[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};  // band ticks; -1 none
    i32 glideTo[16] = {};      // MIDI
    float glideS[16] = {};     // seconds
    bool keep[16] = {};        // M5 audio: runs its own time through a fight's end
    // M5 audio: the scene and what its voices were last told (only changes are posted).
    FightScene scene;
    u8 held[16] = {};          // what a scene voice holds (0: none; else its kind and note, as followScene keeps them)
    i32 heldNote[16] = {};
    float heldParam[16] = {}, heldGain[16] = {}, heldPan[16] = {};
    i8 padTone = 0;            // Feedback's pad: 0 as written, 1 open, -1 ducked and dull
    bool howl = false;
    double wellPosted = -1.0, weaverPosted = -1.0;  // the reversed cymbal and the crackle already sent into these ticks
};

// Plays a fight's cue (a sim::EventType::BossCue event) through the band on `tick` (the band's song ticks), panned;
// `bpm` the band's tempo (beats to seconds). Called from Play::afterTick only.
void playFightCue(audio::CommandSink& band, FightVoices& voices, const sim::Event& e, double tick, float pan, float bpm);
// Each frame: releases the held voices whose time is up (heard = the band's heard tick); `all` releases every one (a
// fight's end, a new run). M5 audio: with a world, the scene is read from it (this device's seat, the band's offset in ticks)
// and the held voices follow it; without one, they follow voices.scene as it was left (a listening sheet writes it).
void updateFightVoices(audio::CommandSink& band, FightVoices& voices, double heard, bool all, const sim::World* w = nullptr,
                       u32 seat = 0, double offset = 0.0);

}  // namespace rf
