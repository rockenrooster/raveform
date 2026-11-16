// M6 (design/screens.md "Transitions", b12310b; the owner: going from the title into the first Track and from game over back
// to the title "are a little jarring and sudden"): nothing cuts.
//
// The drop-in: PLAY (RETRY, a co-op START or RETRY) is answered at once and the run starts on the first bar line at least two
// beats away. Over that wait the screen leaves (the title's side controls on the first beat, the tagline and beat dots on the
// last, the attract run dissolving; a game-over screen in a beat) while the band's low-pass closes under a riser and the drums
// drop out for the last beat. On the downbeat the run arrives: the logo streaks out, the waveform becomes a ring of light from
// the ship and an impulse in the lattice, PLAY shrinks into the halo, the title's ship glides to the centre, a crash and
// Track 1's first Groove; the HUD comes in on beat 2. Home: the game-over screen leaves in a beat, the title builds itself
// over the next bar on Track 1's palette and bed, and its run fades up over the bar after.
//
// Title v2 (screens.md "Title", 2026-11-14): the title builds itself on a cold start (the intro: a point of light, the rule thrown
// out of it, the word played a letter a sixteenth) and on home (the same from the rule's throw); the drop gathers all the title's
// light into one point on the bar line (the word falls into its rule, the rule draws in to the frame's centre, PLAY shrinks into
// it) and the downbeat bursts it (a clamped flash, eight rays in the letters' colours, the ship drawn there).
//
// The band's pieces: its low-pass is the pause's (SetMuffle with this file's cutoff and ramp), the riser the Comet's whoosh
// (Recipe::Whoosh, stretched to end on the bar line), the drop-out the fights' CutBed on the drum buses, the crash the Drop's
// impact (Recipe::Impact). Nothing in src/audio changed.

#include <cmath>

#include "audio/band.h"
#include "audio/command_sink.h"
#include "audio/cues.h"
#include "audio/engine.h"
#include "core/log.h"
#include "game/play.h"
#include "net/session.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
constexpr float kTwoPi = 6.28318530718f;
float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
float smooth01(float x) {
    x = clamp01(x);
    return x * x * (3.0f - 2.0f * x);
}
Vec2 polarV(float a, float r) { return Vec2{std::cos(a) * r, std::sin(a) * r}; }
// (chosen) The drop-in's wait closes the band's low-pass to this; home's leaving screen holds the bed behind this one.
constexpr float kWaitLowpassHz = 700.0f;
constexpr float kHomeLowpassHz = 900.0f;
// (chosen) The halo's radius on screen at 1080 lines, about.
constexpr float kHaloRefPx = 46.0f;
float easeOutBack(float p) {  // (6% past and back: the fan snapping open)
    p = clamp01(p);
    const float q = p - 1.0f, k = 1.3f;
    return 1.0f + (k + 1.0f) * q * q * q + k * q * q;
}
// screens.md "Title", made "much faster" (the owner, 2026-11-14): the point a beat before the rule's throw; the word from b 1 to
// 2.75, a sixteenth a letter; PLAY takes a tap from b 3; the run behind comes up over b 4 to 8. The flash lasts 120 ms.
constexpr float kIntroPointBeats = 1.0f, kWordFrom = 1.0f, kWordStep = 0.25f, kControlsFrom = 3.0f, kBuildDone = 4.0f;
constexpr float kFlashS = 0.12f;
constexpr float kIntroWaitMaxS = 1.0f;  // (a cold start waits this long for the band at most)
constexpr float kRayFrom = 60.0f, kRayTo = 1360.0f;  // the downbeat's rays, px at 1080 lines, over half a beat
// (chosen; screens.md "Title") The title at rest leaves after this many bars without an input (17.5 s at Track 1's 110 BPM),
// on the next bar line.
constexpr double kTitleQuietBars = 8.0;
}  // namespace

double Play::transNow() const {
    if (coop() && session_ && session_->songValid()) return netSongRun_;  // (not held by a co-op pause: LEAVE RUN's home runs on)
    return musicPrev_ + (music1_ - musicPrev_) * static_cast<double>(alpha_);
}

void Play::bandFilter(float hz, float seconds) {
    transFilter_ = hz > 0.0f;
    if (!bandDrives_ || !band_ || !band_->active()) return;
    audio::Command k;
    k.type = audio::CmdType::SetMuffle;
    k.when = audio::Timing::Now;
    k.x = hz;
    k.y = seconds > 0.0f ? seconds : 0.01f;
    static_cast<audio::CommandSink&>(*band_).send(k);
}

void Play::transGod(bool hold) {
    // (the idle ship of a run being left is kept out of harm until the next run: no death, no game-over sound under the wait)
    if (hold && !transGodHeld_) {
        transGodWas_ = world_->rules().godMode;
        world_->rules().godMode = true;
        transGodHeld_ = true;
    } else if (!hold && transGodHeld_) {
        world_->rules().godMode = transGodWas_;
        transGodHeld_ = false;
    }
}

void Play::transitionAudio() {
    // The drop-in's wait in the band: the low-pass closing over it, a riser ending on the bar line, the drums out for the last
    // beat, a crash on the downbeat (the run's first Groove is conductBand's, from its first kick).
    if (!bandDrives_ || !band_ || !band_->active() || !band_->view().running) return;
    const double offset = coop() ? netBandOffset_ : clock_.offset();
    const double bpm = band_->view().bpm > 1.0 ? static_cast<double>(band_->view().bpm) : 120.0;
    const float beatS = static_cast<float>(60.0 / bpm);
    const double wait = std::fmax((transLine_ - transNow()) / kTicksPerBeat, 0.5);
    // (the pause's RESTART: the pause's filter lifts over the wait instead, c9218dc)
    bandFilter(transFrom_ == Screen::Paused ? 0.0f : kWaitLowpassHz, static_cast<float>(wait) * beatS);
    const double riser = std::fmin(wait, 4.0);
    band_->playRecipe(audio::Recipe::Whoosh, -1, 1.0f, 0.0f, transLine_ - riser * kTicksPerBeat + offset, static_cast<float>(riser));
    audio::Command k;
    k.type = audio::CmdType::CutBed;
    k.when = audio::Timing::AtTick;
    k.tick = transLine_ - kTicksPerBeat + offset;
    k.b = 1;  // the hats too
    const u16 drums = static_cast<u16>((1u << static_cast<u32>(audio::Bus::Drums)) | (1u << static_cast<u32>(audio::Bus::HatsBreak)) |
                                       (1u << static_cast<u32>(audio::Bus::Clap)) | (1u << static_cast<u32>(audio::Bus::Snare)));
    k.mods[0] = static_cast<u8>(drums & 255u);
    k.mods[1] = static_cast<u8>(drums >> 8);
    k.y = beatS;
    static_cast<audio::CommandSink&>(*band_).send(k);
    band_->playRecipe(audio::Recipe::Impact, -1, 1.0f, 0.0f, transLine_ + offset);
}

