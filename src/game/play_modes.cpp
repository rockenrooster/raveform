// M8 part 1 (docs/MODES_PLAN.md 3: "the flow: choosing, starting, ending, records; the load-in's band and HUD side"): a mode's run in
// the game. ONE TRACK (design/modes/one_track.md): the load-in (README.md 5: the kit's icons spiralling in from a 300 wu circle, one
// landing a sixteenth with its chirp climbing the Track's scale, a ring and the "upgrade taken" chord on bar 3's downbeat), TRACK
// COMPLETE through the outro, the result (TRACK COMPLETE or RUN OVER, the Track's own best), NEXT TRACK. Sounds the engine does not
// have yet (the bed's layers by the landings, the outro's held chord and reversed cymbal) are listed in game/pending_sounds.cpp's
// notes and PROGRESS.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
// FULL BLAST (full_blast.md "Look"): the torrent's Riffs at 0.8 of an orb, each with a ring as it lands (the page gives no size: 220
// wu, a little over half the load-in's own closing ring), the closing ring to 700 wu; the power back's ring to 600 wu.
constexpr float kTorrentRiffScale = 0.8f, kRiffRingWu = 220.0f, kTorrentRingWu = 700.0f, kPowerRingWu = 600.0f;
// The title's eight (screens.md "Title": A3 C4 E4 A4 C5 E5 G5 A5), as HEADLINERS COMPLETE and the podium's fanfare play them.
constexpr u8 kTitleEight[8] = {57, 60, 64, 69, 72, 76, 79, 81};
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
            if (w.setup().mode == Mode::FullBlast && isRiff(e.b) && !(settings_.reduceFlashing || forcedReduceFlashing_)) {
                // full_blast.md "Look": the torrent: each Riff's landing drawn larger, its ring appearing as it lands.
                const u32 q = seat_ < kMaxPlayers ? seat_ : 0u;
                const float beatS = 60.0f / std::fmax(clock_.bpm(), 30.0f);
                view_.loadInRing({e.x, e.y}, kRiffRingWu, palettes_->player[q], palettes_->playerLight[q], static_cast<float>(tickEnd_ - e.age), 0.25f * beatS);
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
                if (w.setup().mode == Mode::Headliners && bandDrives_ && band_) {
                    // sound.md "HEADLINERS": HEADLINERS COMPLETE: the gong; the band's stab on Last Light's home chord at 1.2; the
                    // title's eight in the lead, a sixteenth apart; the progression held on the home chord through the outro.
                    const double off = coop() ? netBandOffset_ : clock_.offset();
                    const double t16 = kTicksPerBeat / 4.0;
                    const double from = (std::floor(music1_ / t16) + 1.0) * t16;
                    band_->playRecipe(audio::Recipe::Gong, -1, 0.8f);
                    band_->playRecipe(audio::Recipe::Stab, -1, 1.2f, 0.0f, from + off);
                    for (u32 j = 0; j < 8; ++j) band_->leadNoteAt(0, kTitleEight[j], 0.9f, 0.0f, from + off + static_cast<double>(j + 1) * t16);
                    band_->holdChord(0);
                }
                trackComplete_ = true;
                setCompleteT_ = static_cast<float>(tickEnd_ - e.age);
                setCompleteScore_ = w.player(seat_).score;
                RF_LOG("play: TRACK COMPLETE (Track %u, run %.1f s, the boss in %.1f s)", w.phrase().track, runSeconds_, bossSecs_);
            }
            break;
        case EventType::ActWon:
            // M8 HEADLINERS versus (sound.md): ACT 3: P2: a bell on the home chord's root two octaves up, and that player's lead on the root.
            actWonT_ = static_cast<float>(tickEnd_ - e.age);
            actWonSeat_ = e.a;
            actWonAct_ = e.b;
            RF_LOG("play: act %u goes to %s%u", static_cast<unsigned>(e.b), e.a < kMaxPlayers ? "P" : "nobody ", e.a < kMaxPlayers ? e.a + 1u : 0u);
            if (bandDrives_ && band_ && e.a < kMaxPlayers) {
                band_->playRecipe(audio::Recipe::Bell, audio::chordNote(0, 6, audio::NoteFrom::ChordTone), 0.8f);
                band_->leadNote(bandSlot(e.a), 0, 0.9f, 0.0f);
            }
            break;
        case EventType::Crown:
            // V5: the crown changes hands on a bar line, with a bell.
            if (bandDrives_ && band_ && e.a != 0xFF) band_->playRecipe(audio::Recipe::Bell, 84, 0.6f);
            break;
        case EventType::Knock:
            if (e.a != seat_) break;
            pulse(2);
            if (bandDrives_ && band_) {
                // sound.md "Every mode": the knock-out: the hit's own sound, then a saw glide falling an octave over one beat; BACK IN 2
                // and BACK IN 1 the count-in's tick on their bar lines; coming back an opening sweep ending on the bar line and the
                // Cell's note. FULL BLAST (the power cut): the Drop's impact on that bar line.
                const double off = coop() ? netBandOffset_ : clock_.offset();
                const double back = e.tick;
                band_->playRecipe(audio::Recipe::SawGlide, -1, 0.55f, 0.0f, -1.0, -12.0f, 1.0f);
                for (u32 k = 1; k <= 2; ++k)
                    if (back - static_cast<double>(k) * kTicksPerBar > music1_) band_->sfxAt(audio::Sfx::Tick, back - static_cast<double>(k) * kTicksPerBar + off, 0.6f);
                band_->playRecipe(audio::Recipe::Sweep, -1, 0.5f, 0.0f, back - kTicksPerBeat + off, 1.0f, 1.0f);
                band_->leadNoteAt(0, static_cast<u8>(homeNote(5)), 0.85f, 0.0f, back + off);
                if (w.setup().mode == Mode::FullBlast) band_->playRecipe(audio::Recipe::Impact, -1, 0.9f, 0.0f, back + off);
            }
            break;
        case EventType::KnockBack:
            if (e.a != seat_ || w.setup().mode != Mode::FullBlast) break;
            {
                // full_blast.md "Look": the power back: a ring from where the ship re-forms (90 to 600 wu in half a beat), every icon
                // of the band row flashing in turn over one beat.
                const u32 q = seat_ < kMaxPlayers ? seat_ : 0u;
                const float beatS = 60.0f / std::fmax(clock_.bpm(), 30.0f);
                powerBackT_ = static_cast<float>(tickEnd_ - e.age);
                if (!(settings_.reduceFlashing || forcedReduceFlashing_))
                    view_.loadInRing({e.x, e.y}, kPowerRingWu, palettes_->player[q], palettes_->playerLight[q], powerBackT_, 0.5f * beatS);
                pulse(2);
            }
            break;
        case EventType::RunEnd:
            // The outro is over: the result (the Track cleared).
            runCleared_ = true;
            RF_LOG("play: the run ends (Track %u cleared, %.1f s, score %llu)", w.phrase().track, runSeconds_,
                   static_cast<unsigned long long>(w.player(seat_).score));
            if (w.director().formCount) {
                // README.md 6 (a timed run's last downbeat): the music bus low-passed at 600 Hz for a beat and opened over the next
                // (sound.md "Every mode"); FULL BLAST's count, the load-out: the load-in's chirps running down the scale, a sixteenth
                // each, into the back chord on the bar line.
                countTick_ = e.tick;
                if (bandDrives_ && band_) {
                    band_->lowPassBed(600.0f, 60.0f / std::fmax(clock_.bpm(), 30.0f));
                    if (w.setup().mode == Mode::FullBlast) {
                        const double off = coop() ? netBandOffset_ : clock_.offset();
                        for (u32 j = 0; j < 12; ++j)
                            band_->playRecipe(audio::Recipe::Chirp, audio::chordNote(10 - static_cast<i32>(j), 5), 0.5f, 0.0f,
                                              e.tick + off + static_cast<double>(j + 1) * kTicksPerBeat / 4.0);
                        band_->sfxAt(audio::Sfx::UiBack, e.tick + off + kTicksPerBar, 0.9f);
                    }
                }
            }
            if (flow_ && (screen_ == Screen::Run || (coop() && (screen_ == Screen::Paused || screen_ == Screen::Settings)))) {
                if (w.director().formCount) {
                    // screens.md "The count": the bar after the last downbeat counts what was made; the result arrives on the next
                    // bar line, as game over arrives.
                    resultPending_ = true;
                    resultAt_ = e.tick + kTicksPerBar;
                } else {
                    modesResult();
                }
            } else if (!flow_ || opt_.bot) {
                retryRequest_ = true;  // (a benchmark or the bot: the same Track again)
            }
            break;
        case EventType::GateLay:
        case EventType::GateThread: {
            // M8 SONGLINE: a gate threaded is its note in the player's own lead voice on the next thirty-second, a third above in
            // time (sound.md "A gate threaded"; co-op: the string's own harmony is already its note).
            if (e.type == EventType::GateThread && e.a == seat_ && bandDrives_ && band_) {
                const double t32 = kTicksPerBeat / 8.0;
                const double at = (std::floor(music1_ / t32) + 1.0) * t32 + (coop() ? netBandOffset_ : clock_.offset());
                const i32 note = e.c ? songStepsAbove(e.b, 2) : static_cast<i32>(e.b);
                band_->leadNoteAt(0, static_cast<u8>(note < 0 ? 0 : (note > 127 ? 127 : note)), static_cast<u8>(e.f) == 1 ? 1.0f : 0.85f, 0.0f, at);
            }
            // The string as it is flown (the result's bar): this player's gates by their sixteenth.
            if (e.type == EventType::GateLay ? (e.a != 0xFF && e.a != seat_) : e.a != seat_) break;
            const double step = (e.tick - w.director().startTick) / (kTicksPerBeat / 4.0);
            const i64 k = static_cast<i64>(std::llround(step));
            if (k < 0 || k >= static_cast<i64>(kSongMarks)) break;
            if (e.type == EventType::GateLay) {
                if (!songMarks_[k]) songMarks_[k] = 1;
            } else {
                songMarks_[k] = e.c ? 3 : 2;
            }
            break;
        }
        case EventType::GateCount:
            if (e.a != seat_) break;
            songCountT_ = static_cast<float>(tickEnd_ - e.age);
            if (bandDrives_ && band_) {
                // The finale's burst: Impact and Gong, eight Bells up the home chord a thirty-second apart (one a ring of the
                // firework); then the count: a Chirp for every sixteen gates threaded, climbing, over two beats, and a Bell on top
                // for each tier the best streak reached.
                const double at = e.tick + (coop() ? netBandOffset_ : clock_.offset());
                band_->playRecipe(audio::Recipe::Impact, -1, 0.9f, 0.0f, at);
                band_->playRecipe(audio::Recipe::Gong, -1, 0.7f, 0.0f, at);
                for (u32 j = 0; j < 8; ++j)
                    band_->playRecipe(audio::Recipe::Bell, audio::chordNote(static_cast<i32>(j), 5, audio::NoteFrom::ChordTone), 0.55f, 0.0f,
                                      at + static_cast<double>(j) * kTicksPerBeat / 8.0);
                const u32 chirps = static_cast<u32>(e.f) / 16u;
                for (u32 j = 0; j < chirps && j < 32; ++j)
                    band_->playRecipe(audio::Recipe::Chirp, audio::chordNote(static_cast<i32>(j % 15), 5, audio::NoteFrom::ScaleSteps), 0.45f, 0.0f,
                                      at + kTicksPerBeat + 2.0 * kTicksPerBeat * static_cast<double>(j) / static_cast<double>(chirps > 1 ? chirps : 1u));
                const u16 best = w.bestStreak(seat_);
                const u32 tiers = best >= 128 ? 4u : (best >= 64 ? 3u : (best >= 32 ? 2u : (best >= 16 ? 1u : 0u)));
                for (u32 j = 0; j < tiers; ++j)
                    band_->playRecipe(audio::Recipe::Bell, audio::chordNote(static_cast<i32>(4 + j), 5, audio::NoteFrom::ChordTone), 0.6f, 0.0f,
                                      at + 3.0 * kTicksPerBeat + static_cast<double>(j) * kTicksPerBeat / 4.0);
            }
            break;
        case EventType::StreakTier:
            if (e.a != seat_ || !bandDrives_ || !band_) break;
            if (e.c) {
                // A tier: the bells, one more of the four each time (one at 16, all four at 128).
                for (u32 j = 0; j < e.b && j < 4; ++j)
                    band_->playRecipe(audio::Recipe::Bell, audio::chordNote(static_cast<i32>(j), 5, audio::NoteFrom::ChordTone), 0.6f, 0.0f,
                                      (std::floor(music1_ / (kTicksPerBeat / 4.0)) + 1.0 + static_cast<double>(j)) * (kTicksPerBeat / 4.0) +
                                          (coop() ? netBandOffset_ : clock_.offset()));
                pulse(0);
            } else {
                band_->playRecipe(audio::Recipe::Glass, audio::chordNote(0, 5), 0.3f);  // (a step down: soft)
            }
            break;
        case EventType::JamRoom:
            // M8 JAM (sound.md "JAM"): a mixer pad: the interface tick at once and a sweep over the beat that ends on the bar line
            // (opening for a layer on, closing for off); DROP, a Track, SOUND: the confirm chord.
            if (!bandDrives_ || !band_) break;
            if (e.a == 0) {
                u32 now = 0, then = 0;
                for (u32 k = 0; k < 8; ++k) {
                    now += (e.b >> k) & 1u;
                    then += (w.jam().mixer >> k) & 1u;
                }
                band_->playSfx(audio::Sfx::UiTick, 0.8f);
                band_->playRecipe(audio::Recipe::Sweep, -1, 0.45f, 0.0f, e.tick - kTicksPerBeat + (coop() ? netBandOffset_ : clock_.offset()),
                                  now >= then ? 1.0f : -1.0f, 1.0f);
            } else {
                band_->playSfx(audio::Sfx::UiConfirm, 0.8f);
            }
            break;
        case EventType::JamTake:
            // A toy taken: the "upgrade taken" chord; each further stack a bell a scale step up.
            if (e.a != seat_ || !bandDrives_ || !band_) break;
            if (e.c <= 1) band_->playSfx(audio::Sfx::Upgrade, 0.9f);
            else band_->playRecipe(audio::Recipe::Bell, audio::chordNote(static_cast<i32>(e.c), 5, audio::NoteFrom::ScaleSteps), 0.6f);
            break;
        case EventType::JamEmpty:
            // EMPTY: the back chord and a closing sweep.
            if (e.a != seat_ || !bandDrives_ || !band_) break;
            band_->playSfx(audio::Sfx::UiBack, 0.9f);
            band_->playRecipe(audio::Recipe::Sweep, -1, 0.5f, 0.0f, -1.0, -1.0f, 1.0f);
            break;
        case EventType::JamHold:
        case EventType::JamShelf:
            if (e.a == seat_) pulse(0);
            if (e.type == EventType::JamShelf && e.a == seat_ && bandDrives_ && band_) band_->playSfx(audio::Sfx::UiConfirm, 0.6f);
            break;
        case EventType::SongChord:
            // Co-op's chord (sound.md): the band's stab on the bar's chord at 0.8.
            if (bandDrives_ && band_) band_->playRecipe(audio::Recipe::Stab, -1, 0.8f);
            break;
        case EventType::SparkBack:
            if (e.a && bandDrives_ && band_) {
                // The line bending back to a lost player: a saw glide rising a fifth over two beats, panned to where the Spark is.
                const float pan = clampf((e.x - w.player(seat_).pos.x) / 900.0f, -1.0f, 1.0f);
                band_->playRecipe(audio::Recipe::SawGlide, -1, 0.5f, pan, -1.0, 7.0f, 2.0f);
            }
            break;
        case EventType::Shot:
            // The gun (sound.md): no lead notes; each volley a closed hat at 0.3, at most one a thirty-second.
            if (w.setup().mode == Mode::Songline && e.a == seat_ && bandDrives_ && band_) {
                const double t32 = kTicksPerBeat / 8.0;
                const double slot = std::floor(music1_ / t32);
                if (slot != lastHat32_) {
                    lastHat32_ = slot;
                    band_->playRecipe(audio::Recipe::Hat, -1, 0.3f);
                }
            }
            break;
        case EventType::Restart:
            actWonSeat_ = 0xFF;
            actWonT_ = -100.0f;
            std::memset(songMarks_, 0, sizeof songMarks_);
            songCountT_ = -100.0f;
            resultPending_ = false;
            countTick_ = -1.0;
            completeChord_ = false;
            powerBackT_ = -100.0f;
            view_.setHaloGone(0);
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
    const World& w = *world_;
    if (resultPending_ && music1_ >= resultAt_ && (screen_ == Screen::Run || (coop() && (screen_ == Screen::Paused || screen_ == Screen::Settings)))) {
        resultPending_ = false;
        modesResult();
    }
    // FULL BLAST's power cut (sound.md): the music behind the pause's low-pass while the ship is out, opened over a beat as it returns.
    const bool cut = flow_ && w.setup().mode == Mode::FullBlast && w.player(seat_).knocked && !w.runEnded();
    if (cut != powerCutMuffled_) applyPauseMix();
    // ONE TRACK (sound.md): TRACK COMPLETE: the "upgrade taken" chord an octave up as the card finishes writing (its beat).
    if (trackComplete_ && !completeChord_ && w.setup().mode == Mode::OneTrack &&
        static_cast<float>(tickEnd_) >= setCompleteT_ + 60.0f / std::fmax(clock_.bpm(), 30.0f)) {
        completeChord_ = true;
        if (bandDrives_ && band_) band_->playSfx(audio::Sfx::Upgrade, 0.9f, 2.0f);
    }
    // README.md 5: bar 3's downbeat: a ring in the player's colours leaves the ship (90 to 400 wu in half a beat; reduce flashing:
    // none), the HUD's band row flashes once, the "upgrade taken" chord. (FULL BLAST's torrent closes with a ring to 700 wu.)
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
        view_.loadInRing(pl.pos, w.setup().mode == Mode::FullBlast ? kTorrentRingWu : li.closeRingWu[1], palettes_->player[q], palettes_->playerLight[q],
                         static_cast<float>(tickEnd_), li.closeRingBeats * beatS);
    takenT_ = static_cast<float>(tickEnd_);  // (the band row's flash)
    if (bandDrives_ && band_) band_->playSfx(audio::Sfx::Upgrade, 1.0f, 1.0f);
}

