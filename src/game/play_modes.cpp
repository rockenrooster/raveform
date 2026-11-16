// M8 part 1 (docs/MODES_PLAN.md 3: "the flow: choosing, starting, ending, records; the load-in's band and HUD side"): a mode's run in
// the game. ONE TRACK (design/modes/one_track.md): the load-in (README.md 5: the kit's icons spiralling in from a 300 wu circle, one
// landing a sixteenth with its chirp climbing the Track's scale, a ring and the "upgrade taken" chord on bar 3's downbeat), TRACK
// COMPLETE through the outro, the result (TRACK COMPLETE or RUN OVER, the Track's own best), NEXT TRACK. Sounds the engine does not
// have yet (the bed's layers by the landings, the outro's held chord and reversed cymbal) are listed in game/pending_sounds.cpp's
// notes and PROGRESS.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "audio/band.h"
#include "audio/cues.h"
#include "core/log.h"
#include "game/play.h"
#include "game/play_hud.h"
#include "net/session.h"

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
void groupDigits(char* out, size_t cap, u64 v) {
    char raw[32];
    std::snprintf(raw, sizeof raw, "%llu", static_cast<unsigned long long>(v));
    const size_t n = std::strlen(raw);
    size_t o = 0;
    for (size_t i = 0; i < n && o + 2 < cap; ++i) {
        if (i && (n - i) % 3 == 0) out[o++] = ',';
        out[o++] = raw[i];
    }
    out[o] = 0;
}
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
            if (w.setup().mode == Mode::Headliners && w.phrase().track >= 1 && w.phrase().track <= 10) actSecs_[w.phrase().track - 1u] = bossSecs_;
            if ((w.setup().mode == Mode::OneTrack || (w.setup().mode == Mode::Headliners && w.phrase().track >= 10)) && !trackComplete_) {
                // one_track.md: the outro: TRACK COMPLETE with the score under it, where the name card stood, for its four bars.
                trackComplete_ = true;
                setCompleteT_ = static_cast<float>(tickEnd_ - e.age);
                setCompleteScore_ = w.player(seat_).score;
                RF_LOG("play: TRACK COMPLETE (Track %u, run %.1f s, the boss in %.1f s)", w.phrase().track, runSeconds_, bossSecs_);
            }
            break;
        case EventType::Crown:
            // V5: the crown changes hands on a bar line, with a bell.
            if (bandDrives_ && band_ && e.a != 0xFF) band_->playRecipe(audio::Recipe::Bell, 84, 0.6f);
            break;
        case EventType::Knock:
            if (e.a == seat_) pulse(2);
            break;
        case EventType::RunEnd:
            // The outro is over: the result (the Track cleared).
            runCleared_ = true;
            RF_LOG("play: the run ends (Track %u cleared, %.1f s, score %llu)", w.phrase().track, runSeconds_,
                   static_cast<unsigned long long>(w.player(seat_).score));
            if (flow_ && (screen_ == Screen::Run || (coop() && (screen_ == Screen::Paused || screen_ == Screen::Settings)))) {
                endRun(false);
                coopOver_ = coop();
                screen_ = Screen::GameOver;
                overS_ = -0.6f;
                podiumS_ = 0.0f;
                podiumCounted_ = false;
                podiumWinner_ = 0xFF;
                if (w.setup().versus) {
                    // The winner: the best count; a draw when the top is level.
                    u64 best = 0;
                    u32 at = 0;
                    for (u32 q = 0; q < kMaxPlayers; ++q) {
                        if (!w.player(q).present) continue;
                        if (podiumWinner_ == 0xFF || w.versusCount(q) > best) {  // (TRACERS: the tiles in each colour)
                            best = w.versusCount(q);
                            podiumWinner_ = static_cast<u8>(q);
                            at = 1;
                        } else if (w.versusCount(q) == best) {
                            ++at;
                        }
                    }
                    if (at != 1) podiumWinner_ = 0xFF;
                    if (session_ && session_->host() && podiumWinner_ != 0xFF) {
                        // V8: the room keeps each seat's wins as crowns while it stands (the lobby's message carries them).
                        const u8 c = session_->crowns(podiumWinner_);
                        session_->setCrowns(podiumWinner_, static_cast<u8>(c < 255 ? c + 1 : 255));
                        podiumCounted_ = true;
                    }
                    RF_LOG("play: the versus match ends: %s", podiumWinner_ == 0xFF ? "a draw" : "a winner");
                }
            } else if (!flow_ || opt_.bot) {
                retryRequest_ = true;  // (a benchmark or the bot: the same Track again)
            }
            break;
        case EventType::Restart:
            for (float& a : actSecs_) a = 0.0f;
            runCleared_ = trackComplete_ = kitClosed_ = false;
            kitChirps_ = 0;
            bossFromS_ = bossSecs_ = -1.0f;
            break;
        // M8 TRACERS (tracers.md "Sound"; sound.md "TRACERS"): a lock is a glass tick on the chord's root at once (a fizzle one
        // woodblock); the pop is the ring's chord on its eighth line, larger by the bonus (x 5 a tom on the root, x 8 at 1.2 with
        // the sub drop); a full house is a tom roll over the chase into the Drop's impact, the gong and the stab.
        case EventType::RingLock:
            if (!bandDrives_ || !band_) break;
            if (e.c) {
                if (e.a == seat_) band_->playRecipe(audio::Recipe::Woodblock, -1, 0.7f);
            } else {
                band_->playRecipe(audio::Recipe::Glass, -1, e.a == seat_ ? 0.9f : 0.5f);
                const double at = e.tick + (coop() ? netBandOffset_ : clock_.offset());
                const u32 bonus = w.species().modes.tracers.bonusFor(e.b);
                const float gain = bonus >= 8 ? 1.2f : (bonus >= 5 ? 1.0f : (bonus >= 3 ? 0.85f : (bonus >= 2 ? 0.7f : 0.55f)));
                band_->playRecipe(audio::Recipe::Stab, -1, e.a == seat_ ? gain : 0.6f * gain, 0.0f, at, bonus >= 5 ? 1.5f : 1.0f);
                if (bonus >= 5) band_->playRecipe(audio::Recipe::Tom, -1, 0.8f, 0.0f, at);
                if (bonus >= 8) band_->playRecipe(audio::Recipe::SubDrop, -1, 0.9f, 0.0f, at);
            }
            break;
        case EventType::House:
            if (e.b == 1 && markCount_ < kMarks) marks_[markCount_++] = Mark{runSeconds_, 5, 0};  // (the result's gold star)
            if (!bandDrives_ || !band_) break;
            if (e.b == 0) {
                const double pop = e.tick + 2.0 * kTicksPerBeat + (coop() ? netBandOffset_ : clock_.offset());
                band_->playRecipe(audio::Recipe::TomRoll, -1, 0.9f, 0.0f, pop, 2.0f);
                band_->playRecipe(audio::Recipe::Impact, -1, 0.9f, 0.0f, pop);
                band_->playRecipe(audio::Recipe::Gong, -1, 0.7f, 0.0f, pop);
                band_->playRecipe(audio::Recipe::Stab, -1, 1.1f, 0.0f, pop, 2.0f);
                pulse(2);
            }
            break;
        case EventType::RingPop:
            if (e.c >= 5 && e.a == seat_ && markCount_ < kMarks) marks_[markCount_++] = Mark{runSeconds_, 6, e.a};  // (the result's white tick)
            break;
        case EventType::FloorCount:
            // The count: the "upgrade taken" chord as the sweep reaches the far wall (the chirps a column are the engine's part to
            // come: pending_sounds.cpp).
            if (bandDrives_ && band_ && e.a == seat_) band_->sfxAt(audio::Sfx::Upgrade, e.tick + 2.0 * kTicksPerBeat + (coop() ? netBandOffset_ : clock_.offset()));
            break;
        case EventType::GelTake:
            if (bandDrives_ && band_ && e.a == seat_) band_->playSfx(audio::Sfx::Upgrade, 0.8f);
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
    if (su.mode == Mode::FullBlast && !coop() && su.track >= 1 && su.track <= ModeRecords::kTracks) {
        // README.md 9, `blast_<track>`: the best score and its best Chain.
        ModeRecords::Timed& r = records_.blast[su.track - 1u];
        modeBest_ = r.score;
        newBest_ = p.score > 0 && p.score > r.score;
        if (newBest_) {
            r.score = p.score;
            r.second = p.bestChain;
        }
        if (keep && !su.versus) saveModeRecords(records_);
    }
    if (su.mode == Mode::Tracers) {
        // tracers.md "Numbers", `tracers_<track>`: the best score and its full houses (co-op: the team's). The result's numbers and
        // the floor as the run ended (screens.md "Results").
        const TracerFloor& f = w.tracerFloor();
        std::memcpy(resultFloor_, f.owner, sizeof resultFloor_);
        u32 mine = 0;
        for (u8 o : f.owner) mine += (o == seat_ || (!w.setup().versus && o != TracerFloor::kDark)) ? 1u : 0u;
        resultFloorShare_ = static_cast<float>(mine) / static_cast<float>(TracerFloor::kTiles);
        resultHouses_ = f.houses;
        resultBestRing_ = static_cast<u16>(w.bestRing(seat_));
        if (su.track >= 1 && su.track <= ModeRecords::kTracks) {
            if (coop()) {
                ModeRecords::Team& t = records_.team[static_cast<u32>(Mode::Tracers)][su.track - 1u];
                u64 team = 0;
                u8 players = 0;
                for (u32 q = 0; q < kMaxPlayers; ++q)
                    if (w.player(q).present) {
                        team += w.player(q).score;
                        ++players;
                    }
                modeBest_ = t.score;
                if (!su.versus && team > t.score) {
                    t.score = team;
                    t.players = players;
                }
            } else {
                ModeRecords::Timed& r = records_.tracers[su.track - 1u];
                modeBest_ = r.score;
                newBest_ = p.score > 0 && p.score > r.score;
                if (newBest_) {
                    r.score = p.score;
                    r.second = f.houses;
                }
            }
            if (keep && !su.versus) saveModeRecords(records_);
        }
    }
    if (su.mode == Mode::Headliners && !coop()) {
        // README.md 9, `headliners`: the acts cleared, then the time (ten in 9:42 beats ten in 10:05, and both beat nine).
        u32 acts = 0;
        for (float a : actSecs_) acts += a > 0.0f ? 1u : 0u;
        const u32 secs = static_cast<u32>(runSeconds_);
        modeBest_ = records_.headlinersActs;
        newBest_ = acts > records_.headlinersActs || (acts == records_.headlinersActs && acts > 0 && secs < records_.headlinersSeconds);
        if (newBest_) {
            records_.headlinersActs = acts;
            records_.headlinersSeconds = secs;
        }
        if (keep && !su.versus) saveModeRecords(records_);
    }
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