void Play::dropInTap(Screen from) {
    if (trans_ == Trans::Wait || trans_ == Trans::Arrive) return;  // (a second tap on the same control does nothing)
    // The levels the screen leaves from: a tap while home is still building the title starts from where the title is.
    waitFrom_ = from == Screen::Title ? titleLook() : TitleLook{};
    waitRun_ = transRunLight();
    if (from == Screen::Paused) {
        // c9218dc: the pause's RESTART is RETRY's drop-in; the run goes on, unseen, to the bar line (its line from the clock as
        // it comes back from the pause).
        leavePause();
        world_->requestRestartAt((std::floor((transNow() + 2.0 * kTicksPerBeat) / kTicksPerBar) + 1.0) * kTicksPerBar);
    }
    if (!world_->restartPending()) world_->requestRestart();  // (co-op: the host's START or RETRY scheduled it)
    trans_ = Trans::Wait;
    transFrom_ = from;
    backAt_ = -1.0;
    homeFadeLine_ = -1.0;
    transTap_ = transNow();
    transLine_ = world_->restartTick();
    transBeat_ = (std::floor(transTap_ / kTicksPerBeat) + 1.0) * kTicksPerBeat;  // a game-over screen leaves on the next beat
    {
        // The control that was tapped (it shrinks into the point in the wait's last half beat; screens.md "Transitions").
        const float s = ui_.s(), W = ui_.W(), H = ui_.H(), right = W - std::fmax(132.0f * s, ui_.insetMax().x);
        switch (from) {
            case Screen::GameOver:
                dropCtlC_ = Vec2{right - 300.0f * s, H - (coop() ? 270.0f : 240.0f) * s};
                dropCtlHalf_ = Vec2{300.0f * s, 70.0f * s};
                break;
            case Screen::Paused:
                dropCtlC_ = Vec2{0.5f * W - 200.0f * s, 700.0f * s};
                dropCtlHalf_ = Vec2{160.0f * s, 50.0f * s};
                break;
            case Screen::CoopLobby:
                dropCtlC_ = Vec2{0.5f * W, 970.0f * s};
                dropCtlHalf_ = Vec2{300.0f * s, 65.0f * s};
                break;
            case Screen::Modes:  // M8: MODES's PLAY
                dropCtlC_ = Vec2{0.5f * W, 968.0f * s};
                dropCtlHalf_ = Vec2{300.0f * s, 65.0f * s};
                break;
            default:
                dropCtlC_ = Vec2{0.5f * W, 800.0f * s};
                dropCtlHalf_ = Vec2{320.0f * s, 75.0f * s};
                break;
        }
    }
    if ((from == Screen::Title || from == Screen::Paused || from == Screen::Modes) && !coop()) transGod(true);
    if (band_) band_->playSfx(audio::Sfx::UiConfirm);
    pulse(0, true);
    transitionAudio();
    RF_LOG("play: the drop-in: tapped at %.0f, the run's first bar line %.0f (%.1f beats)", transTap_, transLine_,
           (transLine_ - transTap_) / kTicksPerBeat);
}

void Play::dropInCancel() {
    // Back in the wait: PLAY lets go, everything fades back in over a beat, the filter opens (the riser is a one-shot sample,
    // which the band cannot stop: it ends under the open filter).
    if (trans_ != Trans::Wait || (transFrom_ != Screen::Title && transFrom_ != Screen::Modes) || coop()) return;
    if (transFrom_ == Screen::Title) backFrom_ = titleLook();
    backRun_ = transRunLight();
    world_->cancelRestart();
    world_->setNextSetup(RunSetup{});  // (M8: the title's demo is the set's run, whatever PLAY would have started)
    trans_ = Trans::None;
    if (transFrom_ == Screen::Modes) {
        modesBackAt_ = transNow();  // (MODES comes back over a beat; its Track is heard again)
        modesPreview();
    } else {
        backAt_ = transNow();
    }
    transGod(false);
    const double bpm = band_ && band_->view().bpm > 1.0 ? static_cast<double>(band_->view().bpm) : 120.0;
    bandFilter(0.0f, static_cast<float>(60.0 / bpm));
    if (bandDrives_ && band_ && band_->active()) {
        audio::Command k;  // the drums' drop-out lifted before it begins
        k.type = audio::CmdType::CutBed;
        k.when = audio::Timing::Now;
        k.y = 0.0f;
        static_cast<audio::CommandSink&>(*band_).send(k);
        band_->playSfx(audio::Sfx::UiBack);
    }
    RF_LOG("play: the drop-in was cancelled");
}

void Play::dropInLands(double line) {
    // The run's first bar line: the run arrives (its world restarted on this tick).
    if (trans_ != Trans::Wait) return;
    trans_ = Trans::Arrive;
    transLine_ = line;
    screen_ = Screen::Run;
    transGod(false);
    bandFilter(0.0f, 0.02f);
    view_.dropRing(controller_.pose().pos, tickEnd_);
    applySettings();
}

