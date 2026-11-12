#pragma once
// The audio side's own scenes (M2), drawn with the capsule batch and the stroke font:
//  - audio_test: the band on heard time. A ring that flashes on heard beats, the beat dots, shapes that
//    pulse from the heard events (kick, snare, hats, bass, lead, bar, section), the lead shots on the
//    heard grid, and controls for the mix state (fire, sound, section, Track, intensity, Riffs, mode).
//  - sync: the calibration screen of design/screens.md (a 120 BPM metronome, the ring, the beat dots,
//    the offset slider with - and +, AUTO and DONE). The flash is drawn from heard time with the
//    offset being edited, so what the player sees is what the game will do.
// The stage reads the same heard clock (Stage::setBeat; docs/PROGRESS.md, M2).

#include "audio/band.h"
#include "content/look.h"
#include "content/palette.h"
#include "core/math.h"
#include "gfx/renderer.h"
#include "gfx/vector_font.h"

namespace rf {

class AudioScene {
public:
    enum class Kind : u8 { AudioTest, Sync };

    void init(Kind kind, Band& band, const VectorFont& font, const Palettes& palettes, const Palette& palette,
              const Look& look);
    void setPalette(const Palette& p) { palette_ = &p; }
    // Reduce-flashing mode (--reduce-flashing, key R; design/screens.md): the ring's flash disc at 0.2
    // instead of 0.45 and no expanding ring.
    void setReduceFlashing(bool on) { reduceFlashing_ = on; }
    // SDL keycode; true when the scene used it.
    bool key(int sdlKey, bool down);
    // Logical pixels.
    void touchDown(u64 id, Vec2 pos);
    void touchMove(u64 id, Vec2 pos);
    void touchUp(u64 id);
    void render(FrameDraw& draw, FrameParams& params, Background& bg, float w, float h, Vec2 insetMin, Vec2 insetMax);
    bool quitRequested() const { return quit_; }
    void clearQuit() { quit_ = false; }
    Kind kind() const { return kind_; }

private:
    struct Button {
        Vec2 centre;
        Vec2 size;  // pills: width, height; discs: diameter
        bool disc = false;
        bool pressed = false;
        u64 touch = 0;
        const char* label = "";
        Vec3 colour;
    };
    enum ButtonId : u32 {
        // sync
        kMinus, kPlus, kAuto, kDone,
        // audio_test
        kFire, kSound, kSection, kTrack, kIntensityDown, kIntensityUp, kRiff1, kRiff2, kRiff3, kMode, kSync, kHold,
        kButtonCount
    };

    void layout(float w, float h, Vec2 insetMin, Vec2 insetMax);
    void press(u32 id, u64 touch);
    void release(u32 id);
    void drawButton(FrameDraw& d, const Button& b, bool active) const;
    void drawText(CapsuleBatch& batch, Vec2 centreTop, float cap, Vec3 colour, float intensity, const char* text, bool centred = true) const;
    void drawRing(FrameDraw& d, Vec2 c, float radius, bool flashing) const;
    void drawBeatDots(FrameDraw& d, Vec2 c) const;
    void renderSync(FrameDraw& d);
    void renderTest(FrameDraw& d);
    void setOffsetFromX(float x);
    void enterSync();
    void leaveSync();
    u32 hit(Vec2 p) const;

    Kind kind_ = Kind::AudioTest;
    bool startedInSync_ = false;
    bool reduceFlashing_ = false;
    Band* band_ = nullptr;
    const VectorFont* font_ = nullptr;
    const Palettes* palettes_ = nullptr;
    const Palette* palette_ = nullptr;
    const Look* look_ = nullptr;
    Button buttons_[kButtonCount];
    float w_ = 0, h_ = 0, s_ = 1;  // logical size, reference-pixel scale
    Vec2 ringCentre_;
    float sliderX0_ = 0, sliderX1_ = 0, sliderY_ = 0;
    bool dragging_ = false;
    u64 dragTouch_ = 0;
    u64 fireTouch_ = 0;
    bool fireByTouch_ = false, fireByKey_ = false;
    i64 repeatNs_ = 0;     // -/+ held: next repeat
    i32 repeatDir_ = 0;
    bool quit_ = false;
    // Heard-event effects (seconds on the heard clock).
    struct Spark {
        float x, y, vx, vy, born;
        Vec3 colour;
        bool active;
    } sparks_[64];
    u32 sparkHead_ = 0;
};

}  // namespace rf
