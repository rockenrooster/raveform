#pragma once
// The music as data (design/music/*.json, embedded at build time, parsed once at start-up): the sound
// (patches.json), its three variants (variants.json), the ten Tracks of the set list (track01.json to
// track10.json) and the conductor's rules (conductor.json). Numbers are read from the files, never retyped:
// the resolved parameter sets follow the prototype's order (patches.json, then the variant, then the
// Track's own overrides).

#include <cstdint>

#include "core/types.h"

namespace rf::audio {

using i16 = std::int16_t;

// M5: the set list is ten Tracks (design/music/README.md). Every table sized by the Track count holds this many.
constexpr u32 kMaxTracks = 10;
constexpr u32 kMaxVariants = 4;

enum class Section : u8 { Groove, Build, Drop, Break, Count };
const char* sectionName(Section s);  // "groove", ...

// Mix buses, named as in musicproto (send / duck keys).
enum class Bus : u8 { Drums, HatsBreak, Clap, Snare, Pulse, Lead, Bass, Stab, Pad, Fx, Sfx, Count };
constexpr u32 kBusCount = static_cast<u32>(Bus::Count);
const char* busName(Bus b);

// Patterned layers of a Track ("patterns": 16 steps, velocity ninths).
enum class Pat : u8 { Kick, Clap, HatOff, Hat16, Ohat, Shaker, Stab, Pulse, Count };
constexpr u32 kPatCount = static_cast<u32>(Pat::Count);
const char* patName(Pat p);

enum class BassKey : u8 { None, Groove, GrooveAlt, Drop, Count };
enum class LeadMode : u8 { Off, Normal, Double, Sparse };

struct KickPatch {
    float sweepHz, sweepTau, settleHz, settleTau, decay, attack, click, clickDecay, drive;
};
struct HatPatch {
    float closedDecay, openDecay;
};
struct LeadPatch {
    float detuneCents, sub, fcStart, fcEnd, fcTau, q, attack, decay, length, keytrack;
};
struct BassPatch {
    float squareMix, sub, fcStart, fcEnd, fcTau, q, attack, decay, release, drive;
};
struct StabPatch {
    float detuneCents, fcStart, fcEnd, fcTau, q, attack, decay, length;
};
struct PadPatch {
    float detuneCents, fcBase, fcLfo, lfoHz, q, attack, release, highpass;
};
struct DelayPatch {
    float steps, feedback, mix, repeats;
};
struct ReverbPatch {
    float rt60, predelay, highpass, lowpass, mix;
};
struct MixLevels {
    float kick, clap, snare, hat, ohat, shaker, bass, lead, pad, stab, pulse, riser, impact, swell;
};

// One resolved sound (patches.json + variant + Track overrides).
struct SoundParams {
    float targetLufs = -14.0f;
    MixLevels mix{};
    float sectionGain[4][2] = {};  // [Section] = {at the start, at the end} (only the Build ramps)
    float send[kBusCount] = {};
    float duck[kBusCount] = {};
    float kickHz = 55.0f;
    KickPatch kick{};
    HatPatch hat{};
    LeadPatch lead{};
    BassPatch bass{};
    StabPatch stab{};
    PadPatch pad{};
    DelayPatch delay{};
    ReverbPatch reverb{};
    float sidechainTau = 0.11f;
    float masterDrive = 1.25f;
};

struct Chord {
    char name[8] = {};
    u8 bass = 0;
    u8 pad[6] = {};
    u8 padCount = 0;
    u8 stab[4] = {};
    u8 stabCount = 0;
    u8 arp[16] = {};
};

struct BassNote {
    float step = 0.0f;   // sixteenth in the bar
    float semis = 0.0f;  // from the chord's bass note
    float vel = 0.0f;
    float steps = 0.0f;  // length in sixteenths
};
struct BassPattern {
    BassNote note[16];
    u32 count = 0;
};

struct ArrangementPart {
    Section section = Section::Groove;
    u32 bars = 0;
    float pat[kPatCount] = {};  // 0: off; else the layer's value (only hat_off scales its gain by it)
    BassKey bass = BassKey::None;
    LeadMode lead = LeadMode::Off;
    float pad = 0.0f;           // pad level, 0: off
};

struct Track {
    char id[16] = {};
    char name[32] = {};
    float bpm = 120.0f;
    i8 scale[12] = {};
    u32 scaleCount = 0;
    Chord chords[8];
    u32 chordCount = 0;
    u8 progression[16] = {};  // chord indices
    u32 progressionCount = 0;
    float pattern[kPatCount][16] = {};
    bool hasPattern[kPatCount] = {};
    BassPattern bass[static_cast<u32>(BassKey::Count)];
    float longestBassSteps = 0.0f;  // over every bass pattern (musicproto.render_bass voice length)
    ArrangementPart parts[16];
    u32 partCount = 0;
    u32 arrangementBars = 0;
    const Chord& chordAtBar(u32 bar) const { return chords[progression[bar % progressionCount]]; }
};

struct Variant {
    char id[16] = {};
    char name[24] = {};
};

// conductor.json. Structured numbers are read directly; the numbers that the file states in prose are
// extracted from those sentences (with a check of how many there are, so an edit that changes the
// sentence's shape fails loudly instead of being misread).
struct ConductorRules {
    // bed
    struct BedLayer {
        Pat pat = Pat::Kick;
        bool isBass = false;
        bool isPad = false;
        float from = 0.0f;
    } bed[12];
    u32 bedCount = 0;
    float hysteresis = 0.05f;
    float padLevelLo = 0.6f, padAtLo = 0.65f, padLevelHi = 1.0f, padAtHi = 1.0f;
    u32 fillPhraseBars = 8;
    u32 fillPickupStep = 15;
    // build
    float snareRollLo = 0.35f, snareRollHi = 1.0f;
    float buildPadBrightLo = 1.0f, buildPadBrightHi = 1.9f;
    float buildLastBarSpeed = 0.85f;
    // drop
    float dropPadBright = 1.5f;
    // break
    float breakHatLowpassHz = 5000.0f;
    float breakPadLevel = 0.9f, breakPadBright = 0.55f;
    u8 breakSparseSteps[3] = {0, 6, 12};
    // lead
    u32 gridTicks[3] = {24, 16, 12};
    u32 surgeSteps[2] = {1, 2};  // sections.drop.surgeSteps: the Surge's two gears (M3 stage B)
    float accentBeat = 1.0f, accentEighth = 0.82f, accentOther = 0.7f;
    float panDepth = 0.25f, panRate = 0.9f;
    float othersLevel = 0.35f, othersFarLevel = 0.15f, othersFarWu = 1200.0f;
    // mods and boosts
    u8 splitDegrees[4] = {2, 4, 7, 6};
    u32 splitDegreeCount = 4;
    float splitLevel = 0.6f;
    float pierceScale = 1.25f;
    float overchargeDrive = 0.25f, overchargeDb = 0.5f;
    float critLevel = 0.5f;
    float prismLevel = 0.4f;
    u32 pulseRingEveryBeatFrom = 2;

