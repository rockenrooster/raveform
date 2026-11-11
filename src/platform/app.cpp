#include "platform/app.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/alloc_counter.h"
#include "core/json.h"
#include "core/log.h"
#include "core/time.h"
#include "platform/cpu_affinity.h"

#if defined(__ANDROID__)
#include "platform/android/android_glue.h"
#endif

namespace rf {

namespace {
constexpr u32 kTitle = rgba(0, 240, 255);
constexpr u32 kText = rgba(235, 240, 255);
constexpr u32 kWarn = rgba(255, 190, 40);
constexpr u32 kDim = rgba(120, 135, 160);
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
        if (bench_.active()) cpuAtLoopStart_ = FrameProbe::currentCpu();
        SDL_Event e;
        while (SDL_PollEvent(&e)) handleEvent(e);  // Android: blocks here while paused
        eventsDoneNs_ = nowNs();
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
    if (!loadLook(look_)) return false;
    variant_ = look_.variant(args_.look);
    if (!variant_) {
        RF_ERR("unknown look '%s' (see design/look_variants.json)", args_.look);
        return false;
    }
    lastLook_ = static_cast<u32>(variant_ - look_.variants);
    if (!loadPalettes(palettes_)) return false;
    palette_ = palettes_.find(args_.palette);
    if (!palette_) {
        RF_ERR("unknown palette '%s' (see design/palettes.json)", args_.palette);
        return false;
    }
    if (!glyphs_.load(look_.jointFlatBelowDegrees)) return false;
    Tier tier = Tier::High;
    if (args_.tier[0] && !parseTier(args_.tier, tier)) {
        RF_ERR("unknown tier '%s' (low, medium, high, ultra)", args_.tier);
        return false;
    }
    tier_ = tierSettings(tier);

    // Before the renderer and the audio stream exist, so the threads they start (Swappy, the audio
    // callback) inherit the placement.
    applyAffinity();

    RendererDesc rd;
    rd.validation = args_.validation >= 0 ? args_.validation == 1 : kValidationDefault;
    rd.swappy = args_.swappy;
    rd.targetHz = args_.fps;
    refreshDisplay();
    if (!renderer_.init(window_, rd, look_)) return false;
    if (!renderer_.setGlyphs(glyphs_) || !renderer_.setTier(tier_.render)) return false;
    refreshDisplay();
    updateSafeArea();
    if (args_.sceneFile[0]) {
        char path[300];
        resolveOutputPath(args_.sceneFile, path, sizeof path);
        // Canvas pixels are reference pixels on a 1080-high canvas.
        if (!loadRfScene(path, look_.poolMinRadiusRefPx, sceneFile_) || !renderer_.setStaticScene(sceneFile_.view))
            return false;
        sceneLoaded_ = true;
        content_ = Content::SceneFile;
    }

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

    // M2 audio begin
    if (!band_.init(args_.audio, args_.scenario, &audio_)) return false;
    if (std::strcmp(args_.scenario, "audio_test") == 0 || std::strcmp(args_.scenario, "sync") == 0) {
        audioScene_.init(std::strcmp(args_.scenario, "sync") == 0 ? AudioScene::Kind::Sync : AudioScene::Kind::AudioTest, band_,
                         font_, palettes_, *palette_, look_);
        content_ = Content::Audio;
    }
    if (!openAudio()) RF_WARN("audio: no output device; continuing silently");
    // M2 audio end
    tone_.setHold(args_.tone);