void Play::homeTap() {
    if (trans_ == Trans::Home) return;
    transFrom_ = screen_ == Screen::Paused ? Screen::Paused : Screen::GameOver;
    homeFromExposure_ = transFrom_ == Screen::Paused || coop() ? 0.2f : 0.45f;
    if (coop()) {
        // c9218dc: co-op's HOME and LEAVE RUN: the screen leaves in its beat while the session still runs; then the session ends
        // and the title builds from the next bar line of this device's own song (homeLeaveCoop, from transitionFrame).
        trans_ = Trans::Home;
        askQuit_ = false;
        backAt_ = -1.0;
        transTap_ = transNow();
        transBeat_ = (std::floor(transTap_ / kTicksPerBeat) + 1.0) * kTicksPerBeat;
        transLine_ = -1.0;
        homeFrom_ = *palette_;
        if (band_) band_->playSfx(audio::Sfx::UiBack);
        pulse(0, true);
        const double bpm = band_ && band_->view().bpm > 1.0 ? static_cast<double>(band_->view().bpm) : 120.0;
        bandFilter(kHomeLowpassHz, static_cast<float>(60.0 / bpm));
        return;
    }
    if (transFrom_ == Screen::Paused) {
        // QUIT RUN: the paused run goes on, unseen, to the title's bar line.
        leavePause();
    }
    trans_ = Trans::Home;
    askQuit_ = askRestart_ = false;
    backAt_ = -1.0;
    transTap_ = transNow();
    transBeat_ = (std::floor(transTap_ / kTicksPerBeat) + 1.0) * kTicksPerBeat;
    transLine_ = std::ceil((transBeat_ + kTicksPerBeat) / kTicksPerBar - 1e-9) * kTicksPerBar;
    homeFrom_ = *palette_;
    world_->setNextSetup(RunSetup{});  // (M8: the title's demo is the set's run)
    world_->requestRestartAt(transLine_);
    applySettings();
    if (band_) band_->playSfx(audio::Sfx::UiBack);
    pulse(0, true);
    wordNotes(transLine_ + kWordFrom * kTicksPerBeat, kWordStep * kTicksPerBeat);  // (screens.md "Transitions": the word played as it builds)
    if (bandDrives_ && band_ && band_->active() && band_->view().running) {
        const double bpm = band_->view().bpm > 1.0 ? static_cast<double>(band_->view().bpm) : 120.0;
        bandFilter(kHomeLowpassHz, static_cast<float>(60.0 / bpm));  // the bed carries on, filtered
        band_->playRecipe(audio::Recipe::Impact, -1, 1.0f, 0.0f, transLine_ + (coop() ? netBandOffset_ : clock_.offset()));
    }
    RF_LOG("play: home: tapped at %.0f, the game over leaves on %.0f, the title on bar line %.0f", transTap_, transBeat_, transLine_);
}

void Play::leavePause() {
    // The run's clock comes back to the band's whole bars (as a count-in would), moved on by less than a bar; the band lets go
    // of the section it held; nothing of the run is drawn by then, and its ship neither moves nor comes to harm.
    countIn_ = false;
    askQuit_ = askRestart_ = false;
    if (bandDrives_ && band_ && band_->view().running) {
        const double heard = band_->view().heardTick;
        const double off = std::floor((heard - pauseMusic_) / kTicksPerBar) * kTicksPerBar;
        clock_.resume(heard - off, off);
        musicPrev_ = music1_ = std::fmax(music1_, heard - off);
    }
    if (bandDrives_) band_->setHold(false);
    plannedLine_ = -1e9;
    transGod(true);
    screen_ = Screen::GameOver;  // (the pause's screen is drawn leaving: renderScreens)
    applySettings();             // (the pause's dip and its filter's own ramp; the drop-in's wait slows the filter's lift)
}

void Play::homeLeaveCoop(const char* note) {
    // Co-op's home once its screen has left, or at once when the host has gone: the session ends, the title's run starts on the
    // next bar line of this device's own song and the title builds from there (its clock first brought to the band's, so the
    // solo world starts where the music is).
    const bool bandOn = bandDrives_ && band_ && band_->active() && band_->view().running;
    const double now = bandOn ? band_->view().heardTick - clock_.offset() : clock_.heardNow();
    clock_.resume(now, clock_.offset());
    leaveCoop("");
    coopMessage_[0] = 0;
    musicPrev_ = music1_ = now;
    transLine_ = (std::floor(now / kTicksPerBar) + 1.0) * kTicksPerBar;
    world_->setNextSetup(RunSetup{});  // (M8: the title's demo is the set's run)
    world_->requestRestartAt(transLine_);
    trans_ = Trans::Home;
    transFrom_ = Screen::GameOver;
    homeFromExposure_ = 0.2f;
    transTap_ = now;
    transBeat_ = now - 2.0 * kTicksPerBeat;  // (the screen it left has gone)
    homeFrom_ = *palette_;
    screen_ = Screen::GameOver;  // (nothing drawn: renderScreens finds its screen gone)
    transGod(true);
    titleNote_[0] = 0;
    wordNotes(transLine_ + kWordFrom * kTicksPerBeat, kWordStep * kTicksPerBeat);
    if (note && *note) {
        std::snprintf(titleNote_, sizeof titleNote_, "%s", note);
        titleNoteAt_ = transLine_ + 3.0 * kTicksPerBeat;  // (it stands under PLAY from when PLAY comes in, for two bars)
    }
    if (bandOn) {
        const double bpm = band_->view().bpm > 1.0 ? static_cast<double>(band_->view().bpm) : 120.0;
        bandFilter(kHomeLowpassHz, static_cast<float>(60.0 / bpm));
        band_->playRecipe(audio::Recipe::Impact, -1, 1.0f, 0.0f, transLine_ + clock_.offset());
    }
    RF_LOG("play: home from co-op%s%s: the title on bar line %.0f", note && *note ? ": " : "", note ? note : "", transLine_);
}

void Play::homeArrives(double line) {
    // The title's bar line: Track 1's bed from its first bar with a crash, its filter opening over the bar, the palette
    // cross-fading to Ultraviolet over it; the title's own run (just started, empty) fades up a bar later.
    if (trans_ != Trans::Home || screen_ == Screen::Title) return;
    transLine_ = line;
    homeFadeLine_ = line;
    screen_ = Screen::Title;
    countIn_ = false;
    transGod(false);
    const double bpm = band_ && band_->view().bpm > 1.0 ? static_cast<double>(band_->view().bpm) : 120.0;
    bandFilter(0.0f, static_cast<float>(240.0 / bpm));
    applySettings();
}

