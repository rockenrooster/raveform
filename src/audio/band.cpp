#include "audio/band.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#if defined(__ANDROID__) || defined(__linux__)
#include <sched.h>
#include <time.h>
#define RF_THREAD_CPU_TIME 1
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "audio/platform_audio.h"
#include "core/alloc_counter.h"
#include "core/log.h"

namespace rf {

using namespace audio;

namespace {

constexpr double kFallbackExtraMs = 10.0;     // device latency beyond the buffer when there is no timestamp
constexpr double kSnapMs = 25.0;              // a heard-clock error larger than this snaps instead of slewing
constexpr i64 kTimestampMaxAgeNs = 500000000; // an older device timestamp is stale (fresh ones are a few ms old)
constexpr double kSlew = 0.1;                 // share of the error corrected per frame
constexpr double kLeadRuleCapMs = 80.0;       // TECH_SPEC 6.3: min(outputLatency, 80 ms)
constexpr i64 kHapticGapNs = 120000000;       // at most one light tick per 120 ms
constexpr double kHapticGhostVelocity = 0.5;  // M5: kicks softer than this (ghost notes) give no tick
constexpr float kPauseLowpassHz = 600.0f, kPauseLowpassRampS = 0.15f;  // M3 stage C: screens.md pause (about 600 Hz, 150 ms)

bool eq(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

void prefPath(char* out, size_t cap, const char* file) {
    char* pref = SDL_GetPrefPath("raveform", "raveform");  // load / save only (allocates)
    std::snprintf(out, cap, "%s%s", pref ? pref : "", file);
    SDL_free(pref);
}

}  // namespace

float BeatView::envelope(BeatKind k, float decaySeconds) const {
    const float s = since(k);
    return decaySeconds > 0.0f ? std::exp(-s / decaySeconds) : 0.0f;
}

Band::~Band() {
    if (bakeThread_.joinable()) bakeThread_.join();  // M5: the risers baked behind the game
    delete engine_;
    delete bank_;
    delete music_;
    delete[] usHist_;
    delete[] cpuUsHist_;
    delete[] loadHist_;
    delete[] pending_;
    delete[] heard_;
}

bool Band::init(const AudioArgs& args, const BandDefaults& defaults, AudioDevice* device) {
    device_ = device;
    logAudioArgs(args);
    const char* scenario = defaults.scenario ? defaults.scenario : "";
    const bool audioTest = eq(scenario, "audio_test"), sync = eq(scenario, "sync");
    const bool bench = std::strncmp(scenario, "bench_", 6) == 0;
    Mode mode = Mode::Silent;
    bool full = false, stress = false;
    if (args.noAudio) {
        RF_LOG("audio: no band (--no-audio)");
        return true;  // the M0 test tone keeps the device, silent unless held
    }
    if (args.band[0]) {
        if (eq(args.band, "off")) return true;
        if (eq(args.band, "play")) mode = Mode::Conductor;
        if (eq(args.band, "staged")) mode = Mode::Staged;
        if (eq(args.band, "full") || eq(args.band, "stress")) {
            mode = Mode::Conductor;
            full = true;
            stress = eq(args.band, "stress");
        }
        if (eq(args.band, "metronome")) mode = Mode::Metronome;
    } else if (audioTest) {
        mode = Mode::Conductor;
    } else if (sync) {
        mode = Mode::Metronome;
    } else if (defaults.game && !defaults.pictures) {
        // M3: the game drives the band (fire, intensity, effects); stage A holds it in Groove.
        mode = Mode::Conductor;
        game_ = true;
    } else if (bench && !defaults.pictures) {
        // Benchmarks measure the frame-rate contract with sound on: the band's fullest arrangement.
        mode = Mode::Conductor;
        full = true;
    } else {
        return true;  // no band: the M0 test tone keeps the device
    }

    music_ = new (std::nothrow) MusicData;
    bank_ = new (std::nothrow) SampleBank;
    engine_ = new (std::nothrow) Engine;
    usHist_ = new (std::nothrow) std::atomic<u32>[kUsBuckets];
    cpuUsHist_ = new (std::nothrow) std::atomic<u32>[kUsBuckets];
    loadHist_ = new (std::nothrow) std::atomic<u32>[kLoadBuckets];
    pending_ = new (std::nothrow) BeatEvent[kPending];
    heard_ = new (std::nothrow) BeatEvent[kHeardMax];
    if (!music_ || !bank_ || !engine_ || !usHist_ || !cpuUsHist_ || !loadHist_ || !pending_ || !heard_) return false;
    for (u32 i = 0; i < kUsBuckets; ++i) usHist_[i].store(0);
    for (u32 i = 0; i < kUsBuckets; ++i) cpuUsHist_[i].store(0);
    for (u32 i = 0; i < kLoadBuckets; ++i) loadHist_[i].store(0);
    if (!music_->load()) return false;
    rate_ = 48000;  // the stream asks for 48 kHz (TECH_SPEC 6.2); a device at another rate gets silence and a warning
    // The baked one-shots come from the app's cache when it holds this music at this rate (M3 stage B:
    // start-up; baking costs about 1.7 s on the Moto); else they are baked and cached.
    {
        char cache[512];
        prefPath(cache, sizeof cache, "baked_audio.bin");
        const u64 key = music_->hash ^ (static_cast<u64>(rate_) * 0x9E3779B97F4A7C15ull);
        if (!bank_->loadCache(cache, key)) {
            // M5 (start-up, PLAN: cold start to playing within 5 s): the risers of Tracks 2-10 are baked on a worker
            // while Track 1 plays (its first Build is half a minute in, the next Track's minutes), then cached.
            if (!bank_->bake(*music_, rate_, true)) return false;
            std::snprintf(cachePath_, sizeof cachePath_, "%s", cache);
            cacheKey_ = key;
            bakeThread_ = std::thread([this] {
                bank_->bakeDeferred(*music_, rate_);
                if (!bank_->complete() || !bank_->saveCache(cachePath_, cacheKey_))
                    RF_WARN("audio: could not cache the baked one-shots (%s)", cachePath_);
            });
        }
    }
    if (!engine_->init(*music_, *bank_, rate_, 1)) {
        RF_ERR("audio: engine init failed");
        return false;
    }
    if (args.audioProfile) {
        engine_->setProfile(&profile_);
        profiling_ = true;
    }
    view_.rate = rate_;
    for (double& f : view_.lastFrame) f = -1e18;
    tempo_.reset(120.0, rate_);

    loadSync();
    if (args.syncOffsetSet) {
        syncOffsetMs_ = args.syncOffsetMs;
        syncTouched_ = syncOffsetMs_ != 0.0f;
    }
    haptics_ = args.haptics >= 0 ? args.haptics == 1 : (audioTest || sync);
    if (!platformAudioInit()) haptics_ = haptics_ && false;
    mode_ = mode;
    active_ = true;
    sendInitialState(args, bench && !game_ ? defaults.benchTrack : 0);
    if (game_ && !args.section[0]) holdSection(Section::Groove);
    if (full) {
        // The load test, every layer playing: the Drop held (the whole bed), every Riff at level 2, four
        // players firing on the 1/16-triplet grid (Double-Time) with one Split harmony each, and the
        // effects fed every frame. --band stress doubles the harmony (Split x2): the extreme case.
        post({.type = CmdType::SetPlayers, .a = 4, .b = 0});
        post({.type = CmdType::SetSection, .a = static_cast<u8>(Section::Drop)});
        post({.type = CmdType::HoldSection, .a = 1});
        for (u8 p = 0; p < 4; ++p) {
            post({.type = CmdType::SetFire, .a = p, .b = 1});
            post({.type = CmdType::SetTimbre, .a = p, .b = p});
            for (u8 k = 0; k < 3; ++k) post({.type = CmdType::SetRiff, .a = p, .b = k, .c = 2});
            Command m{.type = CmdType::SetMods, .a = p};
            m.mods[0] = stress ? 2 : 1;  // Split
            m.mods[1] = 1;               // Double-Time
            post(m);
        }
        fire_ = true;
        intensity_ = 1.0f;
        post({.type = CmdType::SetIntensity, .x = 1.0f});
        fullLoad_ = true;
        stressLoad_ = stress;
    }
    RF_LOG("audio: band %s, track %s, sound %s, sync offset %.0f ms%s, haptics %s", full ? (stress ? "stress (load test)" : "full (load test)") : (game_ ? "game (Groove held)" : (mode == Mode::Conductor ? "play" : (mode == Mode::Staged ? "staged" : "metronome"))),
           trackName(), variantName(), syncOffsetMs_, syncTouched_ ? "" : " (automatic)", haptics_ ? "on" : "off");
    return true;
}

void Band::sendInitialState(const AudioArgs& args, u32 defaultTrack) {
    if (args.track[0]) {
        const i32 t = music_->trackIndex(args.track);
        if (t < 0) {
            RF_WARN("audio: unknown track '%s'; playing track01", args.track);
        } else {
            track_ = static_cast<u32>(t);
        }
    } else if (defaultTrack > 0) {
        if (defaultTrack <= music_->trackCount) {
            track_ = defaultTrack - 1;
        } else {
            RF_WARN("audio: no Track %u in the music data; playing track01", defaultTrack);
        }
    }
    variant_ = music_->defaultVariant;
    if (args.sound[0]) {
        const i32 v = music_->variantIndex(args.sound);
        if (v < 0) {
            RF_WARN("audio: unknown sound '%s'; playing %s", args.sound, music_->variants[variant_].id);
        } else {
            variant_ = static_cast<u32>(v);
        }
    }
    if (args.intensity >= 0.0f) intensity_ = args.intensity;
    post({.type = CmdType::SetVariant, .a = static_cast<u8>(variant_)});
    post({.type = CmdType::SetIntensity, .x = intensity_});
    post({.type = CmdType::SetMode, .a = static_cast<u8>(mode_), .b = static_cast<u8>(track_), .c = static_cast<u8>(mode_ == Mode::Staged ? 1 : 0)});
    if (args.section[0]) {
        static const char* const kNames[] = {"groove", "build", "drop", "break"};
        for (u8 s = 0; s < 4; ++s)
            if (eq(args.section, kNames[s])) post({.type = CmdType::SetSection, .a = s});
    }
    if (args.hold) setHold(true);
    if (args.fire) setFire(true);
}

void Band::post(const Command& c) {
    if (engine_) engine_->post(c);
}

// ---------------------------------------------------------------- audio thread

void Band::render(void* self, float* out, u32 frames, u32 channels, u32 sampleRate) {
    auto* b = static_cast<Band*>(self);
    const u64 allocs0 = heapAllocCountThisThread();
    const i64 t0 = audioNowNs();
#if defined(RF_THREAD_CPU_TIME)
    // The callback's own CPU time (wall time also counts preemption) and the core it ran on.
    timespec c0{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &c0);
    const int cpu = sched_getcpu();
    if (cpu >= 0 && cpu < 16) b->cpuHist_[cpu].fetch_add(1, std::memory_order_relaxed);
#endif
    const u32 gen = b->device_ ? b->device_->generation() : 0;
    if (gen != b->cbGeneration_) {
        b->cbGeneration_ = gen;
        b->cbStreamFrames_ = 0;
    }
    ClockSnapshot cs;
    cs.engineFrame = b->engine_->writeFrame();
    cs.streamFrame = b->cbStreamFrames_;
    cs.wallNs = t0;
    cs.frames = frames;
    cs.generation = gen;
    b->clock_.publish(cs);
    if (sampleRate == b->rate_ && channels > 0) {
        b->engine_->render(out, frames, channels);
    } else {
        std::memset(out, 0, sizeof(float) * frames * channels);
        b->rateMismatch_.store(sampleRate, std::memory_order_relaxed);
    }
    b->cbStreamFrames_ += frames;
    // M3 stage B: a scope of the master bus for the title's waveform and the sound cards (one value per
    // 128 frames: the sample of largest magnitude, signed), and a level meter for the faders.
    if (channels > 0) {
        float level = b->meter_.load(std::memory_order_relaxed) * 0.9f;
        u32 head = b->scopeHead_.load(std::memory_order_relaxed);
        for (u32 f0 = 0; f0 < frames; f0 += 128) {
            float best = 0.0f;
            const u32 f1 = f0 + 128 < frames ? f0 + 128 : frames;
            for (u32 f = f0; f < f1; ++f) {
                const float v = out[f * channels];
                if (std::fabs(v) > std::fabs(best)) best = v;
            }
            b->scope_[head % kScope] = best;
            ++head;
            if (std::fabs(best) > level) level = std::fabs(best);
        }
        b->scopeHead_.store(head, std::memory_order_release);
        b->meter_.store(level, std::memory_order_relaxed);
    }
    const i64 t1 = audioNowNs();
#if defined(RF_THREAD_CPU_TIME)
    timespec c1{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &c1);
    const i64 cpuNs = (static_cast<i64>(c1.tv_sec) - c0.tv_sec) * 1000000000ll + (c1.tv_nsec - c0.tv_nsec);
    u32 cb = static_cast<u32>(cpuNs / 10000);
    if (cb >= kUsBuckets) cb = kUsBuckets - 1;
    b->cpuUsHist_[cb].fetch_add(1, std::memory_order_relaxed);
#endif
    const double us = static_cast<double>(t1 - t0) * 1e-3;
    const double load = frames ? (static_cast<double>(t1 - t0) * 1e-9) / (static_cast<double>(frames) / sampleRate) : 0.0;
    u32 ub = static_cast<u32>(us / 10.0);
    u32 lb = static_cast<u32>(load * 1000.0);
    if (ub >= kUsBuckets) ub = kUsBuckets - 1;
    if (lb >= kLoadBuckets) lb = kLoadBuckets - 1;
    b->usHist_[ub].fetch_add(1, std::memory_order_relaxed);
    b->loadHist_[lb].fetch_add(1, std::memory_order_relaxed);
    b->histCallbacks_.fetch_add(1, std::memory_order_relaxed);
    const u64 allocs = heapAllocCountThisThread() - allocs0;
    if (allocs) b->callbackAllocs_.fetch_add(allocs, std::memory_order_relaxed);
}

// ---------------------------------------------------------------- main thread

void Band::drainEvents() {
    BeatEvent e;
    while (engine_->popEvent(e)) {
        if (e.kind == BeatKind::Tempo) {
            // The tempo map mirror: a segment at tick 0 starts a new song (it takes over once heard).
            TempoSegment s;
            s.tick = e.tick;
            s.ticksPerFrame = e.x;
            s.frame = static_cast<double>(e.frame);
            if (e.a == 1) {
                // M4: a hold on a bar line (a re-seek): two segments at explicit frames.
                (haveNextTempo_ ? nextTempo_ : tempo_).appendSegment(s);
            } else if (e.tick == 0.0) {
                nextTempo_ = TempoMap{};
                nextTempo_.appendSegment(s);
                nextSongStartFrame_ = s.frame;
                haveNextTempo_ = true;
            } else if (haveNextTempo_) {
                nextTempo_.changeAt(s.tick, s.ticksPerFrame);  // exact start frame, as the engine computes it
            } else {
                tempo_.changeAt(s.tick, s.ticksPerFrame);
            }
            continue;
        }
        if (pendingCount_ < kPending) pending_[pendingCount_++] = e;
    }
}

void Band::releaseHeard() {
    view_.eventCount = 0;
    u32 keep = 0;
    for (u32 i = 0; i < pendingCount_; ++i) {
        const BeatEvent& e = pending_[i];
        if (static_cast<double>(e.frame) > view_.heardFrame) {
            pending_[keep++] = e;
            continue;
        }
        if (view_.eventCount < kHeardMax) heard_[view_.eventCount++] = e;
        const u32 k = static_cast<u32>(e.kind);
        view_.lastFrame[k] = static_cast<double>(e.frame);
        view_.lastValue[k] = e.value;
        view_.lastNote[k] = e.a;
        switch (e.kind) {
            case BeatKind::Bar:
                view_.section = static_cast<Section>(e.a);
                view_.sectionBars = e.b;
                view_.sectionBar = e.c;
                break;
            case BeatKind::Variant: view_.variant = e.a; break;
            case BeatKind::Track: view_.track = e.a; break;
            case BeatKind::Kick:
            case BeatKind::Click:
                // GAME_DESIGN 11.5: a light tick on the kicks the Track plays (M5: on Afterglow, Mirage and Last
                // Light they are not on every beat, and that is the groove to feel); a ghost kick (below half
                // velocity, Last Light's) is too soft to tick.
                if (e.kind == BeatKind::Kick && e.x < kHapticGhostVelocity) break;
                if (haptics_ && view_.running) {
                    const i64 now = audioNowNs();
                    if (now - lastHapticNs_ >= kHapticGapNs) {
                        platformHaptic(HapticKind::Light, hapticStrong_);
                        lastHapticNs_ = now;
                        ++hapticCalls_;
                    }
                }
                break;
            case BeatKind::Impact:
                if (haptics_ && view_.running) {
                    platformHaptic(HapticKind::Heavy, hapticStrong_);
                    lastHapticNs_ = audioNowNs();
                    ++hapticCalls_;
                }
                break;
            default: break;
        }
    }
    pendingCount_ = keep;
    view_.events = heard_;
}

u8 Band::fireAt(i64 wallNs) const {
    u8 mode = 0;
    for (u32 i = 0; i < fireCount_; ++i) {
        const FireEdge& e = fireEdges_[(fireHead_ + i) % countOf(fireEdges_)];
        if (e.ns > wallNs) break;
        mode = e.mode;
    }
    return mode;
}

void Band::computeShots(i64 nowNs, i64 displayLeadNs) {
    view_.shotCount = 0;
    view_.shots = shots_;
    const double tick = view_.heardTick;
    if (mode_ != Mode::Conductor || lastShotTick_ < 0.0 || tick < lastShotTick_ || tick - lastShotTick_ > kTicksPerBar) {
        lastShotTick_ = tick;
        return;
    }
    // The local player's grid in the heard bar, as the engine plays it: Double-Time stacks and the Surge's
    // gear (first half of the Drop: one step along, then two) move it along the grid list, capped at the
    // last; Hyper halves it.
    const ConductorRules& r = music_->conductor;
    const bool surge = view_.section == Section::Drop && view_.sectionBar < view_.sectionBars / 2;
    u32 idx = localDoubleTime_ + (surge ? r.surgeSteps[view_.sectionBar < view_.sectionBars / 4 ? 0 : 1] : 0u);
    if (idx > 2) idx = 2;
    i64 grid = static_cast<i64>(r.gridTicks[idx]);
    if (localHyper_ && grid % 2 == 0) grid /= 2;
    const double capNs = (view_.latencyMs < kLeadRuleCapMs ? view_.latencyMs : kLeadRuleCapMs) * 1e6;
    for (i64 s = static_cast<i64>(std::floor(lastShotTick_ / grid)) + 1; static_cast<double>(s * grid) <= tick; ++s) {
        const double stepTick = static_cast<double>(s * grid);
        if (stepTick <= lastShotTick_) continue;
        const double age = (view_.heardFrame - tempo_.frameAt(stepTick)) / rate_;
        const i64 heardAt = nowNs + displayLeadNs - static_cast<i64>(age * 1e9);
        const u8 held = fireAt(heardAt - static_cast<i64>(capNs));
        if (!held) continue;
        if (held == 2 && ((s * grid % kTicksPerBar) / grid) % 2 != 0) continue;  // M3: half-rate grid (as the lead)
        if (view_.shotCount >= countOf(shots_)) break;
        LeadShot& shot = shots_[view_.shotCount++];
        shot.player = 0;
        shot.tick = stepTick;
        shot.stepInBar = static_cast<u32>((s * grid % kTicksPerBar) / grid);
        shot.ageSeconds = static_cast<float>(age);
    }
    lastShotTick_ = tick;
}

void Band::update(i64 displayLeadNs) {
    if (!active_) return;
    const i64 t0 = audioNowNs();
    // The route: after every (re)open, ask what the stream plays on (Bluetooth adds unreported delay).
    const u32 gen = device_ ? device_->generation() : 0;
    if (gen != routeGeneration_ && device_ && device_->isOpen()) {
        routeGeneration_ = gen;
        const AudioStats st = device_->stats();  // once per stream (Oboe's queries may allocate)
        burst_ = st.framesPerBurst;
        deviceRate_ = st.sampleRate;
        const int type = platformAudioDeviceType(st.deviceId);
        bluetooth_ = isBluetoothDeviceType(type);
        RF_LOG("audio: output device %d (type %d)%s", st.deviceId, type, bluetooth_ ? ", Bluetooth" : "");
        if (const u32 bad = rateMismatch_.load()) RF_WARN("audio: the device runs at %u Hz, the band at %u Hz: silent", bad, rate_);
    }
    drainEvents();

    // Heard clock: which engine frame reaches the ear when this frame reaches the panel.
    const i64 tDisplay = t0 + displayLeadNs;
    const double r = static_cast<double>(rate_);
    ClockSnapshot cs;
    AudioTimestamp ts;
    double raw = 0.0;
    bool rawOk = false;
    bool fromTimestamp = false;
    if (clock_.read(cs) && device_ && cs.generation == device_->generation()) {
        if (device_->timestamp(ts)) {
            const double base = static_cast<double>(cs.engineFrame - cs.streamFrame);  // engine frame of stream frame 0
            raw = base + static_cast<double>(ts.position) + static_cast<double>(tDisplay - ts.timeNs) * r * 1e-9;
            // A stale timestamp is not used: AAudio's first one after a start can carry position 0 at a time
            // long past (seen on the Moto: 88 s), which would put the ear far ahead of the write head.
            const double heardNow = raw - static_cast<double>(displayLeadNs) * r * 1e-9;
            fromTimestamp = t0 - ts.timeNs < kTimestampMaxAgeNs && heardNow <= static_cast<double>(cs.engineFrame + cs.frames);
            if (!fromTimestamp && staleLogs_ < kSnapLogs) {
                ++staleLogs_;
                RF_LOG("audio: device timestamp not used (position %lld, %.1f ms old, write head %lld)",
                       static_cast<long long>(ts.position), static_cast<double>(t0 - ts.timeNs) * 1e-6,
                       static_cast<long long>(cs.engineFrame));
            }
        }
        if (fromTimestamp) {
            timestamped_ = true;
        } else {
            raw = static_cast<double>(cs.engineFrame) - static_cast<double>(device_->bufferedFrames()) +
                  static_cast<double>(tDisplay - cs.wallNs) * r * 1e-9 - kFallbackExtraMs * r * 1e-3;
            timestamped_ = false;
        }
        raw -= static_cast<double>(syncOffsetMs_) * r * 1e-3;
        rawOk = true;
    }
    if (rawOk) {
        const double predicted = model_ + static_cast<double>(tDisplay - modelNs_) * r * 1e-9;
        const double err = raw - predicted;
        if (!modelValid_ || gen != modelGeneration_ || std::fabs(err) > kSnapMs * r * 1e-3) {
            if (snapLogs_ < kSnapLogs) {  // rare: the first ones of a run are logged (diagnosis)
                ++snapLogs_;
                RF_LOG("audio: heard clock %s at engine frame %.0f (%+.1f ms from the model; %s; stream %u, "
                       "write head %lld, stream frame %lld, timestamp position %lld, %.1f ms old)",
                       modelValid_ ? "snapped" : "starts", raw, modelValid_ ? err / r * 1e3 : 0.0,
                       fromTimestamp ? "device timestamp" : "buffer estimate", gen, static_cast<long long>(cs.engineFrame),
                       static_cast<long long>(cs.streamFrame), fromTimestamp ? static_cast<long long>(ts.position) : -1ll,
                       fromTimestamp ? static_cast<double>(t0 - ts.timeNs) * 1e-6 : 0.0);
            }
            model_ = raw;
        } else {
            model_ = predicted + kSlew * err;
        }
        modelNs_ = tDisplay;
        modelValid_ = true;
        modelGeneration_ = gen;
    }
    view_.running = modelValid_;
    view_.timestamped = timestamped_;
    view_.heardFrame = model_;
    const double heardNow = model_ - static_cast<double>(displayLeadNs) * r * 1e-9;
    view_.latencyMs = (static_cast<double>(engine_->writeFrame()) - heardNow - static_cast<double>(syncOffsetMs_) * r * 1e-3) / r * 1e3;
    if (haveNextTempo_ && view_.heardFrame >= nextSongStartFrame_) {
        tempo_ = nextTempo_;
        songStartFrame_ = nextSongStartFrame_;
        haveNextTempo_ = false;
        lastShotTick_ = -1.0;
    }
    const double tick = tempo_.count() ? tempo_.tickAt(view_.heardFrame) : 0.0;
    view_.heardTick = tick;
    view_.songSeconds = (view_.heardFrame - songStartFrame_) / r;
    view_.bpm = tempo_.count() ? tempo_.bpmAt(view_.heardFrame, r) : 120.0;
    const double beats = tick / static_cast<double>(kPpq);
    const double fb = std::floor(beats);
    view_.beatPhase = static_cast<float>(beats - fb);
    const i64 beatIndex = static_cast<i64>(fb);
    view_.beatInBar = static_cast<u32>(((beatIndex % 4) + 4) % 4);
    view_.bar = tick >= 0.0 ? static_cast<u32>(tick / static_cast<double>(kTicksPerBar)) : 0u;
    releaseHeard();
    computeShots(t0, displayLeadNs);
    if (fullLoad_) {
        // The load test also keeps the effects busy: kills on most frames, Shard runs, large kills.
        ++fullFrame_;
        post({.type = CmdType::Kill, .a = 0, .b = 3});
        if (fullFrame_ % 4 == 0) {
            post({.type = CmdType::Kill, .a = 1, .b = 1});
            post({.type = CmdType::Shard, .x = 1.0f});
        }
        if (fullFrame_ % 30 == 0) post({.type = CmdType::Kill, .a = 2, .b = 1});
    }
    const float us = static_cast<float>(audioNowNs() - t0) * 1e-3f;
    updateUs_[updateCount_ % countOf(updateUs_)] = us;
    ++updateCount_;
}

// ---------------------------------------------------------------- mix state

void Band::setFireMode(u8 mode) {
    if (mode > 2) mode = 1;
    if (!active_ || mode == fireMode_) return;
    fireMode_ = mode;
    fire_ = mode != 0;
    const u32 cap = countOf(fireEdges_);
    if (fireCount_ == cap) {
        fireHead_ = (fireHead_ + 1) % cap;
        --fireCount_;
    }
    fireEdges_[(fireHead_ + fireCount_) % cap] = {audioNowNs(), mode};
    ++fireCount_;
    post({.type = CmdType::SetFire, .a = 0, .b = mode});
}

// ---------------------------------------------------------------- M3: the game's effect interface

void Band::kill(u8 size, u32 count) {
    while (count > 0) {
        const u32 n = count > 255 ? 255 : count;
        post({.type = CmdType::Kill, .a = size, .b = static_cast<u8>(n)});
        count -= n;
    }
}

void Band::shard() { post({.type = CmdType::Shard, .x = 1.0f}); }

void Band::nova(double blastTick) { post({.type = CmdType::Nova, .when = Timing::AtTick, .x = 1.0f, .tick = blastTick}); }

void Band::warp(double landTick, float pan) {
    post({.type = CmdType::Warp, .when = Timing::AtTick, .x = 1.0f, .y = pan, .tick = landTick});
}

void Band::sfxAt(Sfx s, double tick, float gain, float pan) {
    post({.type = CmdType::Sfx, .when = Timing::AtTick, .a = static_cast<u8>(s), .x = gain, .y = pan, .tick = tick});
}

void Band::holdSection(Section s) {
    post({.type = CmdType::SetSection, .a = static_cast<u8>(s)});
    setHold(true);
}

void Band::setPhrase(Section s, u32 bar, u32 bars) {
    post({.type = CmdType::SetPhrase, .a = static_cast<u8>(s), .b = static_cast<u8>(bar < 255 ? bar : 255),
          .c = static_cast<u8>(bars < 255 ? bars : 255)});
}

void Band::setForm(Form f) { post({.type = CmdType::SetForm, .a = static_cast<u8>(f)}); }

void Band::changeTrack(u32 track, float bpm, bool now) {
    if (!music_ || track >= music_->trackCount) return;
    track_ = track;
    post({.type = CmdType::SetTrack, .a = static_cast<u8>(track), .b = static_cast<u8>(now ? 1 : 0), .x = bpm});
    RF_LOG("audio: Track %s at %.0f BPM %s", music_->tracks[track].id, bpm, now ? "from the next bar line" : "through a Break");
}

void Band::setBoss(u8 pairs, bool sixteenths, u8 beats, u8 flags) {
    post({.type = CmdType::SetBoss, .a = static_cast<u8>((pairs & 7u) | (sixteenths ? 16u : 0u)), .b = beats, .c = flags});
}

void Band::setLoadout(const u8 mods[5], const u8 riffs[3], bool prism, bool hyper) {
    // SetMods: split, doubleTime, pierce, overcharge, onBeatCrit (engine.h).
    Command c{.type = CmdType::SetMods, .a = 0};
    for (u32 k = 0; k < 5; ++k) c.mods[k] = mods[k];
    post(c);
    for (u32 r = 0; r < 3; ++r) {  // the first slice's three Riffs (M5: setLoadout(slot, PlayerLoadout) for all twelve)
        riffs_[r] = riffs[r];
        post({.type = CmdType::SetRiff, .a = 0, .b = static_cast<u8>(r), .c = riffs[r]});
    }
    post({.type = CmdType::SetBoost, .a = 0, .b = 0, .c = static_cast<u8>(prism ? 1 : 0)});
    post({.type = CmdType::SetBoost, .a = 0, .b = 1, .c = static_cast<u8>(hyper ? 1 : 0)});
    localDoubleTime_ = mods[1];
    localHyper_ = hyper;
}

// ---------------------------------------------------------------- play test 1, M4 co-op

void Band::leadNote(u8 player, u8 degree, float gain, float pan, double tick, u8 timbre) {
    Command c{.type = CmdType::LeadNote, .a = player, .b = degree, .c = timbre, .x = gain, .y = pan};
    if (tick >= 0.0) {
        c.when = Timing::AtTick;
        c.tick = tick;
    }
    post(c);
}

void Band::setPlayers(u8 count) { post({.type = CmdType::SetPlayers, .a = count, .b = 0}); }

void Band::setPlayerTimbre(u8 slot, u8 timbre) { post({.type = CmdType::SetTimbre, .a = slot, .b = timbre}); }

void Band::setPlayerFire(u8 slot, u8 mode) {
    if (slot == 0) {
        setFireMode(mode);
        return;
    }
    post({.type = CmdType::SetFire, .a = slot, .b = mode});
}

void Band::setPlayerDistance(u8 slot, float wu) { post({.type = CmdType::SetDistance, .a = slot, .x = wu}); }

void Band::setPlayerLoadout(u8 slot, const u8 mods[5], const u8 riffs[3], bool prism, bool hyper) {
    if (slot == 0) {
        setLoadout(mods, riffs, prism, hyper);
        return;
    }
    Command c{.type = CmdType::SetMods, .a = slot};
    for (u32 k = 0; k < 5; ++k) c.mods[k] = mods[k];
    post(c);
    for (u32 r = 0; r < 3; ++r) post({.type = CmdType::SetRiff, .a = slot, .b = static_cast<u8>(r), .c = riffs[r]});
    post({.type = CmdType::SetBoost, .a = slot, .b = 0, .c = static_cast<u8>(prism ? 1 : 0)});
    post({.type = CmdType::SetBoost, .a = slot, .b = 1, .c = static_cast<u8>(hyper ? 1 : 0)});
}

void Band::setBeacon(u8 slot, bool on, float pan) {
    post({.type = CmdType::SetBeacon, .a = slot, .b = static_cast<u8>(on ? 1 : 0), .y = pan});
}

void Band::setHostOnly(bool on) { post({.type = CmdType::SetHostOnly, .a = static_cast<u8>(on ? 1 : 0)}); }

// ---------------------------------------------------------------- M5: the content's sound (audio/cues.h)

void Band::setLoadout(u8 slot, const PlayerLoadout& l) {
    if (!active_ || slot >= kMaxPlayers) return;
    PlayerLoadout& was = loadouts_[slot];
    const bool all = !loadoutSent_[slot];
    loadoutSent_[slot] = true;
    for (u32 first = 0; first < kModCount; first += 8) {
        const u32 n = kModCount - first < 8 ? kModCount - first : 8;
        if (!all && std::memcmp(was.mods + first, l.mods + first, n) == 0) continue;
        Command c{.type = CmdType::SetModStacks, .a = slot, .b = static_cast<u8>(first), .c = static_cast<u8>(n)};
        for (u32 k = 0; k < n; ++k) c.mods[k] = l.mods[first + k];
        post(c);
    }
    for (u32 r = 0; r < kRiffCount; ++r)
        if (all || was.riffs[r] != l.riffs[r]) post({.type = CmdType::SetRiff, .a = slot, .b = static_cast<u8>(r), .c = l.riffs[r]});
    for (u32 b = 0; b < kBoostCount; ++b) {
        const bool on = (l.boosts >> b) & 1u;
        if (all || on != (((was.boosts >> b) & 1u) != 0))
            post({.type = CmdType::SetBoost, .a = slot, .b = static_cast<u8>(b), .c = static_cast<u8>(on ? 1 : 0)});
    }
    if (all || was.fusions != l.fusions) post({.type = CmdType::SetFusions, .a = slot, .b = l.fusions});
    if (slot == 0) {
        for (u32 r = 0; r < kRiffCount; ++r) riffs_[r] = l.riffs[r];
        localDoubleTime_ = l.mods[static_cast<u32>(Mod::DoubleTime)];
        localHyper_ = (l.boosts >> static_cast<u32>(Boost::Hyper)) & 1u;
    }
    was = l;
}

void Band::cue(Cue c, u32 count, float value, float pan, double tick, u8 slot) {
    if (!count) return;
    Command k{.type = CmdType::Cue, .when = tick >= 0.0 ? Timing::AtTick : Timing::Next32, .a = static_cast<u8>(c), .b = slot,
              .c = static_cast<u8>(count < 255 ? count : 255), .x = value, .y = pan};
    k.tick = tick;
    post(k);
}

void Band::playRecipe(Recipe r, i32 midi, float gain, float pan, double tick, float p1, float p2) {
    Command k{.type = CmdType::Recipe, .when = tick >= 0.0 ? Timing::AtTick : Timing::Next32, .a = static_cast<u8>(r),
              .c = static_cast<u8>(midi >= 0 && midi < 255 ? midi : 255), .x = gain, .y = pan, .level = p2, .z = p1};
    k.tick = tick;
    post(k);
}

void Band::startVoice(u8 voice, Recipe r, i32 midi, float gain, float pan, float param) {
    post({.type = CmdType::VoiceStart, .a = voice, .b = static_cast<u8>(r), .c = static_cast<u8>(midi >= 0 && midi < 255 ? midi : 255),
          .x = gain, .y = pan, .z = param});
}

void Band::setVoice(u8 voice, float param, float gain) { post({.type = CmdType::VoiceSet, .a = voice, .x = param, .y = gain}); }

void Band::stopVoice(u8 voice, float releaseSeconds) { post({.type = CmdType::VoiceStop, .a = voice, .x = releaseSeconds}); }

void Band::lowPassBed(float hz, float seconds) { post({.type = CmdType::LowPassBed, .x = hz, .y = seconds}); }

u32 Band::scope(float* out, u32 n) const {
    const u32 head = scopeHead_.load(std::memory_order_acquire);
    if (n > kScope) n = kScope;
    if (n > head) n = head;
    for (u32 k = 0; k < n; ++k) out[k] = scope_[(head - n + k) % kScope];
    return n;
}

void Band::setGains(float music, float effects) { post({.type = CmdType::SetGains, .x = music, .y = effects}); }

void Band::setMuffle(bool on) {
    if (on == muffled_) return;
    muffled_ = on;
    post({.type = CmdType::SetMuffle, .x = on ? kPauseLowpassHz : 0.0f, .y = kPauseLowpassRampS});
}

void Band::setIntensity(float v) {
    intensity_ = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    post({.type = CmdType::SetIntensity, .x = intensity_});
}

void Band::nextSection() { post({.type = CmdType::NextSection}); }

void Band::setHold(bool hold) {
    hold_ = hold;
    post({.type = CmdType::HoldSection, .a = static_cast<u8>(hold ? 1 : 0)});
}

void Band::setRate(float scale) { post({.type = CmdType::SetRate, .x = scale}); }

void Band::holdNextBar(float seconds) { post({.type = CmdType::HoldBar, .x = seconds}); }

void Band::nextVariant() { setVariant((variant_ + 1) % (music_ ? music_->variantCount : 1)); }

void Band::setVariant(u32 v) {
    if (!music_ || v >= music_->variantCount) return;
    variant_ = v;
    post({.type = CmdType::SetVariant, .a = static_cast<u8>(v)});
    RF_LOG("audio: sound %s from the next bar line", music_->variants[v].id);
}

void Band::nextTrack() {
    if (!music_) return;
    track_ = (track_ + 1) % music_->trackCount;
    post({.type = CmdType::SetTrack, .a = static_cast<u8>(track_)});
    RF_LOG("audio: Track %s next (%s)", music_->tracks[track_].id, mode_ == Mode::Conductor ? "through a Break" : "next bar");
}

void Band::setMode(Mode m) {
    mode_ = m;
    post({.type = CmdType::SetMode, .a = static_cast<u8>(m), .b = static_cast<u8>(track_), .c = static_cast<u8>(m == Mode::Staged ? 1 : 0)});
    if (m == Mode::Conductor) {
        post({.type = CmdType::SetIntensity, .x = intensity_});
        if (fire_) post({.type = CmdType::SetFire, .a = 0, .b = fireMode_});
    }
}

void Band::toggleRiff(Riff r) {
    u8& level = riffs_[static_cast<u32>(r)];
    level = level ? 0 : 1;
    post({.type = CmdType::SetRiff, .a = 0, .b = static_cast<u8>(r), .c = level});
}

void Band::playSfx(Sfx s, float gain, float rate) { post({.type = CmdType::Sfx, .a = static_cast<u8>(s), .x = gain, .level = rate}); }

void Band::hapticPreview() {
    if (!haptics_) return;
    platformHaptic(HapticKind::Medium, hapticStrong_);
    lastHapticNs_ = audioNowNs();
    ++hapticCalls_;
}

const char* Band::variantName() const { return music_ ? music_->variants[variant_].name : "-"; }
const char* Band::trackName() const { return music_ ? music_->tracks[track_].name : "-"; }

// ---------------------------------------------------------------- sync calibration

void Band::setSyncOffsetMs(float ms) {
    syncOffsetMs_ = ms;
    syncTouched_ = true;
}

void Band::clearSyncOffset() {
    syncOffsetMs_ = 0.0f;
    syncTouched_ = false;
}

void Band::loadSync() {
    char path[512];
    prefPath(path, sizeof path, "audio_sync.txt");
    FILE* f = std::fopen(path, "r");
    if (!f) return;
    char line[64];
    while (std::fgets(line, sizeof line, f)) {
        float v = 0.0f;
        if (std::sscanf(line, "offset_ms=%f", &v) == 1) {
            syncOffsetMs_ = v;
            syncTouched_ = true;
        }
    }
    std::fclose(f);
    RF_LOG("audio: sync calibration %+.0f ms (%s)", syncOffsetMs_, path);
}

bool Band::saveSync() {
    char path[512], tmp[520];
    prefPath(path, sizeof path, "audio_sync.txt");
    std::snprintf(tmp, sizeof tmp, "%s.tmp", path);
    if (!syncTouched_) {
        std::remove(path);  // AUTO: nothing to add to the device's own timing
        RF_LOG("audio: sync calibration cleared (automatic)");
        return true;
    }
    FILE* f = std::fopen(tmp, "w");
    if (!f) return false;
    std::fprintf(f, "offset_ms=%.1f\n", syncOffsetMs_);
    const bool ok = std::fclose(f) == 0;
    std::remove(path);
    const bool renamed = ok && std::rename(tmp, path) == 0;
    RF_LOG("audio: sync calibration %+.0f ms saved to %s%s", syncOffsetMs_, path, renamed ? "" : " (failed)");
    return renamed;
}

// ---------------------------------------------------------------- measurements

void Band::resetMeasurements() {
    if (!active_) return;
    for (u32 i = 0; i < kUsBuckets; ++i) usHist_[i].store(0, std::memory_order_relaxed);
    for (u32 i = 0; i < kUsBuckets; ++i) cpuUsHist_[i].store(0, std::memory_order_relaxed);
    for (u32 i = 0; i < kLoadBuckets; ++i) loadHist_[i].store(0, std::memory_order_relaxed);
    for (auto& h : cpuHist_) h.store(0, std::memory_order_relaxed);
    histCallbacks_.store(0, std::memory_order_relaxed);
    callbackAllocs_.store(0, std::memory_order_relaxed);
    updateCount_ = 0;
    profileStart_ = profile_;  // the measured window's profile is the difference (the audio thread keeps adding)
}

Band::Report Band::report() const {
    Report rep;
    if (!active_) return rep;
    const auto pct = [](const std::atomic<u32>* hist, u32 buckets, float unit, float* p50, float* p95, float* p99, float* mx) {
        u64 total = 0;
        for (u32 i = 0; i < buckets; ++i) total += hist[i].load(std::memory_order_relaxed);
        if (!total) return 0u;
        const u64 q50 = (total * 50 + 99) / 100, q95 = (total * 95 + 99) / 100, q99 = (total * 99 + 99) / 100;
        u64 acc = 0;
        bool d50 = false, d95 = false, d99 = false;
        for (u32 i = 0; i < buckets; ++i) {
            const u32 c = hist[i].load(std::memory_order_relaxed);
            if (!c) continue;
            acc += c;
            const float v = (static_cast<float>(i) + 1.0f) * unit;  // upper edge of the bucket
            if (!d50 && acc >= q50) { *p50 = v; d50 = true; }
            if (!d95 && acc >= q95) { *p95 = v; d95 = true; }
            if (!d99 && acc >= q99) { *p99 = v; d99 = true; }
            *mx = v;
        }
        return static_cast<u32>(total);
    };
    rep.callbacks = pct(usHist_, kUsBuckets, 10.0f, &rep.callbackUsP50, &rep.callbackUsP95, &rep.callbackUsP99, &rep.callbackUsMax);
    pct(cpuUsHist_, kUsBuckets, 10.0f, &rep.cpuUsP50, &rep.cpuUsP95, &rep.cpuUsP99, &rep.cpuUsMax);
    for (u32 i = 0; i < 16; ++i) rep.cpus[i] = cpuHist_[i].load(std::memory_order_relaxed);
    pct(loadHist_, kLoadBuckets, 0.001f, &rep.loadP50, &rep.loadP95, &rep.loadP99, &rep.loadMax);
    rep.burst = burst_;
    rep.burstMs = deviceRate_ ? 1000.0f * burst_ / deviceRate_ : 0.0f;
    // Main thread per-frame cost over the recent frames.
    const u32 n = updateCount_ < countOf(updateUs_) ? updateCount_ : countOf(updateUs_);
    if (n) {
        float tmp[512];
        for (u32 i = 0; i < n; ++i) tmp[i] = updateUs_[i];
        for (u32 i = 1; i < n; ++i) {  // insertion sort (512 at most, report time only)
            const float v = tmp[i];
            u32 j = i;
            for (; j > 0 && tmp[j - 1] > v; --j) tmp[j] = tmp[j - 1];
            tmp[j] = v;
        }
        rep.updateUsP50 = tmp[n / 2];
        rep.updateUsP95 = tmp[(n * 95) / 100 < n ? (n * 95) / 100 : n - 1];
        rep.updateUsMax = tmp[n - 1];
    }
    rep.callbackAllocs = callbackAllocs_.load(std::memory_order_relaxed);
    const EngineStats& es = engine_->stats();
    rep.voicesPeak = es.voicesPeak.load(std::memory_order_relaxed);
    rep.steals = es.steals.load(std::memory_order_relaxed);
    rep.eventsDropped = engine_->eventsDropped();
    rep.commandsDropped = engine_->commandsDropped();
    rep.rateMismatch = rateMismatch_.load(std::memory_order_relaxed);
    rep.latencyMs = view_.latencyMs;
    rep.timestamped = timestamped_;
    rep.hapticCalls = hapticCalls_;
    return rep;
}

void Band::writeJson(JsonWriter& j) const {
    if (!active_) return;
    const Report r = report();
    j.beginObject("audio_engine");
    j.str("mode", mode_ == Mode::Conductor ? (fullLoad_ ? (stressLoad_ ? "stress" : "full") : "play") : (mode_ == Mode::Staged ? "staged" : "metronome"));
    j.str("track", music_->tracks[track_].id);
    j.str("sound", music_->variants[variant_].id);
    j.integer("callbacks", r.callbacks);
    j.integer("burst", r.burst);
    j.num("burst_ms", r.burstMs, 3);
    // The engine's own time per callback (microseconds, 10 us buckets) and as a share of the time the
    // callback's frames last (the contract: within 25% of the burst period).
    j.beginArray("callback_us");
    j.num(nullptr, r.callbackUsP50, 0);
    j.num(nullptr, r.callbackUsP95, 0);
    j.num(nullptr, r.callbackUsP99, 0);
    j.num(nullptr, r.callbackUsMax, 0);
    j.endArray();
    j.beginArray("callback_cpu_us");  // the callback thread's own CPU time (Android), same buckets
    j.num(nullptr, r.cpuUsP50, 0);
    j.num(nullptr, r.cpuUsP95, 0);
    j.num(nullptr, r.cpuUsP99, 0);
    j.num(nullptr, r.cpuUsMax, 0);
    j.endArray();
    j.beginArray("callback_cores");  // callbacks per CPU number
    for (u32 i = 0; i < 8; ++i) j.integer(nullptr, r.cpus[i]);
    j.endArray();
    j.beginArray("callback_load");
    j.num(nullptr, r.loadP50, 3);
    j.num(nullptr, r.loadP95, 3);
    j.num(nullptr, r.loadP99, 3);
    j.num(nullptr, r.loadMax, 3);
    j.endArray();
    j.beginArray("main_thread_us");  // Band::update per frame: [p50, p95, max] over the last 512 frames
    j.num(nullptr, r.updateUsP50, 1);
    j.num(nullptr, r.updateUsP95, 1);
    j.num(nullptr, r.updateUsMax, 1);
    j.endArray();
    j.integer("callback_heap_allocs", static_cast<i64>(r.callbackAllocs));
    j.integer("voices_peak", r.voicesPeak);
    j.integer("voice_steals", r.steals);
    j.integer("events_dropped", r.eventsDropped);
    j.integer("commands_dropped", r.commandsDropped);
    j.num("latency_ms", r.latencyMs, 1);
    j.boolean("timestamped", r.timestamped);
    j.num("sync_offset_ms", syncOffsetMs_, 1);
    j.integer("haptic_calls", r.hapticCalls);
    if (profiling_ && r.callbacks) {
        // Microseconds of engine time per callback, per stage, over the measured window (a diagnostic: the timers cost
        // a few percent themselves). A plain read of counters the audio thread adds to: approximate by design.
        j.beginObject("profile_us_per_callback");
        for (u32 s = 0; s < EngineProfile::Count; ++s)
            j.num(EngineProfile::name(s), static_cast<double>(profile_.ns[s] - profileStart_.ns[s]) * 1e-3 / r.callbacks, 1);
        j.endObject();
    }
    if (r.rateMismatch) j.integer("device_rate_mismatch", r.rateMismatch);
    j.endObject();
}

void Band::overlayLine(char* out, size_t capacity) const {
    if (!active_) {
        if (capacity) out[0] = '\0';
        return;
    }
    const Report r = report();
    std::snprintf(out, capacity, "BAND %s  P95 %.0f US (%.1f%%)  MAIN %.0f US  LAT %.0f MS%s  VOICES %u", music_->variants[variant_].id,
                  r.callbackUsP95, r.loadP95 * 100.0f, r.updateUsP95, r.latencyMs, r.timestamped ? "" : "*", engine_->stats().voices.load());
    for (char* p = out; *p; ++p) *p = (*p >= 'a' && *p <= 'z') ? static_cast<char>(*p - 32) : *p;
}

}  // namespace rf