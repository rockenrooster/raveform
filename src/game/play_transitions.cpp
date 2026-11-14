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
// (chosen) The halo's radius on screen at 1080 lines, about: PLAY's outline shrinks to it.
constexpr float kHaloRefPx = 46.0f;
}  // namespace

double Play::transNow() const {
    if (coop() && session_ && session_->songValid()) return netHeard_;
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
    bandFilter(kWaitLowpassHz, static_cast<float>(wait) * beatS);
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
    if (!world_->restartPending()) world_->requestRestart();  // (co-op: the host's START or RETRY scheduled it)
    trans_ = Trans::Wait;
    transFrom_ = from;
    backAt_ = -1.0;
    homeFadeLine_ = -1.0;
    transTap_ = transNow();
    transLine_ = world_->restartTick();
    transBeat_ = (std::floor(transTap_ / kTicksPerBeat) + 1.0) * kTicksPerBeat;  // a game-over screen leaves on the next beat
    titleShipFrom_ = titleShipAngle_;
    if (from == Screen::Title && !coop()) transGod(true);
    if (band_) band_->playSfx(audio::Sfx::UiConfirm);
    pulse(0);
    transitionAudio();
    RF_LOG("play: the drop-in: tapped at %.0f, the run's first bar line %.0f (%.1f beats)", transTap_, transLine_,
           (transLine_ - transTap_) / kTicksPerBeat);
}

void Play::dropInCancel() {
    // Back in the wait: PLAY lets go, everything fades back in over a beat, the filter opens (the riser is a one-shot sample,
    // which the band cannot stop: it ends under the open filter).
    if (trans_ != Trans::Wait || transFrom_ != Screen::Title || coop()) return;
    backFrom_ = titleLook();
    backRun_ = transRunLight();
    world_->cancelRestart();
    trans_ = Trans::None;
    backAt_ = transNow();
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
    homeFromExposure_ = transFrom_ == Screen::Paused ? 0.2f : 0.45f;
    if (transFrom_ == Screen::Paused) {
        // QUIT RUN: the paused run goes on, unseen, to the title's bar line. Its clock comes back to the band's whole bars
        // (as a count-in would), moved on by less than a bar; nothing of the run is drawn by then.
        countIn_ = false;
        if (bandDrives_ && band_ && band_->view().running) {
            const double heard = band_->view().heardTick;
            const double off = std::floor((heard - pauseMusic_) / kTicksPerBar) * kTicksPerBar;
            clock_.resume(heard - off, off);
            musicPrev_ = music1_ = std::fmax(music1_, heard - off);
        }
        if (bandDrives_) band_->setHold(false);
        plannedLine_ = -1e9;
        transGod(true);
        screen_ = Screen::GameOver;
    }
    trans_ = Trans::Home;
    askQuit_ = askRestart_ = false;
    backAt_ = -1.0;
    transTap_ = transNow();
    transBeat_ = (std::floor(transTap_ / kTicksPerBeat) + 1.0) * kTicksPerBeat;
    transLine_ = std::ceil((transBeat_ + kTicksPerBeat) / kTicksPerBar - 1e-9) * kTicksPerBar;
    homeFrom_ = *palette_;
    world_->requestRestartAt(transLine_);
    applySettings();
    if (band_) band_->playSfx(audio::Sfx::UiBack);
    pulse(0);
    if (bandDrives_ && band_ && band_->active() && band_->view().running) {
        const double bpm = band_->view().bpm > 1.0 ? static_cast<double>(band_->view().bpm) : 120.0;
        bandFilter(kHomeLowpassHz, static_cast<float>(60.0 / bpm));  // the bed carries on, filtered
        band_->playRecipe(audio::Recipe::Impact, -1, 1.0f, 0.0f, transLine_ + (coop() ? netBandOffset_ : clock_.offset()));
    }
    RF_LOG("play: home: tapped at %.0f, the game over leaves on %.0f, the title on bar line %.0f", transTap_, transBeat_, transLine_);
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
    switch (trans_) {
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
            if (screen_ != Screen::Title && now >= transLine_ + 2.0) homeArrives(transLine_);  // (the Restart event's own, normally)
            if (screen_ == Screen::Title && transBeats(transLine_) >= 8.0f) {
                trans_ = Trans::None;
                homeFadeLine_ = -1.0;
            }
            break;
    }
}

