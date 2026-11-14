#pragma once
// The band (TECH_SPEC 6): the audio thread owns the musical clock and plays everything patterned,
// sample-accurately, from the Track data and a small mirror of game state. The DSP is a port of
// design/tools/musicproto.py and sfxproto.py (the specification); docs/PROGRESS.md (M2) lists where a
// real-time engine has to differ.
//
// Threads: render() runs on the audio thread (no allocation, locks, logging, file access or JNI);
// post() and popEvent() are the two ends of lock-free SPSC rings used by one other thread (the main
// thread, or the offline renderer). Everything else is set up by init() at load time.

#include <atomic>

#include "audio/bake.h"
#include "audio/cues.h"
#include "audio/dsp.h"
#include "audio/music_data.h"
#include "audio/reverb.h"
#include "audio/spsc.h"
#include "audio/tempo_map.h"

namespace rf::audio {

constexpr u32 kBlock = 32;  // inner processing block (TECH_SPEC 6.2)
constexpr u32 kMaxPlayers = 4;

enum class Mode : u8 {
    Silent,     // nothing sequenced (effects still play)
    Staged,     // the Track's "arrangement" bar by bar, exactly as the prototype renders it
    Conductor,  // the game: phrase cycle, intensity, players and Riffs (conductor.json)
    Metronome,  // the sync calibration screen: a click on every beat at 120 BPM
    Script,     // offline sheets: notes and effects placed by commands only, dry
};

enum class Timbre : u8 { P1Saw, P2Square, P3Bell, P4Stab, Count };
// Riff, Mod, Boost, Fusion, Cue, Recipe: audio/cues.h (M5).
enum class Form : u8 { Short, Standard };  // 8/4/8/4 or 12/4/16/4 bars (GAME_DESIGN 4.1, play test 2)

// What the band played, stamped in song time: `frame` is the write frame the sound starts on. The main
// thread applies each one when heard time reaches it (TECH_SPEC 6.6).
// M5: Beat and Bar come from the song clock (every beat and bar line, whatever plays); Kick is a kick the Track's
// pattern really has. Afterglow (half time), Mirage (broken kick) and Last Light (two-step) have beats without a
// kick and kicks off the beat: nothing may read Kick as the beat.
enum class BeatKind : u8 {
    Kick, Snare, Clap, Hat, Shaker, Bass, Lead, Stab, Pulse, Pad, Riser, Swell, Impact,
    Bar, Beat, Section, Click, Sfx, Tempo, Variant, Track, Count
};
const char* beatKindName(BeatKind k);

struct BeatEvent {
    i64 frame = 0;       // write frame of the onset
    double tick = -1.0;  // musical position (< 0: not on the grid)
    double x = 0.0;      // Tempo: ticks per frame; notes: frequency in Hz; Kick: the pattern's velocity (0..1);
                         // Riser, Swell, Impact: the playback rate
    float value = 0.0f;  // velocity, level or gain
    u32 index = 0;       // Bar, Section, Beat: bar number; Lead: step in the bar
    BeatKind kind = BeatKind::Beat;
    u8 a = 0;            // note (MIDI) / section / variant / track / effect
    u8 b = 0;            // player / hat kind (0 closed, 1 open) / bars in the section / grid ticks
    u8 c = 0;            // bar in the section / timbre
};

// M5: the upgrades' patterned sounds, reported as Sfx events with a = kUpgradeEvent + this (value: the gain; x: the note,
// or the playback rate where it has none), so tests and sheets can see what played when. (Impacts are not Impact events:
// those are the Drop's, with their heavy haptic.)
enum class UpgradeSound : u8 {
    LancerChord, LancerCharge, MineLaid, MineBlast, SwarmBell, SingularitySwell, SingularityBoom, CometOut, CometBack,
    FlakSnare, FlakClap, CannonOne, CollapseSwell, MurmurationTrill, TeslaZap, HaloGlass, BarLineZap, ModStab, ChainStormZap,
    CarouselTom, KickPulseTom, FreezeGlass, StandingWave, HeavyStepBass, ColossusBass,
    MirrorNote, QuartetNote, ChorusNote, FractalNote, SyncBlip, RearGuardNote, Count
};

// M5 (tests): a lead voice as the engine holds it (Engine::leadVoices).
struct LeadVoiceInfo {
    u8 player = 0;
    u8 timbre = 0;
    i64 start = 0;
    float hz = 0.0f, vel = 0.0f, gain = 0.0f;  // gain: as asked, whatever the pan
    u32 length = 0;
    float glideSemis = 0.0f, vibSemis = 0.0f, vibPhase = 0.0f;
    u32 glideN = 0;
};
constexpr u8 kUpgradeEvent = 96;

enum class Timing : u8 { Now, Next32, AtTick };

enum class CmdType : u8 {
    SetMode,       // a = Mode, b = track (Staged: loop when c = 1)
    SetTrack,      // a = track: Conductor: a Break that changes Track (tempo ramp, key on bar 5); else at the next bar.
                   // M3: b = 1 at the next bar line without a Break; x = tempo (BPM; 0: the Track's own)
    SetVariant,    // a = variant, applied on the next bar line
    SetIntensity,  // x = 0..1 (director intensity)
    NextSection,   // the next section of the cycle starts on the next bar line
    SetSection,    // a = Section, from the next bar line
    HoldSection,   // a = 1: the current section repeats until released
    SetForm,       // a = Form
    SetFire,       // a = player, b = held
    SetTimbre,     // a = player, b = Timbre
    SetPlayers,    // a = player count (1..4); b = the local player
    SetRiff,       // a = player, b = Riff, c = level (0 = none)
    SetMods,       // a = player; mods packed: split, doubleTime, pierce, overcharge, onBeatCrit (one byte each in `mods`)
    SetBoost,      // a = player, b = boost (0 prism, 1 hyper), c = on
    SetDistance,   // a = player, x = distance to the local player in world units
    Sfx,           // a = Sfx, x = gain multiplier, y = pan; timing: when / tick
    Kill,          // a = size (0 small, 1 medium, 2 large), b = count
    Shard,         // one pickup: the next note of the run
    Nova,          // press: inhale now, blast on the next beat
    Warp,          // an enemy becomes solid on `tick` (the warp-in swell ends there); y = pan
    Note,          // Script: a = Timbre, b = MIDI note, x = velocity, y = pan, gain in `level`
    Drum,          // Script: a = Pat (kit sample), x = gain, y = pan
    SetGains,      // x = music gain, y = effects gain (linear)
    Reset,         // silence everything and restart the song clock at the next block
    SetSurge,      // listening tests only: a = grid steps the Surge moves the lead (conductor.json: 1), from the next bar
    SetPhrase,     // M3, the game's director: from the next bar line, a = Section, b = bar in it, c = its length
    SetBoss,       // M3, the boss's bar from the next bar line: a = pairs firing (bits 0-2) | 16 on sixteenths, b = beats
                   // that fire (bits 0-3), c = 1 gong on beat 1, 2 beam riser over the bar, 4 beam stab on beat 1, 8 on
    SetMuffle,     // M3 stage C (screens.md pause): the music bus's low-pass, x = cutoff in Hz (0: open), y = ramp in s
    SetRate,       // M4 (TECH_SPEC 8.2): the song's tempo times x (a co-op client's gentle rate correction), from the next
                   // bar line; the engine takes 0.99..1.01
    HoldBar,       // M4 (TECH_SPEC 8.2 re-seek): the next bar line waits x seconds (the song holds there, then goes on)
    LeadNote,      // play test 1 / M4: a = player, b = degree on the Track's root (0 root, 1 fifth, 2 octave, 3 an octave
                   // down, 4 two octaves up), c = Timbre (0xFF: the player's own), x = gain (x the lead's level), y = pan;
                   // timing Now or AtTick (tick)
    SetBeacon,     // M4 (look.md 6): a = player, b = 1 down: its lead holds the Track's root an octave down with an
                   // eighth-note tremolo, 12 dB under the leads; y = pan
    SetHostOnly,   // M4 co-op, the SOUND setting "music from host only": a = 1: the bed and the other players' leads
                   // are silent on this device (its own lead and the effects stay)
    // M5 (audio/cues.h): the whole loadout, the game's events, the fights' recipes.
    SetModStacks,  // a = player, b = the first Mod, c = how many (<= 8), stacks in `mods`
    SetFusions,    // a = player, b = Fusion bits
    Cue,           // a = Cue, b = player, c = count (<= 255), x = value, y = pan; timing Next32 or AtTick
    Recipe,        // a = Recipe, c = MIDI (255: the default), x = gain, y = pan, z = p1, level = p2; timing Now / Next32 / AtTick;
                   // M5: b = 1 aggregated (one shot of the recipe at its note on a 1/32 step, its gain 1 - e^(-count / 2.5);
                   // mods[7] = how many this command counts for, 0: one)
    VoiceStart,    // a = voice slot, b = Recipe, c = MIDI (255: the default), x = gain, y = pan, z = param
    VoiceSet,      // a = voice slot, x = param, y = gain (< 0: unchanged); M5: c = a note to glide to (0 or 255: none),
                   // z = the glide in seconds
                   // (M5: c = 254 in Recipe, VoiceStart and VoiceSet: a chordNote(), its from / octave / amount + 128 in
                   // mods[0..2]: encodeNote)
    VoiceStop,     // a = voice slot, x = release in seconds
    LowPassBed,    // x = cutoff in Hz, y = seconds (then the bus opens again over 150 ms)
    // Appended 2026-11-13, the fights' pending sounds (game/pending_sounds.cpp):
    CutBed,        // music buses silenced for a while: mods[0] | mods[1] << 8 = the buses (bit Bus::X), b = 1: the hats and the
                   // shaker too (the drums but the kick); timing Now or AtTick (from the tick); y = seconds (0: lift it now)
    HighPassBed,   // the music bus high-passed: x = cutoff in Hz (0: lift it), y = seconds; timing Now or AtTick
    PadTone,       // the band's pad: x = its cutoff x (1: as written), y = its gain x (1), z = the ramp in seconds
    DelayBoost,    // the lead's delay: x = feedback added (0: as written), a = repeats (0: as written)
    LeadEcho,      // a note of a player's lead played again (a fight's), on the fight's own lead (at most kFightLeadVoices):
                   // a = player, b = LeadEchoMode, c (and mods[0..4]) = the note (Note mode; encodeNote: a MIDI note or a
                   // chordNote), x = gain (x the lead's level), y = pan, z = cutoff Hz (0: the patch's own), level = the ticks
                   // back the note was played (Recorded, Mirror, Reverse: the player's note then); timing Now or AtTick;
                   // mods[5] = Timbre (255: the player's own), mods[6] = the octaves it moves (signed), mods[7] = a bend in
                   // semitones (signed: it glides that far over its length)
};
// VoiceStart (M5): mods[5] = the voice's reverb send in percent (0: the effects' own), mods[6] = 1: its chordNote is resolved
// again on every bar line (it follows the chord). VoiceSet (M5): mods[5] = 1: a new pan in `level`.
// LeadEcho's modes: the note as recorded; its mirror about the chord's root (as far below it as the note is above); as recorded
// with its envelope reversed (a swell cut off); a note given (c).
enum class LeadEchoMode : u8 { Recorded, Mirror, Reverse, Note };
constexpr u8 kFightLeadPlayer = 6;   // the lead voices a fight plays (its own cap: kFightLeadVoices); 7 is Kaleidos's bells
constexpr u32 kFightLeadVoices = 2;  // (the lead's ruling: Feedback's echoes two at most)

struct Command {
    CmdType type = CmdType::Reset;
    Timing when = Timing::Now;
    u8 a = 0, b = 0, c = 0;
    u8 mods[8] = {};
    float x = 0.0f, y = 0.0f, level = 1.0f, z = 0.0f;
    double tick = 0.0;
};

// M5: a recipe command's note (Recipe, VoiceStart, VoiceSet): c = the MIDI note, 255 none (the recipe's default), 254 a
// chordNote() with its parts in mods[0..2] (and mods[3..4]: chordNoteAt's ticks before), 253 stabNotes() in mods[0..4].
inline void encodeNote(Command& c, i32 midi) {
    if (midi >= kStabNotes) {
        c.c = 253;
        for (u32 k = 0; k < 5; ++k) c.mods[k] = static_cast<u8>(((midi >> (6 * k)) & 63) ? ((midi >> (6 * k)) & 63) + 36 : 0);
    } else if (midi >= kChordNote) {
        c.c = 254;
        c.mods[0] = static_cast<u8>((midi >> 12) & 15);
        c.mods[1] = static_cast<u8>((midi >> 8) & 15);
        c.mods[2] = static_cast<u8>(midi & 255);
        const u32 back = static_cast<u32>(midi >> 17) & 8191u;
        c.mods[3] = static_cast<u8>(back & 255u);
        c.mods[4] = static_cast<u8>(back >> 8);
    } else {
        c.c = static_cast<u8>(midi >= 0 && midi < 253 ? midi : 255);
    }
}

// Live numbers for overlays and benchmarks (written by the audio thread, read anywhere).
struct EngineStats {
    std::atomic<u32> voices{0};       // active now
    std::atomic<u32> voicesPeak{0};
    std::atomic<u32> leadPeak{0};     // M5: the lead voices' peak (of voicesPeak)
    std::atomic<u32> steals{0};       // a pool was full and the oldest voice gave way
    std::atomic<u32> bar{0};
    std::atomic<u8> section{0};
    std::atomic<u8> sectionBar{0};
    std::atomic<u8> sectionBars{0};
    std::atomic<u8> track{0};
    std::atomic<u8> variant{0};
    std::atomic<u8> mode{0};
    std::atomic<float> bpm{0.0f};
    std::atomic<float> intensity{0.0f};
    std::atomic<float> masterPeak{0.0f};  // since the last read-and-reset by the reader
};

// M5, the worst case for the phone (--band max; raveform_headless --render max): the full band's load test (the Drop held,
// four players firing, intensity 1) with every Riff at level 5 among the four players (and Mirror for each), the heaviest
// Mods at their caps, two Boosts each and the local player's eight Fusions (M5 audio; content_sound.cpp says which).
// maxLoadSetup: the commands that set it up (after SetMode); maxLoadFrame: the game's events of one 1/60 s frame (kills,
// Shards, the Mods' and Riffs' cues; M5 audio, `fights`: the fights' and the arena's sounds on top). Each returns how many it
// wrote (at most `cap`, which should be 64).
u32 maxLoadSetup(Command* out, u32 cap);
u32 maxLoadFrame(u32 frame, Command* out, u32 cap, bool fights = true);

// Offline profiling: nanoseconds spent per stage of renderBlock (set with Engine::setProfile).
// M5: the lead per timbre (each voice timed: a few percent of overhead, profiling runs only).
struct EngineProfile {
    // M5 audio: Content, the held recipe voices (the fights' and the upgrades' drones, pad voices, crackles), was in Samples.
    enum Stage : u32 { Lead, LeadSquare, LeadBell, LeadStab, Bass, Stab, Pad, Samples, BusFx, Delay, Reverb, SfxReverb, Master, Content, Count };
    u64 ns[Count] = {};
    static const char* name(u32 s);
};

// Offline only: called after each block with every bus (after its effects) and the reverb return, all
// before the master stage, so stems can be compared with the prototype's.
using StemTapFn = void (*)(void* user, const float* const* busL, const float* const* busR, const float* wetL,
                           const float* wetR, u32 n);

class Engine {
public:
    Engine() = default;
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    ~Engine();

