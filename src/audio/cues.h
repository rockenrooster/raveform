#pragma once
// M5: the ids the game uses to tell the band about every upgrade and every fight sound (design/music/sound_m5.json).
// A contract with the game (src/game): values are appended, never renumbered. Mods, Riffs, Boosts and Fusions are
// numbered as GAME_DESIGN 6.2 to 6.5 (id = the table's number - 1).
//
// How the game drives them (src/audio/band.h):
//   - Band::setLoadout(slot, PlayerLoadout): every Mod's stacks, Riff levels, Boosts and Fusions a player has, posted
//     whenever they change. Whatever is patterned (the lead's changes, the Riffs' instruments, the Boosts' and Fusions'
//     layers, Kick Pulse's tom on each kick) the band plays from that alone, on its own clock.
//   - Band::cue(Cue, ...): a game event the band answers with a sound (a hit that forked, a chain that jumped, a Dash).
//     Placed on the next 1/32 step (or on a given tick), pitched to the chord; those marked "agg" are aggregated per
//     1/32 step like kills: one sound, its gain 1 - e^(-count / 2.5).
//   - Band::playRecipe / startVoice / setVoice / stopVoice / lowPassBed: the recipes the bosses', mini-bosses',
//     structures', events' and traits' pages name in their "Sound" sections, as one-shots or held voices.

#include "core/types.h"

