#pragma once
// The band on the main thread (TECH_SPEC 6): owns the engine, the baked sounds and the music data, feeds
// the device's callback, and turns what the audio thread played (stamped in song time) into what the
// player hears now. Gameplay and visuals read BeatView, which is on heard time: the device's
// presentation timestamps (or an estimate from the buffer when there are none), plus the per-device
// sync calibration, at the moment the frame being drawn reaches the panel.
//
// Main thread only, except Band::render (the device callback). The frame loop allocates nothing.

#include <atomic>
#include <thread>

#include "audio/audio_args.h"
#include "audio/audio_device.h"
#include "audio/bake.h"
#include "audio/command_sink.h"
#include "audio/engine.h"
#include "audio/music_data.h"
#include "audio/spsc.h"
#include "audio/tempo_map.h"
#include "core/json.h"

namespace rf {

// A grid step of the lead that the local player fired on, applied when it is heard (TECH_SPEC 6.3):
// spawn the bullets of step `tick` if fire was held at heardTime(step) - min(outputLatency, 80 ms).
struct LeadShot {
    u8 player = 0;
    u32 stepInBar = 0;
    double tick = 0.0;
    float ageSeconds = 0.0f;  // how long ago (heard time) the step was: advance the bullets by this
};

// What the game consumes: heard musical time, the events heard this frame, and envelopes.
struct BeatView {
    static constexpr u32 kKinds = static_cast<u32>(audio::BeatKind::Count);
    bool running = false;            // the band plays and the clock is known
    double heardFrame = 0.0;         // engine frame reaching the ear at the frame's display time
    double heardTick = 0.0;          // musical position then (96 ticks per quarter)
    i64 heardNs = 0;                 // that display time (audioNowNs's clock): co-op's music sync places the song by it
    double songSeconds = 0.0;        // heard seconds since the song began
    double bpm = 120.0;
    double latencyMs = 0.0;          // write head to ear
    bool timestamped = false;        // from the device's timestamps (else an estimate)
    u32 bar = 0;                     // song bar heard now
    u32 beatInBar = 0;               // 0..3
    float beatPhase = 0.0f;          // 0..1 inside the beat
    audio::Section section = audio::Section::Groove;
    u32 sectionBar = 0, sectionBars = 0;
    u8 track = 0, variant = 0;
    // Events heard this frame (oldest first).
    const audio::BeatEvent* events = nullptr;
    u32 eventCount = 0;
    // Heard lead steps with fire held (local player), this frame.
    const LeadShot* shots = nullptr;
    u32 shotCount = 0;
    // Per kind: when the last event was heard (engine frame; very negative when never) and its value.
    double lastFrame[kKinds] = {};
    float lastValue[kKinds] = {};
    u8 lastNote[kKinds] = {};
    double rate = 48000.0;

    // Seconds since the last heard event of a kind (large when never).
    float since(audio::BeatKind k) const {
        const double s = (heardFrame - lastFrame[static_cast<u32>(k)]) / rate;
        return s < 0.0 ? 0.0f : static_cast<float>(s);
    }
    // e^(-since / decay): 1 on the event, falling. M5: envelope(Kick) follows the kicks the Track's pattern plays,
    // which on Afterglow, Mirage and Last Light are not on every beat (the pump of look.md §7 is the kick's, as the
    // sidechain is); anything meant "on the beat" takes beat() (or beatPhase), which is the song clock's.
    float envelope(audio::BeatKind k, float decaySeconds) const;
    // M5: 1 on every heard beat of the song clock, e^(-t / decay) after it, whether or not a kick plays there.
    float beat(float decaySeconds) const { return envelope(audio::BeatKind::Beat, decaySeconds); }
};

// What the run asks of the band where no switch (--band, --track, --no-audio) says otherwise.
struct BandDefaults {
    const char* scenario = "";  // audio_test: the phrase cycle; sync: the metronome; bench_*: the load test
    bool pictures = false;      // --screenshot / --fixed-step: no band unless --band asks for one
    u32 benchTrack = 0;         // bench_*: this Track (1-based; the palette's), 0 = the first
    bool game = false;          // M3: the game (play, bench_play, bench_stress): conductor mode, held in Groove,
                                // the game sets fire, intensity and the effects
};

class Band final : public audio::CommandSink {
public:
    Band() = default;
    Band(const Band&) = delete;
    Band& operator=(const Band&) = delete;
    ~Band();
    // M5 (audio/command_sink.h): the fights' sounds (game/pending_sounds.cpp) post their commands through here.
    void send(const audio::Command& c) override { post(c); }