    // Load time. `music` and `bank` must outlive the engine.
    bool init(const MusicData& music, const SampleBank& bank, u32 sampleRate, u64 seed = 1);

    // Audio thread: writes `frames` interleaved frames of `channels` (1 or 2; more: extra channels zero).
    void render(float* out, u32 frames, u32 channels);

    // Producer (one thread).
    bool post(const Command& c) { return commands_.push(c); }
    // Consumer (one thread).
    bool popEvent(BeatEvent& e) { return events_.pop(e); }
    u32 eventsDropped() const { return events_.dropped(); }
    u32 commandsDropped() const { return commands_.dropped(); }

    const EngineStats& stats() const { return stats_; }
    i64 writeFrame() const { return writeFrame_.load(std::memory_order_acquire); }
    u32 sampleRate() const { return sr_; }
    const MusicData& music() const { return *music_; }
    // Offline calibration only: the master without the output gain and the limiter.
    void setRawOutput(bool raw) { rawOutput_ = raw; }
    void setProfile(EngineProfile* p) { profile_ = p; }
    void setStemTap(StemTapFn fn, void* user) {
        stemFn_ = fn;
        stemUser_ = user;
    }
    // M5 audio, offline only (the output stage's check): each block, the stage's input (music and effects summed) and its
    // output, frame for frame (the output kLimDelay frames behind).
    using StageTapFn = void (*)(void* user, const float* inL, const float* inR, const float* outL, const float* outR, u32 n);
    void setStageTap(StageTapFn fn, void* user) {
        stageFn_ = fn;
        stageUser_ = user;
    }