void Play::transitionFrame() {
    if (!flow_) return;
    const double now = transNow();
    wordNotesFrame();
    modesFrame();  // M8: the lift and the fold, the room's setup
    if (introPending_ && trans_ == Trans::None && screen_ == Screen::Title) {
        // A cold start: the intro begins as soon as the band runs (its music is the clock), or after a second without it.
        introWaitS_ += frameDt_;
        const bool bandOn = !bandDrives_ || (band_ && band_->active() && band_->view().running);
        if (bandOn || introWaitS_ >= kIntroWaitMaxS) startIntro();
    }
    switch (trans_) {
        case Trans::Intro: {
            const float b = introBeats();
            if (transFilter_ && b >= 0.0f) {
                const double bpm = band_ && band_->view().bpm > 1.0 ? static_cast<double>(band_->view().bpm) : 120.0;
                bandFilter(0.0f, static_cast<float>(60.0 / bpm));  // (the filter opens over the beat from the rule's throw)
            }
            if (b >= 2.0f * kBuildDone) trans_ = Trans::None;  // (the run behind has come up)
            break;
        }
        case Trans::None:
            // Co-op (screens.md "Transitions"): when the host's START or RETRY lands, every peer plays the drop-in from its own
            // screen on the same bar line.
            if (coop() && world_->restartPending() && (screen_ == Screen::CoopLobby || (screen_ == Screen::GameOver && coopOver_)))
                dropInTap(screen_);
            break;
        case Trans::Wait:
            if (!world_->restartPending() && screen_ != Screen::Run && now > transLine_ + kTicksPerBar) {
                trans_ = Trans::None;  // (the run it waited for never came: a co-op session ended under it)
                transGod(false);
                bandFilter(0.0f, 0.1f);
            }
            break;
        case Trans::Arrive:
            if (transBeats(transLine_) >= 2.0f) trans_ = Trans::None;
            break;
        case Trans::Home:
            if (coop()) {
                if (transBeats(transBeat_) >= 1.0f) homeLeaveCoop("");  // (co-op: its screen has left; the session ends)
                break;
            }
            if (screen_ != Screen::Title && now >= transLine_ + 2.0) homeArrives(transLine_);  // (the Restart event's own, normally)
            if (screen_ == Screen::Title && transBeats(transLine_) >= 8.0f) {
                trans_ = Trans::None;
                homeFadeLine_ = -1.0;
            }
            break;
        case Trans::Lift:
        case Trans::Fold: break;  // (M8: modesFrame keeps their clocks)
    }
}

Play::TitleLook Play::buildLook(float b, bool intro) const {
    // screens.md "Title" and "Transitions", the build home and the intro share, b beats from the rule's throw: the rule thrown from its centre
    // to the edges in half a beat (its ends waking over the next three quarters); the fan snapping open on b 1 (6% past and back);
    // the word played from b 1, a letter a sixteenth; on b 3 the tagline opening the rule from the centre, PLAY rising 14 px into
    // its place (taking a tap from then), then CO-OP and SETTINGS with their captions, then the best, the note and the version.
    // The intro adds the point of light (the beat before the throw) and a clamped flash on the throw.
    TitleLook t = TitleLook::none();
    if (intro && b >= -kIntroPointBeats && b < 0.0f) t.point = 1.0f;
    if (b < 0.0f) return t;
    const double bpm = band_ && band_->view().bpm > 1.0 ? static_cast<double>(band_->view().bpm) : 120.0;
    const float beatS = static_cast<float>(60.0 / bpm);
    const float throwU = clamp01(b / 0.5f);
    t.rule = 1.0f;
    t.ruleLen = 1.0f - (1.0f - throwU) * (1.0f - throwU);  // (thrown: fast, then easing)
    t.point = intro ? 1.0f - throwU : 0.0f;
    t.ends = smooth01((b - 0.25f) / 0.75f);
    t.flash = intro ? clamp01(1.0f - b * beatS / kFlashS) : 0.0f;
    t.fan = b < kWordFrom ? 0.0f : easeOutBack((b - kWordFrom) / 0.5f);
    t.fanLevel = smooth01((b - kWordFrom) / 0.5f);
    t.word = 1.0f;
    t.struck = b - kWordFrom;
    t.strikeStep = kWordStep;
    t.gap = t.tag = smooth01((b - kControlsFrom) / 0.5f);
    t.play = smooth01(b - kControlsFrom);
    t.playLift = 1.0f - t.play;
    // (2026-11-16, the owner: "all buttons should be pressable once drawn"): each control takes a tap from its first light, not
    // once it has fully arrived
    t.playTaps = t.play > 0.0f;
    t.side = smooth01((b - kControlsFrom - 0.25f) * 2.0f);
    t.sideTaps = t.side > 0.0f;
    t.best = smooth01((b - kControlsFrom - 0.5f) * 2.0f);
    t.motes = smooth01(b - kControlsFrom);
    return t;
}

float Play::introBeats() const {
    if (introAt_ < 0.0) return -10.0f;
    float b = transBeats(introAt_);
    if (introSkipAt_ >= 0.0) {
        // Skipped: from where it stood, everything arrives inside a beat.
        const float u = smooth01(transBeats(introSkipAt_));
        b = std::fmax(b, introSkipB_ + (kBuildDone - introSkipB_) * u);
    }
    return b;
}

void Play::startIntro() {
    // screens.md "Title": on the next beat line the point; a beat later the rule's throw with a crash, the filter opening over its
    // beat; a riser over the point's beat; the word's eight notes on their sixteenths.
    introPending_ = false;
    const double now = transNow();
    introAt_ = (std::floor(now / kTicksPerBeat) + 1.0 + kIntroPointBeats) * kTicksPerBeat;
    introSkipAt_ = -1.0;
    trans_ = Trans::Intro;
    titleQuietFrom_ = titleHideLine_ = titleWakeAt_ = -1.0;
    wordNotes(introAt_ + kWordFrom * kTicksPerBeat, kWordStep * kTicksPerBeat);
    if (bandDrives_ && band_ && band_->active() && band_->view().running) {
        const double offset = clock_.offset();
        bandFilter(kWaitLowpassHz, 0.05f);
        band_->playRecipe(audio::Recipe::Whoosh, -1, 0.8f, 0.0f, introAt_ - kIntroPointBeats * kTicksPerBeat + offset, kIntroPointBeats);
        band_->playRecipe(audio::Recipe::Impact, -1, 1.0f, 0.0f, introAt_ + offset);
    }
    RF_LOG("play: the intro: the rule's throw on %.0f (now %.0f)", introAt_, now);
}

