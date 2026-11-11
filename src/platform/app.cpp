#include "platform/app.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/alloc_counter.h"
#include "core/json.h"
#include "core/log.h"
#include "core/time.h"

#if defined(__ANDROID__)
#include "platform/android/android_glue.h"
#endif

namespace rf {

namespace {
constexpr u32 kTitle = rgba(0, 240, 255);
constexpr u32 kText = rgba(235, 240, 255);
constexpr u32 kWarn = rgba(255, 190, 40);
constexpr u32 kDim = rgba(120, 135, 160);
constexpr ClearColor kClear{0.0015f, 0.0012f, 0.006f};
constexpr u64 kOverlayRefreshNs = 500000000ull;

#if defined(RF_DEBUG) && !defined(__ANDROID__)
constexpr bool kValidationDefault = true;  // desktop Debug builds run with validation layers
#else
constexpr bool kValidationDefault = false;
#endif
}  // namespace

int App::run(int argc, char** argv) {
    if (!parseArgs(argc, argv, args_)) return 2;
    if (!init()) {
        shutdown();
        return 1;
    }
    while (!quit_) {
        const u64 loopStart = nowNs();
        SDL_Event e;
        while (SDL_PollEvent(&e)) handleEvent(e);  // Android: blocks here while paused
        if (quit_) break;
        if (minimized_ || backgrounded_) {
            discontinuity_ = true;
            SDL_Delay(16);
            continue;
        }
        frame(loopStart);
    }
    if (bench_.phase() == Bench::Phase::Aborted) exitCode_ = 4;
    const u32 validation = renderer_.gpu().validationMessages();
    if (renderer_.gpu().validationEnabled()) {
        RF_LOG("validation messages: %u (loader messages: %u)", validation, renderer_.gpu().loaderMessages());
        if (validation > 0 && exitCode_ == 0) exitCode_ = 3;
    }
    RF_LOG("exit: %llu frames, code %d", static_cast<unsigned long long>(frames_), exitCode_);
    shutdown();
    return exitCode_;
}

bool App::init() {
    logArgs(args_);
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "0");