    // Load time: music data, one-shots, engine. The scenario picks the default mode ("audio_test" plays
    // the phrase cycle, "sync" the metronome, benchmarks the fullest arrangement so the frame-rate
    // contract is measured with sound on); --band overrides, --no-audio turns the band off. Returns false
    // only on a broken data set.
    bool init(const AudioArgs& args, const BandDefaults& defaults, AudioDevice* device);
    bool active() const { return active_; }
    // The device callback (AudioRenderFn); `self` is the Band.
    static void render(void* self, float* out, u32 frames, u32 channels, u32 sampleRate);

    // Main thread, once per frame. `displayLeadNs`: from now until this frame reaches the panel.
    void update(i64 displayLeadNs);
    const BeatView& view() const { return view_; }

    // Mix state (main thread; each change becomes a command to the audio thread).
    void setFire(bool held) { setFireMode(held ? 1 : 0); }
    // M3: 0 off, 1 every grid step, 2 every other step (the auto-aim assist's half-rate grid, GAME_DESIGN
    // 2). The lead plays a note, and computeShots() reports a shot, only on the steps the mode fires.
    void setFireMode(u8 mode);
    bool fire() const { return fire_; }
    u8 fireMode() const { return fireMode_; }
    void setIntensity(float v);
    float intensity() const { return intensity_; }
    void nextSection();
    void setHold(bool hold);
    // M4 (TECH_SPEC 8.2): a co-op client's song follows the host's: its tempo times `scale` from the next bar line
    // (the gentle rate correction), or a hold of `seconds` on the next bar line (the re-seek).
    void setRate(float scale);
    void holdNextBar(float seconds);
    bool hold() const { return hold_; }
    void nextVariant();
    void setVariant(u32 v);
    u32 variant() const { return variant_; }
    void nextTrack();
    u32 track() const override { return track_; }
    void setMode(audio::Mode m);
    audio::Mode mode() const { return mode_; }
    void toggleRiff(audio::Riff r);
    bool riff(audio::Riff r) const { return riffs_[static_cast<u32>(r)] != 0; }
    void playSfx(audio::Sfx s, float gain = 1.0f, float rate = 1.0f);  // rate: 0.5 an octave down
    // M3: the game's effect interface (TECH_SPEC 6.3). Kills are aggregated by the engine per 1/32 step;
    // each Shard plays the next note of its run; the Nova inhales now and blasts on `blastTick` (the
    // game's beat, song ticks); a warp-in swells into `landTick`; an effect can be placed on a tick.
    void kill(u8 size, u32 count);
    void shard();
    void nova(double blastTick);
    void warp(double landTick, float pan);
    void sfxAt(audio::Sfx s, double tick, float gain = 1.0f, float pan = 0.0f);
    // M3: start a section on the next bar line and hold it (stage A keeps the band in Groove).
    void holdSection(audio::Section s);
    // M3 stage B: the game's director drives the band. Each applies from the next bar line (post it in the
    // bar before): where that bar is in the cycle, the form, a Track change (through its Break, or at once
    // for a new run; `bpm` 0 = the Track's own), the boss's bar (bells, gong, beam), the local player's
    // upgrades (lead and Riff layers; the lead rule's grid), and the music / effects gains.
    void setPhrase(audio::Section s, u32 bar, u32 bars);
    void setForm(audio::Form f);
    void changeTrack(u32 track, float bpm, bool now);
    void setBoss(u8 pairs, bool sixteenths, u8 beats, u8 flags);
    void setLoadout(const u8 mods[5], const u8 riffs[3], bool prism, bool hyper);
    void setGains(float music, float effects);
    // Play test 1 / M4: a note of a player's lead on the Track's root (degree 0 root, 1 fifth, 2 octave, 3 an octave
    // down, 4 two octaves up), now or on `tick` (song ticks, >= 0); timbre 0xFF: the player's own.
    void leadNote(u8 player, u8 degree, float gain, float pan, double tick = -1.0, u8 timbre = 0xFF);
    // M4 co-op (GAME_DESIGN 10): the players in this device's mix. Slot 0 is this device's player (setFireMode and
    // setLoadout speak for it); the others are its teammates, quieter by distance, each in its fixed timbre.
    // M8 (design/modes/sound.md "What the band needs" 1): a note of a player's lead at any MIDI note, now or on `tick`.
    void leadNoteAt(u8 player, u8 midi, float gain, float pan, double tick = -1.0, u8 timbre = 0xFF);
    // M8 (4): the lead slot that is nobody's (SONGLINE's Spark): 0 off, 1 every sixteenth, 3 the beat's eighths, 4 quarters.
    void setSpark(u8 mode, u8 timbre = 2);
    // M8 (5): the bed's layers by JAM's mixer (a bit a layer: KICK, HATS, BASS, CLAP, OPEN HAT, SHAKER, CHORDS, STABS), from the next
    // bar line; -1: by the intensity again.
    void setBedMask(i32 bits);
    // M8 (3): the progression held on one chord from the next bar line (0: its first, the home chord; ONE TRACK's outro,
    // HEADLINERS COMPLETE, the result's bed); -1 releases it.
    void holdChord(i32 index);
    void setPlayers(u8 count);
    void setPlayerTimbre(u8 slot, u8 timbre);
    void setPlayerFire(u8 slot, u8 mode);
    void setPlayerDistance(u8 slot, float wu);
    void setPlayerLoadout(u8 slot, const u8 mods[5], const u8 riffs[3], bool prism, bool hyper);
    void setBeacon(u8 slot, bool on, float pan);  // look.md 6: a downed player's beacon
    // M5 (audio/cues.h, design/music/sound_m5.json). The whole loadout of a player (slot 0: this device's): every Mod's
    // stacks, Riff levels, the Boosts active and the Fusions owned; post it whenever it changes (only what changed is
    // sent on). It supersedes setLoadout(mods[5], riffs[3], ...) and setPlayerLoadout, which stay for the first slice.
    void setLoadout(u8 slot, const audio::PlayerLoadout& l);
    // A game event with a sound, as it happens (Cue lists what each means). Placed on the next 1/32 step, or on `tick`
    // (song ticks, >= 0); value and pan as the Cue says.
    void cue(audio::Cue c, u32 count = 1, float value = 0.0f, float pan = 0.0f, double tick = -1.0, u8 slot = 0);
    // A recipe as a one-shot: now (tick < 0: on the next 1/32 step) or on `tick`; midi -1 = its default (or a chordNote(),
    // resolved against the chord at that tick); p1, p2 per Recipe.
    void playRecipe(audio::Recipe r, i32 midi = -1, float gain = 1.0f, float pan = 0.0f, double tick = -1.0, float p1 = 0.0f,
                    float p2 = 0.0f);
    // A held recipe (Drone, PadVoice, Crackle; any recipe sustains while held) in voice slot 0..kRecipeVoices-1, which
    // the game owns: start, change its parameter (Drone: cutoff Hz; Crackle: bursts a second; PadVoice at the start: its
    // attack in seconds, 0 the pad's own) or gain, stop. M5: setVoice with a `midi` (or chordNote) glides the voice's pitch
    // there over `glideSeconds` (a Voice's death falls a fourth; the Choir's held note rises an octave).
    void startVoice(u8 voice, audio::Recipe r, i32 midi = -1, float gain = 1.0f, float pan = 0.0f, float param = 0.0f);
    void setVoice(u8 voice, float param, float gain = -1.0f, i32 midi = -1, float glideSeconds = 0.0f);
    void stopVoice(u8 voice, float releaseSeconds = 0.1f);
    // recipes.low_pass_bed: the music bus low-passed at `hz` for `seconds` ("stunned", "dazed", "open"), then open again.
    void lowPassBed(float hz, float seconds);
    void setHostOnly(bool on);                    // the SOUND setting "music from host only" (a co-op client)
    // M3 stage C (screens.md pause): the music behind a low-pass of about 600 Hz, ramped over 150 ms (rampS > 0: over that many
    // seconds; the review's fixes, 2026-11-15: the count-in lifts it over its first beat).
    void setMuffle(bool on, float rampS = -1.0f);
    // The master bus, for pictures of the music (title waveform, sound cards): the latest n values, one
    // per 128 frames (signed peak), oldest first; and a decaying peak level (0..1).
    u32 scope(float* out, u32 n) const;
    float meter() const { return meter_.load(std::memory_order_relaxed); }
    bool fullLoad() const { return fullLoad_; }  // --band full / stress: the load test owns the mix
    bool gameMode() const { return game_; }
    const audio::MusicData& music() const override { return *music_; }
    const char* variantName() const;
    const char* trackName() const;