void Play::settleTitle() {
    // The owner (2026-11-16: "mode selection is still unresponsive for a while once main menu is loaded, all buttons should be
    // pressable once drawn, decoupled from intro animations"): a title's control that leaves the title (the mode pill into MODES)
    // waited for the title's transition to end, and the intro's lasts four beats past the build (the run behind coming up), home's
    // and BACK's from MODES as long. A tap on it ends the transition where it stands: the title is all there, the band's filter
    // open; home's palette cross-fade finishes its bar by itself.
    if (screen_ != Screen::Title) return;
    if (trans_ == Trans::Intro) {
        introAt_ = transNow() - 2.0 * static_cast<double>(kBuildDone) * kTicksPerBeat;
        introSkipAt_ = -1.0;
        notesBase_ = -1.0;
        if (transFilter_) bandFilter(0.0f, 0.1f);
        trans_ = Trans::None;
    } else if (trans_ == Trans::Home || (trans_ == Trans::Fold && !liftRoom_)) {
        notesBase_ = -1.0;
        trans_ = Trans::None;
    }
}

void Play::wordNotes(double firstTick, double stepTicks) {
    notesBase_ = firstTick;
    notesStep_ = stepTicks;
    notesPlaced_ = 0;
}

void Play::wordNotesFrame() {
    // screens.md "Title": the word's eight notes, A3 C4 E4 A4 C5 E5 G5 A5, one a letter as it is struck, rising in velocity, panned
    // across the word; each placed on its tick a beat ahead (a skipped intro plays no more of them). The lead's voice is the
    // SawGlide recipe held still (a detuned saw pair through the lead's low-pass), half a beat long.
    if (notesBase_ < 0.0) return;
    static constexpr i32 kWordMidi[8] = {57, 60, 64, 69, 72, 76, 79, 81};
    const double now = transNow();
    const bool bandOn = bandDrives_ && band_ && band_->active() && band_->view().running;
    for (u32 j = 0; j < 8; ++j) {
        if (notesPlaced_ & (1u << j)) continue;
        const double tick = notesBase_ + notesStep_ * static_cast<double>(j);
        if (tick - now > kTicksPerBeat) break;
        notesPlaced_ = static_cast<u8>(notesPlaced_ | (1u << j));
        if (!bandOn || tick < now - 2.0) continue;  // (one already past is not played late)
        const float gain = 0.55f + 0.03f * static_cast<float>(j), pan = (static_cast<float>(j) - 3.5f) / 3.5f * 0.6f;
        band_->playRecipe(audio::Recipe::SawGlide, kWordMidi[j], gain, pan, tick + (coop() ? netBandOffset_ : clock_.offset()), 0.0f, 0.5f);
    }
    if (notesPlaced_ == 0xFF) notesBase_ = -1.0;
}

