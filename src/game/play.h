#pragma once
// The fight (M3 stage A): the simulation (src/sim) on the band's heard clock, played with touch sticks,
// gamepad, keyboard and mouse, or by the bot; the view draws it and the band plays it. It owns the
// fixed-step loop (TECH_SPEC 4: 60 Hz ticks from an accumulator, at most three a frame, drawn
// interpolated), the effect interface to the band (lead rule, kills, Shards, Nova, warp-ins, telegraphs,
// hits) and the scripted picture runs for the lead's review (--picture).
//
// Scenarios (core/args.h): "" or "play" (the game; the default with no scenario), "bench_play" (the bot
// plays, measured like a benchmark) and "bench_stress" (about 700 enemies and 1,500 enemy bullets around
// a bot-driven ship). The simple spawner of sim/world_spawner.cpp stands in for the director (stage B).

#include "audio/band.h"
#include "content/glyphs.h"
#include "content/icons.h"
#include "content/look.h"
#include "content/palette.h"
#include "game/music_clock.h"
#include "game/play_hud.h"
#include "game/play_view.h"
#include "game/screens.h"
#include "game/settings.h"
#include "game/tiers.h"
#include "gfx/renderer.h"
#include "gfx/vector_font.h"
#include "sim/bot.h"
#include "sim/player_controller.h"
#include "sim/species.h"
#include "sim/world.h"

namespace rf {

struct PlayOptions {
    enum class Kind : u8 { Play, BenchPlay, Stress };
    Kind kind = Kind::Play;
    bool bot = false;           // --bot: the bot plays through the same input interface
    bool fixedStep = false;     // pictures: exactly one tick per frame, the simulated clock
    u64 seed = 1;
    float fastForwardS = 0.0f;  // --fast-forward S: run S seconds of the game before the first frame
    char picture[32] = {};      // --picture <name>: a scripted scene for the review strips
    float pxPerMm = 0.0f;       // touch control sizes (0: scale with the screen height)
    bool autoplay = false;      // --autoplay: the screens as in play, the bot playing each run (taps start and retry)
    float recklessAfter = 0.0f; // --reckless-after S
};

class Play {
public:
    Play() = default;
    Play(const Play&) = delete;
    Play& operator=(const Play&) = delete;
    ~Play();

    bool init(const PlayOptions& opt, const Look& look, const Palettes& palettes, const Palette& palette, const GlyphSet& glyphs,
              const VectorFont& font, const TierSettings& tier, Band* band);
    void setTier(const TierSettings& tier) { view_.setTier(tier); }
    void setDensity(float d) { view_.setDensity(d); }
    void setReduceFlashing(bool on) {
        forcedReduceFlashing_ = on;
        view_.setReduceFlashing(on || settings_.reduceFlashing);
    }
    void setPalette(const Palette& p) {
        palette_ = &p;
        basePalette_ = &p;
        view_.setPalette(p);
    }
    // M3 stage B: the palette follows the Track's song (Ultraviolet, Solar), cross-fading in the Track-change
    // Break; off when --palette chose one.
    void setPaletteFollowsTrack(bool on) { paletteFollows_ = on; }
    void setMute(u32 bits) { view_.setMute(bits); }
    void setLayout(float w, float h, Vec2 insetMin, Vec2 insetMax);

    // Once per frame: input -> controller -> ticks -> events (audio, view). realDt in seconds.
    void frame(float realDt);
    void render(FrameDraw& d, FrameParams& params, Background& bg, const LookVariant& variant, float W, float H, Vec2 insetMin,
                Vec2 insetMax);

    // Input (logical pixels). Keys are SDL keycodes; returns true when the game used it.
    bool key(int sdlKey, bool down);
    void mouseButton(int button, bool down, Vec2 pos);
    void mouseMove(Vec2 pos);
    void touchDown(u64 id, Vec2 pos);
    void touchMove(u64 id, Vec2 pos);
    void touchUp(u64 id);
    void padAxis(int axis, float value);  // SDL_GamepadAxis, -1..1 (triggers 0..1)
    void padButton(int button, bool down);
    void padRemoved();