    // Sync calibration (design/screens.md): milliseconds added to the device's own latency.
    float syncOffsetMs() const { return syncOffsetMs_; }
    bool syncOffsetTouched() const { return syncTouched_; }
    void setSyncOffsetMs(float ms);
    void clearSyncOffset();  // AUTO: back to the device's timestamps alone
    bool saveSync();         // DONE: written to the app's preferences
    bool bluetooth() const { return bluetooth_; }

    // Haptics: a light tick on heard kicks (the Track's own, ghost kicks below half velocity skipped) and on the sync
    // screen's clicks, heavy on the Drop. Rate-limited.
    void setHaptics(bool on) { haptics_ = on; }
    void setHapticStrong(bool strong) { hapticStrong_ = strong; }  // the STRONG setting (M3 stage C)
    void hapticPreview();  // one medium tick at the current strength (the settings screen)
    bool haptics() const { return haptics_; }

    // Measurements (benchmarks and the overlay).
    struct Report {
        u32 callbacks = 0;
        float callbackUsP50 = 0, callbackUsP95 = 0, callbackUsP99 = 0, callbackUsMax = 0;
        float loadP50 = 0, loadP95 = 0, loadP99 = 0, loadMax = 0;  // engine time / callback duration
        float cpuUsP50 = 0, cpuUsP95 = 0, cpuUsP99 = 0, cpuUsMax = 0;  // the callback thread's CPU time
        u32 cpus[16] = {};
        u32 burst = 0;
        float burstMs = 0;
        float updateUsP50 = 0, updateUsP95 = 0, updateUsMax = 0;  // main thread, per frame
        u64 callbackAllocs = 0;
        u32 voicesPeak = 0, steals = 0, eventsDropped = 0, commandsDropped = 0, rateMismatch = 0;
        double latencyMs = 0;
        bool timestamped = false;
        u32 hapticCalls = 0;
    };
    Report report() const;
    void resetMeasurements();  // starts a measured window (benchmarks)
    // The "audio_engine" object of a benchmark result (nothing when the band is off).
    void writeJson(JsonWriter& j) const;
    // One overlay line.
    void overlayLine(char* out, size_t capacity) const;

private:
    struct ClockSnapshot {
        i64 engineFrame;   // engine frame at the start of the callback
        i64 streamFrame;   // frames this stream generation delivered before it
        i64 wallNs;        // audioNowNs() at the start of the callback
        u32 frames;
        u32 generation;
    };
    void post(const audio::Command& c);
    void drainEvents();
    void releaseHeard();
    void computeShots(i64 nowNs, i64 displayLeadNs);
    u8 fireAt(i64 wallNs) const;  // the fire mode held at that wall time
    void loadSync();
    void haptic(int kind);
    void sendInitialState(const AudioArgs& args, u32 defaultTrack);