    SDL_InitFlags flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
#if !defined(__ANDROID__)
    flags |= SDL_INIT_AUDIO;  // Android audio goes through Oboe only
#endif
    if (!SDL_Init(flags)) {
        RF_ERR("SDL_Init: %s", SDL_GetError());
        return false;
    }
    SDL_AddEventWatch(eventWatch, this);

#if defined(__ANDROID__)
    window_ = SDL_CreateWindow("RAVEFORM", 0, 0, SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    android::systemProperty("ro.product.model", device_, sizeof device_);
    char release[16] = {};
    android::systemProperty("ro.build.version.release", release, sizeof release);
    std::snprintf(os_, sizeof os_, "Android %s (API %d)", release, SDL_GetAndroidSDKVersion());
#else
    window_ = SDL_CreateWindow("RAVEFORM", args_.width, args_.height,
                               SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    std::snprintf(device_, sizeof device_, "desktop");
    std::snprintf(os_, sizeof os_, "%s", SDL_GetPlatform());
#endif
    if (!window_) {
        RF_ERR("SDL_CreateWindow: %s", SDL_GetError());
        return false;
    }
    RF_LOG("RAVEFORM %s build, SDL %d.%d.%d, %s, device '%s'", RF_BUILD_NAME, SDL_VERSIONNUM_MAJOR(SDL_GetVersion()),
           SDL_VERSIONNUM_MINOR(SDL_GetVersion()), SDL_VERSIONNUM_MICRO(SDL_GetVersion()), os_, device_);

    if (!font_.init()) return false;

    RendererDesc rd;
    rd.validation = args_.validation >= 0 ? args_.validation == 1 : kValidationDefault;
    rd.swappy = args_.swappy;
    rd.targetHz = args_.fps;
    refreshDisplay();
    if (!renderer_.init(window_, rd)) return false;
    refreshDisplay();
    updateSafeArea();

    // PLAN 9 risk 13: OEM game modes may downscale; compare the surface with the display mode.
    const VkExtent2D surface = renderer_.swapchain().logical;
    const int sMax = static_cast<int>(surface.width > surface.height ? surface.width : surface.height);
    const int sMin = static_cast<int>(surface.width > surface.height ? surface.height : surface.width);
    const int dMax = displayW_ > displayH_ ? displayW_ : displayH_;
    const int dMin = displayW_ > displayH_ ? displayH_ : displayW_;
#if defined(__ANDROID__)
    if (sMax != dMax || sMin != dMin) {
        RF_WARN("!!! surface %dx%d differs from the display mode %dx%d: the OS is scaling the game !!!", sMax, sMin,
                dMax, dMin);
    } else {
        RF_LOG("surface %dx%d matches the display mode", sMax, sMin);
    }
#else
    RF_LOG("surface %dx%d, display %dx%d", sMax, sMin, dMax, dMin);
#endif

    if (!audio_.open(TestTone::render, &tone_)) RF_WARN("audio: no output device; continuing silently");
    tone_.setHold(args_.tone);

    startNs_ = nowNs();
    if (std::strcmp(args_.scenario, "bench_idle") == 0) {
        if (!bench_.begin(args_.scenario, args_.warmupSeconds, args_.seconds, startNs_)) return false;
    } else if (std::strcmp(args_.scenario, "lifecycle_test") == 0) {
        RF_LOG("lifecycle_test: %u cycles of surface release/restore and swapchain rebuilds", args_.cycles);
    } else if (args_.scenario[0]) {
        RF_ERR("unknown scenario '%s' (known: bench_idle, lifecycle_test)", args_.scenario);
        return false;
    }
    if (args_.screenshot[0] && args_.frames == 0) args_.frames = 120;
    overlayOn_ = args_.overlay;
    refreshOverlay(startNs_);
    return true;
}

void App::shutdown() {
    audio_.close();
    renderer_.shutdown();
    if (window_) SDL_DestroyWindow(window_);
    window_ = nullptr;
    SDL_RemoveEventWatch(eventWatch, this);
    SDL_Quit();
}

bool SDLCALL App::eventWatch(void* user, SDL_Event* event) {
    auto* app = static_cast<App*>(user);
    switch (event->type) {
        case SDL_EVENT_WILL_ENTER_BACKGROUND: app->enterBackground(); break;
        case SDL_EVENT_DID_ENTER_FOREGROUND: app->enterForeground(); break;
        default: break;
    }
    return true;
}

void App::enterBackground() {
    if (backgrounded_) return;
    backgrounded_ = true;
    ++backgrounds_;
    logTagged("RFLIFE", "background %u", backgrounds_);
    if (bench_.active()) {
        // A paused run is not a measurement: report it now (the pump blocks until resume) and quit then.
        bench_.abort("backgrounded");
        logTagged("RFBENCH", "{\"scenario\":\"%s\",\"aborted\":\"%s\"}", bench_.scenario(), bench_.abortReason());
        quit_ = true;
    }
    audio_.close();
    renderer_.releaseSurface();  // the UI thread's surfaceDestroyed() waits for this
}

void App::enterForeground() {
    if (!backgrounded_) return;
    backgrounded_ = false;
    ++foregrounds_;
    discontinuity_ = true;
    awaitingResumePresent_ = true;
    logTagged("RFLIFE", "foreground %u", foregrounds_);
    if (!audio_.open(TestTone::render, &tone_)) RF_WARN("audio: reopen after foreground failed");
    refreshDisplay();  // the swapchain itself is rebuilt by the next beginFrame()
}

void App::handleEvent(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_TERMINATING:
            quit_ = true;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (e.key.repeat) break;
            if (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_AC_BACK) {
                quit_ = true;
            } else if (e.key.key == SDLK_F1) {
                overlayOn_ = !overlayOn_;
            } else if (e.key.key == SDLK_F12) {
                screenshotRequested_ = true;
            } else if (e.key.key == SDLK_T) {
                tone_.setHold(!tone_.holding());
            }
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
            const VkExtent2D l = renderer_.swapchain().logical;
            if (static_cast<u32>(e.window.data1) != l.width || static_cast<u32>(e.window.data2) != l.height)
                renderer_.requestRebuild();
            updateSafeArea();
            break;
        }
        case SDL_EVENT_DISPLAY_ORIENTATION:  // a 180-degree turn keeps the size but changes the transform
            renderer_.requestRebuild();
            updateSafeArea();
            break;
        case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:
            updateSafeArea();
            break;
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
            refreshDisplay();
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            logTagged("RFLIFE", "focus lost%s", bench_.active() ? " during the benchmark: run marked invalid" : "");
            bench_.noteInterruption();
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            logTagged("RFLIFE", "focus gained");
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            minimized_ = true;
            break;
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_SHOWN:
            minimized_ = false;
            break;
        case SDL_EVENT_FINGER_DOWN:
            touchDown(e.tfinger.fingerID, normalizedToLogical(e.tfinger.x, e.tfinger.y));
            break;
        case SDL_EVENT_FINGER_MOTION:
            touchMove(e.tfinger.fingerID, normalizedToLogical(e.tfinger.x, e.tfinger.y));
            break;
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
            touchUp(e.tfinger.fingerID);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (e.button.which != SDL_TOUCH_MOUSEID && e.button.button == SDL_BUTTON_LEFT)
                touchDown(kMouseId, windowToLogical(e.button.x, e.button.y));
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (e.motion.which != SDL_TOUCH_MOUSEID && (e.motion.state & SDL_BUTTON_LMASK))
                touchMove(kMouseId, windowToLogical(e.motion.x, e.motion.y));
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e.button.which != SDL_TOUCH_MOUSEID && e.button.button == SDL_BUTTON_LEFT) touchUp(kMouseId);
            break;
        default:
            break;
    }
}