    // Measurements (benchmarks, the overlay).
    struct Stats {
        u32 ticksLastFrame = 0;
        float tickMs[4] = {};  // each tick of the last frame (main thread, wall time)
        u32 enemies = 0, bullets = 0, shots = 0, shards = 0;
        u32 kills = 0, deaths = 0, restarts = 0;
        u64 score = 0;
        u32 chain = 0, bestChain = 0;
        u32 overflow = 0;
        bool finite = true;
        u64 ticks = 0;
    };
    const Stats& stats() const { return stats_; }
    const ViewStats& viewStats() const { return view_.stats(); }
    const sim::World& world() const { return *world_; }

    // Picture runs: true on the frames to capture; index() names them (0..strip-1); done() quits.
    bool captureNow() const { return captureNow_; }
    u32 captureIndex() const { return captureIndex_; }
    bool pictureDone() const { return pictureDone_; }
    void setStrip(u32 frames) { stripFrames_ = frames ? frames : 1; }
    const char* pictureName() const { return opt_.picture; }

    // M3 stage B: the game's screens (design/screens.md) around the run. Interactive play only; benchmarks,
    // pictures and --bot runs are the run alone.
    enum class Screen : u8 { Notice, Title, Run, Paused, GameOver, Settings };
    Screen screen() const { return screen_; }
    bool flows() const { return flow_; }
    bool runStarted() const { return !flow_ || (screen_ != Screen::Notice && screen_ != Screen::Title && !(screen_ == Screen::Settings && settingsBack_ == Screen::Title)); }
    bool takeSyncRequest() {
        const bool r = syncRequest_;
        syncRequest_ = false;
        return r;
    }
    void returnFromSync();
    i32 takeTierRequest() {
        const i32 r = tierRequest_;
        tierRequest_ = -1;
        return r;
    }
    const Settings& settings() const { return settings_; }
    // The first-launch tier probe runs behind the title: its stage is drawn, this draws the title over it.
    void renderProbeTitle(FrameDraw& d, FrameParams& params, const LookVariant& variant, float W, float H, Vec2 insetMin, Vec2 insetMax);
    // Pictures (scripted screens): show a screen as it is in the game.
    void showScreen(Screen s) { screen_ = s; }

private:
    // screens.cpp
    void renderScreens(FrameDraw& d, FrameParams& params, const LookVariant& variant, float W, float H, Vec2 insetMin, Vec2 insetMax);
    void screenNotice(Ui& ui);
    void screenTitle(Ui& ui);
    void screenPause(Ui& ui);
    void screenGameOver(Ui& ui);
    void screenSettings(Ui& ui);
    // play.cpp: the flow.
    void startRun();
    void restartRun();
    void toTitle();
    void pauseRun();
    void beginCountIn();
    double countInNow() const;
    void endRun(bool died);
    void openSettings(Screen back);
    void applySettings();
    bool attract() const { return flow_ && (screen_ == Screen::Notice || screen_ == Screen::Title || (screen_ == Screen::Settings && settingsBack_ == Screen::Title)); }
    void uiPointer(Vec2 pos, bool down);
    void buildInput(sim::StickInput& in);
    void runTick(double music0, double music1, bool firstOfFrame);
    void afterTick();
    void bandFrame();
    void conductBand();
    void scriptSetup();
    void scriptInput(sim::StickInput& in);
    void scriptEvents();
    void pictureFrame();
    // play_pictures.cpp (M3 stage B): true when the picture is one of stage B's.
    bool scriptSetupB();
    bool scriptInputB(sim::StickInput& in);
    bool scriptEventsB();
    float scriptFastForwardS_ = 0.0f;

    PlayOptions opt_;
    const Look* look_ = nullptr;
    const Palettes* palettes_ = nullptr;
    const Palette* palette_ = nullptr;
    const GlyphSet* glyphs_ = nullptr;
    const VectorFont* font_ = nullptr;
    Band* band_ = nullptr;
    bool bandDrives_ = false;  // the game owns the band's mix (not --band full / off / pictures)

    sim::SpeciesTable* species_ = nullptr;
    sim::World* world_ = nullptr;
    sim::PlayerController controller_;
    sim::Bot bot_;
    MusicClock clock_;
    PlayView view_;
    TouchControls touch_;
    Stats stats_;

