// THE PENDING SOUNDS (M5): see pending_sounds.h. One row per sound a page asks for; the stand-in is what the band has.

#include "game/pending_sounds.h"

#include "audio/bake.h"
#include "audio/band.h"
#include "sim/world.h"

namespace rf {

using sim::BossCue;
using audio::Sfx;

namespace {
constexpr u8 sfx(Sfx s) { return static_cast<u8>(s); }

// In sim::BossCue's order. "(exists)": the engine has it already; the row is here so the fight speaks in one place.
const PendingSound kRows[] = {
    // ---- Longwave (design/bosses/longwave.md "Sound")
    {BossCue::LwRun, "longwave.md", "a rubber-string voice (the bass patch an octave up, decay x 2), one note on every crest step: the crest's place "
                                    "in its wave as a degree of the Track's scale (0 2 3 5 7 8 10 12 above the chord root)",
     StandIn::Lead, 3, 0.45f},
    {BossCue::LwTrough, "longwave.md", "the telegraph tick on every step a trough fires (exists)", StandIn::Sfx, sfx(Sfx::Tick), 0.45f},
    {BossCue::LwJaw, "longwave.md", "the elite-arrives patch a low fifth down, on beat 1 while the jaw is open", StandIn::Sfx, sfx(Sfx::Elite), 0.6f},
    {BossCue::LwLunge, "(not on the page; chosen)", "the sweep (the Dash's noise) as the head dashes", StandIn::Sfx, sfx(Sfx::Dash), 0.9f},
    {BossCue::LwWhip, "longwave.md", "the impact a fifth up, on the whip's downbeat", StandIn::Sfx, sfx(Sfx::KillLarge), 0.8f},
    // ---- The Choir (choir.md "Sound")
    {BossCue::ChVoice, "choir.md", "pad_voice: the pad patch as a voice (attack 20 ms, release one beat, reverb send 0.5), the Voice's own note "
                                   "(D E F G A B C D) every time it sings",
     StandIn::Lead, 0xFF, 0.5f},
    {BossCue::ChDeath, "choir.md", "the dying Voice's note falling a fourth over one beat into the reverb", StandIn::Sfx, sfx(Sfx::KillMedium), 0.8f},
    {BossCue::ChCageTick, "choir.md", "the telegraph tick on every beat of the cage's telegraph bar (exists)", StandIn::Sfx, sfx(Sfx::Tick), 0.6f},
    {BossCue::ChCageBurn, "choir.md", "the impact as the cage burns", StandIn::Sfx, sfx(Sfx::KillLarge), 0.7f},
    {BossCue::ChHeld, "choir.md", "the last Voice's note held for the telegraph bar, rising an octave as the beam fires", StandIn::Lead, 3, 0.6f},
    // ---- Alpha Hound (minibosses.md)
    {BossCue::AlHowl, "minibosses.md", "saw_glide up a fifth over one beat (the howl, beat 1)", StandIn::Sfx, sfx(Sfx::Elite), 0.7f},
    {BossCue::AlCharge, "minibosses.md", "roll: the Build's snare roll into beat 3 (the pack's charge)", StandIn::Sfx, sfx(Sfx::Spawn), 0.7f},
    {BossCue::AlLunge, "minibosses.md", "sub_drop (the lunge)", StandIn::Sfx, sfx(Sfx::Dash), 0.9f},
    // ---- The Hive (minibosses.md)
    {BossCue::HvShape, "minibosses.md", "rev_cymbal into the bar line (a change of shape)", StandIn::Sfx, sfx(Sfx::Spawn), 0.6f},
    {BossCue::HvRing, "minibosses.md", "closed hats that tighten as the Ring closes", StandIn::Sfx, sfx(Sfx::Tick), 0.5f},
    {BossCue::HvSpear, "minibosses.md", "two toms (tom) for the Spear", StandIn::Sfx, sfx(Sfx::Tick), 0.5f},
    {BossCue::HvWall, "minibosses.md", "a noise sweep for the Wall", StandIn::Sfx, sfx(Sfx::Dash), 0.5f},
    // ---- Battering Ram (minibosses.md)
    {BossCue::RmLock, "minibosses.md", "a tom roll rising through beats 3 and 4 (the lock)", StandIn::Sfx, sfx(Sfx::Tick), 0.6f},
    {BossCue::RmCharge, "minibosses.md", "sub_drop and a driven kick (the charge)", StandIn::Sfx, sfx(Sfx::Dash), 1.0f},
    {BossCue::RmWall, "minibosses.md", "gong (the wall rings)", StandIn::Sfx, sfx(Sfx::KillLarge), 1.0f},
    {BossCue::RmStun, "minibosses.md", "low_pass_bed while it is stunned (exists: Band::setMuffle)", StandIn::MuffleOn, 0, 0.0f},
    // ---- Carom King (minibosses.md)
    {BossCue::CkBounce, "minibosses.md", "a rim shot at the wall's note of the Track's chord (left root, top third, right fifth, bottom seventh); "
                                         "Princes an octave up on the half beats",
     StandIn::Sfx, sfx(Sfx::EnemyShot), 0.6f},
    // ---- Every mini-boss
    {BossCue::MiniEnter, "(not on the page; chosen)", "the elite-arrives patch under the name card", StandIn::Sfx, sfx(Sfx::Elite), 0.9f},
    {BossCue::MiniDeath, "minibosses.md", "(its death plays as a large kill; the bed comes back from a stun)", StandIn::MuffleOff, 0, 0.0f},
    {BossCue::MiniGone, "minibosses.md", "(it warps out; the bed comes back from a stun)", StandIn::MuffleOff, 0, 0.0f},
};
static_assert(sizeof(kRows) / sizeof(kRows[0]) == static_cast<u32>(BossCue::Count), "one row per BossCue, in its order");
}  // namespace

const PendingSound* pendingSounds(u32* count) {
    if (count) *count = static_cast<u32>(sizeof(kRows) / sizeof(kRows[0]));
    return kRows;
}

void playFightCue(Band& band, const sim::Event& e, double tick, float pan) {
    const u32 c = e.a;
    if (c >= static_cast<u32>(BossCue::Count)) return;
    const PendingSound& r = kRows[c];
    switch (r.standIn) {
        case StandIn::Sfx: {
            // A Carom Prince's bounce: the same note an octave up, quieter (the stand-in: quieter).
            const float g = r.cue == BossCue::CkBounce && e.b ? 0.35f : r.gain;
            band.sfxAt(static_cast<Sfx>(r.sfx), tick, g, pan);
            break;
        }
        case StandIn::Lead: {
            // The cue's place (a wave place, a Voice) as a degree of the lead's arpeggio; the Choir's Voices each their timbre.
            const u8 timbre = r.sfx == 0xFF ? static_cast<u8>(e.b % 4u) : r.sfx;
            band.leadNote(0, static_cast<u8>(r.cue == BossCue::ChHeld ? 3u : e.b % 3u), r.gain, pan, tick, timbre);
            break;
        }
        case StandIn::MuffleOn: band.setMuffle(e.b != 0); break;  // (RmStun: b = 1 on, 0 off)
        case StandIn::MuffleOff: band.setMuffle(false); break;
        default: break;
    }
}

}  // namespace rf
