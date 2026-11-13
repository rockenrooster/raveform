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
    Count
};
constexpr u32 kRecipeCount = static_cast<u32>(Recipe::Count);
constexpr u32 kRecipeVoices = 16;  // held voices the game addresses by slot (0..15): startVoice / setVoice / stopVoice

}  // namespace rf::audio