Play::TitleLook Play::titleLook() const {
    if (introPending_) return TitleLook::none();  // (a cold start before its intro: black)
    TitleLook t;
    const double now = transNow();
    switch (trans_) {
        case Trans::Intro:
            if (introAt_ < 0.0) return TitleLook::none();
            return buildLook(introBeats(), true);
        case Trans::Home: {
            const float b = transBeats(transLine_);
            if (screen_ != Screen::Title || b < 0.0f) return TitleLook::none();
            return buildLook(b, false);
        }
        case Trans::Wait: {
            if (transFrom_ != Screen::Title) return t;
            // screens.md "Transitions", the drop's wait: the side controls, their captions and the best fade and sink over its first
            // beat and the motes fall back into the rule; the fan folds flat and the tubes run hot by its last beat (in a two-beat
            // wait over the first); in the last beat's first half the tagline goes and the rule closes its gap in an eighth while
            // the word falls into its line (the rule taking its light); in the second half the rule draws in from both edges to its
            // centre while it slides to the frame's centre and PLAY, still lit, shrinks into the same point.
            t = waitFrom_;
            const float first = smooth01(transBeats(transTap_));
            t.side = waitFrom_.side * (1.0f - first);
            t.best = waitFrom_.best * (1.0f - first);
            t.sideSink = first;
            t.motes = waitFrom_.motes * (1.0f - first);
            t.moteFall = first;
            const double lastBeat = transLine_ - kTicksPerBeat;
            const double span = std::fmax(lastBeat - transTap_, kTicksPerBeat);
            const float fold = smooth01(static_cast<float>((now - transTap_) / span));
            t.fan = waitFrom_.fan * (1.0f - fold);
            t.heat = fold;
            const float h1 = clamp01(static_cast<float>((now - lastBeat) / (0.5 * kTicksPerBeat)));
            const float h2 = clamp01(static_cast<float>((now - (transLine_ - 0.5 * kTicksPerBeat)) / (0.5 * kTicksPerBeat)));
            const float shut = clamp01(h1 * 4.0f);
            t.tag = waitFrom_.tag * (1.0f - shut);
            t.gap = waitFrom_.gap * (1.0f - shut);
            t.fall = h1 * h1;  // (easing in)
            t.ruleHeat = h1;
            t.ends = waitFrom_.ends * (1.0f + 0.5f * fold) * (1.0f - h1);
            t.ruleLen = waitFrom_.ruleLen * (1.0f - h2 * h2);
            t.ruleY = h2 * h2;
            t.playGather = h2 * h2;
            t.point = h2;
            t.pressed = true;
            t.playTaps = t.sideTaps = false;
            return t;
        }
        case Trans::Arrive: return TitleLook::none();
        case Trans::Lift: {
            // M8 (design/modes/screens.md "Transitions", title to MODES): from the title as the tap found it, the mode pill held
            // pressed; over the lift's first beat PLAY, CO-OP, SETTINGS, the pill, their captions and the best fade and sink 30 px,
            // the fan folds flat, the word fades, the tagline goes and the rule closes its gap; over the second the rule rises to
            // MODES's height (y 150), easing out, its ends to half their swing.
            if (liftRoom_) return TitleLook::none();
            t = liftFrom_;
            const float b = liftBeats();
            const float u = smooth01(b);
            t.play = liftFrom_.play * (1.0f - u);
            t.side = liftFrom_.side * (1.0f - u);
            t.best = liftFrom_.best * (1.0f - u);
            t.sideSink = u;
            t.playLift = liftFrom_.playLift + u * (30.0f / 14.0f);
            t.fan = liftFrom_.fan * (1.0f - u);
            t.fanLevel = liftFrom_.fanLevel * (1.0f - u);
            t.word = liftFrom_.word * (1.0f - u);
            t.tag = liftFrom_.tag * (1.0f - u);
            t.gap = liftFrom_.gap * (1.0f - u);
            t.motes = liftFrom_.motes * (1.0f - u);
            t.playTaps = t.sideTaps = false;
            t.pillPressed = true;
            t.rule = 1.0f;
            t.ruleLen = 1.0f;
            const float r = clamp01(b - 1.0f);
            const float e = 1.0f - (1.0f - r) * (1.0f - r) * (1.0f - r);
            t.ruleAt = 432.0f + (150.0f - 432.0f) * e;
            t.endsAmp = 1.0f - 0.5f * e;
            return t;
        }
        case Trans::Fold: {
            // M8: BACK from MODES: once its rule has sunk to the title's place, the title builds from it as home builds it (the fan
            // opens and the word is played, then the tagline, PLAY, the pill, CO-OP and SETTINGS), from the build's half beat.
            if (screen_ != Screen::Title || liftRoom_) return TitleLook::none();
            t = buildLook(transBeats(transTap_) - kFoldTitleAt + 0.5f, false);
            t.rule = t.ruleLen = t.ends = 1.0f;
            return t;
        }
        case Trans::None:
            if (backAt_ >= 0.0) {
                // Back in the wait: everything returns over a beat from where it stood (the fan reopens, the tubes cool).
                const float u = smooth01(transBeats(backAt_));
                const auto up = [&](float from) { return from + (1.0f - from) * u; };
                const auto down = [&](float from) { return from * (1.0f - u); };
                t.rule = up(backFrom_.rule);
                t.ruleLen = up(backFrom_.ruleLen);
                t.ends = up(backFrom_.ends);
                t.gap = up(backFrom_.gap);
                t.fan = up(backFrom_.fan);
                t.fanLevel = up(backFrom_.fanLevel);
                t.word = up(backFrom_.word);
                t.motes = up(backFrom_.motes);
                t.tag = up(backFrom_.tag);
                t.play = up(backFrom_.play);
                t.side = up(backFrom_.side);
                t.best = up(backFrom_.best);
                t.heat = down(backFrom_.heat);
                t.fall = down(backFrom_.fall);
                t.ruleHeat = down(backFrom_.ruleHeat);
                t.ruleY = down(backFrom_.ruleY);
                t.sideSink = down(backFrom_.sideSink);
                t.moteFall = down(backFrom_.moteFall);
                t.playGather = down(backFrom_.playGather);
            }
            if (titleHideLine_ >= 0.0 && (titleWakeAt_ >= 0.0 || transBeats(titleHideLine_) > 0.0f)) {
                // screens.md "Transitions", the quiet title leaving over its bar: beat 1 the best, the note, CO-OP and SETTINGS; beat 2
                // PLAY; half a beat later the tagline, the rule's gap and the motes; beat 3 the fan folding flat and the word fading;
                // beat 4 the rule drawing back into its centre. (2026-11-16, the owner: "all buttons should be pressable once
                // drawn": a control still or again on screen takes a tap; titleIdleFrame eats only the input that wakes a title
                // whose controls have all gone.)
                const float g0 = titleGone(0.0f), g1 = titleGone(1.0f), g15 = titleGone(1.5f), g2 = titleGone(2.0f), g3 = titleGone(3.0f);
                t.best *= 1.0f - g0;
                t.side *= 1.0f - g0;
                t.play *= 1.0f - g1;
                t.tag *= 1.0f - g15;
                t.gap *= 1.0f - g15;
                t.motes *= 1.0f - g15;
                t.fan *= 1.0f - g2;
                t.fanLevel *= 1.0f - g2;
                t.word *= 1.0f - g2;
                t.ruleLen *= 1.0f - g3;
                t.rule *= 1.0f - g3;
                t.ends *= 1.0f - g3;
                t.playTaps = t.play > 0.0f;
                t.sideTaps = t.side > 0.0f;
            }
            return t;
    }
    return t;
}

float Play::titleGone(float fromBeat, float beats) const {
    if (titleHideLine_ < 0.0) return 0.0f;
    // Leaving: from its beat of the bar it leaves on, over `beats`. Waking: back from where it stood over a beat.
    const float into = titleWakeAt_ >= 0.0 ? titleWakeFrom_ : transBeats(titleHideLine_);
    float g = smooth01((into - fromBeat) / beats);
    if (titleWakeAt_ >= 0.0) g *= 1.0f - smooth01(transBeats(titleWakeAt_));
    return g;
}