void Play::versusLog() {
    // (M8 part 1's device check: every device logs the ranking it shows on each bar line: the same on all of them)
    const World& w = *world_;
    const u32 bar = w.phrase().bar;
    if (bar == versusLogBar_) return;
    versusLogBar_ = bar;
    u8 order[kMaxPlayers];
    u32 n = 0;
    for (u32 q = 0; q < kMaxPlayers; ++q)
        if (w.player(q).present) {
            u32 at = n++;
            while (at > 0 && w.player(order[at - 1]).score < w.player(q).score) {
                order[at] = order[at - 1];
                --at;
            }
            order[at] = static_cast<u8>(q);
        }
    char line[64];
    int o = 0;
    for (u32 k = 0; k < n; ++k) o += std::snprintf(line + o, sizeof line - static_cast<size_t>(o), "P%u ", order[k] + 1u);
    line[o < 64 ? o : 63] = 0;
    RF_LOG("versus: run %u bar %u ranking %scrown %d underdog %d", w.runId(), bar, line, w.director().crown == 0xFF ? -1 : static_cast<int>(w.director().crown) + 1,
           w.director().underdog == 0xFF ? -1 : static_cast<int>(w.director().underdog) + 1);
}

void Play::drawVersusWorld(FrameDraw& d, const LookVariant& variant) {
    // README.md 8, V5: the leader's ship wears a small crown (18 wu wide, 26 wu above the ship, the pickup's white-gold at 2.0) on
    // every device; README.md 7: a knocked out ship's place reads BACK IN 2, BACK IN 1 (bars; cap 22 wu, the player's colour).
    const World& w = *world_;
    const float px = view_.pxPerWu();
    const u8 crown = w.director().crown;
    if (crown < kMaxPlayers && w.player(crown).present && w.player(crown).alive) {
        const PlayerState& pl = w.player(crown);
        const Vec2 c = view_.toScreen(pl.prevPos + (pl.pos - pl.prevPos) * alpha_ + Vec2{0.0f, -26.0f});
        const float hw = 9.0f * px, h = 8.0f * px;
        const Vec3 wg = hudColour(*look_, variant, mix(palettes_->pickup, Vec3{1.0f, 1.0f, 1.0f}, 0.35f), 2.0f);
        const Vec2 pts[7] = {{c.x - hw, c.y}, {c.x - hw, c.y - h}, {c.x - 0.5f * hw, c.y - 0.5f * h}, {c.x, c.y - 1.3f * h},
                             {c.x + 0.5f * hw, c.y - 0.5f * h}, {c.x + hw, c.y - h}, {c.x + hw, c.y}};
        for (u32 j = 0; j + 1 < 7; ++j) d.hud.line(pts[j], pts[j + 1], 0.9f, wg, wg, 0);
        d.hud.line(pts[6], pts[0], 0.9f, wg, wg, 0);
    }
    const double now = musicPrev_ + (music1_ - musicPrev_) * static_cast<double>(alpha_);
    for (u32 q = 0; q < kMaxPlayers; ++q) {
        const PlayerState& pl = w.player(q);
        if (!pl.present || !pl.knocked) continue;
        const u32 left = static_cast<u32>(std::ceil((pl.backTick - now) / kTicksPerBar - 1e-6));
        char line[16];
        std::snprintf(line, sizeof line, "BACK IN %u", left < 1 ? 1u : left);
        const float cap = 22.0f * px;
        const Vec2 at = view_.toScreen(pl.pos);
        const Vec3 c = hudColour(*look_, variant, palettes_->player[q], 1.6f);
        const float wdt = font_->measure(TextStyle{cap}, line);
        font_->drawField(d.hud, {at.x - 0.5f * wdt, at.y - 0.5f * cap}, cap, 0.06f * cap, c, c, c, line);
    }
}

