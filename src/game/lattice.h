#pragma once
// The warping lattice (TECH_SPEC 5.3 "Grid", design/look.md §2, §3, §7): a spring-mass grid simulated
// on the CPU only for a window around the camera (toroidal arrays: rows and columns entering the window
// start at rest). Explosions push it, wells pull it, kicks ripple it. Drawn as capsules whose colour
// and brightness grade with the displacement: the tint field at rest, warm from 5 wu, hot from 13 wu.

#include "core/math.h"
#include "core/types.h"
#include "gfx/capsule.h"

namespace rf {

struct LatticeStyle {
    Vec3 grid, gridHot, player, thirdHue;
    float restIntensity = 0.30f, restWidth = 1.1f;
    float warmIntensity = 0.42f, warmWidth = 1.15f;
    float hotIntensity = 0.85f, hotPeakIntensity = 1.2f, hotWidth = 1.3f;
    float warmFromWu = 5.0f, hotFromWu = 13.0f, warmTowardHot = 0.4f;
    float tintRadiusWu = 870.0f, tintNearPlayerMix = 0.45f, tintFarThirdHueMix = 0.3f;
    bool tint = true;  // the tint field (else the flat grid colour)
    float heatGain = 1.0f;  // light budget (look.md v2.1 §3): intensity = rest + heatGain x (graded - rest)
};

struct LatticeView {
    Vec2 camera;          // world position at the screen centre
    float pxPerWu = 1.0f;
    Vec2 screenCentre;    // logical px (shake included)
    float widthPx = 0.0f, heightPx = 0.0f;
    float refPx = 1.0f;   // logical px per reference px (the renderer applies the look's line width)
    Vec2 player;          // tint field centre
    float hatShimmer = 0.0f;  // 0..0.06: brightness +- on alternate nodes
    // Kick ripples (visual displacement): up to four rings leaving the player.
    struct Ripple {
        float radiusWu, amplitudeWu, widthWu;
    } ripples[4];
    u32 rippleCount = 0;
    // M3: the arena (GAME_DESIGN 9): lines whose rest nodes lie outside these world bounds are not drawn.
    bool bounded = false;
    Vec2 boundsMin, boundsMax;
};

class Lattice {
public:
    Lattice() = default;
    Lattice(const Lattice&) = delete;
    Lattice& operator=(const Lattice&) = delete;
    ~Lattice();

    // Sizes the window for a view of up to viewW x viewH world units. Allocates: call outside the frame.
    bool init(float spacingWu, float viewW, float viewH);
    float spacing() const { return spacing_; }

    void recentre(Vec2 camera);
    void impulse(Vec2 at, float strength, float radiusWu);
    void setWells(const Vec2* wells, u32 count, float pullWu) {
        wellCount_ = count < 4 ? count : 4;
        for (u32 i = 0; i < wellCount_; ++i) wells_[i] = wells[i];
        wellPull_ = pullWu;
    }
    void step(float dt);
    // Writes the visible segments; returns how many.
    u32 draw(CapsuleBatch& out, const LatticeView& view, const LatticeStyle& style);

private:
    u32 slot(int i, int j) const {
        const int ci = ((i % cols_) + cols_) % cols_;
        const int rj = ((j % rows_) + rows_) % rows_;
        return static_cast<u32>(rj * cols_ + ci);
    }

    float spacing_ = 20.0f;
    int cols_ = 0, rows_ = 0;
    int i0_ = 0, j0_ = 0;  // world index of the window's first column and row
    bool placed_ = false;
    Vec2* d_ = nullptr;     // displacement
    Vec2* v_ = nullptr;     // velocity
    Vec2* acc_ = nullptr;   // scratch
    Vec2* px_ = nullptr;    // scratch: drawn position (logical px)
    Vec3* col_ = nullptr;   // scratch: colour x intensity
    float* hw_ = nullptr;   // scratch: half width (ref px)
    u8* flat_ = nullptr;    // scratch: bit 0 straight through horizontally, bit 1 vertically
    Vec2 wells_[4];
    u32 wellCount_ = 0;
    float wellPull_ = 0.0f;
    float stepRemainder_ = 0.0f;
};

}  // namespace rf