void Play::titleIdleFrame(UiPointer& ptr) {
    const bool input = uiActivity_;
    uiActivity_ = false;
    const double now = transNow();
    if (trans_ == Trans::Intro || introPending_) {
        // screens.md "Title": any input ends the intro. The owner (2026-11-16: "the menu buttons are locked until the full
        // Raveform intro plays"): it ends at once, everything on screen in this frame (the word's notes not yet placed are not
        // played), and the input is the screen's, not eaten: a tap on PLAY plays, on CO-OP opens it.
        const float b = introBeats();
        if (input && b < kBuildDone) {
            if (introPending_) startIntro();
            introAt_ = now - static_cast<double>(kBuildDone) * kTicksPerBeat;
            introSkipAt_ = -1.0;
            notesBase_ = -1.0;
        }
        titleQuietFrom_ = titleHideLine_ = titleWakeAt_ = -1.0;
        return;
    }
    if (input && screen_ == Screen::Title && trans_ == Trans::Home && transBeats(transLine_) >= 0.0f && transBeats(transLine_) < kBuildDone) {
        // (the same for home's build of the title, after a run: an input completes it, and is the screen's)
        transLine_ = now - static_cast<double>(kBuildDone) * kTicksPerBeat;
        notesBase_ = -1.0;
    }
    if (input && screen_ == Screen::Title && trans_ == Trans::Fold && !liftRoom_ && transBeats(transTap_) < kFoldTitleAt + 3.5f) {
        // (and for the title building back from MODES's BACK)
        transTap_ = now - static_cast<double>(kFoldTitleAt + 3.5f) * kTicksPerBeat;
        notesBase_ = -1.0;
    }
    if (!(flow_ && screen_ == Screen::Title && trans_ == Trans::None && !session_)) {
        titleQuietFrom_ = titleHideLine_ = titleWakeAt_ = -1.0;  // (not at rest: the count starts when it is)
        return;
    }
    if (titleQuietFrom_ < 0.0 || now < titleQuietFrom_) titleQuietFrom_ = now;
    if (titleHideLine_ >= 0.0) {
        if (titleWakeAt_ >= 0.0) {
            if (now < titleWakeAt_ || transBeats(titleWakeAt_) >= 1.0f) titleHideLine_ = titleWakeAt_ = -1.0;  // back
        } else if (input && transBeats(titleHideLine_) <= 0.0f) {
            titleHideLine_ = -1.0;  // an input before the bar line it would leave on: it stays (and the input is the screen's)
        } else if (input) {
            // The input that wakes it: everything comes back over a beat from where it stood. (2026-11-16, the owner: "all buttons
            // should be pressable once drawn"): while PLAY or the side controls are still on screen the input is theirs; once they
            // have all gone it only wakes the title (a tap where PLAY was would otherwise press it as it came back).
            const bool drawn = titleGone(0.0f) < 1.0f || titleGone(1.0f) < 1.0f;
            titleWakeFrom_ = std::fmin(transBeats(titleHideLine_), 5.0f);
            titleWakeAt_ = now;
            if (!drawn) {
                ptr.pressed = ptr.released = ptr.down = false;
                keyPrimary_ = keyBack_ = false;
            }
        } else if (now < titleHideLine_ - kTicksPerBar) {
            titleHideLine_ = now - 5.0 * kTicksPerBeat;  // (the clock ran back under it: it stays gone)
        }
        if (input) titleQuietFrom_ = now;
        return;
    }
    if (input) {
        titleQuietFrom_ = now;
        return;
    }
    if (now - titleQuietFrom_ >= kTitleQuietBars * kTicksPerBar) titleHideLine_ = (std::floor(now / kTicksPerBar) + 1.0) * kTicksPerBar;
}

float Play::transRunLight() const {
    if (!flow_) return 1.0f;
    if (introPending_) return 0.0f;
    const double now = transNow();
    switch (trans_) {
        case Trans::Intro: return clamp01((introBeats() - kBuildDone) * 0.25f);  // (the run behind comes up over the bar after)
        case Trans::Wait: {
            const double span = transLine_ - transTap_;
            const float p = span > 1.0 ? clamp01(static_cast<float>((now - transTap_) / span)) : 1.0f;
            return waitRun_ * (1.0f - p);  // the run behind dissolves over the wait
        }
        case Trans::Home:
            if (screen_ != Screen::Title) return 1.0f - smooth01(transBeats(transBeat_));  // the run fades out as the screen leaves
            return clamp01((transBeats(transLine_) - 4.0f) * 0.25f);  // the title's run fades up over the bar after the title's
        case Trans::None:
            if (backAt_ >= 0.0) return backRun_ + (1.0f - backRun_) * smooth01(transBeats(backAt_));
            return 1.0f;
        default: return 1.0f;
    }
}

float Play::transExposure(float screenLevel) const {
    // The light of the run behind a screen: the drop-in's downbeat brings it to the run's full light over its beat; home
    // moves it from the game over's (or the pause's) to the title's over the title's bar. The intro: black until the rule's throw,
    // the background and its stars coming up over its beat.
    if (introPending_) return 0.0f;
    if (trans_ == Trans::Intro) return screenLevel * smooth01(introBeats());
    // M8: the demo behind dims from the title's third to MODES's fifth over the lift's first beat, and back over BACK's fold.
    if (trans_ == Trans::Lift) return liftRoom_ ? screenLevel : 0.33f + (0.2f - 0.33f) * smooth01(liftBeats());
    if (trans_ == Trans::Fold) return liftRoom_ ? screenLevel : 0.2f + (0.33f - 0.2f) * smooth01(transBeats(transTap_) - kFoldRuleFrom);
    if (trans_ == Trans::Wait && transFrom_ == Screen::Paused) {
        const double span = transLine_ - transTap_;
        const float p = span > 1.0 ? clamp01(static_cast<float>((transNow() - transTap_) / span)) : 1.0f;
        return 0.2f + 0.8f * smooth01(p);  // (c9218dc: the pause's dim lifts over RESTART's wait)
    }
    if (trans_ == Trans::Arrive) {
        const float from = transFrom_ == Screen::Paused ? 1.0f : (transFrom_ == Screen::GameOver ? (coop() ? 0.2f : 0.45f) : (transFrom_ == Screen::Modes ? 0.2f : 0.33f));
        return from + (1.0f - from) * smooth01(transBeats(transLine_));
    }
    if (trans_ == Trans::Home) {
        if (screen_ != Screen::Title) return homeFromExposure_;
        return homeFromExposure_ + (screenLevel - homeFromExposure_) * smooth01(transBeats(transLine_) * 0.25f);
    }
    return screenLevel;
}

bool Play::transInputLocked() const {
    if (introPending_) return true;
    switch (trans_) {
        case Trans::Intro: return introBeats() < kControlsFrom;
        case Trans::Wait:
        case Trans::Arrive: return true;
        case Trans::Home: return !(screen_ == Screen::Title && transBeats(transLine_) >= 3.0f);
        case Trans::Lift: return liftBeats() < 2.0f;  // (M8: MODES's PLAY takes a tap from the lift's third beat)
        case Trans::Fold: return liftRoom_ ? transBeats(transTap_) < 1.5f : !(screen_ == Screen::Title && transBeats(transTap_) >= kFoldTitleAt + 2.5f);
        default: return false;
    }
}

float Play::gameOverOut() const {
    if (trans_ == Trans::Home || (trans_ == Trans::Wait && (transFrom_ == Screen::GameOver || transFrom_ == Screen::Paused)))
        return clamp01(transBeats(transBeat_));
    return 0.0f;
}

