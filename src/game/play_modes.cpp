// M8 part 1 (docs/MODES_PLAN.md 3: "the flow: choosing, starting, ending, records; the load-in's band and HUD side"): a mode's run in
// the game. ONE TRACK (design/modes/one_track.md): the load-in (README.md 5: the kit's icons spiralling in from a 300 wu circle, one
// landing a sixteenth with its chirp climbing the Track's scale, a ring and the "upgrade taken" chord on bar 3's downbeat), TRACK
// COMPLETE through the outro, the result (TRACK COMPLETE or RUN OVER, the Track's own best), NEXT TRACK. Sounds the engine does not
// have yet (the bed's layers by the landings, the outro's held chord and reversed cymbal) are listed in game/pending_sounds.cpp's
// notes and PROGRESS.

#include <cmath>
#include <cstdio>

#include "audio/band.h"
#include "audio/cues.h"
#include "core/log.h"
#include "game/play.h"
#include "game/play_hud.h"

namespace rf {

using namespace sim;

namespace {
struct LookCtx {
    const Look* look;
    const LookVariant* variant;
};
Vec3 lookColour(const void* user, Vec3 c, float i) {
    const LookCtx* k = static_cast<const LookCtx*>(user);
    return hudColour(*k->look, *k->variant, c, i);
}
float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
// The load-in's climbing run (README.md 5: a chirp a landing, one note further up the Track's scale, two octaves, then again from
// the middle): the minor pentatonic over two octaves.
constexpr i32 kClimb[11] = {0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24};
}  // namespace

void Play::modesEvent(const Event& e) {
    const World& w = *world_;
    switch (e.type) {
        case EventType::KitLand: {
            if (e.a != seat_) break;
            // The Shard's chirp one note further up (a Riff's landing: its first note in its own voice: the "upgrade taken" cue
            // stands in until the engine plays a Riff's voice on demand).
            if (bandDrives_ && band_) {
                const u32 k = kitChirps_ < 11 ? kitChirps_ : 5u + (kitChirps_ - 11u) % 6u;
                const float rate = std::exp2(static_cast<float>(kClimb[k]) / 12.0f);
                if (isRiff(e.b)) band_->playSfx(audio::Sfx::Upgrade, 0.45f, rate);
                else band_->playSfx(audio::Sfx::Shard, 0.9f, rate);
            }
            ++kitChirps_;
            break;
        }
        case EventType::BossEnter:
            if (!isMiniBoss(w.boss().kind)) bossFromS_ = runSeconds_;
            break;
        case EventType::BossDeath:
            if (isMiniBoss(static_cast<BossKind>(e.a))) break;
            if (bossFromS_ >= 0.0f) bossSecs_ = runSeconds_ - bossFromS_;
            if (w.setup().mode == Mode::OneTrack && !trackComplete_) {
                // one_track.md: the outro: TRACK COMPLETE with the score under it, where the name card stood, for its four bars.
                trackComplete_ = true;
                setCompleteT_ = static_cast<float>(tickEnd_ - e.age);
                setCompleteScore_ = w.player(seat_).score;
                RF_LOG("play: TRACK COMPLETE (Track %u, run %.1f s, the boss in %.1f s)", w.phrase().track, runSeconds_, bossSecs_);
            }
            break;
        case EventType::RunEnd:
            // The outro is over: the result (the Track cleared).
            runCleared_ = true;
            RF_LOG("play: the run ends (Track %u cleared, %.1f s, score %llu)", w.phrase().track, runSeconds_,
                   static_cast<unsigned long long>(w.player(seat_).score));
            if (flow_ && screen_ == Screen::Run) {
                endRun(false);
                screen_ = Screen::GameOver;
                overS_ = -0.6f;
            } else if (!flow_ || opt_.bot) {
                retryRequest_ = true;  // (a benchmark or the bot: the same Track again)
            }
            break;
        case EventType::Restart:
            runCleared_ = trackComplete_ = kitClosed_ = false;
            kitChirps_ = 0;
            bossFromS_ = bossSecs_ = -1.0f;
            break;
        default: break;
    }
}

void Play::modesFrameRun() {
    // README.md 5: bar 3's downbeat: a ring in the player's colours leaves the ship (90 to 400 wu in half a beat; reduce flashing:
    // none), the HUD's band row flashes once, the "upgrade taken" chord.
    const World& w = *world_;
    const World::KitState& k = w.kit(seat_);
    if (kitClosed_ || !k.count || k.landed < k.count) return;
    const ModesData::LoadIn& li = species_->modes.loadIn;
    const double close = k.from + static_cast<double>(li.bars) * kTicksPerBar;
    if (music1_ < close) return;
    kitClosed_ = true;
    const PlayerState& pl = w.player(seat_);
    const float beatS = 60.0f / std::fmax(clock_.bpm(), 30.0f);
    const u32 q = seat_ < kMaxPlayers ? seat_ : 0u;
    if (!(settings_.reduceFlashing || forcedReduceFlashing_))
        view_.loadInRing(pl.pos, li.closeRingWu[1], palettes_->player[q], palettes_->playerLight[q], static_cast<float>(tickEnd_), li.closeRingBeats * beatS);
    takenT_ = static_cast<float>(tickEnd_);  // (the band row's flash)
    if (bandDrives_ && band_) band_->playSfx(audio::Sfx::Upgrade, 1.0f, 1.0f);
}

void Play::drawLoadIn(FrameDraw& d, const LookVariant& variant) {
    // README.md 5: the kit's icons on a circle of 300 wu round the ship, in the order they were dealt, each flying in on a quarter
    // turn of spiral over the beat before its landing, at half an orb's size with a thin trail in the player's colour.
    const World& w = *world_;
    const World::KitState& k = w.kit(seat_);
    if (!k.count || k.landed >= k.count) return;
    const ModesData::LoadIn& li = species_->modes.loadIn;
    const LookCtx ctx{look_, &variant};
    const PlayerState& pl = w.player(seat_);
    const Vec2 ship = pl.prevPos + (pl.pos - pl.prevPos) * alpha_;
    const double now = musicPrev_ + (music1_ - musicPrev_) * static_cast<double>(alpha_);
    const float px = view_.pxPerWu();
    const Vec3 tone = palettes_->player[seat_ < kMaxPlayers ? seat_ : 0];
    for (u32 i = k.landed; i < k.count; ++i) {
        const double land = w.kitLandTick(seat_, i);
        const float u = static_cast<float>(1.0 - (land - now) / kTicksPerBeat);  // 0 a beat before its landing .. 1 landed
        if (u < 0.0f) break;  // (the later ones wait their turn on the circle)
        const float a0 = 6.2831853f * static_cast<float>(i) / static_cast<float>(k.count) - 1.5707963f;
        const float e = 1.0f - (1.0f - clamp01(u)) * (1.0f - clamp01(u));
        const float a = a0 + 6.2831853f * li.spiralTurns * e;
        const float r = li.ringWu * (1.0f - e);
        const Vec2 at = ship + Vec2{std::cos(a), std::sin(a)} * r;
        const Vec2 sp = view_.toScreen(at);
        const u8 orb = k.orb[i];
        const i32 icon = isFusionOrb(orb) ? fusionIcons_[fusionOfOrb(orb)] : upgradeIcons_[orb < kUpgradeCount ? orb : 0];
        const float size = 34.0f * px * li.iconScale * (0.6f + 0.4f * clamp01(u * 3.0f));
        if (u > 0.05f) {
            // the trail: back along the spiral a little
            const float ab = a0 + 6.2831853f * li.spiralTurns * std::fmax(0.0f, e - 0.15f);
            const float rb = li.ringWu * (1.0f - std::fmax(0.0f, e - 0.15f));
            const Vec2 back = view_.toScreen(ship + Vec2{std::cos(ab), std::sin(ab)} * rb);
            const Vec3 c = hudColour(*look_, variant, tone, 0.9f);
            d.hud.line(back, sp, 0.8f, Vec3{}, c, 0);
        }
        icons_.drawHud(d.hud, icon, sp, size, 1.2f * clamp01(u * 4.0f), lookColour, &ctx);
    }
}

void Play::modesEndRun(bool died) {
    // README.md 9: a solo ONE TRACK run's record, `one_<track>`: its best score, whether the Track was ever cleared, the seconds of the
    // best run. Never the set's table. (Co-op keeps team_<mode>_<track>; versus nothing but the room's crowns.)
    (void)died;
    const World& w = *world_;
    const RunSetup& su = w.setup();
    const PlayerState& p = w.player(seat_);
    const bool keep = !opt_.picture[0] && !opt_.fixedStep;
    finalScore_ = p.score;
    newBest_ = false;
    modeBest_ = 0;
    if (su.mode == Mode::OneTrack && su.track >= 1 && su.track <= 10) {
        ModeRecords::One& r = records_.one[su.track - 1u];
        modeBest_ = r.score;
        if (coop()) {
            ModeRecords::Team& t = records_.team[static_cast<u32>(Mode::OneTrack)][su.track - 1u];
            u64 team = 0;
            u8 players = 0;
            for (u32 q = 0; q < kMaxPlayers; ++q)
                if (w.player(q).present) {
                    team += w.player(q).score;
                    ++players;
                }
            if (!su.versus && team > t.score) {
                t.score = team;
                t.players = players;
            }
        } else {
            newBest_ = p.score > 0 && p.score > r.score;
            if (newBest_) {
                r.score = p.score;
                r.seconds = static_cast<u32>(runSeconds_);
            }
            if (runCleared_) r.cleared = true;
        }
        if (keep && !su.versus) saveModeRecords(records_);
    }
    runEndedBy_[0] = 0;
    if (died) causeName(p.lastHitCause, runEndedBy_, sizeof runEndedBy_);
}

void Play::nextTrack() {
    // one_track.md rule 7: NEXT TRACK starts the next Track as a new run with its own kit (RETRY's drop-in); nothing carries over.
    const RunSetup& cur = world_->setup();
    if (cur.mode != Mode::OneTrack || cur.track >= 10 || trans_ != Trans::None) return;
    RunSetup su = cur;
    su.track = static_cast<u8>(cur.track + 1u);
    world_->setNextSetup(su);
    records_.track[static_cast<u32>(Mode::OneTrack)] = su.track;  // (the pill's Track follows)
    if (!opt_.picture[0]) saveModeRecords(records_);
    if (world_->restartPending()) world_->cancelRestart();
    dropInTap(Screen::GameOver);
    RF_LOG("play: NEXT TRACK: Track %u", su.track);
}

}  // namespace rf