    bool active_ = false;
    AudioDevice* device_ = nullptr;
    audio::MusicData* music_ = nullptr;
    audio::SampleBank* bank_ = nullptr;
    audio::Engine* engine_ = nullptr;
    u32 rate_ = 48000;

    // Audio thread.
    u32 cbGeneration_ = ~0u;
    i64 cbStreamFrames_ = 0;
    std::atomic<u32> rateMismatch_{0};
    std::atomic<u64> callbackAllocs_{0};
    static constexpr u32 kUsBuckets = 2048;     // 10 us each
    static constexpr u32 kLoadBuckets = 2048;   // 0.1% each
    std::atomic<u32>* usHist_ = nullptr;
    std::atomic<u32>* cpuUsHist_ = nullptr;
    std::atomic<u32> cpuHist_[16];  // callbacks per CPU (value-initialised, C++20)
    std::atomic<u32>* loadHist_ = nullptr;
    std::atomic<u32> histCallbacks_{0};
    audio::SeqLatest<ClockSnapshot> clock_;

    // Main thread: heard clock.
    bool modelValid_ = false;
    static constexpr u32 kSnapLogs = 12;  // the first snaps (and stale timestamps) of a run are logged
    u32 snapLogs_ = 0, staleLogs_ = 0;
    double model_ = 0.0;     // heard engine frame at modelNs_
    i64 modelNs_ = 0;
    u32 modelGeneration_ = 0;
    bool timestamped_ = false;
    double latencyMs_ = 0.0;
    float syncOffsetMs_ = 0.0f;
    bool syncTouched_ = false;
    bool bluetooth_ = false;
    u32 routeGeneration_ = 0;
    u32 burst_ = 0, deviceRate_ = 0;  // cached at each stream open
    audio::TempoMap tempo_;         // mirrored, the current song
    audio::TempoMap nextTempo_;     // a song that starts later (until it is heard)
    bool haveNextTempo_ = false;
    double songStartFrame_ = 0.0, nextSongStartFrame_ = 0.0;