    // Diagnostics for tests: the engine's own view (audio thread or a stopped engine only).
    const TempoMap& tempo() const { return tempo_; }
    // M5 (tests): the sidechain envelope (0..1, before each bus's depth) of the last block rendered, frame by frame from
    // the block's first frame (frame_ - n), and the block's length.
    const float* duckBlock() const { return duck_; }
    u32 duckBlockFrames() const { return duckFrames_; }
    // M5 (tests): the active lead voices (at most `cap`), the lead delay's length in frames, the pad voices' opening.
    u32 leadVoices(LeadVoiceInfo* out, u32 cap) const;
    u32 delayFrames() const { return delayD_; }
    float padOpenNow() const;

private:
    struct LeadVoice;
    struct BassVoice;
    struct StabVoice;
    struct PadVoice;
    struct SampleVoice;
    struct Pending;
    struct BarPlan;
    struct Player;

    // Commands and planning.
    void applyCommand(const Command& c);
    void applyContentCommand(const Command& c);  // M5: cues, recipes, held voices, the timed low-pass (content_sound.cpp)
    // M5 (content_sound.cpp): recipes as one-shots at a frame (p1, p2 as cues.h says), the live voices, the cues'
    // aggregation per 1/32 step.
    struct ContentVoice;
    bool initContent();
    void freeContent();
    void stopContent();
    void playRecipeAt(Recipe r, i32 midi, float gain, float pan, i64 frame, double tick, float p1, float p2);
    void startContentVoice(ContentVoice& v, Recipe r, i32 midi, float gain, float pan, float param, float p1, float p2);
    void renderContent(u32 n);
    void flushCues(i64 tick);
    i32 chordRootMidi(i32 octave) const;  // the bar's chord root in an octave (4: MIDI 60..71)
    const Chord& currentChord() const;
    i32 playerLastNote(u32 p) const;
    Timbre playerTimbre(u32 p) const;
    bool playerBoost(u32 p, Boost b) const;
    bool playerFusion(u32 p, Fusion f) const;
    ContentVoice* content_ = nullptr;  // kRecipeVoices held slots, then the one-shot SawGlides, Burn, the upgrades' voices
    u8 cueCount_[kMaxPlayers][kCueCount] = {};
    float cueValue_[kMaxPlayers][kCueCount] = {}, cuePan_[kMaxPlayers][kCueCount] = {};
    u32 arcStep_ = 0, stormStep_ = 0;
    i64 bedFramesLeft_ = 0;  // recipes.low_pass_bed: frames until the bus opens again
    // M5 audio (GAME_DESIGN 6.6, play test 3: "too loud (multiple enemy pops at the same time)"): everything a kill sets off is
    // one family on a 1/32 step (the kill pops of every size, Supernova's thud, Shatter's and Freeze Frame's glass, Arc's zap,
    // Confetti's chirps, Burn's crackle): together never louder than one large kill, and each further step of a chain in the
    // same beat at 0.7 of the one before, down to 0.3. The step's members are gathered (familyAdd), then played (flushFamily).
    struct FamilyShot {
        bool recipe = false;  // a Recipe (id) at a gain x its level, else an Sfx (id) at an absolute gain
        u8 id = 0;
        u8 runs = 1;          // Confetti: a run of this many Shard chirps, a 1/32 apart
        i32 midi = -1;
        float level = 0.0f;   // its absolute gain (what the family's sum counts)
        float pan = 0.0f;
    };
    static constexpr u32 kFamilyShots = 16;
    FamilyShot family_[kFamilyShots];
    u32 familyCount_ = 0;
    i64 familyBeat_ = -1000000;
    float familyMult_ = 1.0f;
    void familyAdd(bool recipe, u8 id, i32 midi, float level, float pan, u8 runs = 1);
    void flushFamily(i64 tick);
    // M5, the fights' pending sounds (content_sound.cpp). A Recipe command with b = 1 is aggregated like the cues: one shot of
    // a recipe at a note on each 1/32 step, its gain 1 - e^(-count / 2.5) (sound_m5.json), the latest pan.
    struct AggShot {
        Recipe r = Recipe::Tom;
        i32 midi = -1;
        u32 count = 0;
        float gain = 0.0f, pan = 0.0f, p1 = 0.0f, p2 = 0.0f;
        double tick = 0.0;  // the 1/32 step it plays on
    };
    static constexpr u32 kAggShots = 16;
    AggShot agg_[kAggShots];
    u32 aggCount_ = 0;
    void flushAgg(double tick);
    // CutBed: these music buses (and the hats) silent from cutFrom_ to cutTo_, ramped over 8 ms; each bus's gain now.
    u16 cutMask_ = 0;
    bool cutHats_ = false;
    i64 cutFrom_ = 0, cutTo_ = -1;
    float cutGain_[kBusCount] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    bool hatsCut(i64 frame) const { return cutHats_ && frame >= cutFrom_ && frame < cutTo_; }
    void applyCuts(u32 n);
    // HighPassBed: the music bus high-passed from bedHpFrom_ to bedHpTo_ (faded in and out over 50 ms, as Freeze Frame's).
    Biquad bedHp_[2];
    float bedHpMix_ = 0.0f, bedHpHz_ = 0.0f;
    i64 bedHpFrom_ = 0, bedHpTo_ = -1;
    // PadTone: the band's pad cutoff and gain multipliers, ramped toward their targets once a block.
    float padTone_ = 1.0f, padToneTarget_ = 1.0f, padGain_ = 1.0f, padGainTarget_ = 1.0f, padRampPerBlock_ = 1.0f;
    void stepPadTone(u32 n);
    // DelayBoost: feedback added to the lead's delay, and its repeats (0: the patch's).
    float delayBoost_ = 0.0f;
    u32 delayBoostRepeats_ = 0;
    // LeadEcho: each player's main lead notes as played (song tick, MIDI), the newest last, for a fight that plays them again.
    struct LeadMark {
        double tick = -1e18;
        u8 note = 0;
    };
    static constexpr u32 kLeadMarks = 64;
    LeadMark leadMarks_[kMaxPlayers][kLeadMarks];
    u32 leadMarkHead_[kMaxPlayers] = {};
    void markLead(u32 player, double tick, u8 note) {
        leadMarks_[player][leadMarkHead_[player] % kLeadMarks] = {tick, note};
        ++leadMarkHead_[player];
    }
    i32 leadNoteAt(u32 player, double tick) const;  // the note it played on that tick (the latest at or before it)
    const Chord& chordBack(double tick) const;      // the chord a past tick was played over (or a later one's)
    void leadEcho(const Command& c);
    // The content voices' own reverb send above the effects' (choir.md's pad voice: 0.5), summed here a block.
    float sendL_[kBlock] = {}, sendR_[kBlock] = {};
    bool sendUsed_ = false;
    u8 stabNotes_[5] = {};  // Recipe::Stab's notes for the command being applied (stabNotes())
    u32 stabNotesN_ = 0;
    void followChordVoices(i64 barFrame);  // the held voices that follow the chord (VoiceStart mods[6]) retune on the bar line