Vec2 App::normalizedToLogical(float nx, float ny) const {
    const VkExtent2D l = renderer_.swapchain().logical;
    return {nx * static_cast<float>(l.width), ny * static_cast<float>(l.height)};
}

Vec2 App::windowToLogical(float wx, float wy) const {
    int ww = 0, wh = 0;
    SDL_GetWindowSize(window_, &ww, &wh);
    if (ww <= 0 || wh <= 0) return {wx, wy};
    return normalizedToLogical(wx / static_cast<float>(ww), wy / static_cast<float>(wh));
}

u32 App::activeTouches() const {
    u32 n = 0;
    for (const TouchPoint& t : touches_) n += t.active ? 1u : 0u;
    return n;
}

void App::touchDown(u64 id, Vec2 pos) {
    for (TouchPoint& t : touches_) {
        if (t.active) continue;
        t = {id, pos, true};
        break;
    }
    // Pitch follows x: A4 at the left edge to A6 at the right.
    const float w = static_cast<float>(renderer_.swapchain().logical.width);
    const float k = w > 0.0f ? clampf(pos.x / w, 0.0f, 1.0f) : 0.5f;
    tone_.trigger(440.0f * std::exp2(2.0f * k));
    pulseNs_ = nowNs();
    pulseAt_ = pos;
    // Three fingers toggle the overlay (once per gesture).
    if (activeTouches() >= 3 && !threeFingerLatch_) {
        threeFingerLatch_ = true;
        overlayOn_ = !overlayOn_;
    }
}

void App::touchMove(u64 id, Vec2 pos) {
    for (TouchPoint& t : touches_)
        if (t.active && t.id == id) t.pos = pos;
}

void App::touchUp(u64 id) {
    for (TouchPoint& t : touches_)
        if (t.active && t.id == id) t.active = false;
    if (activeTouches() == 0) threeFingerLatch_ = false;
}

void App::refreshDisplay() {
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window_);
    if (const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display)) {
        displayHz_ = mode->refresh_rate;
        displayW_ = mode->w;
        displayH_ = mode->h;
    }
