#pragma once
// The screens outside the run (design/screens.md, M3): the first-launch notice, the title, pause, game
// over and settings. They are drawn with the game's light on the HUD layer (after tone mapping, colours
// tone-mapped by hand) with a few soft pools in the scene, over the run (or the title's attract run),
// which the game dims. `Ui` is a small immediate-mode helper: pills, segmented pills, faders, cards and
// text, with the pressed state of look.md 6; the screens themselves are Play's (game/screens.cpp).

#include "content/look.h"
#include "gfx/renderer.h"
#include "gfx/vector_font.h"

namespace rf {

struct UiPointer {
    Vec2 pos;
    bool down = false;      // held now
    bool pressed = false;   // went down this frame
    bool released = false;  // went up this frame
};

class Ui {
public:
    void begin(FrameDraw& d, const VectorFont& font, const Look& look, const LookVariant& variant, float W, float H, Vec2 insetMin,
               Vec2 insetMax, const UiPointer& p, Vec3 player, Vec3 accent);
    void end();
    float s() const { return s_; }
    float W() const { return W_; }
    float H() const { return H_; }
    Vec2 insetMin() const { return insetMin_; }
    Vec2 insetMax() const { return insetMax_; }
    Vec3 col(Vec3 c, float i) const;
    FrameDraw& draw() { return *d_; }
    const VectorFont& font() const { return *font_; }

    // Text: `at` is the baseline at `align` (0 left, 0.5 centre, 1 right). Returns the width.
    float text(Vec2 at, float cap, Vec3 c, float i, const char* t, float align = 0.5f, float halfWidth = 0.0f);
    // A headline tube (screens.md: the stroke at 6% of its cap height in a colour field, a white copy a
    // quarter as wide on top).
    void tube(Vec2 at, float cap, Vec3 top, Vec3 mid, Vec3 bottom, float i, const char* t, float align = 0.5f);
    float measure(float cap, const char* t) const;
    void ring(Vec2 c, float r, float hw, Vec3 c0, float i, u32 segs = 40);
    void roundRect(Vec2 a, Vec2 b, float r, float hw, Vec3 c, float i);
    void dot(Vec2 c, float r, Vec3 colour, float i);

    // A pill control (screens.md): outline 2 px at 0.9, label at 1.1; pressed: glass in its colour at
    // 0.45, outline 3 px at 2.0, label at 1.8, a pool behind. `primary` carries the player colour. Returns
    // true when tapped (released over it after pressing on it).
    bool pill(u32 id, Vec2 centre, Vec2 size, const char* label, float cap, bool primary, bool enabled = true, bool selected = false,
              Vec3 colour = Vec3{});
    // A row of segmented pills; returns the index tapped, or `current`.
    u32 segmented(u32 id, Vec2 centre, float width, float height, const char* const* labels, u32 n, u32 current, float cap);
    // A fader with a meter track of forty bars (screens.md): returns the new value.
    float fader(u32 id, Vec2 left, float width, float value, float meter, const char* label);
    // A tappable rectangle (cards); `pressed` says whether it is held now.
    bool area(u32 id, Vec2 a, Vec2 b, bool& pressed);

    // M4 stage B (screens.md, "Rules for every screen"): controls never touch: at least 24 px between any two, and
    // between a control and a line of text. Every control and every line drawn between begin() and end() is kept
    // (a control's own label and the lines inside a control are not lines of their own); end() reports a pair that
    // breaks the rule once, as a warning naming the screen (the review scripts look for it).
    void setScreenName(const char* name) { screen_ = name; }
    static constexpr float kControlGapPx = 24.0f;
    u32 layoutWarnings() const { return layoutWarnings_; }

private:
    bool hit(u32 id, Vec2 a, Vec2 b, bool& pressed);
    void keep(Vec2 a, Vec2 b, bool control, const char* label);
    void checkLayout();
    bool inside(Vec2 a, Vec2 b) const { return p_.pos.x >= a.x && p_.pos.x <= b.x && p_.pos.y >= a.y && p_.pos.y <= b.y; }
    struct Box {
        Vec2 a, b;
        bool control;
        char label[24];
    };
    static constexpr u32 kBoxes = 96;
    Box boxes_[kBoxes];
    u32 boxCount_ = 0;
    u32 quiet_ = 0;  // > 0: drawing inside a control (its label, a segmented row's pills): nothing kept
    const char* screen_ = "";
    u32 reported_[64] = {};
    u32 reportedCount_ = 0, layoutWarnings_ = 0;
    FrameDraw* d_ = nullptr;
    const VectorFont* font_ = nullptr;
    const Look* look_ = nullptr;
    const LookVariant* variant_ = nullptr;
    float W_ = 0, H_ = 0, s_ = 1;
    Vec2 insetMin_, insetMax_;
    UiPointer p_;
    Vec3 player_, accent_;
    u32 active_ = 0;     // the control the pointer went down on
    u32 dragging_ = 0;   // a fader being dragged
};

}  // namespace rf