    // M5 (content_sound.cpp): the patterned sound of the upgrades (conductor.json "mods", "riffs", "boosts"; sound_m5.json
    // "fusions"), on the band's own clock from each player's loadout. Nothing here runs unless someone owns an upgrade.
    void upgradeBar();                                   // the end of planConductorBar: what the bar plays
    void upgradeStep(i64 tick, i64 inBar);               // each scanned tick of a conductor bar: Riffs 4-12, patterned Boosts
    void leadUpgrades(u32 player, i64 tick, i64 inBar, u32 k, u8 note, float vel, float gain, float pan, float scale,
                      float drive, float level, float glideFrom);  // after a fired lead note: the Mods' and Boosts' extra notes
    void rearGuardStep(u32 player, i64 tick, i64 inBar, float gain, float level);
    void kickUpgrades(float velocity, i64 frame, double tick);  // Kick Pulse: a tom with each kick the pattern plays
    void boostChanged(u32 player, Boost b, bool on);     // a Boost started or ended (Freeze Frame's glass chord and filter)
    void flushStabs(i64 frame);                          // the stabs the Mods asked for on this tick, as one voice
    LeadVoice* bendLead(LeadVoice* v, u32 player, float glideFrom, bool opposite);  // Seeker's glide, Wave's vibrato
    void crescendoHit(u32 player);                       // Crescendo: a hit drops the lead's rise and the pad's opening
    i32 chordTone(const Chord& ch, u32 k, i32 octave) const;  // k 0 root, 1 third, 2 fifth, 3 seventh, 4 octave
    float scaleStep(float base, i32 degrees) const;      // `degrees` steps of the Track's scale from a note (negative: down)
    // A recipe's baked one-shot at a note (-1: unpitched) on a bus, at an absolute gain; recipeGain: the recipe's own level.
    void recipeShot(Recipe r, i32 note, float gain, float pan, i64 frame, Bus bus, u16 tag = 0, float rate = 1.0f, double from = 0.0);
    float recipeGain(Recipe r) const;
    void swellInto(double endTick, double startTick, float gain);  // the Track's reverse swell over a span, ending on a tick
    // A held Drone of the upgrades (content slot): on from `frame` at a note (retuned there, over 20 ms, when it sounds
    // already), or released there; mode 1: its cutoff follows the Sweeper's arm.
    void holdDrone(u32 slot, bool on, i32 midi, float cutoff, float gain, u8 mode, i64 frame);
    float playerLevel(u32 player) const;  // a player's lead level in this device's mix (1: its own; conductor.json lead.others)
    void upgradeEvent(UpgradeSound s, i64 frame, double tick, float value, double x = 0.0) {
        emit(BeatKind::Sfx, frame, tick, static_cast<u8>(kUpgradeEvent + static_cast<u8>(s)), 0, 0, value, 0, x);
    }
    i32 echoPending_ = -1;            // Echo: the stacks the delay takes on at echoAt_ (the bar line's frame)
    i64 echoAt_ = -1;
    u8 crescBars_[kMaxPlayers] = {};  // Crescendo: bars without a hit
    u32 stretchBar_ = 0;               // Time Stretch: bars it has run (the half of its two-bar drum pattern)
    bool mineArmed_ = false;           // Minelayer: a mine was laid in the last bar
    bool singArmed_ = false;           // Singularity: a well is pulling (its swell played)
    u32 echo_ = 0;                     // Echo: the stacks the lead's delay is set for
    bool kickPulse_ = false;           // Kick Pulse: someone owns it
    float stabAcc_ = 0.0f;             // Lancer, the Cannon, Accent, Bar Line: stab velocity asked for on this tick (one stab, summed)
    float stabMax_ = 0.0f, stabLength_ = 1.0f;  // ... its ceiling (the Cannon's 1.2) and the longest length asked (Lancer's x 2)
    float burn_[kMaxPlayers] = {};     // Burn: each player's burning enemies (one crackle for all)
    u32 haloTick_ = 0, standNext_ = 0;
    float freezeMix_ = 0.0f, freezeTarget_ = 0.0f;  // Freeze Frame: the music bus's high-pass, 0..1
    Biquad freezeHp_[2];
    void startMode(Mode m, u32 track, bool loop);
    void applyParams();
    void applyDelay();  // the lead's ping-pong delay (M5: Echo makes it a beat long and louder)
    void onTick(i64 tick);
    void planBar(i64 barTick);
    void planStagedBar(i64 barTick);
    void planConductorBar(i64 barTick);
    bool advanceCycle();
    void stepEvents(i64 tick, i64 inBar);
    void stagedStep(i64 tick, i64 inBar);
    void conductorStep(i64 tick, i64 inBar);
    void leadStep(i64 tick, i64 inBar);
    void flushKills(i64 tick);
    void firePending(i64 frameEnd);
    void addSectionGain(double frame, float gain);

