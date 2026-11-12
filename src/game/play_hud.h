#pragma once
// The fight's minimal HUD and touch controls (GAME_DESIGN 2, design/look.md §6, M3 stage A): score,
// Chain, Nova stock, Dash charge, the floating twin sticks with the Nova and Dash buttons, and a bare
// "TAP TO RETRY" after death. Health is the ship's halo (drawn by the view), never a bar. Drawn after
// tone mapping (the HUD batch, look.md §7), so the colours here are tone-mapped by hand; a few soft
// pools in the scene give the lit parts their glow, as in the target frame.

#include "content/look.h"
#include "content/palette.h"
#include "gfx/renderer.h"
#include "gfx/vector_font.h"
#include "sim/player_controller.h"

namespace rf {

// The floating sticks and the two buttons (GAME_DESIGN 2): left half moves, right half aims and fires
// while deflected; Nova above the aim zone, Dash beside it. Sizes are physical (pxPerMm) where the
// platform says how large a millimetre is, else look.md §6's pixels at 1080 scaled with the height.
class TouchControls {
public:
    void layout(float w, float h, Vec2 insetMin, Vec2 insetMax, float pxPerMm);
    bool down(u64 id, Vec2 p);  // true when a control took the touch
    void move(u64 id, Vec2 p);
    void up(u64 id);
    void reset();
    void apply(sim::StickInput& in) const;  // adds what the controls hold now
    bool used() const { return used_; }      // a finger has touched the screen this session
    void draw(CapsuleBatch& hud, GlowBatch& pools, const VectorFont& font, const Look& look, const LookVariant& variant,
              Vec3 player, float dashCharge, bool novaReady) const;

private:
    struct Stick {
        bool active = false;
        u64 id = 0;
        Vec2 origin, thumb, rest;
    };
    struct Button {
        Vec2 centre;
        float radius = 0.0f;
        bool pressed = false;
        u64 id = 0;
    };
    Vec2 deflection(const Stick& s) const;  // dead zone applied, magnitude <= 1

    Stick move_, aim_;
    Button nova_, dash_;
    float stickR_ = 150.0f, padR_ = 44.0f, s_ = 1.0f, w_ = 0.0f, h_ = 0.0f;
    bool used_ = false;
};

struct HudState {
    u64 score = 0;
    u32 chain = 0;
    float chainPulse = 0.0f;  // 1 on a pickup, decaying
    u32 novas = 0, novaCap = 3;
    float dashCharge = 1.0f;
    bool alive = true;
    bool retryPending = false;
    float kick = 0.0f;
    bool showTouch = false;
};

// Tone maps a linear colour x intensity with the look's curve (look.md §3), for the HUD batch.
Vec3 hudColour(const Look& look, const LookVariant& variant, Vec3 c, float intensity);

void drawHud(FrameDraw& d, const VectorFont& font, const Look& look, const LookVariant& variant, const Palettes& palettes,
             const Palette& palette,
             const HudState& st, const TouchControls& touch, float W, float H, Vec2 insetMin, Vec2 insetMax);

}  // namespace rf
