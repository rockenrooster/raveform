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
#include "sim/world.h"

namespace rf {

class IconSet;

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
    void markUsed() { used_ = true; }        // (M6 pictures: the controls shown on the desktop)
    void setSwap(bool on) { swap_ = on; }    // left-handed: aim on the left (takes effect at the next layout)
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
    bool swap_ = false;
};

struct HudState {
    u64 score = 0;
    u32 chain = 0;
    float chainPulse = 0.0f;  // 1 on a pickup, decaying
    // Play test 2 (look.md 6): the Shard level under the Chain.
    u32 level = 0;            // LV n
    float levelFill = 0.0f;   // the meter toward the next level (0..1)
    bool levelFull = false;   // a level waits for its Trio (the bar stays full, flashing white)
    float levelFlash = 0.0f;  // 1 as a level fills, decaying
    u32 novas = 0, novaCap = 3;
    float novaFlash = 0.0f;     // 2026-11-15: 1 as a Shard level gives a Nova, decaying (the newest star flashes gold)
    u32 extraLives = 0;         // play test 3 (screens.md "Extra Life (HUD)"): each held (up to two since 2026-11-14) a small gold ship right of the Novas
    float bossFlash = 0.0f;     // play test 3 (GAME_DESIGN 7.6): a weak point's hit: its segment of the boss's bar flashes
    bool reduceFlashing = false;    // M6: the HUD's own flashes (a boss hit's bar segment) at half
    float setCompleteBars = -1.0f;  // M6: bars since the set's last boss fell (SET COMPLETE for two; < 0: none)
    u64 setCompleteScore = 0;
    const char* completeWords = "SET COMPLETE";  // M8: TRACK COMPLETE for ONE TRACK's outro
    float completeBars = 2.0f;                   // ... standing this many bars
    const char* modeWords = nullptr;             // M8: the mode's words under the name card's name (BEAT ITS BOSS)
    const char* cardSmall = nullptr;             // M8 HEADLINERS: ACT 3 OF 10 in place of TRACK n ...
    const char* cardName = nullptr;              // ... and the boss's name in place of the Track's
    const char* completeLine = nullptr;          // M8 HEADLINERS: the time under HEADLINERS COMPLETE in place of the score
    const char* goalLine = nullptr;              // M8 (design/modes/screens.md "The goal line"): ACT 3 OF 10   4:12
    // M8 TRACERS (tracers.md "HUD"): the Shard level's bar labelled TRACER 9 BEATS; no Riff slots (the Boost cards a row lower);
    // the gels' cards beside the Boosts' (an upgrade's icon, its name and words, the bars it has left).
    const char* levelLabel = nullptr;
    bool noRiffs = false;
    bool turf = false;  // TRACERS versus: the floor's shares as a bar across the top
    struct GelCard {
        u8 icon = 0;  // an upgrade (its icon)
        const char* name = nullptr;
        const char* words = nullptr;
        double from = 0.0, until = 0.0;
    } gels[2];
    u32 gelCount = 0;
    u8 bossFlashPart = 0xFF;    // ... the part (Kaleidos's pair, a Longwave segment, a Voice; 0xFF the whole bar)
    float dashCharge = 1.0f;
    bool alive = true;
    bool retryPending = false;
    float beat = 0.0f;  // the clock's beat envelope (M5: "on the beat", never the kick)
    bool showTouch = false;
    // M3 stage B (look.md 6): Track and tempo, the phrase bar, the pause ring, the band row (Riff slots and
    // Mods), active Boosts, the boss's bar and name card, a debut's name, the Trio's hint.
    const sim::World* world = nullptr;
    double music = 0.0;          // the rendered musical position
    const IconSet* icons = nullptr;
    const i32* upgradeIcons = nullptr;
    const i32* boostIcons = nullptr;
    const i32* fusionIcons = nullptr;  // (the review's fixes, M20, 2026-11-15)
    const char* trackName = "";
    float bpm = 120.0f;
    Vec2 takenFrom;              // where the orb just taken was on screen
    float takenAge = -1.0f;      // seconds since it was taken (< 0: none)
    const char* spotlight = nullptr;  // a debut's name
    float spotlightAge = -1.0f;
    bool runHud = true;          // the run's HUD (off on the title and the game-over screen)
    bool pausePressed = false;
    // M4 stage B (screens.md, look.md 6): in co-op the score is the team's; each player's own sits under it in their
    // colour, ranked.
    u32 coopPlayers = 0;         // players in the run (> 1: co-op)
    u64 playerScore[4] = {};
    u8 rank[4] = {};             // seats, the highest score first
    u8 rankCount = 0;
    u8 me = 0;                   // this device's seat
    bool versus = false;         // M8 versus: the score is this player's own; the ranking carries the crown and the underdog
    u8 crown = 0xFF, underdog = 0xFF;
    // M5 (screens.md "Boss hint"): the boss's rule in plain words, under its name; 0..1.
    const char* bossHint = nullptr;
    float bossHintLevel = 0.0f;
};

// The boss hint line (screens.md "Boss hint (HUD, M5)"); nullptr for a fight without one.
const char* bossHint(sim::BossKind k);

// The review's fixes (H2, 2026-11-15): the Nova stock's star pitch (50 px, closed up so the row never meets "DROP IN n"); s the
// HUD's scale, mr its right margin.
float novaStarPitch(float W, float s, float mr, u32 cap);
// The pause ring's centre and radius (top right corner, look.md 6), for its touch target.
Vec2 pauseButtonCentre(float W, float H, Vec2 insetMin, Vec2 insetMax, float& radius);
// M6 (SETTINGS, LOOK): the UI scale of HUD text and touch controls (0.9, 1, 1.15, 1.3); re-lay the touch controls after it.
void setHudScale(float scale);
float hudScale();
// The review's fixes (H2, 2026-11-15): the screens and the HUD are laid out in 2400 x 1080 reference px on a 2,222-wide canvas round
// the centre; one scale fits both the height and that width, min(H / 1080, W / 2222), wherever a layout scale is made (the 20:9
// phone, 2400 x 1080, keeps 1: pixel-identical). A wider screen than 20:9 keeps the height's.
inline float layoutScale(float W, float H) {
    const float sh = H / 1080.0f, sw = W / 2222.0f;
    return sw < sh ? sw : sh;
}

// Tone maps a linear colour x intensity with the look's curve (look.md §3), for the HUD batch.
Vec3 hudColour(const Look& look, const LookVariant& variant, Vec3 c, float intensity);
// M6 (screens.md "Transitions"): everything hudColour gives (the HUD and the screens) x this level while it is set, so a
// screen's pieces fade in and out (the HUD batch is additive); 1 is the normal.
void setHudFade(float level);
float hudFade();

void drawHud(FrameDraw& d, const VectorFont& font, const Look& look, const LookVariant& variant, const Palettes& palettes,
             const Palette& palette,
             const HudState& st, const TouchControls& touch, float W, float H, Vec2 insetMin, Vec2 insetMax);
void drawRunHud(FrameDraw& d, const VectorFont& font, const Look& look, const LookVariant& variant, const Palettes& palettes,
                const Palette& palette, const HudState& st, float W, float H, Vec2 insetMin, Vec2 insetMax);

}  // namespace rf
