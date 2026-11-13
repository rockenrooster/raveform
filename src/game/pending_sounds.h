#pragma once
// ======================================================================================================
// THE FIGHTS' SOUNDS AND THE PENDING ONES (M5). Every sound a design page asks for goes through this one table:
// the fights' cues (design/bosses/*.md "Sound", in the words of design/music/sound_m5.json "recipes"), and later
// the structures', events' and traits'. Each row says what the page asks for and what plays: the engine's recipe
// (src/audio/cues.h, Band::playRecipe / startVoice / lowPassBed) where it can say it, else the nearest sound the
// band has, marked PENDING with what is missing. docs/PROGRESS.md ("Pending sounds") lists the PENDING rows, and the
// lead passes them to the audio engineer; when the engine can say one, its row changes here and nowhere else.
// ======================================================================================================

#include "core/types.h"
#include "sim/boss.h"

namespace rf {
class Band;
namespace sim {
struct Event;
}

// What a row plays.
enum class SoundPlay : u8 {
    None,      // nothing
    Sfx,       // a baked effect (audio::Sfx `id`) at `gain` on the cue's tick
    Lead,      // a lead note (the cue's place as a degree of the arpeggio; timbre `id`, 0xFF: the cue's Voice)
    Recipe,    // Band::playRecipe(audio::Recipe `id`, midi, gain, pan, tick, p1, p2); midi: the cue's f when `pitched`, else -1
    Recipe2,   // the same recipe twice, an eighth apart (the Hive's two toms)
    PadVoice,  // a held pad voice at the cue's note (f) in slot (cue.b % 8) for p1 beats, then released over p2 beats
    LowPass,   // Band::lowPassBed(p1 Hz, p2 beats) when the cue's b is 1
};

struct FightSound {
    sim::BossCue cue;
    const char* page;     // where the design asks for it
    const char* asks;     // what it asks for (sound_m5.json "recipes" words)
    const char* pending;  // nullptr: plays as asked. Else what the engine lacks (the row plays the nearest it has)
    SoundPlay play;
    u8 id;                // audio::Sfx, audio::Recipe or a timbre, by `play`
    bool pitched;         // the cue's f is a MIDI note for the recipe
    float gain, p1, p2;
};

// The table, one row per sim::BossCue (in its order).
const FightSound* fightSounds(u32* count);

// The held voices a fight starts (the Choir's pads): released when their time is up. Play owns one.
struct FightVoices {
    double stopAt[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};  // band ticks; -1 idle
    float release[16] = {};
};

// Plays a fight's cue (a sim::EventType::BossCue event) through the band on `tick` (the band's song ticks), panned;
// `bpm` the band's tempo (beats to seconds). Called from Play::afterTick only.
void playFightCue(Band& band, FightVoices& voices, const sim::Event& e, double tick, float pan, float bpm);
// Each frame: releases the held voices whose time is up (heard = the band's heard tick); `all` releases every one (a
// fight's end, a new run).
void updateFightVoices(Band& band, FightVoices& voices, double heard, bool all);

}  // namespace rf