    // M5: the other Mods, Riffs 4-12 and the other Boosts (conductor.json "mods", "riffs", "boosts": the blocks that came
    // from sound_m5.json). Levels are linear, relative to the voice's own mix level; steps are sixteenths; ticks 96 a beat.
    // Riffs.
    float lancerPattern[16] = {}, lancerFastPattern[16] = {};  // level 1; from lancerFastFrom: the beats of "says"
    float lancerLength = 2.0f, lancerLevel = 1.0f, lancerSwell = 0.3f;
    u32 lancerFastFrom = 3;
    float minePattern[16] = {};
    float mineClap = 0.8f, mineImpact = 0.4f;
    u32 mineHatStep = 8;
    float stormLevel = 0.25f;
    u32 swarmGridTicks = 12, swarmFrom = 0;
    u32 swarmCount[5] = {8, 12, 16, 20, 24};
    float swarmLevel = 0.3f;
    u32 singEveryBars = 4, singSwellBeats = 3;
    float singSwell = 0.5f, singImpact = 0.8f;
    i32 mirrorTranspose = 12;
    float mirrorLevel[5] = {0.4f, 0.5f, 0.55f, 0.6f, 0.7f};
    float cometPattern[16] = {};
    float cometLevel = 0.3f;
    float sweepCutLo = 300.0f, sweepCutHi = 2400.0f, sweepLevel = 0.2f;
    u32 sweepTurnBars = 2;
    float flakPattern[16] = {};
    float flakLevel = 0.5f;
    u32 flakClapFrom = 4, flakClapTicks = 12;
    // Mods.
    u32 forkTicks = 12;
    float forkLevel = 0.4f;
    float seekerMs = 12.0f;
    u32 heavyEvery = 4, heavyAt = 3;  // lead steps k with k % heavyEvery == heavyAt
    float heavyLevel = 0.8f, heavyThud = 0.5f;
    float accentLevel = 1.4f, accentStab = 0.3f;
    float echoMix = 0.15f;
    float waveHz = 5.5f, waveCents = 18.0f;
    u8 rearSteps[4] = {2, 6, 10, 14}, rearSteps2[2] = {1, 9};
    float rearLevel = 0.5f;
    float burnLevel = 0.08f, shatterLevel = 0.25f, gravityLevel = 0.15f, aegisDb = 6.0f, lastLightLevel = 0.6f;
    float syncLouder = 0.3f, barLineStab = 0.5f, crescendoDbPerBar = 0.2f, crescendoMaxDb = 2.0f, kickPulseLevel = 0.2f;
    // Boosts.
    float supernovaLevel = 0.3f, colossusLevel = 0.5f, freezeHz = 800.0f, chainStormLevel = 0.35f, chorusLevel = 0.4f;
    u32 fractalTicks = 12;
    float fractalOctave = 0.3f, fractalDouble = 0.15f;
};

// M5: the lead's grid index (into ConductorRules::gridTicks) for a player's Double-Time stacks and Afterparty stacks in a
// bar of a section, as the engine plays it and the main thread mirrors it for the lead rule (Band::computeShots): the
// Surge (the first half of the Drop) moves it one step along in its first half and two in its second; Afterparty makes
// the Surge two bars longer a stack (the second gear held) and moves its grid one step further; capped at the last.
u32 leadGridIndex(const u32 surgeSteps[2], u32 doubleTime, u32 afterparty, Section section, u32 sectionBar, u32 sectionBars);

// Loudness calibration (src/audio/loudness_calibration.json, written by raveform_headless --calibrate):
// the output gain that puts each Track x variant's staged arrangement at target_lufs, as the prototype
// normalises its renders. `hash` identifies the music data it was made from.
struct LoudnessCalibration {
    float gainDb[kMaxTracks][kMaxVariants] = {};  // [track][variant]
    bool have[kMaxTracks][kMaxVariants] = {};
    u64 hash = 0;
};

struct MusicData {
    static constexpr u32 kMaxTracks = audio::kMaxTracks;
    static constexpr u32 kMaxVariants = audio::kMaxVariants;
    Track tracks[kMaxTracks];
    u32 trackCount = 0;
    Variant variants[kMaxVariants];
    u32 variantCount = 0;
    u32 defaultVariant = 0;  // variants.json "reference"
    SoundParams sound[kMaxTracks][kMaxVariants];  // resolved
    ConductorRules conductor;
    LoudnessCalibration calibration;
    u64 hash = 0;  // FNV-1a of the embedded music files (and the DSP version)
    bool calibrationCurrent = false;

    // Parses the embedded files. Logs and returns false on any error. Load time only (allocates
    // temporarily for the JSON documents).
    bool load();
    i32 trackIndex(const char* id) const;    // "track01" or "1"; -1 when unknown
    i32 variantIndex(const char* id) const;  // "club"; -1 when unknown
    float outputGainDb(u32 track, u32 variant) const;
};

// Bumped whenever the engine's DSP changes the loudness of a render (invalidates the calibration).
// 4 (M5): voices summed in the order they start; the section gain's ramp across a bar line planned a section ahead.
// 5 (M5): the lead's and the pads' PolyBLEPs four at a time (t x 1/dt), the bell and the bass's sub on a polynomial sine.
constexpr u32 kDspVersion = 5;

}  // namespace rf::audio