    // Voices.
    i64 frameOf(double tick) const { return tempo_.gridFrame(tick); }
    void emit(BeatKind k, i64 frame, double tick, u8 a = 0, u8 b = 0, u8 c = 0, float value = 0.0f, u32 index = 0,
              double x = 0.0);
    SampleVoice* playSample(i32 id, Bus bus, i64 frame, float gain, float pan, float rate = 1.0f, double from = 0.0);
    void driveSample(SampleVoice* v, float drive);  // M5 (the driven kick): the voice through tanh(drive x), its peak kept
    void gateSample(SampleVoice* v, u32 gateN, i64 origin);  // M5 (Phasing's fifth): sounding gateN frames, silent as many
    // M5 (LeadEcho): a fight's lead note bent (gliding in from `bend` semitones off, over its length), low-passed at `cutoff` Hz
    // (> 0), or its envelope reversed (a swell cut off).
    void shapeLead(LeadVoice* v, float bend, float cutoff, bool reverse);
    i32 resolveNote(const Command& c, double tick) const;  // M5: a command's note (encodeNote) at a tick (< 0: now); -1: none
    const Chord& chordAtTick(double tick) const;           // M5: the chord that plays on a tick (this bar's or a later one's)
    // cullAt (M5, >= 0): a note that starts later (Fractal's, Fork's) makes room under its player's cap at this frame (now),
    // so the notes it cuts short do not sound on until it starts.
    LeadVoice* playLead(u8 player, Timbre timbre, float midi, float vel, float gain, float pan, i64 frame, float lengthScale = 1.0f,
                        float drive = 0.0f, i64 cullAt = -1);
    // M5: decayScale (the rubber string: x 2), lengthS (> 0: the voice's length; Heavy Step's short notes), the bus.
    BassVoice* playBass(float midi, float vel, float steps, i64 frame, float decayScale = 1.0f, float lengthS = -1.0f, Bus bus = Bus::Bass);
    void fastBass(BassVoice* v);  // M5 (cost): an upgrade's bass note saturates by fastTanh
    void playStab(const Chord& chord, float vel, float pan, i64 frame, float lengthScale = 1.0f);
    void playStabNotes(const u8* notes, u32 count, float vel, float pan, i64 frame, float lengthScale = 1.0f);  // M5: up to five
    void playPad(const Chord& chord, float level, float bright, i64 frame);
    void playKick(float gain, float velocity, i64 frame, double tick);
    void playSfx(Sfx kind, i32 midi, float gain, float pan, i64 frame, double tick, float rateMul = 1.0f);
    // M5: each pool's active voices in the order they start (start frame, then allocation), the order they are summed
    // into the buses in: the sums do not depend on which slot a voice got, which depends on where the device's
    // callbacks fell (random bursts render bit for bit like fixed ones).
    struct Order {
        u8 idx[128];
        u32 n = 0;
    };
    enum Pool : u32 { PoolLead, PoolBass, PoolStab, PoolPad, PoolSample, PoolCount };
    Order order_[PoolCount];
    u32 voiceSeq_ = 0;
    template <typename V>
    V* allocVoice(V* pool, u32 count, Order& order);
    template <typename V>
    void placeVoice(V* pool, Order& order, V* v, i64 frame);  // after the voice is set up: start frame, sum order
    template <typename V>
    static void compactOrder(Order& order, const V* pool);