#if defined(__ANDROID__)
    const float hz = android::displayRefreshHz();  // the panel's current mode, more current than SDL's copy
    if (hz > 1.0f) displayHz_ = hz;
#endif
    renderer_.pacer().setDisplayRefreshHz(displayHz_);
    RF_LOG("display: %dx%d @ %.2f Hz", displayW_, displayH_, displayHz_);
}

void App::updateSafeArea() {
    SDL_Rect safe{};
    int ww = 0, wh = 0, pw = 0, ph = 0;
    if (!window_ || !SDL_GetWindowSafeArea(window_, &safe)) return;
    SDL_GetWindowSize(window_, &ww, &wh);
    SDL_GetWindowSizeInPixels(window_, &pw, &ph);
    if (ww <= 0 || wh <= 0) return;
    const float sx = static_cast<float>(pw) / static_cast<float>(ww);
    const float sy = static_cast<float>(ph) / static_cast<float>(wh);
    insetMin_ = {safe.x * sx, safe.y * sy};
    insetMax_ = {(ww - safe.x - safe.w) * sx, (wh - safe.y - safe.h) * sy};
}

void App::frame(u64 loopStartNs) {
    audio_.update();
    CapsuleBatch batch;
    if (!renderer_.beginFrame(batch)) {
        discontinuity_ = true;
        SDL_Delay(2);
        return;
    }
    const VkExtent2D logical = renderer_.swapchain().logical;
    const float w = static_cast<float>(logical.width);
    const float h = static_cast<float>(logical.height);
    const float s = h / 1080.0f;  // reference pixels
    const u64 now = nowNs();
    const float t = static_cast<float>(nsToSec(now - startNs_));

    SceneInput in;
    in.width = w;
    in.height = h;
    in.scale = s;
    in.time = t;
    in.insetMin = insetMin_;
    in.insetMax = insetMax_;
    in.touches = touches_;
    in.touchCount = kMaxTouches;
    in.intervals = &intervals_;
    in.targetMs = static_cast<float>(renderer_.pacer().targetIntervalNs() * 1e-6);
    in.pulseAge = pulseNs_ ? static_cast<float>(nsToSec(now - pulseNs_)) : 1e9f;
    in.pulseAt = pulseAt_;
    in.toneHold = tone_.holding();
    drawM0Scene(batch, font_, in);

    if (overlayOn_) {
        const Vec2 origin{insetMin_.x + 36.0f * s, insetMin_.y + 30.0f * s};
        const float y = overlay_.draw(batch, font_, origin, 24.0f * s, 38.0f * s);
        char line[DevOverlay::kLineChars];
        int n = std::snprintf(line, sizeof line, "TOUCH %u", activeTouches());
        for (const TouchPoint& tp : touches_) {
            if (!tp.active || n >= static_cast<int>(sizeof line) - 16) continue;
            n += std::snprintf(line + n, sizeof line - static_cast<size_t>(n), "  %d,%d", static_cast<int>(tp.pos.x),
                               static_cast<int>(tp.pos.y));
        }
        TextStyle style;
        style.capHeight = 24.0f * s;
        style.color = kText;
        font_.draw(batch, {origin.x, y}, style, line);
    }
    lastCapsules_ = batch.count();

    const bool capture = screenshotRequested_ || (args_.screenshot[0] && args_.frames && frames_ + 1 >= args_.frames);
    renderer_.endFrame(batch, kClear, t, capture);
    afterPresent(loopStartNs, nowNs(), capture);
}