void Play::drawTitleShip(Ui& ui, Vec2 c, float angle, float scale, float level, bool ring, u32 seat) {
    const float s = ui.s() * scale;
    const float a = angle;
    const Vec2 nose = c + polarV(a, 26.0f * s), l = c + polarV(a + 2.5f, 22.0f * s), r = c + polarV(a - 2.5f, 22.0f * s),
               tail = c + polarV(a + kTwoPi * 0.5f, 8.0f * s);
    const u32 q = seat < kMaxPlayers ? seat : (seat_ < kMaxPlayers ? seat_ : 0u);
    const Vec3 pc = ui.col(palettes_->player[q], 1.8f * level), lc = ui.col(palettes_->playerLight[q], 1.6f * level);
    ui.draw().hud.line(nose, l, 1.2f * s, pc, pc, 0);
    ui.draw().hud.line(l, tail, 1.2f * s, pc, pc, 0);
    ui.draw().hud.line(tail, r, 1.2f * s, lc, lc, 0);
    ui.draw().hud.line(r, nose, 1.2f * s, lc, lc, 0);
    if (ring) ui.ring(c, 40.0f * s, 0.6f * ui.s(), palettes_->player[q], 0.7f * level);
}

void Play::drawArrive(Ui& ui) {
    // screens.md "Transitions", the downbeat (every drop-in: PLAY, RETRY, the pause's RESTART, a co-op START or RETRY): the point the
    // light gathered into bursts: a clamped flash pool, eight rays (one a letter of the word, in its colour) leaving it from 60 to
    // 1,360 px over half a beat, the lattice lit from it (PlayView::dropRing), and the ship drawing itself there with its halo over
    // the beat. Reduce flashing: no flash, no rays.
    const float a = transBeats(transLine_);
    if (a >= 1.0f || a < 0.0f) return;
    const float s = ui.s();
    const PlayerPose& pose = controller_.pose();
    const Vec2 ship = view_.toScreen(pose.pos);
    const float u = smooth01(a);
    const Vec3 tone = palettes_->player[seat_ < kMaxPlayers ? seat_ : 0], white{1.0f, 1.0f, 1.0f};
    const bool calm = settings_.reduceFlashing || forcedReduceFlashing_;
    const double bpm = band_ && band_->view().bpm > 1.0 ? static_cast<double>(band_->view().bpm) : 120.0;
    const float beatS = static_cast<float>(60.0 / bpm);
    if (!calm) {
        const float flash = clamp01(1.0f - a * beatS / kFlashS);
        if (flash > 0.0f) ui.draw().pools.glow(ship, 1000.0f * s, white * (0.3f * flash * hudFade()));
        const float r = a / 0.5f;
        if (r < 1.0f) {
            const Vec3 c1 = palettes_->player[0], c3 = palette_->hue[0], c4 = palette_->hue[2];
            const RunSetup& su = world_->setup();
            const Vec3 m0 = modeColour(su.mode, su.track, false), m1 = modeColour(su.mode, su.track, true);
            for (u32 j = 0; j < 8; ++j) {
                // (the letter's colour in the word's field: P1 cyan, white, the first enemy hue, the third; M8: from MODES, the mode's
                // two colours)
                const float f = (static_cast<float>(j) + 0.5f) / 8.0f;
                const Vec3 c = transFrom_ == Screen::Modes ? mix(m0, m1, f)
                                                           : (f < 1.0f / 3.0f ? c1 + (white - c1) * (f * 3.0f) : (f < 2.0f / 3.0f ? white + (c3 - white) * (f * 3.0f - 1.0f) : c3 + (c4 - c3) * (f * 3.0f - 2.0f)));
                const float ang = -0.25f * kTwoPi + kTwoPi * (static_cast<float>(j) + 0.5f) / 8.0f;
                const float head = kRayFrom + (kRayTo - kRayFrom) * (1.0f - (1.0f - r) * (1.0f - r));
                const float tail = kRayFrom + (kRayTo - kRayFrom) * smooth01(r - 0.25f);
                const Vec3 k = ui.col(c, 2.0f * (1.0f - r));
                ui.draw().hud.line(ship + polarV(ang, tail * s), ship + polarV(ang, head * s), 1.6f * s, k * 0.2f, k, 0);
                ui.draw().hud.line(ship + polarV(ang, tail * s), ship + polarV(ang, head * s), 6.0f * s, k * 0.02f, k * 0.12f, 0);
            }
        }
    }
    // The ship draws itself at the point with its halo over the beat (as RETRY's always has).
    drawTitleShip(ui, ship, pose.heading, 0.4f + 0.6f * u, u, false);
    const u32 segs = static_cast<u32>(48.0f * u);
    const Vec3 k = ui.col(tone, 1.4f);
    for (u32 j = 0; j < segs; ++j) {
        const float a0 = -0.25f * kTwoPi + kTwoPi * static_cast<float>(j) / 48.0f, a1 = a0 + kTwoPi / 48.0f;
        ui.draw().hud.line(ship + polarV(a0, kHaloRefPx * s), ship + polarV(a1, kHaloRefPx * s), 1.0f * s, k, k, 0);
    }
}

void Play::drawDropGather(Ui& ui) {
    // screens.md "Transitions": RETRY, the pause's RESTART and the co-op lobby's START: in the wait's last half beat their lit control
    // shrinks into the point at the frame's centre (PLAY's own is drawn by the title).
    if (trans_ != Trans::Wait || transFrom_ == Screen::Title) return;
    const float h2 = clamp01(static_cast<float>((transNow() - (transLine_ - 0.5 * kTicksPerBeat)) / (0.5 * kTicksPerBeat)));
    if (h2 <= 0.0f) return;
    const float s = ui.s(), g = h2 * h2;
    const Vec2 to{0.5f * ui.W(), 0.5f * ui.H()};
    const Vec2 c = dropCtlC_ + (to - dropCtlC_) * g;
    const Vec2 half = dropCtlHalf_ * (1.0f - g);
    const Vec3 tone = palettes_->player[seat_ < kMaxPlayers ? seat_ : 0], white{1.0f, 1.0f, 1.0f};
    if (half.y > 0.5f * s) ui.roundRect(c - half, c + half, half.y, 1.5f * s, tone, 2.2f);
    ui.dot(c, (2.0f + 3.0f * h2) * s, white, 0.6f + 1.4f * h2);
    ui.draw().pools.glow(c, (40.0f + 80.0f * h2) * s, tone * (0.15f * h2 * hudFade()));
}

}  // namespace rf
