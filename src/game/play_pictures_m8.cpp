// M8 part 0 (docs/MODES_PLAN.md 7): the m8_ review pictures (scripts/review-m8.ps1 -Part p0; captures\m8\p0\): the title with each
// mode on its pill, MODES with each mode at the centre, and the transitions as strips a quarter beat apart (the rule's lift, BACK,
// another mode, another Track heard on the bar line, PLAY's drop-in). The room's pictures are co-op shots (review-m4b.ps1's m8_
// entries: --coop-shot lobby2 / modes / join with --coop-mode). Fixed 1/60 s steps and no band, like the other pictures: a Track
// chosen on MODES changes on the clock's next bar line.

#include <cmath>
#include <cstring>

#include "core/log.h"
#include "game/play.h"

namespace rf {

using namespace sim;

namespace {
enum class M8 : u8 { TitlePill, Modes, Lift, Back, Step, Track, Drop, LoadIn, CardOne, OneOutro, ResultOne, CardAct, ResultActs };
struct M8Pic {
    const char* name;  // m8_<name>
    M8 kind;
    u8 mode;           // sim::Mode
};
const M8Pic kM8Pics[] = {
    {"title_pill_set", M8::TitlePill, 0}, {"title_pill_one", M8::TitlePill, 1},     {"title_pill_headliners", M8::TitlePill, 2},
    {"title_pill_blast", M8::TitlePill, 3}, {"title_pill_tracers", M8::TitlePill, 4}, {"title_pill_songline", M8::TitlePill, 5},
    {"title_pill_jam", M8::TitlePill, 6},
    {"modes_set", M8::Modes, 0},          {"modes_one", M8::Modes, 1},              {"modes_headliners", M8::Modes, 2},
    {"modes_blast", M8::Modes, 3},        {"modes_tracers", M8::Modes, 4},          {"modes_songline", M8::Modes, 5},
    {"modes_jam", M8::Modes, 6},
    // Strips a quarter beat apart: the pill tapped on a bar's second beat (20 frames from the tap: the title leaving, the rule
    // rising, MODES struck, the cards thrown, the controls rising); BACK with TRACERS at the centre (24: the fold, the rule
    // sinking, the title building); the row stepped on from THE SET (8); a Track tapped with ONE TRACK at the centre (16: NEXT
    // BAR counting out, the bar line, the palette's cross-fade); PLAY with THE SET at the centre (16: the wait, the downbeat).
    {"t_lift", M8::Lift, 0}, {"t_back", M8::Back, 4}, {"t_mode", M8::Step, 0}, {"t_track", M8::Track, 1}, {"t_drop", M8::Drop, 0},
    // Part 1 (ONE TRACK; the run's Track in `mode`'s place): the load-in on Track 6 (a strip two sixteenths apart from the run's
    // first downbeat: 27 upgrades spiralling in), the name card on Track 4 (TRACK 4, AURORA, BEAT ITS BOSS), TRACK COMPLETE in
    // Track 1's outro and the result after it (the bot plays to the boss, hits costing nothing).
    {"loadin", M8::LoadIn, 6}, {"card_one", M8::CardOne, 4}, {"one_outro", M8::OneOutro, 1}, {"result_one", M8::ResultOne, 1},
    // Part 2 (HEADLINERS): act 1's card (ACT 1 OF 10, KALEIDOS, FIRST LIGHT 110 BPM) and the result after the ten acts.
    {"card_act", M8::CardAct, 0}, {"result_headliners", M8::ResultActs, 0},
};
constexpr u32 kM8Count = sizeof(kM8Pics) / sizeof(kM8Pics[0]);
constexpr u32 kM8At = 300;  // (the title at rest: as screen_title's)
}  // namespace

bool Play::scriptSetupM8() {
    const char* p = opt_.picture;
    if (std::strncmp(p, "m8_", 3) != 0) return false;
    for (u32 k = 0; k < kM8Count; ++k)
        if (std::strcmp(p + 3, kM8Pics[k].name) == 0) m8Pic_ = static_cast<u16>(k);
    if (m8Pic_ == 0xFFFF) {
        RF_ERR("play: no picture '%s'", p);
        return true;
    }
    World& w = *world_;
    const M8Pic& pic = kM8Pics[m8Pic_];
    const Mode m = static_cast<Mode>(pic.mode);
    flow_ = true;
    screen_ = Screen::Title;
    w.spawner().mode = Spawner::Mode::Slice;  // (the title's demo run behind)
    w.rules().godMode = true;
    // (the strips' frames a quarter beat apart)
    stripStride_ = static_cast<u32>(std::lround(900.0 / std::fmax(w.tempoAt(0.0), 30.0f)));
    switch (pic.kind) {
        case M8::TitlePill:
            records_.mode = pic.mode;  // (in memory only: pictures save nothing)
            break;
        case M8::Modes:
        case M8::Back:
        case M8::Step:
        case M8::Track:
        case M8::Drop:
            modesPictureSetup(m, false);
            modesPreview();
            if (previewWant_ >= 0) {
                // (at rest: the centre's Track heard and its palette on the screen since long ago)
                previewSong_ = previewWant_;
                if (const Palette* tp = trackPalette(static_cast<u32>(previewWant_) + 1u)) {
                    palFrom_ = *tp;
                    palTo_ = tp;
                    palFadeLine_ = 0.0;
                }
            }
            break;
        case M8::Lift: break;
        case M8::LoadIn:
        case M8::CardOne:
        case M8::OneOutro:
        case M8::ResultOne:
        case M8::CardAct:
        case M8::ResultActs: {
            // A ONE TRACK (or HEADLINERS) run from its first downbeat (hits cost nothing; the results' pictures have the screens).
            const bool acts = pic.kind == M8::CardAct || pic.kind == M8::ResultActs;
            RunSetup su;
            su.mode = acts ? Mode::Headliners : Mode::OneTrack;
            su.track = acts ? 1 : pic.mode;
            flow_ = pic.kind == M8::ResultOne || pic.kind == M8::ResultActs;
            screen_ = Screen::Run;
            w.resetRun(opt_.seed, 0.0, 1, 0xF, su);
            w.rules().godMode = true;
            controller_.reset(w.player(0).pos);
            marks_[0] = Mark{0.0f, 0, su.track};
            runTrackMax_ = su.track;
            if (pic.kind == M8::LoadIn) stripStride_ = static_cast<u32>(std::lround(2.0 * 900.0 / std::fmax(w.tempoAt(0.0), 30.0f)));
            if (pic.kind == M8::OneOutro || pic.kind == M8::ResultOne) scriptFastForwardS_ = 112.0f;  // (into the boss's fight)
            if (pic.kind == M8::ResultActs) scriptFastForwardS_ = 495.0f;  // (into act 10's fight)
            break;
        }
    }
    return true;
}

bool Play::scriptInputM8(StickInput& in) {
    if (m8Pic_ == 0xFFFF) return false;
    World& w = *world_;
    const M8Pic& pic = kM8Pics[m8Pic_];
    const u32 t = scriptTick_;
    const i64 now = static_cast<i64>(frameNo_);
    if (pic.kind == M8::LoadIn || pic.kind == M8::CardOne || pic.kind == M8::OneOutro || pic.kind == M8::ResultOne || pic.kind == M8::CardAct ||
        pic.kind == M8::ResultActs) {
        // A ONE TRACK run: the bot plays it.
        in = w.player(0).alive ? bot_.think(w, 0, controller_, clock_.heardNow()) : StickInput{};
        if (captureFrom_ >= 0 || ffRunning_) return true;
        const double bars = music1_ / kTicksPerBar;
        if (pic.kind == M8::LoadIn && t >= 1) captureFrom_ = now + 1;
        if ((pic.kind == M8::CardOne || pic.kind == M8::CardAct) && bars >= 1.25) captureFrom_ = now + 1;
        if (pic.kind == M8::ResultActs && screen_ == Screen::GameOver && overS_ > 2.4f) captureFrom_ = now + 1;
        if (pic.kind == M8::OneOutro && trackComplete_ && hudBarsSince(setCompleteT_) >= 2.0f) captureFrom_ = now + 1;
        if (pic.kind == M8::ResultOne && screen_ == Screen::GameOver && runCleared_ && overS_ > 2.4f) captureFrom_ = now + 1;
        if (captureFrom_ >= 0) RF_LOG("play: picture '%s' captures from frame %lld (bar %.2f)", opt_.picture, static_cast<long long>(captureFrom_), bars);
        return true;
    }
    // The title's demo plays behind (as in the game) until a transition takes the screen.
    in = trans_ == Trans::None || trans_ == Trans::Lift || trans_ == Trans::Fold ? bot_.think(w, 0, controller_, clock_.heardNow()) : StickInput{};
    if (trans_ == Trans::None || trans_ == Trans::Lift || trans_ == Trans::Fold) attractInput(in);
    if (captureFrom_ >= 0 || t < kM8At) return true;
    const double inBar = std::fmod(music1_, kTicksPerBar);
    const bool secondBeat = inBar >= kTicksPerBeat && inBar < kTicksPerBeat + 4.0;  // (a bar's second beat, as m6_drop_title's)
    switch (pic.kind) {
        case M8::TitlePill:
        case M8::Modes:
            captureFrom_ = now + 1;
            break;
        case M8::Lift:
            if (secondBeat) {
                openModes(Screen::Title);
                captureFrom_ = now + 1;
            }
            break;
        case M8::Back:
            if (secondBeat) {
                modesBackTap();
                captureFrom_ = now + 1;
            }
            break;
        case M8::Step:
            if (secondBeat) {
                modesStep(1);
                captureFrom_ = now + 1;
            }
            break;
        case M8::Track:
            if (secondBeat) {
                modesChooseTrack(7);
                captureFrom_ = now + 1;
            }
            break;
        default: break;
        case M8::Drop:
            if (secondBeat) {
                // (MODES's PLAY with THE SET at the centre)
                world_->setNextSetup(RunSetup{});
                previewWant_ = -1;
                dropInTap(Screen::Modes);
                captureFrom_ = now + 1;
            }
            break;
    }
    if (captureFrom_ >= 0) RF_LOG("play: picture '%s' captures from frame %lld", opt_.picture, static_cast<long long>(captureFrom_));
    return true;
}

bool Play::scriptEventsM8() { return m8Pic_ != 0xFFFF; }

float Play::hudBarsSince(float t) const {
    // (the HUD's own clock: view seconds as the run's cards count them)
    const double bpm = clock_.bpm() > 1.0f ? static_cast<double>(clock_.bpm()) : 120.0;
    return static_cast<float>((tickEnd_ - static_cast<double>(t)) * bpm / 240.0);
}

}  // namespace rf