    startNs_ = nowNs();
    const bool benchMid = std::strcmp(args_.scenario, "bench_mid") == 0;
    const bool benchEndgame = std::strcmp(args_.scenario, "bench_endgame") == 0;
    if (benchMid || benchEndgame) {
        if (!stage_.init(benchMid ? StageKind::Mid : StageKind::Endgame, look_, palettes_, *palette_, glyphs_, tier_))
            return false;
        stage_.setMute(args_.mute);
        if (args_.mute) RF_LOG("stage: muted light 0x%x (analysis picture)", args_.mute);
        content_ = Content::Stage;
    } else if (std::strcmp(args_.scenario, "glyph_sheet") == 0) {
        sheet_.init(look_, palettes_, *palette_, glyphs_, font_, args_.page);
        content_ = Content::Sheet;
    }
    if (std::strcmp(args_.scenario, "bench_idle") == 0 || benchMid || benchEndgame) {
        if (!bench_.begin(args_.scenario, args_.warmupSeconds, args_.seconds, startNs_)) return false;
        // Room for every frame of the run at up to 240 Hz (allocated now, before measuring).
        const u32 cap = static_cast<u32>((args_.seconds + 5.0f) * 240.0f) + 1024;
        if (!probe_.init(cap)) return false;
        Series* all[] = {&inView_, &bulletsInView_, &glyphCount_, &sparksLive_, &capsuleCount_, &crowdGain_, &swarmGain_, &load_};
        for (Series* s : all) {
            s->v = static_cast<float*>(std::malloc(sizeof(float) * cap));
            s->cap = s->v ? cap : 0;
        }
        for (Series& s : gpu_) {
            s.v = static_cast<float*>(std::malloc(sizeof(float) * cap));
            s.cap = s.v ? cap : 0;
        }
        renderer_.pacer().enableStats(true);  // Swappy's display-side histograms (Swappy only)
    } else if (std::strcmp(args_.scenario, "lifecycle_test") == 0) {
        RF_LOG("lifecycle_test: %u cycles of surface release/restore and swapchain rebuilds", args_.cycles);
    } else if (args_.scenario[0] && content_ != Content::Sheet && content_ != Content::Audio) {  // M2 audio: Audio
        RF_ERR("unknown scenario '%s' (known: bench_idle, bench_mid, bench_endgame, glyph_sheet, lifecycle_test)",
               args_.scenario);
        return false;
    }
    // Governor v1: on for interactive stages; benchmarks keep the tier fixed (TECH_SPEC 5.4).
    governorOn_ = content_ == Content::Stage && (args_.governor == 1 || (args_.governor < 0 && !bench_.active()));
    governor_.reset(tier_.tier, static_cast<float>(renderer_.pacer().targetIntervalNs() * 1e-6));
    RF_LOG("tier %s (bloom %u levels, afterglow %s, distortion %s, aberration %s, sparks x%.2f), governor %s", tier_.name,
           tier_.render.bloomLevels, tier_.render.afterglowShift == 0 ? "off" : (tier_.render.afterglowShift == 1 ? "1/2" : "1/4"),
           tier_.render.distortionShift == 0 ? "off" : (tier_.render.distortionShift == 3 ? "1/8" : "1/4"),
           tier_.render.aberration ? "on" : "off", tier_.particleDensity, governorOn_ ? "on" : "off");
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
    applyAffinity();  // Android resets thread affinities when the app changes cpuset
    if (!openAudio()) RF_WARN("audio: reopen after foreground failed");  // M2 audio: the band or the tone
    refreshDisplay();  // the swapchain itself is rebuilt by the next beginFrame()
}

void App::handleEvent(const SDL_Event& e) {
    // M2 audio begin
    if (content_ == Content::Audio && (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) && !e.key.repeat &&
        audioScene_.key(static_cast<int>(e.key.key), e.type == SDL_EVENT_KEY_DOWN))
        return;
    // M2 audio end
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
                if (overlayOn_) refreshOverlay(nowNs());
            } else if (e.key.key == SDLK_F12) {
                screenshotRequested_ = true;
            } else if (e.key.key == SDLK_T) {
                tone_.setHold(!tone_.holding());
            } else if (e.key.key == SDLK_L) {
                nextLook();
            } else if (e.key.key == SDLK_P) {
                nextPalette();
            } else if (e.key.key == SDLK_K && !bench_.active()) {
                applyTier(static_cast<Tier>((static_cast<u32>(tier_.tier) + 1) % 4u));
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
    if (content_ == Content::Audio) audioScene_.touchDown(id, pos);  // M2 audio
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
    // Gestures, decided when the last finger lifts: two fingers = next look, three = overlay, four = next
    // palette.
    const u32 n = activeTouches();
    if (n >= 2) twoFingerLatch_ = true;
    if (n >= 3) threeFingerLatch_ = true;
    if (n >= 4) fourFingerLatch_ = true;
}

void App::touchMove(u64 id, Vec2 pos) {
    if (content_ == Content::Audio) audioScene_.touchMove(id, pos);  // M2 audio
    for (TouchPoint& t : touches_)
        if (t.active && t.id == id) t.pos = pos;
}

void App::touchUp(u64 id) {
    if (content_ == Content::Audio) audioScene_.touchUp(id);  // M2 audio
    for (TouchPoint& t : touches_)
        if (t.active && t.id == id) t.active = false;
    if (activeTouches() == 0) {
        if (fourFingerLatch_) {
            nextPalette();
        } else if (threeFingerLatch_) {
            overlayOn_ = !overlayOn_;
            if (overlayOn_) refreshOverlay(nowNs());
        } else if (twoFingerLatch_) {
            nextLook();
        }
        twoFingerLatch_ = threeFingerLatch_ = fourFingerLatch_ = false;
    }
}

void App::applyAffinity() {
    if (!args_.fastCores) {
        std::snprintf(affinity_, sizeof affinity_, "os");
        return;
    }
    char cpus[sizeof affinity_];
    if (pinToFastCores(cpus, sizeof cpus)) {
        std::snprintf(affinity_, sizeof affinity_, "%s", cpus);
        RF_LOG("cpu: main thread pinned to the fast cores %s", affinity_);
    } else {
        std::snprintf(affinity_, sizeof affinity_, "os");
        RF_LOG("cpu: no faster cores to pin to; the scheduler places the main thread");
    }
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
    audioDoneNs_ = nowNs();
    FrameDraw draw;
    if (!renderer_.beginFrame(draw)) {
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
    // Scenes advance by real time, or by exactly 1/60 s per frame when pictures are taken
    // (design/scenes.md: repeatable stills and strips).
    float dt = lastFrameNs_ ? static_cast<float>(nsToSec(now - lastFrameNs_)) : 1.0f / 60.0f;
    if (dt > 0.1f) dt = 0.1f;
    if (args_.fixedStep || args_.screenshot[0]) dt = 1.0f / 60.0f;
    lastFrameNs_ = now;

    FrameParams params;
    params.look = variant_;
    params.bloom = args_.bloom;
    params.vignette = !args_.still;
    params.frameIndex = static_cast<u32>(frames_);
    params.gpuBurn = args_.gpuBurn;
    // M2 audio begin
    // Heard time at the moment this frame reaches the panel (about two refresh periods from now).
    band_.update(static_cast<i64>(2 * renderer_.pacer().refreshPeriodNs()));
    if (content_ == Content::Audio) {
        audioScene_.render(draw, params, background_, w, h, insetMin_, insetMax_);
        renderer_.setBackground(background_);
        if (audioScene_.quitRequested()) quit_ = true;
    }
    // M2 audio end
    if (content_ == Content::Stage) {
        stage_.advance(dt);
        stage_.render(draw, params, background_, w, h);
        renderer_.setBackground(background_);
        if (args_.still) {
            params.vignette = false;
            params.afterglow = false;
            params.ringCount = params.wellCount = 0;
            params.aberration = false;
        }
    } else if (content_ == Content::Sheet) {
        sheet_.render(draw, params, background_, w, h);
        renderer_.setBackground(background_);
    } else if (content_ == Content::M0) {
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
        drawM0Scene(draw.scene, font_, in);
    }

    if (overlayOn_) {  // after tone mapping (the HUD batch): never bloomed
        const Vec2 origin{insetMin_.x + 36.0f * s, insetMin_.y + 30.0f * s};
        const float y = overlay_.draw(draw.hud, font_, origin, 24.0f * s, 38.0f * s);
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
        font_.draw(draw.hud, {origin.x, y}, style, line);
    }

    const u64 firstShot = args_.frames > args_.strip ? args_.frames - (args_.strip ? args_.strip - 1 : 0) : 1;
    const bool capture = screenshotRequested_ || (args_.screenshot[0] && args_.frames && frames_ + 1 >= firstShot);
    renderer_.endFrame(draw, params, capture);
    lastCapsules_ = renderer_.lastCapsuleCount();
    afterPresent(loopStartNs, nowNs(), capture);
}

void App::captureName(char* out, size_t capacity, u32 stripIndex) const {
    // Strip frames: <name>_<k>.png (k = 0 for the first frame of the strip).
    char base[260];
    std::snprintf(base, sizeof base, "%s", args_.screenshot[0] ? args_.screenshot : "raveform.png");
    if (args_.strip > 1) {
        char* dot = std::strrchr(base, '.');
        char ext[16] = ".png";
        if (dot) {
            std::snprintf(ext, sizeof ext, "%s", dot);
            *dot = '\0';
        }
        char named[300];
        std::snprintf(named, sizeof named, "%s_%u%s", base, stripIndex, ext);
        resolveOutputPath(named, out, capacity);
        return;
    }
    resolveOutputPath(base, out, capacity);
}

void App::nextLook() {
    lastLook_ = (lastLook_ + 1) % (look_.variantCount ? look_.variantCount : 1);
    variant_ = &look_.variants[lastLook_];
    RF_LOG("look: %s", variant_->id);
    if (overlayOn_) refreshOverlay(nowNs());
}

void App::nextPalette() {
    const u32 i = (palettes_.indexOf(palette_) + 1) % palettes_.count;
    palette_ = &palettes_.list[i];
    stage_.setPalette(*palette_);
    sheet_.setPalette(*palette_);
    RF_LOG("palette: %s", palette_->id);
    if (overlayOn_) refreshOverlay(nowNs());
}

void App::applyTier(Tier t) {
    tier_ = tierSettings(t);
    renderer_.setTier(tier_.render);
    if (content_ == Content::Stage) stage_.setTier(tier_);
    RF_LOG("tier: %s", tier_.name);
    if (overlayOn_) refreshOverlay(nowNs());
}

void App::afterPresent(u64 loopStartNs, u64 presentedNs, bool captured) {
    ++frames_;
    // CPU frame time: the loop minus the time blocked in the fence wait, acquire and present.
    const FrameTimings& tm = renderer_.timings();
    const u64 loopNs = presentedNs - loopStartNs;
    const u64 blockedNs = tm.fenceWaitNs + tm.acquireNs + tm.presentNs;
    const float cpuMs = static_cast<float>(nsToMs(loopNs > blockedNs ? loopNs - blockedNs : 0));
    const u64 prevPresentNs = lastPresentNs_;
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
            const u64 firstShot = args_.frames > args_.strip ? args_.frames - (args_.strip ? args_.strip - 1 : 0) : 1;
            captureName(path, sizeof path, static_cast<u32>(frames_ >= firstShot ? frames_ - firstShot : 0));
        }
        if (!renderer_.saveCapture(path) && !screenshotRequested_) exitCode_ = 5;
        screenshotRequested_ = false;
        if (content_ == Content::Stage) {  // what the picture holds (outside any measurement that counts)
            const StageStats& st = stage_.stats();
            RF_LOG("stage: song %.3f s, %u enemies in view (%u slots), %u enemy bullets in view (%u alive), %u Shards, "
                   "%u glyph instances, load %.1f, crowd gain %.3f, swarm gain %.3f, %u kills so far",
                   stage_.time(), st.enemiesInView, st.enemies, st.bulletsInView, st.bullets, st.shards, st.glyphInstances,
                   st.load, st.crowdGain, st.swarmGain, st.kills);
        }
    }