namespace rf::audio {

// GAME_DESIGN 6.2 (36).
enum class Mod : u8 {
    Split, DoubleTime, Pierce, Ricochet, Fork, Seeker, HeavyStep, Accent, Echo, Wave, Overcharge, RearGuard,
    Arc, Burn, Shatter, GravityRounds, ExtraCell, Regen, Afterimage, Reserve, Shockstep, Aegis, LastLight, Slipstream,
    Magnet, Prospector, NovaStock, Encore, LuckyBreak, Collector, OnBeatCrit, Syncopation, BarLine, Crescendo,
    KickPulse, Afterparty, Count
};
constexpr u32 kModCount = static_cast<u32>(Mod::Count);

// GAME_DESIGN 6.3 (12).
enum class Riff : u8 {
    Orbitals, PulseRing, Sentinel, Lancer, Minelayer, StormCoil, Swarm, Singularity, Mirror, Comet, Sweeper, Flak, Count
};
constexpr u32 kRiffCount = static_cast<u32>(Riff::Count);

// GAME_DESIGN 6.4 (16).
enum class Boost : u8 {
    Prism, Hyper, SupernovaRounds, Colossus, TimeStretch, FreezeFrame, Halo, MagnetStorm, BlackHole, ChainStorm, Chorus,
    Beam, RicochetFever, Confetti, Fractal, Carousel, Count
};
constexpr u32 kBoostCount = static_cast<u32>(Boost::Count);

// GAME_DESIGN 6.5 (8). A Fusion replaces the sounds of its two ingredients.
enum class Fusion : u8 {
    HelixArray, StandingWave, TeslaBloom, ShardHalo, DownbeatCannon, CollapseField, Murmuration, Quartet, Count
};
constexpr u32 kFusionCount = static_cast<u32>(Fusion::Count);

// What one player has. Stacks and levels as the game counts them (0: none).
struct PlayerLoadout {
    u8 mods[kModCount] = {};
    u8 riffs[kRiffCount] = {};
    u16 boosts = 0;   // bit Boost::X: active now
    u8 fusions = 0;   // bit Fusion::X: owned
};

// Game events with a sound. `count`: how many happened at once; `value`: as noted; `pan` -1..1.
enum class Cue : u8 {
    // Mods
    ForkSplit,        // Fork: a shot forked on its first hit (agg): the fifth above, a 1/32 later, at 0.4
    HeavyStepBurst,   // Heavy Step: its orb exploded (agg): the small kill's thud at 0.5
    ArcChain,         // Arc: a chain jumped; value = the jump's number in its chain (1..3): a zap a chord tone higher each
    BurnCount,        // Burn: value = enemies burning now (0..30): the crackle's density (post when it changes)
    ShatterBurst,     // Shatter: a kill threw shrapnel (agg): a glass burst at 0.25
    GravityHit,       // Gravity Rounds: a hit (agg): a low thup at 0.15
    AfterimageDecoy,  // Afterimage: a decoy was left: the Dash's sweep a beat later at half level, from the decoy's pan
    ShockstepDash,    // Shockstep: a Dash: a tom on the root; value = 1 for a Perfect Dash (a second tom a fifth above)
    AegisBlock,       // Aegis: the arc stopped a bullet (agg): a high tick at 0.15
    LastLightNova,    // Last Light: the free mini-Nova: the Nova's blast at 0.6, no inhale
    CollectorBoost,   // Collector: a Boost was spawned: the Boost bells
    PlayerHit,        // the player was hit (Crescendo: the lead and the pad fall back at once)
    RicochetBounce,   // Ricochet (and Ricochet Fever): a bounce; value = the bounce's number: a blip a scale step up each
    // Riffs (the rest of each Riff is patterned)
    StormCoilStrike,  // Storm Coil: something was struck on this triplet eighth (agg): the next zap of the cycle
    // Boosts
    SupernovaImpact,  // Supernova Rounds: an impact (agg): the small kill's thud at 0.3
    ColossusRam,      // Colossus: an enemy rammed (agg): a tom on the chord's root
    FreezeShatter,    // Freeze Frame: an enemy shattered (agg): a glass burst
    ConfettiKill,     // Confetti: a kill burst (agg): a run of six Shard chirps up the scale
    BlackHoleNova,    // Black Hole: the Nova pressed: the three-beat rising inhale, then the blast with the sub drop
    // Appended 2026-11-13 (M5 audio, play test 3: GAME_DESIGN 5 Extra Life, 6.3 Orbitals' guard ring).
    ExtraLifePickup,  // an Extra Life picked up: the Cell's note (the player's lead an octave over the root) and the fifth above
    ExtraLifeUse,     // an Extra Life spent: the Nova's blast with the Track's tonic chord on the stab
    OrbitalsBreak,    // the Orbitals' ring broke on a hit: the glass crack, two octaves down
    OrbitalsRelight,  // a shard of the ring relit; value = its place (0, 1, ...): a shaker tick a scale step higher each
    Count
};
constexpr u32 kCueCount = static_cast<u32>(Cue::Count);

// The recipes of sound_m5.json "recipes": one-shots (playRecipe) and held voices (startVoice).
// Parameters: `midi` the note (-1: the recipe's default, or the chord's root where it takes one); p1, p2 as noted.
enum class Recipe : u8 {
    Tom,        // the pulse voice at a note (default: the chord's root, octave 3)
    SubDrop,    // the impact's sine alone, 120 -> 38 Hz, 700 ms
    Gong,       // the P3 bell two octaves under `midi` (default the chord's root), ratio 1.4, index 3, 2.5 s
    Bell,       // the P3 bell at a note
    Zap,        // a sine gliding from twice the note to the note, 90 ms, ring-modulated at 2.76 x
    Glass,      // three sines at 3, 4.2, 5.4 x a note in the seventh octave (default the chord's root), 60 ms
    SawGlide,   // a detuned saw pair through the lead's low-pass; p1 = interval in semitones (+ up), p2 = beats (Alpha: 7, 1)
    Sweep,      // the Dash's noise through a low-pass; p1 = +1 opening / -1 closing, p2 = beats (0: the Dash's 110 ms)
    RevCymbal,  // the impact's crash reversed; p1 = beats long (1 or 4); it ENDS on the tick given (without one: p1 beats on)
    Woodblock,  // 1 kHz -> 700 Hz in 8 ms, 35 ms, a 3 kHz click; `midi` >= 0 pitches it (by scale step on the page)
    Chirp,      // the Shard's chirp at a note
    Drone,      // voice: a detuned saw pair an octave under the bass; setVoice param = low-pass cutoff in Hz
    PadVoice,   // voice: one note of the pad patch, held
    Crackle,    // voice: 5 ms bursts of 2-6 kHz noise; setVoice param = bursts a second
    Whoosh,     // noise through a band-pass sweeping 400 Hz -> 3 kHz over p1 beats (default 1); p2 = 1: and back
    Roll,       // the Build's snare roll over p1 beats (default 4), ending on the tick given (without one: p1 beats on)
    // Appended 2026-11-13 (the content engineer's eight: longwave.md, choir.md, minibosses.md).
    RubberString,  // the bass patch with twice its decay, a note held p1 sixteenths (default 1); `midi` default: the chord's
                   // bass an octave up (longwave.md's run: chordNote(degree, 3) up the scale)
    Impact,        // the Drop's impact (crash and sine) as a one-shot, p1 semitones up (the whip: 7); a `midi` instead puts
                   // its sine's settling pitch (38 Hz) on that note
    Hat,           // the Track's closed hat, on the tick given (the game's own grid); p1 = 1: its open hat
    TomRoll,       // a tom roll over p1 beats (default 2) ending on the tick given: quarters, eighths, sixteenths, then
                   // thirty-seconds, rising from 0.35 to 1 and from `midi` (default the chord's root, octave 3) up p2
                   // semitones (default 12)
    DrivenKick,    // the Track's kick through more drive: tanh(p1 x kick) (p1 default 3), its peak kept
    RimShot,       // a rim shot at a note (Carom King's walls: chordNote(0..3, 5, NoteFrom::ChordTone))
    // Appended 2026-11-13, the fights' pending sounds (game/pending_sounds.cpp; docs/PROGRESS_m5_audio.md).
    Clap,          // the band's clap (the Moth Queen's burst on the downbeat)
    Snare,         // the band's snare; at a low gain a ghost note (a mirage's blink, "a soft tick on the snare")
    Stab,          // the band's stab synth on the music's stab bus: the chord at the tick (midi -1), or up to five notes given
                   // with stabNotes(); gain = its velocity (1.2 allowed); p1 = its length x (default 1)
    Crack,         // a glass crack: a 3 ms burst of 3-9 kHz noise into the glass's three sines (a note's pitch class, octave 7)
    CrackleShot,   // the crackle as a one-shot (baked): p1 = beats long (default 1); p2 = 1: swelling (density and level rising
                   // to its end, which lands on the tick given: the Weaver's cage "swelling into the chord's stab")
    DashReverse,   // the Dash's sweep reversed (the Mirror Mimic's late Dash)
    Thup,          // Gravity Rounds' soft low thup at a note (default its own, about 120 Hz; the Deep Well's swallow)
    SubSine,       // voice: a sine at a note, held (Event Horizon's drone); setVoice glides it
    EliteGated,    // the elite's arrival fifth (in the Track's key) gated on the eighths from the tick: sounding on the beat's first
                   // eighth, silent on its second (traits.md Phasing)
    Count
};
// The held recipes (startVoice); the rest are one-shots (playRecipe).
constexpr bool recipeHeld(Recipe r) { return r == Recipe::Drone || r == Recipe::PadVoice || r == Recipe::Crackle || r == Recipe::SubSine; }
constexpr u32 kRecipeCount = static_cast<u32>(Recipe::Count);
constexpr u32 kRecipeVoices = 16;  // held voices the game addresses by slot (0..15): startVoice / setVoice / stopVoice

// Appended 2026-11-13: a note named against the chord, resolved by the band at the tick it plays (the game does not know
// the chord). Pass what chordNote() returns wherever a recipe takes `midi` (playRecipe, startVoice, setVoice): `amount`
// scale steps of the Track's scale, or semitones, or chord tones (0 root, 1 third, 2 fifth, 3 seventh, 4 octave, 5 the
// third an octave up ...; found in the chord's own notes, else the scale's) above the chord's root in `octave` (MIDI
// octaves: 4 holds 60..71). Scale steps and semitones may be negative (down).
// Appended 2026-11-13 (the fights' pending sounds): Arp, the bar's arpeggio note `amount` (0..15, the lead's own) with the
// octave field as a shift (4: as written, 3: an octave down); ArpRank, the bar's arpeggio's distinct pitches from the lowest
// (amount 0, 1, ...; past the last, an octave up), the octave field a shift as for Arp; Lead, a player's last lead note
// (the octave field: the player) plus `amount` semitones.
enum class NoteFrom : u8 { ScaleSteps, Semitones, ChordTone, Arp, ArpRank, Lead };
constexpr i32 kChordNote = 0x10000;  // a `midi` at or above this is a chordNote()
constexpr i32 chordNote(i32 amount, i32 octave = 4, NoteFrom from = NoteFrom::ScaleSteps) {
    return kChordNote | (static_cast<i32>(from) << 12) | ((octave & 15) << 8) | ((amount + 128) & 255);
}
// A chordNote resolved `ticks` (up to 8191) before the tick it plays on: an Arpeggiator block sounds its own note (the one of
// the step it left on) when it passes the player's row.
constexpr i32 chordNoteAt(i32 note, i32 ticksBefore) { return note | ((ticksBefore & 8191) << 17); }
// Recipe::Stab's notes (MIDI, up to five), for the `midi` of playRecipe: a chord that is not the bar's (Cadence's pillars, the
// Blink Star's five).
constexpr i32 kStabNotes = 0x40000000;
constexpr i32 stabNotes(i32 n0, i32 n1 = 0, i32 n2 = 0, i32 n3 = 0, i32 n4 = 0) {
    // Each note as 0..63 above MIDI 36 (C2..D#7); 0 for none (the root C2 cannot be given, nor wanted).
    const auto p = [](i32 n) { return n <= 36 || n > 99 ? 0 : n - 36; };
    return kStabNotes | p(n0) | (p(n1) << 6) | (p(n2) << 12) | (p(n3) << 18) | (p(n4) << 24);
}

}  // namespace rf::audio