void App::afterPresent(u64 loopStartNs, u64 presentedNs, bool captured) {
    ++frames_;
    // CPU frame time: the loop minus the time blocked in the fence wait, acquire and present.
    const FrameTimings& tm = renderer_.timings();
    const u64 loopNs = presentedNs - loopStartNs;
    const u64 blockedNs = tm.fenceWaitNs + tm.acquireNs + tm.presentNs;
    const float cpuMs = static_cast<float>(nsToMs(loopNs > blockedNs ? loopNs - blockedNs : 0));
    float intervalMs = -1.0f;
    if (!discontinuity_ && lastPresentNs_) {
        intervalMs = static_cast<float>(nsToMs(presentedNs - lastPresentNs_));
        intervals_.add(intervalMs);
        cpu_.add(cpuMs);
    }
    lastPresentNs_ = presentedNs;
    discontinuity_ = false;

    if (awaitingResumePresent_ && renderer_.hasSwapchain()) {
        awaitingResumePresent_ = false;
        const Swapchain& sc = renderer_.swapchain();
        logTagged("RFLIFE", "resumed %u: presenting, swapchain %ux%u rotation %d, %s, rebuilds %u", foregrounds_,
                  sc.extent.width, sc.extent.height, sc.rotationDeg, renderer_.pacer().modeName(),
                  renderer_.rebuildCount());
    }

    if (captured) {
        char path[300];
        if (screenshotRequested_ && !args_.screenshot[0]) {
            char name[64];
            std::snprintf(name, sizeof name, "raveform_%llu.png", static_cast<unsigned long long>(frames_));
            resolveOutputPath(name, path, sizeof path);
        } else {
            resolveOutputPath(args_.screenshot[0] ? args_.screenshot : "raveform.png", path, sizeof path);
        }
        if (!renderer_.saveCapture(path) && !screenshotRequested_) exitCode_ = 5;
        screenshotRequested_ = false;
    }

    if (bench_.active()) {
        const Bench::Event ev = bench_.onFrame(presentedNs, intervalMs, cpuMs);
        if (ev == Bench::Event::MeasureStarted) {
            audio_.resetPeaks();
            underrunsAtMeasure_ = audio_.stats().underruns;
            allocsAtMeasure_ = heapAllocCount();
            presentAllocsAtMeasure_ = renderer_.pacer().presentAllocations();
        } else if (ev == Bench::Event::Finished) {
            allocsMeasured_ = heapAllocCount() - allocsAtMeasure_;
            presentAllocsMeasured_ = renderer_.pacer().presentAllocations() - presentAllocsAtMeasure_;
            reportBench();
            quit_ = true;
        }
    }
    if (!displayRechecked_ && presentedNs - startNs_ > 1500000000ull) {
        // The refresh-rate vote switches the panel mode asynchronously; look again during the warm-up.
        displayRechecked_ = true;
        refreshDisplay();
    }
    if (std::strcmp(args_.scenario, "lifecycle_test") == 0) stepLifecycleTest();
    if (args_.frames && frames_ >= args_.frames) quit_ = true;
    if (presentedNs - lastOverlayNs_ >= kOverlayRefreshNs) refreshOverlay(presentedNs);
}

