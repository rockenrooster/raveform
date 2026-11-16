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

// A well pulling the lattice (the M1 stages' wells; 2026-11-14, Singularity's well and Black Hole's singularity): at most pullWu
// of displacement at peakWu from its centre, none from goneWu.
struct LatticeWell {
    Vec2 at;
    float pullWu = 24.0f, peakWu = 60.0f, goneWu = 200.0f;
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
    // morgana.md "The storm", "The oasis" (M5 part 6's rest, 2026-11-14): Fata Morgana's wind whips the drawn lattice along it,
    // in gusts that travel downwind (up to windAmpWu); inside her oasis the lattice lies at rest and takes calmTint.
    bool wind = false;
    Vec2 windDir{1.0f, 0.0f};
    float windAmpWu = 0.0f, windPhase = 0.0f;  // the gusts' phase (radians; it grows with time)
    Vec2 calmAt;
    float calmRadiusWu = 0.0f, calmTintMix = 0.0f;
    Vec3 calmTint;
    // M8 TRACERS (design/modes/tracers.md "Look", a lit tile): the floor's tiles (tilesX x tilesY of tileWu from tileOrigin); a
    // line whose two sides lie in tiles of the same colour index (tileOwner, 0xFF dark) is drawn in tileColour[index] x tileLevel x
    // that tile's light (tileLight, 0..1), over a little of its own heat. Null: none.
    const u8* tileOwner = nullptr;
    const float* tileLight = nullptr;
    const Vec3* tileColour = nullptr;
    Vec2 tileOrigin;
    float tileWu = 40.0f, tileLevel = 0.55f;
    u32 tilesX = 0, tilesY = 0;
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
        for (u32 i = 0; i < wellCount_; ++i) wells_[i] = LatticeWell{wells[i], pullWu, 60.0f, 200.0f};
    }
    void setWells(const LatticeWell* wells, u32 count) {
        wellCount_ = count < 4 ? count : 4;
        for (u32 i = 0; i < wellCount_; ++i) wells_[i] = wells[i];
    }
    void step(float dt);
    // Writes the visible segments; returns how many.
    u32 draw(CapsuleBatch& out, const LatticeView& view, const LatticeStyle& style);
    // M4 stage B (a measure for the lead): the heat of the nodes in the middle third of the last frame drawn, split by
    // what made it: the springs (the impulses: kills, hits, Novas, the Drop's wave) or the kick ripples.
    struct HeatStats {
        u32 nodes = 0, warm = 0, hot = 0;
        float fromSprings = 0.0f, fromRipples = 0.0f;
    };
    const HeatStats& heatStats() const { return heat_; }

private:
    u32 slot(int i, int j) const {
        const int ci = ((i % cols_) + cols_) % cols_;
        const int rj = ((j % rows_) + rows_) % rows_;
        return static_cast<u32>(rj * cols_ + ci);
    }
    // The window's slots by column and by row (colSlot_[i - i0_] + rowBase_[j - j0_] == slot(i, j)), rebuilt when the window
    // has moved: slot() costs four integer divisions, and step and draw took it a dozen times a node, a few thousand nodes a
    // frame (on a Cortex-A53 a fifth of the frame). The arithmetic on the nodes is the same, in the same order.
    void ensureIndex();

    HeatStats heat_;
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
    int* colSlot_ = nullptr;  // ensureIndex(): cols_ entries
    int* rowBase_ = nullptr;  // ensureIndex(): rows_ entries
    int indexI0_ = 0, indexJ0_ = 0;
    bool indexed_ = false;
    LatticeWell wells_[4];
    u32 wellCount_ = 0;
    float stepRemainder_ = 0.0f;
};

}  // namespace rf
