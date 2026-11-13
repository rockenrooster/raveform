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
     "a rubber-string voice (the bass patch up an octave, twice its decay), pitched in semitones above the chord's root", SoundPlay::Lead, 3, false,
     0.45f, 0, 0},
    {BossCue::LwTrough, "longwave.md", "the telegraph tick on every step a trough fires", nullptr, SoundPlay::Sfx, sx(Sfx::Tick), false, 0.45f, 0, 0},
    {BossCue::LwJaw, "longwave.md", "the elite-arrives patch, its low fifth, on beat 1 while the jaw is open", nullptr, SoundPlay::Sfx, sx(Sfx::Elite),
     false, 0.6f, 0, 0},
    {BossCue::LwLunge, "(not on the page; chosen)", "a closing sweep as the head dashes", nullptr, SoundPlay::Recipe, rc(Recipe::Sweep), false, 0.8f, -1,
     1},
    {BossCue::LwWhip, "longwave.md", "the impact a fifth up, on the whip's downbeat", "the impact (crash and sine) as a one-shot with a pitch",
     SoundPlay::Sfx, sx(Sfx::KillLarge), false, 0.8f, 0, 0},
    // ---- The Choir (choir.md "Sound")
    {BossCue::ChVoice, "choir.md", "pad_voice: the pad patch as a voice (attack 20 ms, release one beat, reverb send 0.5), the Voice's own note every "
                                   "time it sings",
     nullptr, SoundPlay::PadVoice, rc(Recipe::PadVoice), true, 0.5f, 0.5f, 1},
    {BossCue::ChDeath, "choir.md", "the dying Voice's note falling a fourth over one beat into the reverb",
     "a pad note that glides (the stand-in glides a saw pair down a fourth)", SoundPlay::Recipe, rc(Recipe::SawGlide), true, 0.5f, -5, 1},
    {BossCue::ChCageTick, "choir.md", "the telegraph tick on every beat of the cage's telegraph bar", nullptr, SoundPlay::Sfx, sx(Sfx::Tick), false, 0.6f,
     0, 0},
    {BossCue::ChCageBurn, "choir.md", "the impact as the cage burns", "the impact (crash and sine) as a one-shot", SoundPlay::Sfx, sx(Sfx::KillLarge),
     false, 0.7f, 0, 0},
    {BossCue::ChHeld, "choir.md", "the last Voice's note held for the telegraph bar, rising an octave as the beam fires",
     "a held pad note that rises an octave (the stand-in holds the note for the bar)", SoundPlay::PadVoice, rc(Recipe::PadVoice), true, 0.6f, 4, 1},
    // ---- Alpha Hound (minibosses.md)
    {BossCue::AlHowl, "minibosses.md", "saw_glide up a fifth over one beat (the howl, beat 1)", nullptr, SoundPlay::Recipe, rc(Recipe::SawGlide), false,
     0.7f, 7, 1},
    {BossCue::AlCharge, "minibosses.md", "roll: the Build's snare roll into beat 3 (the pack's charge)", nullptr, SoundPlay::Recipe, rc(Recipe::Roll),
     false, 0.7f, 2, 0},
    {BossCue::AlLunge, "minibosses.md", "sub_drop (the lunge)", nullptr, SoundPlay::Recipe, rc(Recipe::SubDrop), false, 0.9f, 0, 0},
    // ---- The Hive (minibosses.md)
    {BossCue::HvShape, "minibosses.md", "rev_cymbal into the bar line (a change of shape)", nullptr, SoundPlay::Recipe, rc(Recipe::RevCymbal), false,
     0.6f, 1, 0},
    {BossCue::HvRing, "minibosses.md", "closed hats that tighten as the Ring closes", "closed hats on a tightening grid (the stand-in: the tick)",
     SoundPlay::Sfx, sx(Sfx::Tick), false, 0.5f, 0, 0},
    {BossCue::HvSpear, "minibosses.md", "two toms for the Spear", nullptr, SoundPlay::Recipe2, rc(Recipe::Tom), false, 0.6f, 0, 0},
    {BossCue::HvWall, "minibosses.md", "a noise sweep for the Wall", nullptr, SoundPlay::Recipe, rc(Recipe::Sweep), false, 0.5f, 1, 2},
    // ---- Battering Ram (minibosses.md)
    {BossCue::RmLock, "minibosses.md", "a tom roll rising through beats 3 and 4 (the lock)", "a tom roll that rises (the stand-in: the snare roll)",
     SoundPlay::Recipe, rc(Recipe::Roll), false, 0.6f, 2, 0},
    {BossCue::RmCharge, "minibosses.md", "sub_drop and a driven kick (the charge)", "a driven kick as a one-shot (the sub drop plays)", SoundPlay::Recipe,
     rc(Recipe::SubDrop), false, 1.0f, 0, 0},
    {BossCue::RmWall, "minibosses.md", "gong (the wall rings)", nullptr, SoundPlay::Recipe, rc(Recipe::Gong), false, 1.0f, 0, 0},
    {BossCue::RmStun, "minibosses.md", "low_pass_bed while it is stunned (three beats, two from half health)", nullptr, SoundPlay::LowPass, 0, false, 1.0f,
     600, 3},
    // ---- Carom King (minibosses.md)
    {BossCue::CkBounce, "minibosses.md", "a rim shot at the wall's note of the Track's chord (left root, top third, right fifth, bottom seventh); "
                                         "Princes an octave up on the half beats",
     "a rim shot, pitched to a chord tone (root, third, fifth, seventh) by the band", SoundPlay::Sfx, sx(Sfx::EnemyShot), false, 0.6f, 0, 0},
    // ---- Every mini-boss
    {BossCue::MiniEnter, "(not on the page; chosen)", "the elite-arrives patch under the name card", nullptr, SoundPlay::Sfx, sx(Sfx::Elite), false, 0.9f,
     0, 0},
    {BossCue::MiniDeath, "minibosses.md", "(its death is a large kill, Play)", nullptr, SoundPlay::None, 0, false, 0, 0, 0},
    {BossCue::MiniGone, "minibosses.md", "(it warps out without a sound)", nullptr, SoundPlay::None, 0, false, 0, 0, 0},
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
    const i32 midi = r.pitched ? static_cast<i32>(std::lround(e.f)) : -1;
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
        if (voices.stopAt[k] < 0.0 || (!all && heard < voices.stopAt[k])) continue;
        band.stopVoice(k, all ? 0.1f : std::fmax(voices.release[k], 0.02f));
        voices.stopAt[k] = -1.0;
    }
}

}  // namespace rf