void App::refreshOverlay(u64 nowNs) {
    lastOverlayNs_ = nowNs;
    const Summary iv = intervals_.summarize();
    const Summary cpu = cpu_.summarize();
    const AudioStats a = audio_.stats();
    const Swapchain& sc = renderer_.swapchain();
    const GpuContext& g = renderer_.gpu();
    const FramePacer& p = renderer_.pacer();
    const float target = static_cast<float>(p.targetIntervalNs() * 1e-6);

    overlay_.clear();
    overlay_.addLine(kTitle, "RAVEFORM M0  %s BUILD  FRAME %llu", RF_BUILD_NAME, static_cast<unsigned long long>(frames_));
    overlay_.addLine(iv.p99 > target * 1.05f ? kWarn : kText, "INTERVAL P50 %.2f  P99 %.2f  MAX %.2f MS", iv.p50,
                     iv.p99, iv.max);
    overlay_.addLine(kText, "CPU FRAME P50 %.2f  P99 %.2f MS  CAPSULES %u", cpu.p50, cpu.p99, lastCapsules_);
    overlay_.addLine(kText, "SURFACE %uX%u  IMAGE %uX%u  ROTATION %d", sc.logical.width, sc.logical.height,
                     sc.extent.width, sc.extent.height, sc.rotationDeg);
    overlay_.addLine(kText, "DISPLAY %.1f HZ  PACING %s %.2f MS  %u IMAGES", displayHz_, p.modeName(), target,
                     sc.imageCount);
    overlay_.addLine(kText, "GPU %s  VULKAN %s", g.props.deviceName, g.apiVersionText);
    if (a.open) {
        char underruns[16];
        if (a.underruns >= 0) {
            std::snprintf(underruns, sizeof underruns, "%d", a.underruns);
        } else {
            std::snprintf(underruns, sizeof underruns, "N/A");
        }
        overlay_.addLine(a.underruns > 0 ? kWarn : kText, "AUDIO %s %u HZ  BURST %u  BUFFER %u  UNDERRUNS %s", a.api,
                         a.sampleRate, a.framesPerBurst, a.bufferFrames, underruns);
    } else {
        overlay_.addLine(kWarn, "AUDIO CLOSED");
    }
    overlay_.addLine(kText, "AUDIO CALLBACK WORST %.3f MS  NOTES %u", a.worstCallbackMs, tone_.notesStarted());
    if (g.validationEnabled()) {
        overlay_.addLine(g.validationMessages() ? kWarn : kText, "VALIDATION MESSAGES %u", g.validationMessages());
    }
    overlay_.addLine(kDim, "PAUSES %u  RESUMES %u  SWAPCHAIN REBUILDS %u", backgrounds_, foregrounds_,
                     renderer_.rebuildCount());
}

void App::stepLifecycleTest() {
    if (frames_ < cycleFrame_ + 30) return;
    cycleFrame_ = frames_;
    if (cyclesDone_ >= args_.cycles) {
        reportLifecycleTest();
        quit_ = true;
        return;
    }
    if (cyclesDone_ % 2 == 0) {
        // The same path as an Android pause/resume: surface, swapchain and audio go away and come back.
        enterBackground();
        enterForeground();
    } else {
#if defined(__ANDROID__)
        renderer_.requestRebuild();
#else
        const bool small = (cyclesDone_ / 2) % 2 == 0;
        SDL_SetWindowSize(window_, small ? args_.width * 3 / 4 : args_.width, small ? args_.height * 3 / 4 : args_.height);
#endif
    }
    ++cyclesDone_;
}

void App::reportLifecycleTest() {
    char buffer[768];
    JsonWriter j(buffer, sizeof buffer);
    j.beginObject();
    j.str("scenario", "lifecycle_test");
    j.str("build", RF_BUILD_NAME);
    j.str("device", device_);
    j.integer("cycles", cyclesDone_);
    j.integer("frames", static_cast<i64>(frames_));
    j.integer("surface_releases", backgrounds_);
    j.integer("swapchain_rebuilds", renderer_.rebuildCount());
    j.integer("audio_underruns", audio_.stats().underruns);
    j.integer("validation_messages", renderer_.gpu().validationMessages());
    j.integer("loader_messages", renderer_.gpu().loaderMessages());
    j.boolean("validation", renderer_.gpu().validationEnabled());
    j.endObject();
    logTagged("RFBENCH", "%s", j.text());
}