Play::TitleLook Play::titleLook() const {
    TitleLook t;
    const double now = transNow();
    switch (trans_) {
        case Trans::Home: {
            // Built over the bar: the waveform outward from the centre in a beat, the logo over the next two, then the tagline
            // and the beat dots; on its last beat PLAY (which takes a tap from then), CO-OP and SETTINGS, the best and the note.
            const float b = transBeats(transLine_);
            if (screen_ != Screen::Title || b < 0.0f) return TitleLook{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false};
            t.waveDrawn = clamp01(b);
            t.logoWrite = clamp01((b - 1.0f) * 0.5f);
            t.tag = smooth01((b - 3.0f) * 2.0f);
            t.play = smooth01((b - 3.0f) * 2.0f);
            t.side = smooth01((b - 3.25f) * 2.0f);
            t.best = smooth01((b - 3.5f) * 2.0f);
            t.playTaps = b >= 3.0f;
            t.sideTaps = t.side >= 1.0f;
            return t;
        }
        case Trans::Wait: {
            if (transFrom_ != Screen::Title) return t;
            t = waitFrom_;
            const float first = smooth01(transBeats(transTap_));  // the wait's first beat
            const float last = smooth01(1.0f - static_cast<float>((transLine_ - now) / kTicksPerBeat));  // its last
            t.side = waitFrom_.side * (1.0f - first);
            t.best = waitFrom_.best * (1.0f - first);
            t.tag = waitFrom_.tag * (1.0f - last);
            t.waveFlat = last;
            t.playTaps = t.sideTaps = false;
            return t;
        }
        case Trans::Arrive:
            t.logoOut = clamp01(transBeats(transLine_));
            t.playTaps = t.sideTaps = false;
            return t;
        case Trans::None:
            if (backAt_ >= 0.0) {
                const float u = smooth01(transBeats(backAt_));
                const auto up = [&](float from) { return from + (1.0f - from) * u; };
                t.wave = up(backFrom_.wave);
                t.waveDrawn = up(backFrom_.waveDrawn);
                t.waveFlat = backFrom_.waveFlat * (1.0f - u);
                t.logo = up(backFrom_.logo);
                t.logoWrite = up(backFrom_.logoWrite);
                t.tag = up(backFrom_.tag);
                t.play = up(backFrom_.play);
                t.side = up(backFrom_.side);
                t.best = up(backFrom_.best);
            }
            return t;
    }
    return t;
}

