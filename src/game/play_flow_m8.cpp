// M8 part 7 (docs/MODES_PLAN.md 8, part 7: "every mode from every way in (the title, MODES, RETRY, REMATCH, NEXT TRACK, a room, the
// pause's RESTART) and out"): --flow-test. The game's own screens, driven as a player drives them: taps on their controls, found by
// their labels where the screen drew them last frame (Ui::findControl), pressed on one frame and let go on the next; Esc's pause.
// For each mode: in by the title's PLAY (the pill's mode on its Track), by MODES's PLAY (the row stepped to it, a Track chosen), by
// the pause's RESTART, by the result's RETRY and (ONE TRACK) NEXT TRACK; out by the pause's QUIT RUN (JAM's LEAVE) and the result's
// HOME. The runs are played by the bot (--autoplay's) and fast-forwarded to their ends (hits cost nothing there). The room's ways
// (a room's START, REMATCH, LOBBY, LEAVE) are scripts\flow-m8.ps1's, two processes on this PC. Each check is a FLOW line; the last
// says how many passed, and the app exits 6 if any failed.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "audio/band.h"
#include "core/log.h"
#include "game/play.h"

namespace rf {

using namespace sim;

namespace {
// The modes in the order the test drives them, each on a Track it takes (ONE TRACK on 2, so NEXT TRACK has a Track to go to).
struct FlowMode {
    Mode mode;
    u8 track;
};
constexpr FlowMode kFlowModes[] = {{Mode::OneTrack, 2}, {Mode::FullBlast, 3}, {Mode::Tracers, 3}, {Mode::Songline, 4},
                                   {Mode::Jam, 12},     {Mode::Headliners, 1}, {Mode::Set, 1}};
constexpr u32 kFlowModeCount = sizeof(kFlowModes) / sizeof(kFlowModes[0]);
enum FlowStep : u8 { kTitlePlay, kPauseRestart, kPauseQuit, kModesPlay, kEndRun, kRetry, kEndRun2, kNextTrack, kEndRun3, kHome, kNextMode };
const char* stepName(u8 s) {
    static const char* const k[] = {"the title's PLAY", "the pause's RESTART", "the pause's QUIT RUN", "MODES's PLAY", "the run to its end",
                                    "the result's RETRY", "the run to its end", "the result's NEXT TRACK", "the run to its end",
                                    "the result's HOME", "next"};
    return s <= kNextMode ? k[s] : "?";
}
}  // namespace

bool Play::flowTap(const char* label) {
    // A tap on the control drawn last frame with this label: down now, up on the next frame (flowFrame lets go).
    Vec2 c;
    if (flowTapPhase_ || !ui_.findControl(label, c)) return false;
    flowTapAt_ = c;
    flowTapPhase_ = 1;
    uiPointer(c, true);
    RF_LOG("flow: tap %s at (%.0f, %.0f)", label, static_cast<double>(c.x), static_cast<double>(c.y));
    return true;
}

void Play::flowCheck(bool ok, const char* what) {
    const FlowMode& fm = kFlowModes[flowMode_ < kFlowModeCount ? flowMode_ : 0];
    ++flowChecks_;
    if (!ok) ++flowFails_;
    RF_LOG("FLOW %s %s (%s): %s", ok ? "ok  " : "FAIL", modeId(fm.mode), stepName(flowStep_), what);
}

bool Play::flowRunAs(Mode m, u8 track) const {
    const World& w = *world_;
    return screen_ == Screen::Run && trans_ == Trans::None && w.setup().mode == m && (!species_->modes.of(m).takesTrack || w.setup().track == track);
}

void Play::flowEndRun() {
    // The run fast-forwarded (the bot playing, hits costing nothing) until its result is up: as the startup's fast-forward runs it.
    World& w = *world_;
    w.rules().godMode = true;
    const bool drives = bandDrives_;
    bandDrives_ = false;
    const bool fixed = opt_.fixedStep;
    opt_.fixedStep = true;
    ffRunning_ = true;
    const u32 cap = 60u * 60u * 12u;  // (twelve minutes: HEADLINERS's ten acts take about seven)
    u32 k = 0;
    for (; k < cap && screen_ != Screen::GameOver; ++k) frame(kDt);
    ffRunning_ = false;
    opt_.fixedStep = fixed;
    bandDrives_ = drives;
    view_.resetEffects();
    if (bandDrives_) {
        pauseMusic_ = music1_;
        realign_ = true;
        countIn_ = true;
        countInEnd_ = 1e18;
    }
    RF_LOG("flow: fast-forwarded %.1f s to the result (screen %u)", static_cast<double>(k) / 60.0, static_cast<unsigned>(screen_));
}

void Play::flowFrame() {
    // One frame of the test: a tap's release, then the step's next move once the screen it waits for is at rest.
    if (!opt_.flowTest || flowDone_) return;
    if (flowTapPhase_) {
        uiPointer(flowTapAt_, false);
        flowTapPhase_ = 0;
        return;
    }
    flowWaitS_ += frameDt_;
    World& w = *world_;
    const FlowMode& fm = kFlowModes[flowMode_];
    const bool takes = species_->modes.of(fm.mode).takesTrack;
    const u8 track = takes ? fm.track : 1u;
    const bool rest = trans_ == Trans::None;
    const bool timedOut = flowWaitS_ > 40.0f;
    const auto next = [&](u8 step) {
        flowStep_ = step;
        flowSub_ = 0;
        flowWaitS_ = 0.0f;
    };
    const auto give = [&](const char* what) {
        flowCheck(false, what);
        // (back to the title and on to the next mode: a failed step leaves the rest of this mode undriven)
        flowSub_ = 99;
    };
    if (flowSub_ == 99) {
        if (screen_ == Screen::Paused) {
            endRun(false);
            toTitle();
        } else if (screen_ == Screen::GameOver || screen_ == Screen::Modes || screen_ == Screen::Run) {
            if (screen_ == Screen::Run) pauseRun();
            else toTitle();
        }
        if (screen_ == Screen::Title && rest) next(kNextMode);
        return;
    }
    switch (flowStep_) {
        case kTitlePlay:
            // The title at rest a second, the pill's mode and Track as MODES leaves them; PLAY.
            if (flowSub_ == 0) {
                if (screen_ != Screen::Title || !rest || flowWaitS_ < 1.0f) {
                    if (timedOut) give("the title never came to rest");
                    return;
                }
                records_.mode = static_cast<u8>(fm.mode);
                if (takes) records_.track[static_cast<u32>(fm.mode)] = track;
                if (flowTap("PLAY")) flowSub_ = 1, flowWaitS_ = 0.0f;
                else if (timedOut) give("no PLAY on the title");
                return;
            }
            if (flowRunAs(fm.mode, track) && flowWaitS_ > 0.5f) {
                flowCheck(true, "in by the title's PLAY: the run is the pill's mode on its Track");
                flowRunId_ = w.runId();
                next(kPauseRestart);
            } else if (timedOut) {
                give("the title's PLAY did not start the pill's mode");
            }
            return;
        case kPauseRestart:
        case kPauseQuit: {
            const bool quit = flowStep_ == kPauseQuit;
            if (flowSub_ == 0) {
                if (screen_ != Screen::Run || !rest || flowWaitS_ < 1.5f) return;
                pauseRun();  // (Esc, Back or the pause ring)
                flowSub_ = 1;
                flowWaitS_ = 0.0f;
                return;
            }
            if (flowSub_ == 1) {
                if (screen_ != Screen::Paused || flowWaitS_ < 0.5f) {
                    if (timedOut) give("the pause never came");
                    return;
                }
                if (flowTap(quit ? (fm.mode == Mode::Jam ? "LEAVE" : "QUIT RUN") : "RESTART")) flowSub_ = 2, flowWaitS_ = 0.0f;
                else if (timedOut) give(quit ? "no QUIT RUN on the pause" : "no RESTART on the pause");
                return;
            }
            if (flowSub_ == 2) {
                if (flowWaitS_ < 0.3f) return;
                if (flowTap("YES")) flowSub_ = 3, flowWaitS_ = 0.0f;
                else if (timedOut) give("no YES under the pause's question");
                return;
            }
            if (quit) {
                if (screen_ == Screen::Title && rest) {
                    flowCheck(true, fm.mode == Mode::Jam ? "out by the pause's LEAVE: the title" : "out by the pause's QUIT RUN: the title");
                    next(kModesPlay);
                } else if (timedOut) {
                    give("QUIT RUN did not reach the title");
                }
                return;
            }
            if (flowRunAs(fm.mode, track) && w.runId() != flowRunId_ && flowWaitS_ > 0.5f) {
                flowCheck(true, "in by the pause's RESTART: a new run of the same mode and Track");
                flowRunId_ = w.runId();
                next(kPauseQuit);
            } else if (timedOut) {
                give("RESTART did not start the same mode again");
            }
            return;
        }
        case kModesPlay:
            // The pill opens MODES (the rule lifts); the row stepped to the mode; a Track chosen; PLAY.
            if (flowSub_ == 0) {
                if (screen_ != Screen::Title || !rest || flowWaitS_ < 1.0f) return;
                records_.mode = static_cast<u8>(Mode::Set);  // (the pill on THE SET: MODES opens on it and is stepped from there)
                char pill[24];
                std::snprintf(pill, sizeof pill, "%s", species_->modes.of(Mode::Set).name);
                if (flowTap(pill)) flowSub_ = 1, flowWaitS_ = 0.0f;
                else if (flowWaitS_ > 3.0f) {
                    openModes(Screen::Title);  // (a release build without the pill: MODES as the pill opens it)
                    flowSub_ = 1;
                    flowWaitS_ = 0.0f;
                }
                return;
            }
            if (flowSub_ == 1) {
                if (screen_ != Screen::Modes || !rest || flowWaitS_ < 0.5f) {
                    if (timedOut) give("MODES never came");
                    return;
                }
                if (modesCentre_ != static_cast<u8>(fm.mode)) {
                    if (flowWaitS_ > 0.4f) {
                        modesStep(static_cast<u8>(fm.mode) > modesCentre_ ? 1 : -1);  // (the arrows, a swipe)
                        flowWaitS_ = 0.0f;
                    }
                    return;
                }
                if (takes) modesChooseTrack(track);
                flowSub_ = 2;
                flowWaitS_ = 0.0f;
                return;
            }
            if (flowSub_ == 2) {
                if (flowWaitS_ < 1.0f) return;
                if (flowTap("PLAY")) flowSub_ = 3, flowWaitS_ = 0.0f;
                else if (timedOut) give("no PLAY on MODES");
                return;
            }
            if (flowRunAs(fm.mode, track) && flowWaitS_ > 0.5f) {
                flowCheck(true, "in by MODES's PLAY: the run is the mode at the centre on the Track chosen");
                flowRunId_ = w.runId();
                next(fm.mode == Mode::Jam || fm.mode == Mode::Set ? kHome : kEndRun);
            } else if (timedOut) {
                give("MODES's PLAY did not start the mode at the centre");
            }
            return;
        case kEndRun:
        case kEndRun2:
        case kEndRun3:
            if (screen_ != Screen::Run || !rest || flowWaitS_ < 1.0f) {
                if (timedOut) give("the run was not there to finish");
                return;
            }
            flowEndRun();
            if (screen_ == Screen::GameOver) {
                flowCheck(true, "the run plays to its end: the result is up");
                next(flowStep_ == kEndRun ? kRetry : (flowStep_ == kEndRun2 ? kNextTrack : kHome));
            } else {
                give("the run never reached its result");
            }
            return;
        case kRetry:
            if (flowSub_ == 0) {
                if (screen_ != Screen::GameOver || overS_ < 3.0f) return;
                if (flowTap("RETRY")) flowSub_ = 1, flowWaitS_ = 0.0f;
                else if (timedOut) give("no RETRY on the result");
                return;
            }
            if (flowRunAs(fm.mode, track) && w.runId() != flowRunId_ && flowWaitS_ > 0.5f) {
                flowCheck(true, "in by the result's RETRY: the same mode and Track again");
                flowRunId_ = w.runId();
                next(fm.mode == Mode::OneTrack ? kEndRun2 : kEndRun3);
            } else if (timedOut) {
                give("RETRY did not start the same mode again");
            }
            return;
        case kNextTrack:
            if (flowSub_ == 0) {
                if (screen_ != Screen::GameOver || overS_ < 3.0f) return;
                if (flowTap("NEXT TRACK")) flowSub_ = 1, flowWaitS_ = 0.0f;
                else if (timedOut) give("no NEXT TRACK on ONE TRACK's result");
                return;
            }
            if (flowRunAs(fm.mode, static_cast<u8>(track + 1u)) && flowWaitS_ > 0.5f) {
                flowCheck(true, "in by the result's NEXT TRACK: the next Track");
                flowRunId_ = w.runId();
                next(kEndRun3);
            } else if (timedOut) {
                give("NEXT TRACK did not start the next Track");
            }
            return;
        case kHome:
            // Out by the result's HOME (JAM and THE SET: by the pause, as their runs have no end the test waits for).
            if (flowSub_ == 0) {
                if (screen_ == Screen::Run && rest && flowWaitS_ > 1.5f) {
                    pauseRun();
                    flowSub_ = 2;
                    flowWaitS_ = 0.0f;
                    return;
                }
                if (screen_ != Screen::GameOver || overS_ < 3.0f) {
                    if (timedOut) give("no screen to leave by");
                    return;
                }
                if (flowTap("HOME")) flowSub_ = 1, flowWaitS_ = 0.0f;
                else if (timedOut) give("no HOME on the result");
                return;
            }
            if (flowSub_ == 2) {
                if (screen_ != Screen::Paused || flowWaitS_ < 0.5f) return;
                if (flowTap(fm.mode == Mode::Jam ? "LEAVE" : "QUIT RUN")) flowSub_ = 3, flowWaitS_ = 0.0f;
                else if (timedOut) give("no way out of the pause");
                return;
            }
            if (flowSub_ == 3) {
                if (flowWaitS_ < 0.3f) return;
                if (flowTap("YES")) flowSub_ = 1, flowWaitS_ = 0.0f;
                return;
            }
            if (screen_ == Screen::Title && rest && flowWaitS_ > 0.5f) {
                flowCheck(true, flowSub_ == 1 && fm.mode != Mode::Jam && fm.mode != Mode::Set ? "out by the result's HOME: the title"
                                                                                                : "out by the pause: the title");
                next(kNextMode);
            } else if (timedOut) {
                give("the way out did not reach the title");
            }
            return;
        case kNextMode:
        default:
            if (++flowMode_ >= kFlowModeCount) {
                flowDone_ = true;
                RF_LOG("FLOW: %u checks, %u failures (%u modes, every way in and out a run of each has on its own)", flowChecks_, flowFails_,
                       kFlowModeCount);
                pictureDone_ = true;
                return;
            }
            next(kTitlePlay);
            return;
    }
}

}  // namespace rf
