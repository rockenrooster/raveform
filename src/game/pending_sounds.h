#pragma once
// ======================================================================================================
// THE PENDING SOUNDS (M5). Every sound a design page asks for that the band's engine does not have yet goes
// through this one table, and nowhere else: the fights' cues (design/bosses/*.md "Sound", in the words of
// design/music/sound_m5.json "recipes"), and later the upgrades', structures' and traits'. Each row says what the
// page asks for and what plays in its place today (the nearest effect the band has). When the audio engineer's
// commands exist (src/audio, branch m5-audio), each row's stand-in becomes the real call, in one pass, here.
// docs/PROGRESS.md (M5 part 1, "Pending sounds") lists the same rows.
// ======================================================================================================

#include "core/types.h"
#include "sim/boss.h"

namespace rf {
class Band;
namespace sim {
struct Event;
}

// What plays in a pending sound's place today.
enum class StandIn : u8 {
    None,      // nothing (a rest)
    Sfx,       // a baked effect (audio::Sfx `sfx`) at `gain`, on the cue's tick
    Lead,      // a lead note (scale degree from the cue, timbre from the cue) at `gain`
    MuffleOn,  // the music bus low-passed (Band::setMuffle(true): the recipe "low_pass_bed")
    MuffleOff,
};

struct PendingSound {
    sim::BossCue cue;
    const char* page;    // where the design asks for it
    const char* recipe;  // what it asks for (sound_m5.json "recipes" words)
    StandIn standIn;
    u8 sfx;              // audio::Sfx when standIn == Sfx
    float gain;
};

// The table, one row per sim::BossCue (in its order).
const PendingSound* pendingSounds(u32* count);

// Plays a fight's cue (a sim::EventType::BossCue event) through the band: the stand-in of its row, on `tick` (the band's
// song ticks), panned. Called from Play::afterTick only.
void playFightCue(Band& band, const sim::Event& e, double tick, float pan);

}  // namespace rf