i32 Play::songStepsAbove(i32 note, u32 steps) const {
    // `steps` scale steps above `note` in the run's Track's scale (songline.md: in time a third above; co-op's harmonies).
    const World& w = *world_;
    const u32 t = w.setup().track >= 1 && w.setup().track <= kModeTracks ? w.setup().track : 1u;
    const ModesData::Tune& tn = species_->modes.tunes[t - 1u];
    if (!tn.scaleCount) return note + (steps == 2 ? 4 : 0);
    const i32 pc = ((note - static_cast<i32>(tn.tonic)) % 12 + 12) % 12;
    u32 idx = 0;
    for (u32 j = 0; j < tn.scaleCount; ++j)
        if (tn.scale[j] <= pc) idx = j;
    const i32 base = note - (pc - tn.scale[idx]);
    const u32 to = idx + steps;
    return base + tn.scale[to % tn.scaleCount] - tn.scale[idx] + 12 * static_cast<i32>(to / tn.scaleCount);
}

void Play::songlineBand() {
    // M8 SONGLINE (sound.md): the Spark is the band's lead in its own slot, on the section's grid: the beat's eighths in a Groove
    // and a Build, sixteenths in a Drop, quarters in a Break; the Build's last bar its slot off and the bar's top note held as a
    // pad voice, swelling, released on the Drop's downbeat; the spiral's two bars a tom roll into the last downbeat.
    const World& w = *world_;
    const bool on = w.setup().mode == Mode::Songline && screen_ != Screen::Title && w.director().formCount && !w.phrase().ended && !w.runEnded();
    const Phrase& ph = w.phrase();
    u8 mode = 0;
    bool pad = false;
    if (on) {
        if (ph.buildLastBar()) pad = true;
        else mode = ph.section == Section::Drop ? 1 : (ph.section == Section::Break ? 4 : 3);
    }
    band_->setSpark(mode, 2);
    constexpr u8 kPadSlot = 13;
    if (pad && !sparkPad_) {
        // (the bar's arpeggio's top note: the tune's own data, its chord where the band has it)
        const u32 t = w.setup().track >= 1 && w.setup().track <= kModeTracks ? w.setup().track : 1u;
        const ModesData::Tune& tn = species_->modes.tunes[t - 1u];
        i32 top = 72;
        if (tn.chords) {
            const i64 shift = std::llround((coop() ? netBandOffset_ : clock_.offset()) / kTicksPerBar);
            const u32 chord = static_cast<u32>(((static_cast<i64>(ph.bar) + ((shift % 4) + 4) % 4) % tn.chords + tn.chords) % tn.chords);
            top = 0;
            for (u32 j = 0; j < 16; ++j) top = std::max(top, static_cast<i32>(tn.arp[chord][j]));
        }
        const float barS = 240.0f / std::fmax(clock_.bpm(), 30.0f);
        band_->startVoice(kPadSlot, audio::Recipe::PadVoice, top, 0.7f, 0.0f, 0.8f * barS);
    } else if (!pad && sparkPad_) {
        band_->stopVoice(kPadSlot, 0.15f);
    }
    sparkPad_ = pad;
    const bool spiral = on && w.spark().spiral;
    if (spiral && !sparkSpiral_) {
        u32 total = 0;
        for (u32 j = 0; j < w.director().formCount; ++j) total += w.director().formBars[j];
        band_->playRecipe(audio::Recipe::TomRoll, -1, 0.9f, 0.0f, w.tickOfBar(total) + (coop() ? netBandOffset_ : clock_.offset()),
                          4.0f * static_cast<float>(species_->modes.songline.finaleBars));
    }
    sparkSpiral_ = spiral;
}

