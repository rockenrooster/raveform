#pragma once
// The application: SDL window and events, the frame loop, lifecycle, measurement and M0 scenarios.

#include <SDL3/SDL_events.h>

#include "audio/audio_device.h"
#include "audio/test_tone.h"
#include "content/glyphs.h"
#include "content/look.h"
#include "content/palette.h"
#include "core/args.h"
#include "game/glyph_sheet.h"
#include "game/m0_scene.h"
#include "game/rfscene.h"
#include "game/stage.h"
#include "game/tiers.h"
#include "gfx/renderer.h"
#include "gfx/vector_font.h"
#include "tools/bench.h"
#include "tools/dev_overlay.h"
#include "tools/frame_probe.h"

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
    void probeFrame(u64 loopStartNs, u64 prevPresentNs, u64 presentedNs, float intervalMs, bool measuring);
    void refreshOverlay(u64 nowNs);
    void refreshDisplay();
    void applyAffinity();
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
    void nextLook();
    void nextPalette();
    void applyTier(Tier t);
    void captureName(char* out, size_t capacity, u32 stripIndex) const;

    AppArgs args_;
    SDL_Window* window_ = nullptr;
    Look look_;
    const LookVariant* variant_ = nullptr;
    RfScene sceneFile_;
    bool sceneLoaded_ = false;
    Palettes palettes_;
    const Palette* palette_ = nullptr;
    GlyphSet glyphs_;
    enum class Content : u8 { M0, SceneFile, Stage, Sheet };
    Content content_ = Content::M0;
    Stage stage_;
    GlyphSheet sheet_;
    TierSettings tier_;
    Governor governor_;
    bool governorOn_ = false;
    Background background_;
    u64 lastFrameNs_ = 0;
    u32 lastLook_ = 0;
    // Per-frame GPU pass times and stage counts over the measured window (allocated before measuring).
    struct Series {
        float* v = nullptr;
        u32 count = 0, cap = 0;
        void add(float x) {
            if (count < cap) v[count++] = x;
        }
    };
    Series gpu_[GpuPassTimes::Count];
    Series inView_, bulletsInView_, glyphCount_, sparksLive_, capsuleCount_, crowdGain_, swarmGain_, load_;
    u64 gpuFrameSeen_ = ~0ull;
    float governorMissed_ = 0.0f;  // governor input: share of recent frames that missed a refresh
    bool fourFingerLatch_ = false;
    bool twoFingerLatch_ = false;
    Renderer renderer_;
    AudioDevice audio_;
    TestTone tone_;
    VectorFont font_;
    DevOverlay overlay_;
    Bench bench_;
    FrameProbe probe_;
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
    bool displayRechecked_ = false;
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
    char affinity_[32] = "os";  // CPUs the main thread is pinned to, "os" = the scheduler decides

    // Benchmark bookkeeping. Allocation counts over the measured window: the main thread (ours, plus
    // what Swappy allocates inside its calls on our thread) and every other thread.
    i32 underrunsAtMeasure_ = 0;
    u64 mainAllocsAtMeasure_ = 0;
    u64 mainAllocsMeasured_ = 0;
    u64 pacerAllocsAtMeasure_ = 0;
    u64 pacerAllocsMeasured_ = 0;
    u64 allAllocsAtMeasure_ = 0;
    u64 allAllocsMeasured_ = 0;
    // Phase boundaries of the current loop iteration (FrameProbe).
    u64 eventsDoneNs_ = 0;
    u64 audioDoneNs_ = 0;
    int cpuAtLoopStart_ = -1;

    // lifecycle_test.
    u32 cyclesDone_ = 0;
    u64 cycleFrame_ = 0;
};

}  // namespace rf
