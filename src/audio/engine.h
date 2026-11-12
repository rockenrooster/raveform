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
enum class Riff : u8 { Orbitals, PulseRing, Sentinel, Count };
enum class Form : u8 { Short, Standard };  // 8/4/8/4 or 16/8/16/8 bars (GAME_DESIGN 4.1)

// What the band played, stamped in song time: `frame` is the write frame the sound starts on. The main
// thread applies each one when heard time reaches it (TECH_SPEC 6.6).
enum class BeatKind : u8 {
    Kick, Snare, Clap, Hat, Shaker, Bass, Lead, Stab, Pulse, Pad, Riser, Swell, Impact,
    Bar, Beat, Section, Click, Sfx, Tempo, Variant, Track, Count
};
const char* beatKindName(BeatKind k);

struct BeatEvent {
    i64 frame = 0;       // write frame of the onset
    double tick = -1.0;  // musical position (< 0: not on the grid)
    double x = 0.0;      // Tempo: ticks per frame; notes: frequency in Hz
    float value = 0.0f;  // velocity, level or gain
    u32 index = 0;       // Bar, Section, Beat: bar number; Lead: step in the bar
    BeatKind kind = BeatKind::Beat;
    u8 a = 0;            // note (MIDI) / section / variant / track / effect
    u8 b = 0;            // player / hat kind (0 closed, 1 open) / bars in the section / grid ticks
    u8 c = 0;            // bar in the section / timbre
};

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
};

struct Command {
    CmdType type = CmdType::Reset;
    Timing when = Timing::Now;
    u8 a = 0, b = 0, c = 0;
    u8 mods[5] = {};
    float x = 0.0f, y = 0.0f, level = 1.0f;
    double tick = 0.0;
};

// Live numbers for overlays and benchmarks (written by the audio thread, read anywhere).
struct EngineStats {
    std::atomic<u32> voices{0};       // active now
    std::atomic<u32> voicesPeak{0};
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

// Offline profiling: nanoseconds spent per stage of renderBlock (set with Engine::setProfile).
struct EngineProfile {
    enum Stage : u32 { Lead, Bass, Stab, Pad, Samples, BusFx, Delay, Reverb, SfxReverb, Master, Count };
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

    // Diagnostics for tests: the engine's own view (audio thread or a stopped engine only).
    const TempoMap& tempo() const { return tempo_; }

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
    void startMode(Mode m, u32 track, bool loop);
    void applyParams();
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
    void playSample(i32 id, Bus bus, i64 frame, float gain, float pan, float rate = 1.0f);
    void playLead(u8 player, Timbre timbre, float midi, float vel, float gain, float pan, i64 frame, float lengthScale = 1.0f,
                  float drive = 0.0f);
    void playBass(float midi, float vel, float steps, i64 frame);
    void playStab(const Chord& chord, float vel, float pan, i64 frame);
    void playPad(const Chord& chord, float level, float bright, i64 frame);
    void playKick(float gain, i64 frame, double tick);
    void playSfx(Sfx kind, i32 midi, float gain, float pan, i64 frame, double tick, float rateMul = 1.0f);
    template <typename V>
    V* allocVoice(V* pool, u32 count);

    // Rendering.
    void renderBlock(float* out, u32 n, u32 channels);
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
    struct GainPoint {
        double frame;
        float gain;
    } gainPts_[64];
    u32 gainHead_ = 0, gainCount_ = 0;
    float outGain_ = 1.0f, outGainTarget_ = 1.0f;
    float musicGain_ = 1.0f, sfxGain_ = 1.0f;
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
    EngineProfile* profile_ = nullptr;
    StemTapFn stemFn_ = nullptr;
    void* stemUser_ = nullptr;
    void* memory_ = nullptr;
};

}  // namespace rf::audio