float Play::transRunLight() const {
    if (!flow_) return 1.0f;
    const double now = transNow();
    switch (trans_) {
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
    // moves it from the game over's (or the pause's) to the title's over the title's bar.
    if (trans_ == Trans::Arrive) {
        const float from = transFrom_ == Screen::GameOver ? (coop() ? 0.2f : 0.45f) : 0.33f;
        return from + (1.0f - from) * smooth01(transBeats(transLine_));
    }
    if (trans_ == Trans::Home) {
        if (screen_ != Screen::Title) return homeFromExposure_;
        return homeFromExposure_ + (screenLevel - homeFromExposure_) * smooth01(transBeats(transLine_) * 0.25f);
    }
    return screenLevel;
}

bool Play::transInputLocked() const {
    switch (trans_) {
        case Trans::Wait:
        case Trans::Arrive: return true;
        case Trans::Home: return !(screen_ == Screen::Title && transBeats(transLine_) >= 3.0f);
        default: return false;
    }
}

float Play::gameOverOut() const {
    if (trans_ == Trans::Home || (trans_ == Trans::Wait && transFrom_ == Screen::GameOver)) return clamp01(transBeats(transBeat_));
    return 0.0f;
}

void Play::drawTitleShip(Ui& ui, Vec2 c, float angle, float scale, float level, bool ring) {
    const float s = ui.s() * scale;
    const float a = angle;
    const Vec2 nose = c + polarV(a, 26.0f * s), l = c + polarV(a + 2.5f, 22.0f * s), r = c + polarV(a - 2.5f, 22.0f * s),
               tail = c + polarV(a + kTwoPi * 0.5f, 8.0f * s);
    const Vec3 pc = ui.col(palettes_->player[seat_ < kMaxPlayers ? seat_ : 0], 1.8f * level),
               lc = ui.col(palettes_->playerLight[seat_ < kMaxPlayers ? seat_ : 0], 1.6f * level);
    ui.draw().hud.line(nose, l, 1.2f * s, pc, pc, 0);
    ui.draw().hud.line(l, tail, 1.2f * s, pc, pc, 0);
    ui.draw().hud.line(tail, r, 1.2f * s, lc, lc, 0);
    ui.draw().hud.line(r, nose, 1.2f * s, lc, lc, 0);
    if (ring) ui.ring(c, 40.0f * s, 0.6f * ui.s(), palettes_->player[seat_ < kMaxPlayers ? seat_ : 0], 0.7f * level);
}

void Play::drawArrive(Ui& ui) {
    // The drop-in's downbeat beat (screens.md "Transitions"): what the title (or the screen before) leaves on the way into the
    // run's first bar.
    const float a = transBeats(transLine_);
    if (a >= 1.0f || a < 0.0f) return;
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const PlayerPose& pose = controller_.pose();
    const Vec2 ship = view_.toScreen(pose.pos);
    const float u = smooth01(a);
    const Vec3 tone = palettes_->player[seat_ < kMaxPlayers ? seat_ : 0];
    const bool calm = settings_.reduceFlashing || forcedReduceFlashing_;
    // The waveform, straight now, snaps outward as a ring from the ship (it lights the lattice: PlayView::dropRing).
    if (!calm) {
        const float reach = 0.5f * std::sqrt(W * W + H * H);  // (to the frame's corners from its centre at the beat's end)
        const float r = (1.0f - (1.0f - a) * (1.0f - a)) * reach;
        const Vec3 cyan = palette_->hue[1], mag = palette_->accent, hue0 = palette_->hue[0], white{1.0f, 1.0f, 1.0f};
        const u32 segs = 96;
        for (u32 j = 0; j < segs; ++j) {
            const float v = static_cast<float>(j) / segs;
            const Vec3 c = v < 0.33f ? cyan + (white - cyan) * (v / 0.33f)
                                     : (v < 0.66f ? white + (hue0 - white) * ((v - 0.33f) / 0.33f) : hue0 + (mag - hue0) * ((v - 0.66f) / 0.34f));
            const Vec3 k = ui.col(c, 1.6f * (1.0f - a));
            const float a0 = kTwoPi * v, a1 = kTwoPi * (static_cast<float>(j) + 1.0f) / segs;
            ui.draw().hud.line(ship + polarV(a0, r), ship + polarV(a1, r), 1.2f * s, k, k, 0);
        }
    }
    if (transFrom_ == Screen::Title) {
        // The logo's letters streak out to both sides (as the Track name card's do).
        drawTitleLogo(ui, 1.0f, 1.0f, a, 0.0f);
        // PLAY shrinks into the ship; its outline becomes the halo, which the run draws once the beat is over.
        const Vec2 c0{cx, 759.0f * s}, size0{600.0f * s, 140.0f * s};
        const float r1 = kHaloRefPx * s;
        const Vec2 c = c0 + (ship - c0) * u;
        const Vec2 size = size0 + (Vec2{2.0f * r1, 2.0f * r1} - size0) * u;
        ui.roundRect(c - size * 0.5f, c + size * 0.5f, 0.5f * size.y, 1.4f * s, tone, 1.6f * (1.0f - smooth01((a - 0.7f) / 0.3f)));
        // The title's ship glides from under PLAY to the centre of the frame (where the run's ship is).
        const Vec2 from{cx, 930.0f * s};
        const float turn = wrapAngle(pose.heading - titleShipFrom_);
        drawTitleShip(ui, from + (ship - from) * u, titleShipFrom_ + turn * u, 1.0f, 1.0f, false);
    } else {
        // RETRY (or a co-op START): no logo; the ship draws itself at the centre with its halo.
        drawTitleShip(ui, ship, pose.heading, 0.4f + 0.6f * u, u, false);
        const u32 segs = static_cast<u32>(48.0f * u);
        const Vec3 k = ui.col(tone, 1.4f);
        for (u32 j = 0; j < segs; ++j) {
            const float a0 = -0.25f * kTwoPi + kTwoPi * static_cast<float>(j) / 48.0f, a1 = a0 + kTwoPi / 48.0f;
            ui.draw().hud.line(ship + polarV(a0, kHaloRefPx * s), ship + polarV(a1, kHaloRefPx * s), 1.0f * s, k, k, 0);
        }
    }
}

}  // namespace rf