void Play::drawSongCount(FrameDraw& d, const LookVariant& variant, float W, float H) {
    // screens.md "The count" (SONGLINE): the string as it was flown, drawn across the lower third as one line of marks (one a gate,
    // 1,600 px wide): it lights from the left in two beats, each threaded gate a tick of the player's colour, each one in time white;
    // GATES 446 OF 512 counts beside it. Until the result arrives on the bar line.
    const float age = static_cast<float>(tickEnd_) - songCountT_;
    const float beatS = 60.0f / std::fmax(clock_.bpm(), 30.0f);
    if (age < 0.0f || age > 4.0f * beatS + 0.5f) return;
    const float s = H / 1080.0f;
    const float wide = std::fmin(1600.0f * s, 0.9f * W);
    const float x0 = 0.5f * (W - wide), y = 0.74f * H;
    const float sweep = clamp01(age / (2.0f * beatS));
    const u32 q = seat_ < kMaxPlayers ? seat_ : 0u;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    u32 counted = 0, laid = 0;
    for (u32 k = 0; k < kSongMarks; ++k) {
        const u8 m = songMarks_[k];
        if (!m) continue;
        ++laid;
        const float u = (static_cast<float>(k) + 0.5f) / static_cast<float>(kSongMarks);
        const float x = x0 + wide * u;
        const bool lit = u <= sweep;
        if (lit && m >= 2) ++counted;
        const float just = lit ? clamp01(1.0f - (sweep - u) * 12.0f) : 0.0f;
        Vec3 c;
        if (m == 1) c = hudColour(*look_, variant, white, 0.10f);
        else if (!lit) c = hudColour(*look_, variant, white, 0.18f);
        else c = hudColour(*look_, variant, m == 3 ? white : palettes_->player[q], 1.3f + 0.8f * just);
        const float h = (m >= 2 && lit ? 16.0f + 10.0f * just : 10.0f) * s;
        d.hud.line({x, y - h}, {x, y + h}, 0.45f * s, c, c, 0);
    }
    char text[40];
    std::snprintf(text, sizeof text, "GATES %u OF %u", counted, laid);
    const float cap = 30.0f * s;
    const Vec3 c = hudColour(*look_, variant, white, 1.5f);
    font_->drawField(d.hud, {x0, y + 40.0f * s}, cap, 1.2f * s, c, c, c, text);
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
        const float scale = w.setup().mode == Mode::FullBlast && isRiff(orb) ? kTorrentRiffScale : li.iconScale;  // (full_blast.md: 0.8)
        const float size = 34.0f * px * scale * (0.6f + 0.4f * clamp01(u * 3.0f));
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
    if (su.mode == Mode::Songline) {
        // songline.md "Numbers", `songline_<track>`: the best score and its gates threaded in a hundred (co-op: the team's,
        // team_songline_<track>). The result's numbers as the run ended.
        resultGates_ = w.gatesThreaded(seat_);
        resultLaid_ = w.gatesLaid(seat_);
        resultInTime_ = w.gatesInTime(seat_);
        resultStreak_ = w.bestStreak(seat_);
        const u32 pct = resultLaid_ ? static_cast<u32>(100.0 * resultGates_ / resultLaid_ + 0.5) : 0u;
        if (su.track >= 1 && su.track <= ModeRecords::kTracks) {
            if (coop()) {
                ModeRecords::Team& t = records_.team[static_cast<u32>(Mode::Songline)][su.track - 1u];
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
                ModeRecords::Timed& r = records_.songline[su.track - 1u];
                modeBest_ = r.score;
                newBest_ = p.score > 0 && p.score > r.score;
                if (newBest_) {
                    r.score = p.score;
                    r.second = pct;
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

void Play::modesResult() {
    // The result on its bar line (or at once for a mode that is not timed): game over's screen with the mode's numbers; a versus match's
    // podium (the winner the best count, a draw when the top is level; the room's crown to the winner).
    const World& w = *world_;
    endRun(false);
    coopOver_ = coop();
    screen_ = Screen::GameOver;
    overS_ = -0.6f;
    podiumS_ = 0.0f;
    podiumCounted_ = false;
    podiumWinner_ = 0xFF;
    if (w.setup().versus) {
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
        if (at > 1 && w.setup().mode == Mode::Headliners && w.billLeader() < kMaxPlayers && w.versusCount(w.billLeader()) == best)
            podiumWinner_ = w.billLeader();  // (headliners.md: level on acts, the most damage over the whole bill)
        if (session_ && session_->host() && podiumWinner_ != 0xFF) {
            // V8: the room keeps each seat's wins as crowns while it stands (the lobby's message carries them).
            const u8 c = session_->crowns(podiumWinner_);
            session_->setCrowns(podiumWinner_, static_cast<u8>(c < 255 ? c + 1 : 255));
            podiumCounted_ = true;
        }
        RF_LOG("play: the versus match ends: %s", podiumWinner_ == 0xFF ? "a draw" : "a winner");
        // The podium arrives on this bar line: the table in seat order, its sounds placed from it.
        podiumCrowned_ = false;
        u32 k = 0;
        for (u32 q = 0; q < kMaxPlayers; ++q)
            if (w.player(q).present) podiumRowY_[q] = 700.0f + 46.0f * static_cast<float>(k++);
        podiumSounds(std::round(music1_ / kTicksPerBar) * kTicksPerBar + (coop() ? netBandOffset_ : clock_.offset()));
    }
}

i32 Play::homeNote(i32 octave, i32 semis) const {
    const World& w = *world_;
    const u32 t0 = w.director().bonus ? w.director().bonus : (w.setup().track ? w.setup().track : w.phrase().track);
    const u32 t = t0 >= 1 && t0 <= kModeTracks ? t0 : 1u;
    const i32 pc = static_cast<i32>(species_->modes.tunes[t - 1u].tonic) % 12;
    const i32 n = 12 * (octave + 1) + pc + semis;
    return n < 0 ? 0 : (n > 127 ? 127 : n);
}

void Play::modesBarPlan(double nextLine, double offset, const Phrase& next) {
    // design/modes/sound.md: what plays on the coming bar lines (band ticks), planned half a bar ahead; a reversed cymbal or the last
    // bar's roll, which start before their line, a bar earlier.
    (void)offset;
    const World& w = *world_;
    const RunSetup& su = w.setup();
    const DirectorState& d = w.director();
    if (su.mode == Mode::Set || (su.mode == Mode::Jam && !d.formCount)) return;
    const double bar = kTicksPerBar, beat = kTicksPerBeat;
    if (d.formCount && !next.ended) {
        // LAST CALL: two bells, the chord's fifth then its root an octave up, an eighth apart, as it appears.
        if (next.barsLeft == 8) {
            band_->playRecipe(audio::Recipe::Bell, audio::chordNote(2, 5, audio::NoteFrom::ChordTone), 0.6f, 0.0f, nextLine);
            band_->playRecipe(audio::Recipe::Bell, audio::chordNote(4, 5, audio::NoteFrom::ChordTone), 0.6f, 0.0f, nextLine + beat / 2.0);
        }
        // The last bar: the Build's snare roll and a riser over its four beats, both ending on the last downbeat, and the impact there.
        if (next.barsLeft == 1) {
            band_->playRecipe(audio::Recipe::Roll, -1, 0.7f, 0.0f, nextLine + bar, 4.0f);
            band_->playRecipe(audio::Recipe::Whoosh, -1, 0.45f, 0.0f, nextLine, 4.0f);
            band_->playRecipe(audio::Recipe::Impact, -1, 1.0f, 0.0f, nextLine + bar);
        }
        if (su.mode == Mode::FullBlast) {
            // A wave's name: a reversed cymbal of four beats ending on its Drop's downbeat (planned as its Build's last bar begins);
            // the band's stab on the bar's chord as the name's letters are struck (the card writes over its first beat).
            if (next.section == Section::Build && next.sectionBar + 1u == next.sectionBars)
                band_->playRecipe(audio::Recipe::RevCymbal, -1, 0.7f, 0.0f, nextLine + bar, 4.0f);
            if (next.section == Section::Drop && next.sectionBar == 0 && next.wave) {
                band_->playRecipe(audio::Recipe::Stab, -1, 1.0f, 0.0f, nextLine + beat / 2.0);
                pulse(2);  // (sound.md's haptics: heavy for a wave's Drop)
            }
        }
    }
    if (su.mode == Mode::OneTrack && d.outroBar != kNoBar) {
        // ONE TRACK (sound.md): the outro's first downbeat a gong on the home chord's root, the progression held on that chord under
        // the outro's Break; into the result a reversed cymbal of four beats ending on the outro's last bar line.
        if (next.bar == d.outroBar) {
            band_->playRecipe(audio::Recipe::Gong, -1, 0.75f, 0.0f, nextLine);
            band_->holdChord(0);
        }
        if (d.outroBars && next.bar + 1u == d.outroBar + d.outroBars) band_->playRecipe(audio::Recipe::RevCymbal, -1, 0.7f, 0.0f, nextLine + bar, 4.0f);
    }
    if (su.mode == Mode::Headliners && !next.trackChange && next.track != actCardTrack_ && next.track >= 1) {
        // HEADLINERS (sound.md): the act's card: the elite's arrival fifth in the new Track's key; then a bell for each act already
        // won, a sixteenth apart, chordNote(k, 5) for the k-th: the bill read out.
        actCardTrack_ = next.track;
        band_->sfxAt(audio::Sfx::Elite, nextLine, 0.8f);
        for (u32 k = 0; k + 1u < next.track && k < 9; ++k)
            band_->playRecipe(audio::Recipe::Bell, audio::chordNote(static_cast<i32>(k), 5), 0.55f, 0.0f, nextLine + static_cast<double>(k + 1) * beat / 4.0);
    }
}

i32 Play::loadInBedMask(double line) const {
    // design/modes/sound.md "Every mode": the bed during the load-in: kick and hats at the start, one more bed layer every four
    // landings, the full Groove on bar 3 (the mask holds from a bar line, so a layer joins on the bar line after its landings).
    const World& w = *world_;
    const RunSetup& su = w.setup();
    if (su.mode != Mode::OneTrack && su.mode != Mode::FullBlast) return -1;
    const World::KitState& k = w.kit(seat_);
    const ModesData::LoadIn& li = species_->modes.loadIn;
    if (!k.count || line >= k.from + static_cast<double>(li.bars) * kTicksPerBar - 1.0) return -1;
    u32 landed = 0;
    while (landed < k.count && w.kitLandTick(seat_, landed) <= line + 1.0) ++landed;
    const u32 layers = 2u + landed / (li.bedLayerEvery ? li.bedLayerEvery : 4u);
    return layers >= 8u ? 0xFF : static_cast<i32>((1u << layers) - 1u);
}

u32 Play::bandRowItems(const PlayerState& p, u8* out, u32 cap) const {
    // The HUD's band row in its order (play_hud.cpp): the Riffs as they were taken, then every Fusion, then the Mods held as taken.
    u32 n = 0;
    for (u32 pass = 1; pass < 256 && n < cap; ++pass)
        for (u8 u = kOrbitals; u < kUpgradeCount && n < cap; ++u)
            if (p.loadout.order[u] == pass) out[n++] = u;
    for (u8 f = 0; f < kFusionCount && n < cap; ++f)
        if (p.loadout.fused(f)) out[n++] = static_cast<u8>(kFusionOrb + f);
    for (u32 pass = 1; pass < 256 && n < cap; ++pass)
        for (u8 u = 0; u < kModCount && n < cap; ++u)
            if (p.loadout.order[u] == pass && p.loadout.mods[u]) out[n++] = u;
    return n;
}

namespace {
// FULL BLAST's load-out (design/modes/screens.md "The count"): the band row's icons leave over the bar's first three beats (three or
// four a sixteenth, as the torrent landed), each flying out of the ship to the frame's edges over a beat; the halo's arcs in the last.
constexpr double kLoadOutIconsBeats = 3.0;
}  // namespace

u32 Play::loadOutHaloGone(double music) const {
    const World& w = *world_;
    if (countTick_ < 0.0 || w.setup().mode != Mode::FullBlast) return 0;
    const double from = countTick_ + kLoadOutIconsBeats * kTicksPerBeat;
    if (music < from) return 0;
    const u32 cells = w.player(seat_).cells > 8 ? 8u : w.player(seat_).cells;
    if (!cells) return 0;
    const u32 n = static_cast<u32>((music - from) / (kTicksPerBeat / static_cast<double>(cells))) + 1u;
    return n < cells ? n : cells;
}

u64 Play::rolledScore(u64 score, double music) const {
    // The count's bar: the score's digits roll, and settle on their value from the left, one after another.
    if (countTick_ < 0.0 || world_->setup().mode != Mode::FullBlast || music >= countTick_ + kTicksPerBar || music < countTick_) return score;
    char raw[24];
    const int n = std::snprintf(raw, sizeof raw, "%llu", static_cast<unsigned long long>(score));
    if (n <= 1) return score;
    const double u = (music - countTick_) / kTicksPerBar;
    const int settled = static_cast<int>(u * static_cast<double>(n + 1));
    const u32 spin = static_cast<u32>(music / (kTicksPerBeat / 8.0));
    for (int i = settled; i < n; ++i) {
        const u32 dgt = static_cast<u32>(raw[i] - '0');
        const u32 r = (dgt + spin + static_cast<u32>(i) * 3u) % 10u;
        raw[i] = static_cast<char>('0' + (i == 0 && r == 0 ? 1u : r));
    }
    return std::strtoull(raw, nullptr, 10);
}

void Play::drawLoadOut(FrameDraw& d, const LookVariant& variant, float W, float H) {
    const World& w = *world_;
    if (countTick_ < 0.0 || w.setup().mode != Mode::FullBlast) return;
    const double now = musicPrev_ + (music1_ - musicPrev_) * static_cast<double>(alpha_);
    if (now < countTick_ || now > countTick_ + kTicksPerBar + kTicksPerBeat) return;
    const LookCtx ctx{look_, &variant};
    const PlayerState& pl = w.player(seat_);
    const Vec2 ship = view_.toScreen(pl.prevPos + (pl.pos - pl.prevPos) * alpha_);
    const float px = view_.pxPerWu();
    const float reach = 0.6f * std::sqrt(W * W + H * H);  // (to the frame's edges and past them)
    u8 items[64];
    const u32 n = bandRowItems(pl, items, 64);
    const double span = kLoadOutIconsBeats * kTicksPerBeat;
    for (u32 i = 0; i < n; ++i) {
        const double launch = countTick_ + span * static_cast<double>(i) / static_cast<double>(n);
        const float u = static_cast<float>((now - launch) / kTicksPerBeat);  // 0 leaving .. 1 gone
        if (u < 0.0f || u > 1.0f) continue;
        const float a = 2.39996323f * static_cast<float>(i) - 1.5707963f;  // (the golden angle: the circle filled evenly)
        const float e = u * u;
        const Vec2 at = ship + Vec2{std::cos(a), std::sin(a)} * (reach * e + 40.0f * px * u);
        const u8 orb = items[i];
        const i32 icon = isFusionOrb(orb) ? fusionIcons_[fusionOfOrb(orb)] : upgradeIcons_[orb < kUpgradeCount ? orb : 0];
        const float size = 34.0f * px * (isRiff(orb) ? kTorrentRiffScale : species_->modes.loadIn.iconScale);
        icons_.drawHud(d.hud, icon, at, size, 1.4f * (1.0f - 0.6f * u), lookColour, &ctx);
    }
    // The halo's arcs last: each leaves as a short arc of light flying outward.
    const double from = countTick_ + span;
    const u32 cells = pl.cells > 8 ? 8u : pl.cells;
    const Vec3 tone = palettes_->player[seat_ < kMaxPlayers ? seat_ : 0];
    for (u32 c = 0; c < cells; ++c) {
        const double launch = from + kTicksPerBeat * static_cast<double>(c) / static_cast<double>(cells);
        const float u = static_cast<float>((now - launch) / kTicksPerBeat);
        if (u < 0.0f || u > 1.0f) continue;
        const float mid = 1.5707963f + 6.2831853f * static_cast<float>(c) / static_cast<float>(cells);
        const float r = (30.0f + 900.0f * u * u) * px;
        const float half = 0.35f * 6.2831853f / static_cast<float>(cells) * (1.0f - 0.5f * u);
        const Vec3 col = hudColour(*look_, variant, tone, 1.6f * (1.0f - u));
        Vec2 prev = ship + Vec2{std::cos(mid - half), -std::sin(mid - half)} * r;
        for (u32 j = 1; j <= 6; ++j) {
            const float aa = mid - half + 2.0f * half * static_cast<float>(j) / 6.0f;
            const Vec2 q = ship + Vec2{std::cos(aa), -std::sin(aa)} * r;
            d.hud.line(prev, q, 1.4f, col, col, 0);
            prev = q;
        }
    }
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
    // design/modes/screens.md "The podium", arriving on the music: on its bar line the table writes itself in seat order, each row's
    // count climbing over the bar a sixteenth at a time (a tick a step) and the rows sliding into their places as the counts pass;
    // on the next bar line the winner is named (their row flashes and rises to the ring, the ring draws itself, the ship arrives in
    // it, the crown drops onto it over the beat and lands with a bell, WINNER struck, a clamped flash, confetti in their colour, the
    // lattice ringing once); a beat later the controls rise: the host's REMATCH and LOBBY, everyone's LEAVE. A draw: both rings
    // side by side, no crown. Reduce flashing: no flash; the confetti stays.
    const float s = ui.s(), W = ui.W(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const World& w = *world_;
    podiumS_ += frameDt_;
    const float beatS = 60.0f / std::fmax(static_cast<float>(w.bpm()), 30.0f), barS = 4.0f * beatS;
    const float tTable = clamp01(podiumS_ / barS);
    const float tWin = (podiumS_ - barS) / beatS;  // < 0 before the winner's bar line; 0..1 its beat
    const float tCtl = clamp01((podiumS_ - barS - beatS) / (0.5f * beatS));
    const bool rf = settings_.reduceFlashing || forcedReduceFlashing_;
    const u8 win = podiumWinner_;
    const sim::Mode mode = w.setup().mode;
    // The count and its word (TILES, GATES, ACTS, POPS; a score has none).
    char word[12] = {};
    {
        const char* c = species_ ? species_->modes.of(mode).counts : "";
        u32 k = 0;
        if (std::strcmp(c, "score") != 0)
            for (; c[k] && k + 1 < sizeof word; ++k) word[k] = static_cast<char>(c[k] >= 'a' && c[k] <= 'z' ? c[k] - 32 : c[k]);
        word[k] = 0;
    }
    // WINNER (A DRAW): struck on the winner's bar line, its letters written over the beat.
    if (tWin >= 0.0f) {
        const char* top = win == 0xFF ? "A DRAW" : "WINNER";
        const u32 n = static_cast<u32>(std::strlen(top));
        const float cap = 44.0f * s, x0 = cx - 0.5f * ui.measure(cap, top);
        char pre[12];
        for (u32 i = 0; i < n; ++i) {
            const float on = clamp01((clamp01(tWin) * static_cast<float>(n + 2) - static_cast<float>(i)) * 0.5f);
            if (on <= 0.0f || top[i] == ' ') continue;
            std::memcpy(pre, top, i);
            pre[i] = 0;
            const char one[2] = {top[i], 0};
            const float x = x0 + (i ? ui.measure(cap, pre) : 0.0f);
            ui.font().drawField(ui.draw().hud, {x, 104.0f * s - cap}, cap, 1.6f * s, ui.col(palette_->accent, 1.6f * on), ui.col(palette_->accent, 1.6f * on),
                                ui.col(palette_->accent, 1.6f * on), one);
        }
    }
    // The rings: the winner's, or a draw's two side by side; each draws itself over the winner's beat.
    const auto ringAt = [&](Vec2 c, u8 q, float u, bool crown) {
        const Vec3 pc = palettes_->player[q < kMaxPlayers ? q : 0];
        ui.draw().pools.glow(c, 500.0f * s, pc * (0.06f * u * hudFade()));
        ui.dot(c, 140.0f * s, pc, 0.07f * u);
        const u32 segs = static_cast<u32>(72.0f * u);
        for (u32 j = 0; j < segs; ++j) {
            const float a0 = -1.5707963f + 6.2831853f * static_cast<float>(j) / 72.0f, a1 = -1.5707963f + 6.2831853f * static_cast<float>(j + 1) / 72.0f;
            const Vec3 rc = ui.col(pc, 2.0f);
            ui.draw().hud.line(c + Vec2{std::cos(a0), std::sin(a0)} * (150.0f * s), c + Vec2{std::cos(a1), std::sin(a1)} * (150.0f * s), 1.4f * s, rc, rc, 0);
        }
        // the ship arrives in it, at three times the seat's size, turning slowly
        drawTitleShip(ui, c, -1.5707963f + 0.3f * podiumS_, 3.0f * (0.4f + 0.6f * u), u, false, q);
        if (!crown) return;
        // the crown drops onto it from above over the beat (120 px wide, the pickup's white-gold at 2.0)
        const float drop = 1.0f - u;
        const Vec3 wg = ui.col(mix(palettes_->pickup, white, 0.35f), 2.0f * clamp01(u * 3.0f));
        const float hw = 60.0f * s, h = 52.0f * s, by = c.y - 150.0f * s - 8.0f * s - 260.0f * s * drop * drop;
        const Vec2 pts[7] = {{c.x - hw, by}, {c.x - hw, by - h}, {c.x - 0.5f * hw, by - 0.5f * h}, {c.x, by - 1.3f * h},
                             {c.x + 0.5f * hw, by - 0.5f * h}, {c.x + hw, by - h}, {c.x + hw, by}};
        for (u32 j = 0; j + 1 < 7; ++j) ui.draw().hud.line(pts[j], pts[j + 1], 1.4f * s, wg, wg, 0);
        ui.draw().hud.line(pts[6], pts[0], 1.4f * s, wg, wg, 0);
    };
    // The order the table settles in (best first; seat order at the start), the counts as they climb.
    u8 seats[kMaxPlayers];
    u32 n = 0;
    for (u32 q = 0; q < kMaxPlayers; ++q)
        if (w.player(q).present) seats[n++] = static_cast<u8>(q);
    const float step = std::floor(tTable * 16.0f) / 16.0f;  // (a tick a step: sixteen over the bar)
    u64 shown[kMaxPlayers] = {};
    for (u32 k = 0; k < n; ++k) shown[seats[k]] = static_cast<u64>(static_cast<double>(w.versusCount(seats[k])) * static_cast<double>(step));
    u8 order[kMaxPlayers];
    for (u32 k = 0; k < n; ++k) {
        u32 at = k;
        while (at > 0 && shown[order[at - 1]] < shown[seats[k]]) {
            order[at] = order[at - 1];
            --at;
        }
        order[at] = seats[k];
    }
    if (podiumS_ <= frameDt_ + 1e-4f)
        for (u32 k = 0; k < n; ++k) podiumRowY_[seats[k]] = 700.0f + 46.0f * static_cast<float>(k);  // (seat order to begin with)
    const float slide = 1.0f - std::exp(-frameDt_ * 12.0f);
    for (u32 k = 0; k < n; ++k) {
        const u8 q = order[k];
        podiumRowY_[q] += (700.0f + 46.0f * static_cast<float>(k) - podiumRowY_[q]) * slide;
    }
    const float tableIn = clamp01(podiumS_ / (0.5f * beatS));
    for (u32 k = 0; k < n; ++k) {
        const u8 q = order[k];
        const float y = podiumRowY_[q] * s;
        const Vec3 pc = palettes_->player[q];
        // the winner's row flashes as it is named
        const float flash = q == win && tWin >= 0.0f && tWin < 0.25f ? 1.0f + 2.0f * (1.0f - tWin / 0.25f) : 1.0f;
        char left[48], right[64], num[32], chain[32];
        char seatLabel[8];
        std::snprintf(seatLabel, sizeof seatLabel, "P%u", q + 1u);
        const char* given0 = session_ ? session_->seatName(q) : "";
        const char* given = std::strcmp(given0, seatLabel) == 0 ? "" : given0;  // (a seat named for itself once)
        std::snprintf(left, sizeof left, "%u  P%u%s%s%s", k + 1u, q + 1u, given[0] ? "  " : "", given, q == seat_ ? " (YOU)" : "");
        groupDigits(num, sizeof num, shown[q]);
        groupDigits(chain, sizeof chain, w.player(q).bestChain);
        const u8 crowns = session_ ? session_->crowns(q) : 0;
        std::snprintf(right, sizeof right, "%s%s%s   X%s   %u CROWN%s", num, word[0] ? " " : "", word, chain, crowns, crowns == 1 ? "" : "S");
        ui.text({cx - 660.0f * s, y}, 22.0f * s, pc, 1.4f * tableIn * flash, left, 0.0f);
        ui.text({cx + 660.0f * s, y}, 22.0f * s, pc, 1.4f * tableIn * flash, right, 1.0f);
    }
    if (tWin >= 0.0f) {
        const float u = clamp01(tWin);
        const float e = 1.0f - (1.0f - u) * (1.0f - u);
        if (win != 0xFF) {
            const Vec2 c{cx, 330.0f * s};
            ringAt(c, win, e, true);
            // the winner's row rises to the ring: their name and count, under it
            const Vec3 pc = palettes_->player[win];
            const float rowY = podiumRowY_[win] * s;
            char name[32], seatLabel[8];
            std::snprintf(seatLabel, sizeof seatLabel, "P%u", win + 1u);
            const char* given0 = session_ ? session_->seatName(win) : "";
            const char* given = std::strcmp(given0, seatLabel) == 0 ? "" : given0;
            std::snprintf(name, sizeof name, "P%u%s%s", win + 1u, given[0] ? "  " : "", given);
            ui.text({cx, rowY + (540.0f * s - rowY) * e}, 34.0f * s, pc, 1.6f * e, name);
            char num[32], line[48];
            groupDigits(num, sizeof num, w.versusCount(win));
            std::snprintf(line, sizeof line, "%s%s%s", num, word[0] ? " " : "", word);
            const float cap = 60.0f * s;
            ui.font().drawField(ui.draw().hud, {cx - 0.5f * ui.measure(cap, line), 620.0f * s - cap}, cap, 2.4f * s, ui.col(white, 1.6f * e),
                                ui.col(white, 1.6f * e), ui.col(pc, 1.6f * e), line);
            if (!rf && tWin < 120.0f / 1000.0f / beatS) ui.draw().pools.glow(c, 1000.0f * s, pc * (0.3f * hudFade()));  // (the clamped flash, 120 ms)
            // Confetti in the winner's colour (the Drop's recipe): flecks thrown up from the ring, falling and turning.
            const float age = podiumS_ - barS;
            for (u32 j = 0; j < 90 && age < 3.0f; ++j) {
                const u32 hsh = (j + 1u) * 2654435761u;
                const float r0 = static_cast<float>(hsh & 1023u) / 1023.0f, r1 = static_cast<float>((hsh >> 10) & 1023u) / 1023.0f;
                const float r2 = static_cast<float>((hsh >> 20) & 1023u) / 1023.0f;
                const float a = -1.5707963f + (r0 - 0.5f) * 2.6f;
                const float v = (500.0f + 700.0f * r1) * s;
                const Vec2 p = c + Vec2{std::cos(a) * v * age, std::sin(a) * v * age + 0.5f * 900.0f * s * age * age};
                const float spin = 6.0f * age + 6.2831853f * r2;
                const Vec2 d = Vec2{std::cos(spin), std::sin(spin)} * (7.0f * s);
                const Vec3 fc = ui.col(j % 3 == 0 ? white : (j % 3 == 1 ? pc : palettes_->playerLight[win]), 1.8f * clamp01(1.0f - age / 3.0f));
                ui.draw().hud.line(p - d, p + d, 1.6f * s, fc, fc, 0);
            }
        } else {
            // A draw: both rings side by side, 300 px apart, no crown (the two best).
            if (n >= 2) {
                ringAt({cx - 300.0f * s, 330.0f * s}, order[0], e, false);
                ringAt({cx + 300.0f * s, 330.0f * s}, order[1], e, false);
            }
        }
        if (tWin >= 1.0f && !podiumCrowned_) {
            podiumCrowned_ = true;
            if (win != 0xFF) pulse(2);  // (sound.md's haptics: heavy for the winner's crown)
            if (win != 0xFF && !rf) view_.dropRing(w.player(seat_).pos, tickEnd_);  // (the lattice ringing once)
        }
    }
    const bool host = session_ && session_->host();
    const float rise = (1.0f - tCtl) * 14.0f * s;
    if (tCtl > 0.0f) {
        setHudFade(tCtl);
        // (the device check, 2026-11-16: the pill 120 tall at 950, so WAITING FOR HOST keeps the 24 px rule under it)
        if (ui.pill(43, {cx, 950.0f * s + rise}, {600.0f * s, 120.0f * s}, "REMATCH", 52.0f * s, true, host && tCtl >= 1.0f) || (keyPrimary_ && host && tCtl >= 1.0f)) {
            session_->requestRetry();
            if (band_) band_->playSfx(audio::Sfx::UiConfirm);
        }
        if (!host) ui.text({cx, 1052.0f * s + rise}, 14.0f * s, white, 0.8f, "WAITING FOR HOST", 0.5f);
        // LOBBY (the host's): back to the lobby, the room standing, to change the mode.
        const bool lobbyAuto = opt_.coopBackLobby && host && podiumS_ > barS + 1.5f * beatS + 2.0f;
        if (host && ((ui.pill(45, {W - 412.0f * s, 950.0f * s + rise}, {330.0f * s, 90.0f * s}, "LOBBY", 28.0f * s, false, tCtl >= 1.0f) && tCtl >= 1.0f) || lobbyAuto) &&
            trans_ == Trans::None) {
            opt_.coopBackLobby = false;
            if (band_) band_->playSfx(audio::Sfx::UiBack);
            session_->backToLobby();
            RF_LOG("net: LOBBY: the room goes back to its lobby");
        }
        if ((ui.pill(44, {282.0f * s, 950.0f * s + rise}, {300.0f * s, 90.0f * s}, "LEAVE", 28.0f * s, false, true, trans_ == Trans::Home) || keyBack_) &&
            trans_ == Trans::None)
            homeTap();
        setHudFade(1.0f);
    }
    keyPrimary_ = keyBack_ = false;
}

void Play::podiumSounds(double base) {
    // design/modes/sound.md "The screens": the table: a tick for each step of each row's count, each row at its player's pitch (the
    // home chord's root, third, fifth and octave by seat); the winner named: the crown's landing (a bell on the home chord's root two
    // octaves up, an impact at half level), then the fanfare: the winner's lead playing the title's eight a sixteenth apart over the
    // band's stab on the home chord at 1.2. A draw: the stab alone. `base`: the podium's bar line (band ticks).
    if (!bandDrives_ || !band_) return;
    const World& w = *world_;
    const double t16 = kTicksPerBeat / 4.0;
    static constexpr i32 kSeatTone[kMaxPlayers] = {0, 1, 2, 4};
    for (u32 q = 0; q < kMaxPlayers; ++q) {
        if (!w.player(q).present) continue;
        for (u32 j = 1; j <= 16; ++j)
            band_->playRecipe(audio::Recipe::Tick, audio::chordNote(kSeatTone[q], 6, audio::NoteFrom::ChordTone), 0.22f, 0.0f, base + static_cast<double>(j - 1) * t16);
    }
    const double named = base + kTicksPerBar;
    if (podiumWinner_ == 0xFF) {
        band_->playRecipe(audio::Recipe::Stab, -1, 1.0f, 0.0f, named);
        return;
    }
    const double lands = named + kTicksPerBeat;
    band_->playRecipe(audio::Recipe::Bell, audio::chordNote(0, 6, audio::NoteFrom::ChordTone), 0.8f, 0.0f, lands);
    band_->playRecipe(audio::Recipe::Impact, -1, 0.5f, 0.0f, lands);
    band_->playRecipe(audio::Recipe::Stab, -1, 1.2f, 0.0f, lands);
    for (u32 j = 0; j < 8; ++j) band_->leadNoteAt(bandSlot(podiumWinner_), kTitleEight[j], 0.9f, 0.0f, lands + static_cast<double>(j) * t16);
}

}  // namespace rf