    // Rendering.
    void renderBlock(float* out, u32 n, u32 channels);
    template <Timbre T>
    struct LeadRun;  // one lead voice's per-sample state in locals (engine.cpp)
    template <Timbre T>
    void renderLead(LeadVoice& v, float* __restrict L, float* __restrict R, u32 cnt, float sr);
    void updateBpmStat();

    const MusicData* music_ = nullptr;
    const SampleBank* bank_ = nullptr;
    u32 sr_ = 48000;
    Random rng_{1};
    Random sfxRng_{7};

    SpscRing<Command, 512> commands_;
    SpscRing<BeatEvent, 4096> events_;
    EngineStats stats_;
    std::atomic<i64> writeFrame_{0};

    // Song clock (audio thread).
    i64 frame_ = 0;          // write frame of the next sample
    i64 songStartFrame_ = 0;
    TempoMap tempo_;
    i64 nextTick_ = 0;       // next grid candidate (2-tick resolution)
    double nominalTpf_ = 0.0;
    double rateScale_ = 1.0;  // M4: a co-op client's rate correction (SetRate), taken on bar lines
    double holdFrames_ = 0.0; // M4: the next bar line waits this long (HoldBar)

    // Musical state.
    Mode mode_ = Mode::Silent;
    bool loop_ = false;
    u32 track_ = 0, variant_ = 0;
    i32 pendingVariant_ = -1;
    i32 pendingTrack_ = -1;
    i32 nextTrackAtBar_ = -1;  // Conductor Track change: the key changes on this bar of the Break
    bool ramping_ = false;     // Conductor Track change: the tempo ramps across the Break
    double rampFrom_ = 0.0, rampTo_ = 0.0;
    const SoundParams* sp_ = nullptr;
    const Track* tr_ = nullptr;
    BarPlan* plan_ = nullptr;
    // Phrase cycle.
    Section section_ = Section::Groove;
    u32 sectionBar_ = 0, sectionBars_ = 8;
    u32 bar_ = 0;            // bars since the song start
    u32 chordBar_ = 0;       // bar counter for the chord progression
    Form form_ = Form::Short;
    bool formSet_ = false;
    bool hold_ = false;
    u32 surgeSteps_[2] = {1, 2};  // conductor.json drop.surgeSteps: the Surge's two gears (SetSurge: both, tests)
    // M3: the game's director drives the cycle (SetPhrase), the boss's bells (SetBoss), the Track's tempo.
    i32 phraseSection_ = -1;
    u32 phraseBar_ = 0, phraseBars_ = 0;
    bool trackNow_ = false;
    double pendingBpm_ = 0.0, bpmOverride_ = 0.0;
    struct BossBar {
        u8 mask = 0, beats = 0, flags = 0;
        bool sixteenths = false;
    };
    BossBar boss_, bossNext_;
    bool bossPending_ = false;
    double trackBpm() const;
    bool nextSection_ = false;
    i32 forceSection_ = -1;
    bool buildFollowed_ = false;   // the current Drop follows a Build (impact)
    float intensity_ = 0.6f;
    bool bedOn_[12] = {};          // hysteresis state per bed layer
    float bedSnapshotPat_[kPatCount] = {};
    bool bedSnapshotBass_ = false;
    float bedSnapshotPad_ = 0.0f;
    u32 stabCount_ = 0;            // alternating stab pan (musicproto)
    Player* players_ = nullptr;
    u32 playerCount_ = 1;
    u32 localPlayer_ = 0;
    u32 killCount_[3] = {};
    u32 shardRun_ = 0;
    i64 lastShardFrame_ = -1000000;

