#pragma once
// The application: SDL window and events, the frame loop, lifecycle, measurement and M0 scenarios.

#include <SDL3/SDL_events.h>

#include "audio/audio_device.h"
#include "audio/test_tone.h"
#include "core/args.h"
#include "game/m0_scene.h"
#include "gfx/renderer.h"
#include "gfx/vector_font.h"
#include "tools/bench.h"
#include "tools/dev_overlay.h"

struct SDL_Window;

namespace rf {

class App {
public:
    int run(int argc, char** argv);

private:
    static constexpr u32 kMaxTouches = 10;
    static constexpr u64 kMouseId = ~0ull;

    bool init();
    void shutdown();
    void handleEvent(const SDL_Event& e);
    // Android lifecycle arrives inside the event pump, which then blocks while paused, so it is
    // handled in an event watch on the SDL thread (TECH_SPEC 3.1).
    static bool SDLCALL eventWatch(void* user, SDL_Event* event);
    void enterBackground();
    void enterForeground();

    void frame(u64 loopStartNs);
    void afterPresent(u64 loopStartNs, u64 presentedNs, bool captured);
    void refreshOverlay(u64 nowNs);
    void refreshDisplay();
    void updateSafeArea();
    void stepLifecycleTest();
    void reportBench();
    void reportLifecycleTest();
    void resolveOutputPath(const char* in, char* out, size_t capacity) const;

    void touchDown(u64 id, Vec2 pos);
    void touchMove(u64 id, Vec2 pos);
    void touchUp(u64 id);
    Vec2 normalizedToLogical(float nx, float ny) const;
    Vec2 windowToLogical(float wx, float wy) const;
    u32 activeTouches() const;

    AppArgs args_;
    SDL_Window* window_ = nullptr;
    Renderer renderer_;
    AudioDevice audio_;
    TestTone tone_;
    VectorFont font_;
    DevOverlay overlay_;
    Bench bench_;
    FrameHistory intervals_;
    RecentSamples<240> cpu_;
    TouchPoint touches_[kMaxTouches];

    bool quit_ = false;
    int exitCode_ = 0;
    bool minimized_ = false;
    bool backgrounded_ = false;
    bool overlayOn_ = true;
    bool discontinuity_ = true;       // the next interval spans a pause: do not record it
    bool awaitingResumePresent_ = false;
    bool screenshotRequested_ = false;
    bool threeFingerLatch_ = false;
    u64 startNs_ = 0;
    u64 lastPresentNs_ = 0;
    u64 lastOverlayNs_ = 0;
    u64 pulseNs_ = 0;
    Vec2 pulseAt_;
    u64 frames_ = 0;
    u32 lastCapsules_ = 0;
    u32 backgrounds_ = 0;
    u32 foregrounds_ = 0;
    float displayHz_ = 0.0f;
    int displayW_ = 0;
    int displayH_ = 0;
    Vec2 insetMin_;
    Vec2 insetMax_;
    char device_[64] = {};
    char os_[32] = {};

    // Benchmark bookkeeping.
    i32 underrunsAtMeasure_ = 0;
    u64 allocsAtMeasure_ = 0;
    u64 allocsMeasured_ = 0;

    // lifecycle_test.
    u32 cyclesDone_ = 0;
    u64 cycleFrame_ = 0;
};

}  // namespace rf