    double tickEnd_ = 0.0;     // sim time at the end of the newest tick
    double music1_ = 0.0, musicPrev_ = 0.0;  // musical position at the end of the newest and the one before
    float alpha_ = 1.0f;
    double hitStopUntil_ = -1.0;  // sim time: a boss's death holds the action 50 ms (look.md §5), not the song
    float frameDt_ = 1.0f / 60.0f;
    bool retryRequest_ = false;

    // Devices.
    bool keys_[8] = {};          // W A S D, up left down right
    bool mouseFire_ = false, mouseNova_ = false, keyNova_ = false, keyDash_ = false;
    Vec2 mousePos_;
    bool mouseSeen_ = false;
    Vec2 padMove_, padAim_;
    bool padNova_ = false, padDash_ = false;

    // The band bridge.
    float intensity_ = 0.15f;
    float intensityPosted_ = -1.0f;
    u32 bandShots_ = 0;  // lead-rule shots the band reported (diagnostics)
    u32 killsPending_[3] = {};
    u32 shardsPending_ = 0;
    double lastShardTick_ = -1e9;
    double lastWarpTick_ = -1e9, lastTellTick_ = -1e9;
    double plannedLine_ = -1e9;  // the bar line the band was last told about
    bool overHeld_ = false, bossPosted_ = false, holdPosted_ = true, paused_ = false;
    u32 loadoutSig_ = ~0u;
    float postedBpm_ = 120.0f;

    // Pictures.
    u64 frameNo_ = 0;
    i64 captureFrom_ = -1;
    u32 stripFrames_ = 8;
    bool captureNow_ = false, pictureDone_ = false;
    u32 captureIndex_ = 0;
    u32 scriptTick_ = 0;
    u32 scriptFlags_ = 0;

    // M3 stage B: icons, screens, settings, the run's record.
    IconSet icons_;
    i32 upgradeIcons_[sim::kUpgradeCount] = {};
    i32 boostIcons_[sim::kBoostCount] = {};
    bool flow_ = false;
    Screen screen_ = Screen::Run, settingsBack_ = Screen::Title;
    u8 settingsTab_ = 0;
    Settings settings_;
    HighScores scores_;
    Ui ui_;
    UiPointer uiPtr_;
    u64 uiTouch_ = ~0ull;
    bool keyPrimary_ = false, keyBack_ = false;
    bool forcedReduceFlashing_ = false;
    const Palette* basePalette_ = nullptr;
    Palette mixed_;
    bool paletteFollows_ = true;
    void updatePalette(double music);
    bool realign_ = false;
    float layoutW_ = 2400.0f, layoutH_ = 1080.0f;
    Vec2 insetMin_, insetMax_;
    bool syncRequest_ = false;
    i32 tierRequest_ = -1;
    static constexpr u32 kScopeN = 750;  // about a bar of the master bus at 120 BPM
    float scope_[kScopeN] = {};
    float titleShipAngle_ = -1.5708f;
    bool pausePressed_ = false;
    // Pause and the count-in (band ticks, or the simulated clock's seconds without a band).
    bool countIn_ = false;
    double pauseMusic_ = 0.0, countInEnd_ = 0.0, resumeOffset_ = 0.0;
    double countInClockS_ = 0.0;
    bool askRestart_ = false, askQuit_ = false;
    // The run: its time, Tracks, the set list's marks, the game-over numbers.
    float runSeconds_ = 0.0f;
    u32 runTrackMax_ = 1;
    struct Mark {
        float t;   // seconds into the run
        u8 kind;   // 0 a Track began (a = Track), 1 a boss fell, 2 an upgrade, 3 a hit
        u8 a;
    };
    static constexpr u32 kMarks = 256;
    Mark marks_[kMarks];
    u32 markCount_ = 0;
    float overS_ = -1.0f;
    u64 finalScore_ = 0;
    bool newBest_ = false;
    // HUD moments.
    char spotlightName_[32] = {};
    float spotlightT_ = -100.0f;
    float takenT_ = -100.0f;
    Vec2 takenFrom_;
};

}  // namespace rf