    // Voices.
    LeadVoice* lead_ = nullptr;
    BassVoice* bass_ = nullptr;
    StabVoice* stab_ = nullptr;
    PadVoice* pad_ = nullptr;
    SampleVoice* smp_ = nullptr;
    Pending* pending_ = nullptr;
    u32 pendingCount_ = 0;

    // Buses and effects.
    float bus_[kBusCount][2][kBlock] = {};
    float wet_[2][kBlock] = {};
    Biquad padHp_[2], hatsLp_[2], masterHp_[2];
    FdnReverb reverb_;
    float* delayLine_ = nullptr;   // mono history of the lead bus
    u32 delayMask_ = 0;
    u32 delayPos_ = 0;
    u32 delayD_ = 0, delayDOld_ = 0, delayFade_ = 0;
    float delayTap_[16] = {};
    u32 delayRepeats_ = 0;
    i64 lastKick_ = -(1ll << 40);
    i64 prevKick_ = -(1ll << 40);
    u32 duckLen_ = 0;
    float duck_[kBlock] = {};  // the sidechain envelope of the block being rendered
    u32 duckFrames_ = 0;
    struct GainPoint {
        double frame;
        float gain;
    } gainPts_[64];
    u32 gainHead_ = 0, gainCount_ = 0;
    double gainPredictedF_ = -1.0;  // M5: the next section's first gain point, planned on the last bar (-1: none)
    float keyRate_ = 1.0f;          // M5: the A-minor effects moved to the Track's root (a playback rate) ...
    bool keyMajor_ = false;
    bool keyNoNinth_ = false;       // ... and the Upgrade without its ninth where the scale has no second (Furnace)         // ... with their major third on the major and lydian Tracks
    float outGain_ = 1.0f, outGainTarget_ = 1.0f;
    float musicGain_ = 1.0f, sfxGain_ = 1.0f;
    // M4 co-op, "music from host only": the bed's gain (every music bus but the lead), ramped toward 0 or 1.
    bool hostOnly_ = false;
    float bedGain_ = 1.0f;
    float leadNoteMidi(u8 degree) const;
    // The pause's low-pass on the music (two one-poles a channel): 0 open .. 1 at muffleHz_, ramped.
    float muffle_ = 0.0f, muffleTarget_ = 0.0f, muffleStep_ = 1.0f, muffleHz_ = 600.0f;
    float muffleZ_[2][2] = {};
    struct Dip {
        i64 frame;
        float depth, tau;
    } dips_[8];
    u32 dipCount_ = 0;
    float peak_ = 0.0f;

