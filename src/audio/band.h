#pragma once
// The band on the main thread (TECH_SPEC 6): owns the engine, the baked sounds and the music data, feeds
// the device's callback, and turns what the audio thread played (stamped in song time) into what the
// player hears now. Gameplay and visuals read BeatView, which is on heard time: the device's
// presentation timestamps (or an estimate from the buffer when there are none), plus the per-device
// sync calibration, at the moment the frame being drawn reaches the panel.
//
// Main thread only, except Band::render (the device callback). The frame loop allocates nothing.

#include <atomic>

#include "audio/audio_args.h"
#include "audio/audio_device.h"
#include "audio/bake.h"
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
    // e^(-since / decay): 1 on the event, falling.
    float envelope(audio::BeatKind k, float decaySeconds) const;
};

// What the run asks of the band where no switch (--band, --track, --no-audio) says otherwise.
struct BandDefaults {
    const char* scenario = "";  // audio_test: the phrase cycle; sync: the metronome; bench_*: the load test
    bool pictures = false;      // --screenshot / --fixed-step: no band unless --band asks for one
    u32 benchTrack = 0;         // bench_*: this Track (1-based; the palette's), 0 = the first
};

class Band {
public:
    Band() = default;
    Band(const Band&) = delete;
    Band& operator=(const Band&) = delete;
    ~Band();

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
    void setFire(bool held);
    bool fire() const { return fire_; }
    void setIntensity(float v);
    float intensity() const { return intensity_; }
    void nextSection();
    void setHold(bool hold);
    bool hold() const { return hold_; }
    void nextVariant();
    void setVariant(u32 v);
    u32 variant() const { return variant_; }
    void nextTrack();
    u32 track() const { return track_; }
    void setMode(audio::Mode m);
    audio::Mode mode() const { return mode_; }
    void toggleRiff(audio::Riff r);
    bool riff(audio::Riff r) const { return riffs_[static_cast<u32>(r)] != 0; }
    void playSfx(audio::Sfx s, float gain = 1.0f);
    const audio::MusicData& music() const { return *music_; }
    const char* variantName() const;
    const char* trackName() const;

    // Sync calibration (design/screens.md): milliseconds added to the device's own latency.
    float syncOffsetMs() const { return syncOffsetMs_; }
    bool syncOffsetTouched() const { return syncTouched_; }
    void setSyncOffsetMs(float ms);
    void clearSyncOffset();  // AUTO: back to the device's timestamps alone
    bool saveSync();         // DONE: written to the app's preferences
    bool bluetooth() const { return bluetooth_; }

    // Haptics: a light tick on heard kicks (and clicks), heavy on the Drop. Rate-limited.
    void setHaptics(bool on) { haptics_ = on; }
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
    bool fireAt(i64 wallNs) const;
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
    float intensity_ = 0.6f;
    bool hold_ = false;
    u32 variant_ = 0, track_ = 0;
    u8 riffs_[static_cast<u32>(audio::Riff::Count)] = {};
    struct FireEdge {
        i64 ns;
        bool held;
    } fireEdges_[64];
    u32 fireHead_ = 0, fireCount_ = 0;

    // The load test (--band full) also feeds the effects every frame.
    bool fullLoad_ = false;
    bool stressLoad_ = false;
    u32 fullFrame_ = 0;

    // Haptics.
    bool haptics_ = false;
    i64 lastHapticNs_ = 0;
    u32 hapticCalls_ = 0;

    // Main-thread cost of update(), per frame.
    float updateUs_[512] = {};
    u32 updateCount_ = 0;
};

}  // namespace rf