void Play::screenPodium(Ui& ui) {
    // design/modes/screens.md "The podium": WINNER (A DRAW when level at the top), the winner's ring with their ship and the crown,
    // their name and count, the table (every player best first: place, name, count, best Chain, crowns in this room), the host's
    // REMATCH (the same match on a new seed: the drop-in on every device; the others WAITING FOR HOST), LEAVE for everyone.
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const World& w = *world_;
    podiumS_ += frameDt_;
    const float in = clamp01(podiumS_ / 0.6f);
    const u8 win = podiumWinner_;
    ui.text({cx, 104.0f * s}, 44.0f * s, palette_->accent, 1.6f * in, win == 0xFF ? "A DRAW" : "WINNER");
    if (win != 0xFF) {
        const Vec3 pc = palettes_->player[win], pl = palettes_->playerLight[win];
        const Vec2 c{cx, 330.0f * s};
        ui.draw().pools.glow(c, 500.0f * s, pc * (0.06f * in * hudFade()));
        ui.dot(c, 140.0f * s, pc, 0.07f * in);
        ui.ring(c, 150.0f * s, 1.4f * s, pc, 2.0f * in, 72);
        drawTitleShip(ui, c, -1.5707963f + 0.3f * podiumS_, 3.0f, in, false);
        // the crown over the ring, 120 px wide, the pickup's white-gold at 2.0
        const Vec3 wg = ui.col(mix(palettes_->pickup, white, 0.35f), 2.0f * in);
        const float hw = 60.0f * s, h = 52.0f * s, by = c.y - 150.0f * s - 18.0f * s;
        const Vec2 pts[7] = {{cx - hw, by}, {cx - hw, by - h}, {cx - 0.5f * hw, by - 0.5f * h}, {cx, by - 1.3f * h},
                             {cx + 0.5f * hw, by - 0.5f * h}, {cx + hw, by - h}, {cx + hw, by}};
        for (u32 j = 0; j + 1 < 7; ++j) ui.draw().hud.line(pts[j], pts[j + 1], 1.4f * s, wg, wg, 0);
        ui.draw().hud.line(pts[6], pts[0], 1.4f * s, wg, wg, 0);
        (void)pl;
        char name[32];
        const char* given = session_ ? session_->seatName(win) : "";
        std::snprintf(name, sizeof name, "P%u%s%s", win + 1u, given[0] ? "  " : "", given);
        ui.text({cx, 540.0f * s}, 34.0f * s, pc, 1.6f * in, name);
        char num[32];
        groupDigits(num, sizeof num, w.player(win).score);
        ui.font().drawField(ui.draw().hud, {cx - 0.5f * ui.measure(60.0f * s, num), 620.0f * s - 60.0f * s}, 60.0f * s, 2.4f * s, ui.col(white, 1.6f * in),
                            ui.col(white, 1.6f * in), ui.col(pc, 1.6f * in), num);
    }
    // The table, best first.
    u8 order[kMaxPlayers];
    u32 n = 0;
    for (u32 q = 0; q < kMaxPlayers; ++q)
        if (w.player(q).present) {
            u32 at = n++;
            while (at > 0 && w.player(order[at - 1]).score < w.player(q).score) {
                order[at] = order[at - 1];
                --at;
            }
            order[at] = static_cast<u8>(q);
        }
    for (u32 k = 0; k < n; ++k) {
        const u8 q = order[k];
        const float y = (700.0f + 46.0f * static_cast<float>(k)) * s;
        const Vec3 pc = palettes_->player[q];
        char left[48], right[64], num[32], chain[32];
        std::snprintf(left, sizeof left, "%u  P%u%s", k + 1u, q + 1u, q == seat_ ? " (YOU)" : "");
        groupDigits(num, sizeof num, w.player(q).score);
        groupDigits(chain, sizeof chain, w.player(q).bestChain);
        const u8 crowns = session_ ? session_->crowns(q) : 0;
        std::snprintf(right, sizeof right, "%s   X%s   %u CROWN%s", num, chain, crowns, crowns == 1 ? "" : "S");
        ui.text({cx - 420.0f * s, y}, 22.0f * s, pc, 1.4f * in, left, 0.0f);
        ui.text({cx + 420.0f * s, y}, 22.0f * s, pc, 1.4f * in, right, 1.0f);
    }
    const bool host = session_ && session_->host();
    // (the device check, 2026-11-16: the pill 120 tall at 950, so WAITING FOR HOST keeps the 24 px rule under it)
    if (ui.pill(43, {cx, 950.0f * s}, {600.0f * s, 120.0f * s}, "REMATCH", 52.0f * s, true, host && in >= 1.0f) || (keyPrimary_ && host)) {
        session_->requestRetry();
        if (band_) band_->playSfx(audio::Sfx::UiConfirm);
    }
    if (!host) ui.text({cx, 1052.0f * s}, 14.0f * s, white, 0.8f, "WAITING FOR HOST", 0.5f);
    if ((ui.pill(44, {282.0f * s, 950.0f * s}, {300.0f * s, 90.0f * s}, "LEAVE", 28.0f * s, false, true, trans_ == Trans::Home) || keyBack_) &&
        trans_ == Trans::None)
        homeTap();
    keyPrimary_ = keyBack_ = false;
    (void)H;
}

}  // namespace rf