    bool rawOutput_ = false;
    // M5 audio (play test 3: "sometimes the nova SFX sounds heavily distorted"): the output stage is a look-ahead limiter. Music
    // and effects summed are delayed by kLimDelay frames; the gain each 32-frame segment (aligned to the song's frames, so the
    // callbacks do not matter) may take is ceiling / its peak; a segment's gain is the least of the segments from kLimHold
    // before it to kLimAhead after it, released over kLimReleaseS, and moves linearly across the segment. A sub-bass sine under
    // it keeps one gain over a whole period (the hold), so it stays a sine; nothing over the ceiling reaches the output.
    static constexpr u32 kLimSeg = 32, kLimAhead = 3, kLimHold = 40, kLimDelay = (kLimAhead + 1) * kLimSeg;
    static constexpr u32 kLimRing = 256, kLimReqs = 64;  // powers of two over the delay and over hold + ahead + 1
    static constexpr float kLimCeiling = 0.8f, kLimReleaseS = 0.12f;
    float limBuf_[2][kLimRing] = {};
    float limReq_[kLimReqs] = {};
    float limPeak_ = 0.0f, limPrev_ = 1.0f, limCur_ = 1.0f, limRelease_ = 0.0f;

public:
    // M5 audio: frames from the engine's write head to its output (the limiter's look-ahead; 0 for the raw output).
    u32 outputDelayFrames() const { return rawOutput_ ? 0u : kLimDelay; }
    float limiterGain() const { return limCur_; }  // (tests) the gain of the last segment it planned

private:
    EngineProfile* profile_ = nullptr;
    StemTapFn stemFn_ = nullptr;
    void* stemUser_ = nullptr;
    StageTapFn stageFn_ = nullptr;
    void* stageUser_ = nullptr;
    void* memory_ = nullptr;
};

}  // namespace rf::audio
