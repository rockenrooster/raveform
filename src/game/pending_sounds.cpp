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