    if (bench_.active()) {
        // The bench records this interval when it was already measuring before this frame.
        const bool measuring = bench_.phase() == Bench::Phase::Measure && intervalMs >= 0.0f;
        const Bench::Event ev = bench_.onFrame(presentedNs, intervalMs, cpuMs);
        probeFrame(loopStartNs, prevPresentNs, presentedNs, intervalMs, measuring);
        if (measuring) {
            const GpuPassTimes& gt = renderer_.gpuTimes();
            if (gt.valid && gt.frame != gpuFrameSeen_) {
                gpuFrameSeen_ = gt.frame;
                for (u32 k = 0; k < GpuPassTimes::Count; ++k) gpu_[k].add(gt.ms[k]);
            }
            if (content_ == Content::Stage) {
                const StageStats& st = stage_.stats();
                inView_.add(static_cast<float>(st.enemiesInView));
                bulletsInView_.add(static_cast<float>(st.bulletsInView));
                glyphCount_.add(static_cast<float>(st.glyphInstances));
                sparksLive_.add(static_cast<float>(renderer_.liveParticles()));
                capsuleCount_.add(static_cast<float>(lastCapsules_));
                crowdGain_.add(st.crowdGain);
                swarmGain_.add(st.swarmGain);
                load_.add(st.load);
            }
        }
        FramePacer& pacer = renderer_.pacer();
        if (ev == Bench::Event::MeasureStarted) {
            audio_.resetPeaks();
            band_.resetMeasurements();  // M2 audio
            underrunsAtMeasure_ = audio_.stats().underruns;
            pacer.clearStats();
            probe_.begin(presentedNs, pacer.displayTimingActive());
            mainAllocsAtMeasure_ = heapAllocCountThisThread();
            pacerAllocsAtMeasure_ = pacer.pacerAllocations();
            allAllocsAtMeasure_ = heapAllocCount();
        } else if (ev == Bench::Event::Finished) {
            mainAllocsMeasured_ = heapAllocCountThisThread() - mainAllocsAtMeasure_;
            pacerAllocsMeasured_ = pacer.pacerAllocations() - pacerAllocsAtMeasure_;
            allAllocsMeasured_ = heapAllocCount() - allAllocsAtMeasure_;
            probe_.finish(static_cast<float>(pacer.targetIntervalNs() * 1e-6),
                          static_cast<float>(pacer.refreshPeriodNs() * 1e-6));
            reportBench();
            quit_ = true;
        }
    }
    if (!displayRechecked_ && presentedNs - startNs_ > 1500000000ull) {
        // The refresh-rate vote switches the panel mode asynchronously; look again during the warm-up.
        displayRechecked_ = true;
        refreshDisplay();
        const FramePacer& pacer = renderer_.pacer();
        if (pacer.votedHz() > displayHz_ + 1.0f) {
            RF_WARN("display: voted %.2f Hz but the panel runs at %.2f Hz: a system or user refresh-rate setting "
                    "overrules the vote",
                    pacer.votedHz(), displayHz_);
        }
    }
    if (governorOn_ && intervalMs >= 0.0f) {
        // Governor v1 (TECH_SPEC 5.4): the share of the last 120 frames that missed a refresh, and the
        // GPU work per frame.
        if ((frames_ & 15u) == 0) {
            const float limit = 1.5f * static_cast<float>(renderer_.pacer().targetIntervalNs() * 1e-6);
            const u32 n = intervals_.size() < 120 ? intervals_.size() : 120u;
            u32 late = 0;
            for (u32 i = intervals_.size() - n; i < intervals_.size(); ++i) late += intervals_.at(i) > limit ? 1u : 0u;
            governorMissed_ = n ? static_cast<float>(late) / static_cast<float>(n) : 0.0f;
        }
        const GpuPassTimes& gt = renderer_.gpuTimes();
        const Tier before = governor_.tier();
        const bool changed = governor_.update(intervalMs * 0.001f, governorMissed_, gt.valid ? gt.ms[GpuPassTimes::Total] : 0.0f, 0);
        stage_.setDensity(governor_.density());
        if (changed && governor_.tier() != before) applyTier(governor_.tier());
    }
    if (std::strcmp(args_.scenario, "lifecycle_test") == 0) stepLifecycleTest();
    if (args_.frames && frames_ >= args_.frames) quit_ = true;
    // The overlay text (and its audio stream queries) is only refreshed while the overlay is shown.
    if (overlayOn_ && presentedNs - lastOverlayNs_ >= kOverlayRefreshNs) refreshOverlay(presentedNs);
}