void App::reportBench() {
    const GpuContext& g = renderer_.gpu();
    const Swapchain& sc = renderer_.swapchain();
    const FramePacer& p = renderer_.pacer();
    const double targetMs = p.targetIntervalNs() * 1e-6;
    const u32 frames = bench_.intervalCount();
    const u32 missed = bench_.intervalsAbove(static_cast<float>(targetMs * 1.5));
    const u32 over50 = bench_.intervalsAbove(50.0f);
    const Summary iv = bench_.finishIntervals();
    const Summary cpu = bench_.finishCpu();
    const AudioStats a = audio_.stats();
    float displayHz = displayHz_;
#if defined(__ANDROID__)
    const float hz = android::displayRefreshHz();  // after the run: did the panel hold the requested mode?
    if (hz > 1.0f) displayHz = hz;
#endif

    char buffer[2048];
    JsonWriter j(buffer, sizeof buffer);
    j.beginObject();
    j.str("scenario", bench_.scenario());
    // Valid only if the activity kept focus for the whole run (shared test phones).
    j.boolean("valid", bench_.interruptions() == 0 && !bench_.overflowed());
    j.integer("interruptions", bench_.interruptions());
    j.str("build", RF_BUILD_NAME);
    j.str("device", device_);
    j.str("os", os_);
    j.str("gpu", g.props.deviceName);
    j.str("vk", g.apiVersionText);
    j.str("driver", g.driverVersionText);
    j.pair("surface", sc.logical.width, sc.logical.height);
    j.pair("image", sc.extent.width, sc.extent.height);
    j.integer("rotation", sc.rotationDeg);
    j.integer("images", sc.imageCount);
    j.str("present", p.modeName());
    j.num("display_hz", displayHz, 2);
    j.num("refresh_ms", p.refreshPeriodNs() * 1e-6, 3);
    j.num("target_ms", targetMs, 3);
    j.num("warmup_s", bench_.warmupSeconds(), 1);
    j.num("seconds", bench_.measuredSeconds(), 2);
    j.integer("frames", frames);
    j.beginObject("interval_ms");
    j.num("mean", iv.mean);
    j.num("p50", iv.p50);
    j.num("p95", iv.p95);
    j.num("p99", iv.p99);
    j.num("p999", iv.p999);
    j.num("max", iv.max);
    j.endObject();
    j.integer("missed", missed);
    j.num("missed_pct", frames ? 100.0 * missed / frames : 0.0, 2);
    j.integer("over_50ms", over50);
    j.beginObject("cpu_ms");
    j.num("p50", cpu.p50);
    j.num("p99", cpu.p99);
    j.num("max", cpu.max);
    j.endObject();
    j.beginObject("audio");
    j.str("api", a.api);
    j.integer("rate", a.sampleRate);
    j.integer("burst", a.framesPerBurst);
    j.integer("buffer", a.bufferFrames);
    j.integer("underruns", (a.underruns >= 0 && underrunsAtMeasure_ >= 0) ? a.underruns - underrunsAtMeasure_ : -1);
    j.integer("callbacks", static_cast<i64>(a.callbacks));
    j.num("callback_worst_ms", a.worstCallbackMs);
    j.boolean("tone_hold", tone_.holding());
    j.boolean("mmap", a.mmap);
    j.boolean("exclusive", a.exclusive);
    j.endObject();
    // operator new calls in the measured window: ours (must be 0) and those inside the present call.
    j.integer("heap_allocs", static_cast<i64>(allocsMeasured_ - presentAllocsMeasured_));
    j.integer("heap_allocs_in_present", static_cast<i64>(presentAllocsMeasured_));
    j.integer("capsules", lastCapsules_);
    j.integer("swapchain_rebuilds", renderer_.rebuildCount());
    j.boolean("validation", g.validationEnabled());
    j.integer("validation_messages", g.validationMessages());
    j.integer("loader_messages", g.loaderMessages());
    j.boolean("samples_overflowed", bench_.overflowed());
    j.endObject();
    if (j.overflowed()) RF_ERR("bench: JSON buffer too small");
    logTagged("RFBENCH", "%s", j.text());

#if defined(__ANDROID__)
    char path[300];
    char name[64];
    std::snprintf(name, sizeof name, "%s.json", bench_.scenario());
    resolveOutputPath(name, path, sizeof path);
    if (FILE* f = std::fopen(path, "wb")) {
        std::fprintf(f, "%s\n", j.text());
        std::fclose(f);
    }
#endif
}

void App::resolveOutputPath(const char* in, char* out, size_t capacity) const {
#if defined(__ANDROID__)
    // Relative paths go to the app's external files dir, which adb can pull.
    if (in[0] != '/') {
        if (const char* dir = SDL_GetAndroidExternalStoragePath()) {
            std::snprintf(out, capacity, "%s/%s", dir, in);
            return;
        }
    }
#endif
    std::snprintf(out, capacity, "%s", in);
}

}  // namespace rf
