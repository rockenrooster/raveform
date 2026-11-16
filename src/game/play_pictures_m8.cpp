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
enum class M8 : u8 {
    TitlePill, Modes, Lift, Back, Step, Track, Drop, LoadIn, CardOne, OneOutro, ResultOne, CardAct, ResultActs,
    TrLine, TrLock, TrPop, TrFloor4, TrHouse, TrCount, TrResult,
    SlGroove, SlBuild, SlDrop, SlInTime, SlBraid, SlFinale, SlCount, SlResult,
    JamRoom1, JamRoom4, JamRoom8, JamShelf, JamDropReady, JamDropResting, JamRoomDrop, PopDraft, PopDrop
};
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
    // Part 4 (TRACERS on Reef, the ring-flier flying; tracers.md "What to check"): a line half drawn with its hint, a ring just
    // locked round a pack, the pop as a strip a thirty-second apart from the lock, the floor at 60% in four colours, FULL HOUSE as a
    // strip a quarter beat apart from its bar line, the count at the end (a strip), the result.
    {"tr_line", M8::TrLine, 3}, {"tr_lock", M8::TrLock, 3}, {"tr_pop", M8::TrPop, 3}, {"tr_floor4", M8::TrFloor4, 3},
    {"tr_house", M8::TrHouse, 3}, {"tr_count", M8::TrCount, 3}, {"tr_result", M8::TrResult, 3},
    // Part 5 (SONGLINE on Aurora, the rider flying; songline.md "What to check"): a Groove's string with the guide, a Build's runway,
    // a Drop's tube as a strip a sixteenth apart, a gate threaded in time, four players' braid (the other three seats stand), the
    // finale as a strip a quarter beat apart from bar 47, the count, the result.
    {"sl_groove", M8::SlGroove, 4}, {"sl_build", M8::SlBuild, 4}, {"sl_drop", M8::SlDrop, 4}, {"sl_intime", M8::SlInTime, 4},
    {"sl_braid", M8::SlBraid, 4}, {"sl_finale", M8::SlFinale, 4}, {"sl_count", M8::SlCount, 4}, {"sl_result", M8::SlResult, 4},
    // Part 6 (JAM on Toybox; jam.md "What to check"): the room with one layer on, with four, with all eight (the mirror ball, the fan,
    // the beams), a shelf open, DROP ready and resting, the room's Drop; POP-OFF's draft (two seats) and its Drop.
    {"jam_room1", M8::JamRoom1, 12}, {"jam_room4", M8::JamRoom4, 12}, {"jam_room8", M8::JamRoom8, 12}, {"jam_shelf", M8::JamShelf, 12},
    {"jam_drop_ready", M8::JamDropReady, 12}, {"jam_drop_resting", M8::JamDropResting, 12}, {"jam_room_drop", M8::JamRoomDrop, 12},
    {"popoff_draft", M8::PopDraft, 12}, {"popoff_drop", M8::PopDrop, 12},
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
        case M8::TrLine:
        case M8::TrLock:
        case M8::TrPop:
        case M8::TrFloor4:
        case M8::TrHouse:
        case M8::TrCount:
        case M8::TrResult: {
            // A TRACERS run on Reef from its first downbeat, the ring-flier flying (hits cost nothing); the floor's pictures with four
            // seats (the other three stand) and a floor set as the picture needs it.
            RunSetup su;
            su.mode = Mode::Tracers;
            su.track = pic.mode;
            flow_ = pic.kind == M8::TrResult;
            screen_ = Screen::Run;
            const u32 seats = pic.kind == M8::TrFloor4 ? 4u : 1u;
            w.resetRun(opt_.seed, 0.0, seats, 0xF, su);
            w.rules().godMode = true;
            controller_.reset(w.player(0).pos);
            bot_.reset(opt_.seed, Bot::Policy::Play);
            marks_[0] = Mark{0.0f, 0, su.track};
            runTrackMax_ = su.track;
            if (pic.kind == M8::TrPop) stripStride_ = static_cast<u32>(std::lround(0.125 * 3600.0 / std::fmax(w.tempoAt(0.0), 30.0f)));  // (a 32nd)
            if (pic.kind == M8::TrLine || pic.kind == M8::TrLock || pic.kind == M8::TrPop) scriptFastForwardS_ = 20.0f;  // (a crowd in)
            if (pic.kind == M8::TrFloor4 || pic.kind == M8::TrHouse) scriptFastForwardS_ = 20.0f;
            if (pic.kind == M8::TrCount || pic.kind == M8::TrResult) scriptFastForwardS_ = 90.0f;  // (into the last bars)
            break;
        }
        case M8::JamRoom1:
        case M8::JamRoom4:
        case M8::JamRoom8:
        case M8::JamShelf:
        case M8::JamDropReady:
        case M8::JamDropResting:
        case M8::JamRoomDrop:
        case M8::PopDraft:
        case M8::PopDrop: {
            // JAM on Toybox (the room: the layers asked for on its first bar, so they come on with the second), or POP-OFF with two seats.
            const bool pop = pic.kind == M8::PopDraft || pic.kind == M8::PopDrop;
            RunSetup su;
            su.mode = Mode::Jam;
            su.track = pic.mode;
            su.versus = pop;
            screen_ = Screen::Run;
            w.resetRun(opt_.seed, 0.0, pop ? 2u : 1u, 0xF, su);
            w.rules().godMode = true;
            controller_.reset(w.player(0).pos);
            bot_.reset(opt_.seed, Bot::Policy::Play);
            marks_[0] = Mark{0.0f, 0, su.track};
            runTrackMax_ = su.track;
            const u32 layers = pic.kind == M8::JamRoom1 ? 1u : (pic.kind == M8::JamRoom4 ? 4u : (pic.kind == M8::JamShelf ? 3u : (pop ? 0u : 8u)));
            for (u32 k = 1; k < layers; ++k) w.jamAsk(0, static_cast<u8>(k));
            if (pic.kind == M8::JamDropResting || pic.kind == M8::JamRoomDrop) w.jamAsk(0, ModesData::Jam::kPadDrop);
            const float barS = 240.0f / std::fmax(w.tempoAt(0.0), 30.0f);
            if (pic.kind == M8::JamDropResting) scriptFastForwardS_ = 14.5f * barS;
            if (pic.kind == M8::JamRoomDrop) scriptFastForwardS_ = 6.5f * barS;
            if (pic.kind == M8::PopDrop) scriptFastForwardS_ = 20.0f * barS;
            break;
        }
        case M8::SlGroove:
        case M8::SlBuild:
        case M8::SlDrop:
        case M8::SlInTime:
        case M8::SlBraid:
        case M8::SlFinale:
        case M8::SlCount:
        case M8::SlResult: {
            // A SONGLINE run on Aurora from its first downbeat, the rider flying (hits cost nothing); the braid with four seats (the
            // other three stand where they began).
            RunSetup su;
            su.mode = Mode::Songline;
            su.track = pic.mode;
            flow_ = pic.kind == M8::SlResult;
            screen_ = Screen::Run;
            w.resetRun(opt_.seed, 0.0, pic.kind == M8::SlBraid ? 4u : 1u, 0xF, su);
            w.rules().godMode = true;
            controller_.reset(w.player(0).pos);
            // (a Groove's string and a Drop's tube are seen from a bar behind: the half-speed bot; the rest the rider)
            bot_.reset(opt_.seed, pic.kind == M8::SlGroove || pic.kind == M8::SlDrop ? Bot::Policy::Slow : Bot::Policy::Play);
            marks_[0] = Mark{0.0f, 0, su.track};
            runTrackMax_ = su.track;
            const float barS = 240.0f / std::fmax(w.tempoAt(0.0), 30.0f);
            if (pic.kind == M8::SlDrop) stripStride_ = static_cast<u32>(std::lround(0.25 * 3600.0 / std::fmax(w.tempoAt(0.0), 30.0f)));  // (a 16th)
            if (pic.kind == M8::SlBuild) scriptFastForwardS_ = 9.5f * barS;
            if (pic.kind == M8::SlDrop || pic.kind == M8::SlInTime) scriptFastForwardS_ = 13.0f * barS;
            if (pic.kind == M8::SlFinale || pic.kind == M8::SlCount || pic.kind == M8::SlResult) scriptFastForwardS_ = 45.0f * barS;
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
    if (pic.kind >= M8::JamRoom1) {
        // JAM: the ship stands in the room (the shelf's picture: it holds TOYS, then rests by the shelf); POP-OFF: the bot plays.
        const bool pop = pic.kind == M8::PopDraft || pic.kind == M8::PopDrop;
        in = pop && w.player(0).alive ? bot_.think(w, 0, controller_, clock_.heardNow()) : StickInput{};
        if (pic.kind == M8::JamShelf && w.jam().shelf[0].pad == 0xFF) {
            const Vec2 to = w.species().modes.jam.pad[ModesData::Jam::kPadToys].at - w.player(0).pos;
            const float l = length(to);
            if (l > 8.0f) in.move = to * (1.0f / l) * (l < 60.0f ? 0.5f : 1.0f);
        }
        if (pic.kind == M8::JamDropReady) {
            const Vec2 to = w.species().modes.jam.pad[ModesData::Jam::kPadDrop].at + Vec2{-150.0f, 0.0f} - w.player(0).pos;
            const float l = length(to);
            if (l > 8.0f) in.move = to * (1.0f / l) * (l < 60.0f ? 0.5f : 1.0f);
        }
        if (captureFrom_ >= 0 || ffRunning_) return true;
        const double bars = (music1_ - w.director().startTick) / kTicksPerBar;
        if ((pic.kind == M8::JamRoom1 || pic.kind == M8::JamRoom4 || pic.kind == M8::JamRoom8) && bars >= 3.25) captureFrom_ = now + 1;
        if (pic.kind == M8::JamShelf && w.jam().shelf[0].pad != 0xFF && bars >= 4.0) captureFrom_ = now + 1;
        if (pic.kind == M8::JamDropReady && bars >= 3.5) captureFrom_ = now + 1;
        if (pic.kind == M8::JamDropResting && bars >= 15.5) captureFrom_ = now + 1;
        if (pic.kind == M8::JamRoomDrop && w.phrase().section == Section::Drop && w.phrase().sectionBar >= 1) captureFrom_ = now + 1;
        if (pic.kind == M8::PopDraft && w.trio(0).open && bars >= 3.3) captureFrom_ = now + 1;
        if (pic.kind == M8::PopDrop && w.phrase().section == Section::Drop && w.phrase().sectionBar >= 2) captureFrom_ = now + 1;
        if (captureFrom_ >= 0) RF_LOG("play: picture '%s' captures from frame %lld (bar %.2f)", opt_.picture, static_cast<long long>(captureFrom_), bars);
        return true;
    }
    if (pic.kind >= M8::SlGroove) {
        // SONGLINE: the rider flies (the other seats of the braid stand); each picture waits for its moment.
        in = w.player(0).alive ? bot_.think(w, 0, controller_, clock_.heardNow()) : StickInput{};
        if (captureFrom_ >= 0 || ffRunning_) return true;
        const double bars = (music1_ - w.director().startTick) / kTicksPerBar;
        if (pic.kind == M8::SlGroove && bars >= 5.5) captureFrom_ = now + 1;
        if (pic.kind == M8::SlBraid && bars >= 6.5) captureFrom_ = now + 1;
        if (pic.kind == M8::SlBuild && bars >= 10.6) captureFrom_ = now + 1;
        if (pic.kind == M8::SlDrop && bars >= 14.0) captureFrom_ = now + 1;
        if (pic.kind == M8::SlInTime && bars >= 13.0 && w.gatesInTime(0) > m8Last_ && m8Last_ > 0) captureFrom_ = now + 1;
        if (pic.kind == M8::SlInTime) m8Last_ = w.gatesInTime(0);
        if (pic.kind == M8::SlFinale && bars >= 47.0) captureFrom_ = now + 1;
        if (pic.kind == M8::SlCount && songCountT_ > 0.0f && static_cast<float>(tickEnd_) - songCountT_ > 1.6f * 60.0f / std::fmax(clock_.bpm(), 30.0f))
            captureFrom_ = now + 1;
        if (pic.kind == M8::SlResult && screen_ == Screen::GameOver && overS_ > 2.4f) captureFrom_ = now + 1;
        if (captureFrom_ >= 0) RF_LOG("play: picture '%s' captures from frame %lld (bar %.2f)", opt_.picture, static_cast<long long>(captureFrom_), bars);
        return true;
    }
    if (pic.kind >= M8::TrLine) {
        // TRACERS: the ring-flier flies; each picture waits for its moment.
        in = w.player(0).alive ? bot_.think(w, 0, controller_, clock_.heardNow()) : StickInput{};
        if (captureFrom_ >= 0 || ffRunning_) return true;
        if (pic.kind == M8::TrFloor4 || pic.kind == M8::TrHouse) {
            if (!m8FloorSet_) {
                // The floor at 60% in four colours (a quarter of the room's ellipse each), or (the house) all but a hole by the ship.
                static u8 owners[TracerFloor::kTiles];
                for (u32 tl = 0; tl < TracerFloor::kTiles; ++tl) {
                    const Vec2 c = w.tileCentre(tl);
                    const float ex = c.x / w.halfW(), ey = c.y / w.halfH();
                    if (pic.kind == M8::TrFloor4) {
                        const u8 q = static_cast<u8>((c.x < 0.0f ? 0u : 1u) + (c.y < 0.0f ? 0u : 2u));
                        owners[tl] = ex * ex + ey * ey < 0.87f * 0.87f ? q : TracerFloor::kDark;
                    } else {
                        owners[tl] = 0;
                    }
                }
                w.scriptFloor(owners, pic.kind == M8::TrHouse);
                m8FloorSet_ = true;
            }
            if (pic.kind == M8::TrFloor4 && t >= 60) captureFrom_ = now + 1;
            if (pic.kind == M8::TrHouse && w.tracerFloor().chaseTick < 0.0 && w.tracerFloor().popTick >= 0.0) captureFrom_ = now;  // (the chase's bar line)
        }
        if (pic.kind == M8::TrLine) {
            Vec2 at{};
            float dist = 0.0f;
            if (w.tracer(0).count > 60 && w.closingHint(0, at, dist) && dist > w.species().modes.tracers.snapWu + 20.0f) captureFrom_ = now + 1;
        }
        if (pic.kind == M8::TrLock || pic.kind == M8::TrPop) {
            for (u32 k = 0; k < World::kRings; ++k) {
                const TracerRing& r = w.ring(k);
                if (r.active && !r.popped && r.caught >= 8 && r.lockTick >= music1_ - 4.0) {
                    captureFrom_ = now + 1;
                    break;
                }
            }
        }
        if (pic.kind == M8::TrCount && w.phrase().barsLeft <= 1 && music1_ >= w.tickOfBar(w.phrase().bar)) {
            const double end = w.tickOfBar(static_cast<u32>(w.species().modes.forms.timed48.bars()));
            if (music1_ >= end) captureFrom_ = now + 1;
        }
        if (pic.kind == M8::TrResult && screen_ == Screen::GameOver && overS_ > 2.4f) captureFrom_ = now + 1;
        if (captureFrom_ >= 0) RF_LOG("play: picture '%s' captures from frame %lld (bar %.2f)", opt_.picture, static_cast<long long>(captureFrom_), music1_ / kTicksPerBar);
        return true;
    }
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