void App::probeFrame(u64 loopStartNs, u64 prevPresentNs, u64 presentedNs, float intervalMs, bool measuring) {
    // Split the interval since the previous present into the loop's phases (they add up to it).
    const auto span = [](u64 from, u64 to) { return to > from ? to - from : u64{0}; };
    const FrameTimings& tm = renderer_.timings();
    u64 ph[FrameProbe::kPhases] = {};
    const auto at = [&ph](Phase p) -> u64& { return ph[static_cast<u32>(p)]; };
    if (intervalMs >= 0.0f) {
        at(Phase::Post) = span(prevPresentNs, loopStartNs);
        at(Phase::Events) = span(loopStartNs, eventsDoneNs_);
        at(Phase::Audio) = span(eventsDoneNs_, audioDoneNs_);
        at(Phase::Begin) = span(audioDoneNs_, tm.fenceStart) + span(tm.acquireEnd, tm.beginEnd);
        at(Phase::Fence) = span(tm.fenceStart, tm.fenceEnd);
        at(Phase::Acquire) = span(tm.fenceEnd, tm.acquireEnd);
        at(Phase::Scene) = span(tm.beginEnd, tm.recordStart);
        at(Phase::Record) = span(tm.recordStart, tm.recordEnd);
        at(Phase::Submit) = span(tm.recordEnd, tm.submitEnd);
        at(Phase::Present) = span(tm.submitEnd, presentedNs);
    }
    FramePacer& pacer = renderer_.pacer();
    const float targetMs = static_cast<float>(pacer.targetIntervalNs() * 1e-6);
    const float refreshMs = static_cast<float>(pacer.refreshPeriodNs() * 1e-6);
    probe_.onFrame(presentedNs, intervalMs, ph, cpuAtLoopStart_, targetMs, pacer.lastPresentId(), measuring);
    DisplayTiming timings[16];
    const u32 n = pacer.pollDisplayTimings(timings, countOf(timings));
    for (u32 i = 0; i < n; ++i) probe_.onDisplayTiming(timings[i], refreshMs);
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
    overlay_.addLine(kTitle, "RAVEFORM M1  %s BUILD  FRAME %llu", RF_BUILD_NAME, static_cast<unsigned long long>(frames_));
    {
        const GpuPassTimes& gt = renderer_.gpuTimes();
        // The stroke font has capitals only.
        const auto upper = [](char* out, size_t cap, const char* in) {
            size_t i = 0;
            for (; in[i] && i + 1 < cap; ++i) out[i] = (in[i] >= 'a' && in[i] <= 'z') ? static_cast<char>(in[i] - 32) : in[i];
            out[i] = '\0';
        };
        char tierU[16], lookU[16], paletteU[16];
        upper(tierU, sizeof tierU, tier_.name);
        upper(lookU, sizeof lookU, variant_->id);
        upper(paletteU, sizeof paletteU, palette_->id);
        overlay_.addLine(kText, "TIER %s%s  LOOK %s  PALETTE %s", tierU, governorOn_ ? " (GOVERNOR)" : "", lookU, paletteU);
        if (gt.valid) {
            overlay_.addLine(kText, "GPU %.2f MS  SCENE %.2f  POOLS %.2f  AFTERGLOW %.2f  DIST %.2f  BLOOM %.2f  COMP %.2f",
                             gt.ms[GpuPassTimes::Total], gt.ms[GpuPassTimes::Scene], gt.ms[GpuPassTimes::Pools],
                             gt.ms[GpuPassTimes::Afterglow], gt.ms[GpuPassTimes::Distortion], gt.ms[GpuPassTimes::Bloom],
                             gt.ms[GpuPassTimes::Composite]);
        }
        if (content_ == Content::Stage) {
            const StageStats& st = stage_.stats();
            overlay_.addLine(kText, "ENEMIES %u IN VIEW  BULLETS %u  GLYPHS %u  SPARKS %u  SHARDS %u", st.enemiesInView,
                             st.bulletsInView, st.glyphInstances, renderer_.liveParticles(), st.shards);
            overlay_.addLine(kText, "LIGHT BUDGET: LOAD %.0f  CROWD %.2f  SWARM %.2f", st.load, st.crowdGain, st.swarmGain);
        }
    }
    // Contract (PLAN 5, changed to P95 on 2026-11-10): warn when the P95 interval is a missed vsync,
    // i.e. more than 5% of the recent frames missed. These are present-call timestamps, which jitter by
    // a millisecond or two around the refresh period, so they are graded in refresh periods.
    overlay_.addLine(iv.p95 > target * 1.5f ? kWarn : kText, "INTERVAL P50 %.2f  P95 %.2f  P99 %.2f  MAX %.2f MS",
                     iv.p50, iv.p95, iv.p99, iv.max);
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
    // M2 audio begin
    if (band_.active()) {
        char bandLine[DevOverlay::kLineChars];
        band_.overlayLine(bandLine, sizeof bandLine);
        overlay_.addLine(kText, "%s", bandLine);
    }
    // M2 audio end
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
    const double refreshMs = p.refreshPeriodNs() * 1e-6;
    const u32 frames = bench_.intervalCount();
    const u32 missed = bench_.intervalsAbove(static_cast<float>(targetMs * 1.5));
    const u32 over50 = bench_.intervalsAbove(50.0f);
    const Summary iv = bench_.finishIntervals();
    const Summary cpu = bench_.finishCpu();
    const AudioStats a = audio_.stats();
    const PacerStats ps = p.stats();
    float displayHz = displayHz_;
#if defined(__ANDROID__)
    const float hz = android::displayRefreshHz();  // after the run: did the panel hold the requested mode?
    if (hz > 1.0f) displayHz = hz;
#endif

    // GPU pass times and stage counts over the measured window: [p50, p95, max] (sorted in place now
    // that measuring is over).
    struct Pct {
        float p50 = 0, p95 = 0, max = 0, mean = 0, min = 0;
        u32 n = 0;
    };
    const auto pct = [](Series& s) {
        Pct r;
        r.n = s.count;
        if (!s.count) return r;
        double sum = 0.0;
        for (u32 i = 0; i < s.count; ++i) sum += s.v[i];
        std::sort(s.v, s.v + s.count);
        const auto at = [&s](float q) { return s.v[static_cast<u32>(q * static_cast<float>(s.count - 1) + 0.5f)]; };
        r.p50 = at(0.5f);
        r.p95 = at(0.95f);
        r.max = s.v[s.count - 1];
        r.min = s.v[0];
        r.mean = static_cast<float>(sum / s.count);
        return r;
    };
    Pct gpu[GpuPassTimes::Count];
    for (u32 k = 0; k < GpuPassTimes::Count; ++k) gpu[k] = pct(gpu_[k]);
    const Pct inView = pct(inView_), bullets = pct(bulletsInView_), glyphs = pct(glyphCount_), sparks = pct(sparksLive_),
              capsules = pct(capsuleCount_), crowd = pct(crowdGain_), swarm = pct(swarmGain_), load = pct(load_);
    const char* const passNames[GpuPassTimes::Count] = {"scene",      "pools",     "background", "bloom", "afterglow",
                                                        "distortion", "composite", "wait",       "total"};

    // The same result twice: a summary that fits one logcat line (4068 bytes) and the full result with
    // the worst-frame table and the per-second timeline (Android: written to the app's files dir).
    const auto write = [&](JsonWriter& j, bool full) {
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
        j.num("voted_hz", p.votedHz(), 2);
        j.integer("vote_result", p.voteResult());
        j.str("vote_api", p.voteApi());
        j.num("refresh_ms", refreshMs, 3);
        j.num("requested_ms", p.requestedIntervalNs() * 1e-6, 3);
        j.num("swap_interval_ms", p.swapIntervalNs() * 1e-6, 3);
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
        j.num("p95", cpu.p95);
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
        band_.writeJson(j);  // M2 audio: "audio_engine" when the band plays
        // operator new calls in the measured window: our main thread outside Swappy (must be 0), inside
        // Swappy's calls on our thread, and on all other threads (Swappy's, the audio system's, ...).
        j.integer("heap_allocs", static_cast<i64>(mainAllocsMeasured_ - pacerAllocsMeasured_));
        j.integer("heap_allocs_in_pacer", static_cast<i64>(pacerAllocsMeasured_));
        j.integer("heap_allocs_other_threads", static_cast<i64>(allAllocsMeasured_ - mainAllocsMeasured_));
        j.integer("capsules", lastCapsules_);
        j.str("tier", tier_.name);
        j.str("look", variant_->id);
        j.str("palette", palette_->id);
        j.boolean("governor", governorOn_);
        if (gpu[GpuPassTimes::Total].n) {
            j.beginObject("gpu_ms");  // per pass: [p50, p95, max], timestamp queries (two frames late)
            j.integer("frames", gpu[GpuPassTimes::Total].n);
            for (u32 k = 0; k < GpuPassTimes::Count; ++k) {
                j.beginArray(passNames[k]);
                j.num(nullptr, gpu[k].p50, 2);
                j.num(nullptr, gpu[k].p95, 2);
                j.num(nullptr, gpu[k].max, 2);
                j.endArray();
            }
            j.endObject();
        }
        if (inView.n) {
            j.beginObject("scene_counts");  // per frame: [mean, max]
            const auto pair = [&j](const char* key, const Pct& p) {
                j.beginArray(key);
                j.num(nullptr, p.mean, 0);
                j.num(nullptr, p.max, 0);
                j.endArray();
            };
            pair("enemies_in_view", inView);
            pair("bullets_in_view", bullets);
            pair("glyph_instances", glyphs);
            pair("sparks_live", sparks);
            pair("capsules", capsules);
            pair("load", load);
            // Light budget (look.md v2.1 §3): [mean, min] per frame.
            const auto gain = [&j](const char* key, const Pct& p) {
                j.beginArray(key);
                j.num(nullptr, p.mean, 3);
                j.num(nullptr, p.min, 3);
                j.endArray();
            };
            gain("crowd_gain", crowd);
            gain("swarm_gain", swarm);
            j.integer("kills", stage_.stats().kills);
            j.integer("drops", stage_.stats().drops);
            j.integer("boss_breaks", stage_.stats().bossBreaks);
            j.integer("enemy_slots", stage_.stats().enemies);
            j.endObject();
        }
        j.boolean("overlay", overlayOn_);
        j.str("affinity", affinity_);
        j.integer("swapchain_rebuilds", renderer_.rebuildCount());
        j.boolean("validation", g.validationEnabled());
        j.integer("validation_messages", g.validationMessages());
        j.integer("loader_messages", g.loaderMessages());
        j.boolean("samples_overflowed", bench_.overflowed());
        if (ps.valid) {
            // Swappy's own display-side statistics (it owns VK_GOOGLE_display_timing in this mode).
            // A frame is missed when the panel showed it one or more refresh periods after the time
            // Swappy asked for. (Swappy floors the frame-to-frame offsets, so a 33.2 ms interval lands
            // in bucket 1 of refresh_periods: that histogram cannot count misses.)
            u64 late = 0;
            for (u32 i = 1; i < 6; ++i) late += ps.lateFrames[i];
            j.beginObject("display");
            j.str("source", "swappy_stats");
            j.integer("frames", static_cast<i64>(ps.totalFrames));
            j.integer("missed", static_cast<i64>(late));
            j.num("missed_pct",
                  ps.totalFrames ? 100.0 * static_cast<double>(late) / static_cast<double>(ps.totalFrames) : 0.0, 2);
            const auto hist = [&j](const char* key, const u64* v) {
                j.beginArray(key);  // index = refresh periods (5 = 5 or more)
                for (u32 i = 0; i < 6; ++i) j.integer(nullptr, static_cast<i64>(v[i]));
                j.endArray();
            };
            hist("refresh_periods", ps.offsetFromPrevious);
            hist("late_periods", ps.lateFrames);
            hist("idle_periods", ps.idleFrames);
            hist("latency_periods", ps.latencyFrames);
            j.endObject();
        }
        probe_.write(j, full, static_cast<float>(targetMs), static_cast<float>(refreshMs));
        j.endObject();
    };

    static char fullText[32768];
    JsonWriter jf(fullText, sizeof fullText);
    write(jf, true);
    if (jf.overflowed()) RF_ERR("bench: full JSON buffer too small");
#if defined(__ANDROID__)
    static char summaryText[4000];  // logcat payload limit 4068 bytes, minus priority and tag
    JsonWriter js(summaryText, sizeof summaryText);
    write(js, false);
    if (js.overflowed()) RF_ERR("bench: summary JSON longer than %u bytes", static_cast<unsigned>(sizeof summaryText));
    RF_LOG("bench: result %u bytes (summary line %u bytes)", static_cast<unsigned>(jf.length()),
           static_cast<unsigned>(js.length()));
    logTagged("RFBENCH", "%s", js.text());
    char path[300];
    char name[64];
    std::snprintf(name, sizeof name, "%s.json", bench_.scenario());
    resolveOutputPath(name, path, sizeof path);
    if (FILE* f = std::fopen(path, "wb")) {
        std::fprintf(f, "%s\n", jf.text());
        std::fclose(f);
    }
#else
    logTagged("RFBENCH", "%s", jf.text());  // stdout has no line limit
#endif
}

// M2 audio begin
bool App::openAudio() {
    if (band_.active()) return audio_.open(Band::render, &band_);
    return audio_.open(TestTone::render, &tone_);
}
// M2 audio end

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