    // Main thread: events.
    static constexpr u32 kPending = 2048;
    audio::BeatEvent* pending_ = nullptr;
    u32 pendingCount_ = 0;
    static constexpr u32 kHeardMax = 256;
    audio::BeatEvent* heard_ = nullptr;
    LeadShot shots_[64];
    BeatView view_;
    double lastShotTick_ = -1.0;

    // Mix state.
    audio::Mode mode_ = audio::Mode::Silent;
    bool fire_ = false;
    u8 fireMode_ = 0;
    u8 sparkMode_ = 0;  // M8: what setSpark last sent
    i32 bedMask_ = -1;  // M8: what setBedMask last sent
    i32 holdChord_ = -1;  // M8: what holdChord last sent
    bool game_ = false;
    float intensity_ = 0.6f;
    bool hold_ = false;
    u32 variant_ = 0, track_ = 0;
    u8 riffs_[static_cast<u32>(audio::Riff::Count)] = {};
    static constexpr u32 kScope = 1024;
    float scope_[kScope] = {};
    std::atomic<u32> scopeHead_{0};
    std::atomic<float> meter_{0.0f};
    u8 localDoubleTime_ = 0;  // the local player's Double-Time stacks and Hyper (the lead rule's grid)
    u8 localAfterparty_ = 0;  // M5: and Afterparty's stacks
    bool localHyper_ = false;
    audio::PlayerLoadout loadouts_[audio::kMaxPlayers];  // M5: what was last sent per player (only changes are posted)
    bool loadoutSent_[audio::kMaxPlayers] = {};
    struct FireEdge {
        i64 ns;
        u8 mode;  // 0 off, 1 every step, 2 every other step
    } fireEdges_[64];
    u32 fireHead_ = 0, fireCount_ = 0;

    // The load test (--band full) also feeds the effects every frame.
    bool fullLoad_ = false;
    bool stressLoad_ = false;
    bool maxLoad_ = false;  // M5: --band max, the upgrades' worst case (engine.h maxLoadSetup)
    u32 fullFrame_ = 0;

    // Haptics.
    bool haptics_ = false;
    bool hapticStrong_ = false;
    bool muffled_ = false;
    i64 lastHapticNs_ = 0;
    u32 hapticCalls_ = 0;

    // Main-thread cost of update(), per frame.
    float updateUs_[512] = {};
    u32 updateCount_ = 0;
    // M5 (--audio-profile): the engine's time per stage, written by the audio thread, read once at the end of a run.
    audio::EngineProfile profile_, profileStart_;
    // M5 (start-up): the later Tracks' risers baked behind the game on a first launch, then the cache written.
    std::thread bakeThread_;
    char cachePath_[512] = {};
    u64 cacheKey_ = 0;
    bool profiling_ = false;
};

}  // namespace rf
