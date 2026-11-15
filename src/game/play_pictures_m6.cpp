// M6: the m6_ review pictures (scripts/review-m3b.ps1 -Only m6_...; captures\m6\): every first-run hint line over a scene that
// fits it, ENDED BY and SET COMPLETE on the game over, SET COMPLETE's card in the run, the title's gold star, the UI scale at 90%
// and 130% with the touch controls shown, the mini-boss that stays (13 bars after its arrival, the Drop held), and the boss
// hit flash (a white copy of the part's own shape) on three fights. Fixed 1/60 s steps and no band, like the other pictures.

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/args.h"
#include "core/log.h"
#include "game/play.h"
#include "sim/boss_prose.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
enum class M6 : u8 { Hint, EndedBy, SetCard, SetOver, TitleStar, UiScale, MiniStays, Flash, Credits, LwEntrance, LwLunge, LwClose, DropTitle,
                     DropRetry, Home, PauseRestart, PauseQuit, HzWave, HzTouch, TitleIdle, FbRiffs, FxCrit, FxCresc, FxSync, FxBurn, FxWell,
                     FxHole, FxCoil, FxAegis, FxMeteor, FxHzDeath, T2Intro, T2Drop };
struct M6Pic {
    const char* name;   // m6_<name>
    M6 kind;
    u8 arg;             // the hint's index; the UI scale's setting (0 90%, 3 130%)
    const char* fight;  // sim/boss.h bossId (MiniStays, Flash)
};
const M6Pic kM6Pics[] = {
    {"hint_0", M6::Hint, 0, nullptr},          {"hint_1", M6::Hint, 1, nullptr},  {"hint_2", M6::Hint, 2, nullptr},
    {"hint_3", M6::Hint, 3, nullptr},          {"hint_4", M6::Hint, 4, nullptr},  {"hint_5", M6::Hint, 5, nullptr},
    {"hint_6", M6::Hint, 6, nullptr},          {"hint_7", M6::Hint, 7, nullptr},  {"hint_8", M6::Hint, 8, nullptr},
    {"hint_9", M6::Hint, 9, nullptr},          {"hint_10", M6::Hint, 10, nullptr},
    {"ended_by", M6::EndedBy, 0, nullptr},     {"set_complete", M6::SetCard, 0, nullptr},
    {"set_complete_over", M6::SetOver, 0, nullptr}, {"title_star", M6::TitleStar, 0, nullptr},
    {"uiscale_90", M6::UiScale, 0, nullptr},   {"uiscale_130", M6::UiScale, 3, nullptr},
    {"mini_stays", M6::MiniStays, 0, "alpha_hound"},
    {"flash_longwave", M6::Flash, 0, "longwave"}, {"flash_choir", M6::Flash, 0, "choir"}, {"flash_alpha", M6::Flash, 0, "alpha_hound"},
    // M7 (screens.md "Credits (M7)"): the credits screen at its top, and scrolled into the licences.
    {"credits", M6::Credits, 0, nullptr},       {"credits_licences", M6::Credits, 1, nullptr},
    // a953828 (Longwave never jumps), strips of eight: the entrance unwinding in from the wall (a frame each half second over
    // its two bars), a Whitecap lunge whipping down the body (every quarter second from the dash), a dead segment's gap closing
    // (every second frame from the death).
    {"lw_entrance", M6::LwEntrance, 0, nullptr}, {"lw_lunge", M6::LwLunge, 2, "longwave"}, {"lw_close", M6::LwClose, 0, "longwave"},
    // b12310b (screens.md "Transitions"), strips of eight a beat apart (a quarter beat after each): the drop-in from the title
    // (PLAY on a bar's second beat: three beats of wait, the downbeat, the run's first bar), RETRY's drop-in from game over, and
    // home (HOME on a bar's second beat: the game over leaving, the title building over the next bar, its run fading up).
    {"drop_title", M6::DropTitle, 0, nullptr}, {"drop_retry", M6::DropRetry, 0, nullptr}, {"home", M6::Home, 0, nullptr},
    // c9218dc (screens.md "Every other way in or out of a run"): the pause's RESTART (RETRY's drop-in) and QUIT RUN (home), the
    // pause taken on a bar's second beat, the choice half a second later.
    {"pause_restart", M6::PauseRestart, 0, nullptr}, {"pause_quit", M6::PauseQuit, 0, nullptr},
    // 3283249 (horizon.md rules 6 and 7: a shove is a push the ship rides), strips of eight a sixteenth apart in Event Horizon's
    // Tidal phase held: a tidal wave reaching a ship standing still 380 wu out (from a sixteenth before it), and a ship flying into
    // the horizon and thrown out over half a beat.
    {"hz_wave", M6::HzWave, 1, "horizon"}, {"hz_touch", M6::HzTouch, 1, "horizon"},  // (phase 1: Tidal, 0-based as --fight)
    // screens.md "Title" (the owner, 2026-11-14): the quiet title leaving over its bar, and a tap on PLAY's place two and a half
    // bars later bringing it back (PLAY not pressed): eight frames a beat apart from a beat before the bar line it leaves on, or
    // with --strip above 8 every frame from there (the video).
    {"title_idle", M6::TitleIdle, 0, nullptr},
    // feedback.md rule 4 (2026-11-14): the Echo carries its player's Orbitals, Pulse Rings and Sweeper. The ship, given the three
    // at level 3, held 300 wu from the Head (its first Echo 300 wu the other side), the fight held; from its fourth bar.
    {"fb_riffs", M6::FbRiffs, 0, "feedback"},
    // The owner, 2026-11-14 ("things that need more visual feedback"; play_view_feedback.cpp), strips (--strip N) from each moment:
    // On-Beat Crit's burst (every second frame from a crit's hit), Crescendo's meter (arg: the clean bars; 0: 14 broken by a hit),
    // Syncopation's accented shots (from an off-beat volley), Burn's flames on a crowd, Singularity's well (half a beat a frame from
    // its placing), Black Hole's singularity (a third of a beat a frame from the Nova), Storm Coil with Aegis among bodies, an
    // Aegis block (bullets fired into the arc), a meteor's last beat and its impact (every fourth frame), Event Horizon's death (half
    // a beat a frame from the kill).
    {"fx_crit", M6::FxCrit, 0, nullptr},   {"fx_cresc", M6::FxCresc, 10, nullptr}, {"fx_cresc_full", M6::FxCresc, 20, nullptr},
    {"fx_cresc_break", M6::FxCresc, 0, nullptr}, {"fx_sync", M6::FxSync, 0, nullptr}, {"fx_burn", M6::FxBurn, 0, nullptr},
    {"fx_well", M6::FxWell, 0, nullptr},   {"fx_hole", M6::FxHole, 0, nullptr},    {"fx_coil", M6::FxCoil, 0, nullptr},
    {"fx_aegis", M6::FxAegis, 0, nullptr}, {"fx_meteor", M6::FxMeteor, 0, nullptr}, {"fx_hz_death", M6::FxHzDeath, 1, "horizon"},
    // Title v2 (screens.md "Title"): the intro as a cold start plays it, a strip an eighth apart from the beat before its point; the
    // drop from PLAY on a bar's second beat, an eighth apart from two beats before its bar line.
    {"t2_intro", M6::T2Intro, 0, nullptr}, {"t2_drop", M6::T2Drop, 0, nullptr},
};
constexpr u32 kM6Count = sizeof(kM6Pics) / sizeof(kM6Pics[0]);
constexpr u32 kM6At = 300;  // after the run's Track card (2.25 bars)
}  // namespace

bool Play::scriptSetupM6() {
    const char* p = opt_.picture;
    if (std::strncmp(p, "m6_", 3) != 0) return false;
    for (u32 k = 0; k < kM6Count; ++k)
        if (std::strcmp(p + 3, kM6Pics[k].name) == 0) m6Pic_ = static_cast<u16>(k);
    if (m6Pic_ == 0xFFFF) {
        RF_ERR("play: no picture '%s'", p);
        return true;
    }
    World& w = *world_;
    const M6Pic& pic = kM6Pics[m6Pic_];
    w.rules().godMode = true;
    switch (pic.kind) {
        case M6::Hint: {
            // The hint over a still ship; a ring of bodies for the lines about enemies, bullets and the Nova.
            forcedHint_ = pic.arg;
            w.spawner().mode = Spawner::Mode::Off;
            w.playerMut(0).novas = pic.arg == 6 ? 1 : 0;
            if (pic.arg == 2 || pic.arg == 3 || pic.arg == 6 || pic.arg == 7) {
                const u32 n = pic.arg == 6 ? 14u : 8u;
                for (u32 k = 0; k < n; ++k) {
                    const float a = kTau * static_cast<float>(k) / static_cast<float>(n);
                    const Vec2 at{std::cos(a) * (pic.arg == 6 ? 260.0f : 520.0f), std::sin(a) * (pic.arg == 6 ? 200.0f : 300.0f)};
                    const Family f = pic.arg == 7 && k % 2 == 0 ? Family::Sentry : Family::Drifter;
                    const i32 i = w.spawnEnemy(SpeciesTable::index(f, 1, f == Family::Sentry ? kVariantB : kVariantA), at, angleOf(Vec2{} - at), 0.0);
                    if (i >= 0) w.setEnemyHp(static_cast<u32>(i), 4000.0f);
                }
            }
            if (pic.arg == 0 || pic.arg == 3) touch_.markUsed();  // (the touch lines: the controls shown)
            break;
        }
        case M6::EndedBy:
        case M6::SetOver:
        case M6::DropRetry:
        case M6::Home:
        case M6::PauseRestart:
        case M6::PauseQuit:
            flow_ = true;
            screen_ = Screen::Run;
            w.spawner().mode = Spawner::Mode::Slice;
            if (pic.kind != M6::EndedBy && pic.kind != M6::SetOver) stripStride_ = static_cast<u32>(std::lround(3600.0 / std::fmax(w.tempoAt(0.0), 30.0f)));
            break;
        case M6::DropTitle:
            flow_ = true;
            screen_ = Screen::Title;
            w.spawner().mode = Spawner::Mode::Slice;
            stripStride_ = static_cast<u32>(std::lround(3600.0 / std::fmax(w.tempoAt(0.0), 30.0f)));
            break;
        case M6::TitleIdle:
        case M6::T2Intro:
        case M6::T2Drop:
            flow_ = true;
            screen_ = Screen::Title;
            w.spawner().mode = Spawner::Mode::Slice;
            if (pic.kind != M6::TitleIdle) stripStride_ = static_cast<u32>(std::lround(1800.0 / std::fmax(w.tempoAt(0.0), 30.0f)));
            if (pic.kind == M6::T2Intro) introPending_ = false;  // (started by the script)
            break;
        case M6::SetCard:
            w.spawner().mode = Spawner::Mode::Slice;
            break;
        case M6::TitleStar: {
            flow_ = true;
            screen_ = Screen::Title;
            w.spawner().mode = Spawner::Mode::Slice;
            HighScore h;
            h.score = 4862310;
            h.track = 11;
            h.bestChain = 412;
            h.seconds = 3127;
            h.kills = 21904;
            h.setComplete = true;
            scores_.add(h);  // (in memory only: pictures save nothing)
            break;
        }
        case M6::UiScale:
            w.spawner().mode = Spawner::Mode::Slice;
            scriptFastForwardS_ = 8.0f;
            settings_.uiScale = pic.arg;
            applySettings();
            touch_.markUsed();
            break;
        case M6::Credits:
            flow_ = true;
            screen_ = Screen::Credits;
            settingsBack_ = Screen::Title;
            w.spawner().mode = Spawner::Mode::Slice;
            break;
        case M6::LwEntrance:
            w.spawner().mode = Spawner::Mode::Off;
            stripStride_ = 30;
            break;
        case M6::LwLunge:
        case M6::LwClose:
            if (!setupFight(pic.fight, pic.arg)) return true;
            opt_.fightHold = true;
            stripStride_ = pic.kind == M6::LwLunge ? 15u : 2u;
            break;
        case M6::HzWave:
        case M6::HzTouch:
            if (!setupFight(pic.fight, pic.arg)) return true;
            opt_.fightHold = true;
            break;
        case M6::FbRiffs: {
            if (!setupFight(pic.fight, pic.arg)) return true;
            opt_.fightHold = true;
            Loadout& l = w.playerMut(0).loadout;
            l.riffs[riffIndex(kOrbitals)] = 3;
            l.riffs[riffIndex(kPulseRing)] = 3;
            l.riffs[riffIndex(kSweeper)] = 3;
            break;
        }
        case M6::MiniStays:
        case M6::Flash:
            if (!setupFight(pic.fight, 0)) return true;
            opt_.fightHold = true;  // (it keeps its health: the mini-boss stays to its sixteenth bar; the flash picture waits for a hit)
            break;
        case M6::FxCrit:
        case M6::FxCresc:
        case M6::FxSync:
        case M6::FxBurn:
        case M6::FxWell:
        case M6::FxHole:
        case M6::FxCoil:
        case M6::FxAegis:
        case M6::FxMeteor: {
            w.spawner().mode = Spawner::Mode::Off;
            Loadout& l = w.playerMut(0).loadout;
            const float beatFrames = 3600.0f / std::fmax(w.tempoAt(0.0), 30.0f);
            bool crowd = true, ring = false;
            switch (pic.kind) {
                case M6::FxCrit: l.mods[kOnBeatCrit] = 3; l.mods[kSplit] = 2; stripStride_ = 2; break;
                case M6::FxCresc: l.mods[kCrescendo] = 2; l.mods[kSplit] = 2; stripStride_ = pic.arg ? 1u : 3u; break;
                case M6::FxSync: l.mods[kSyncopation] = 2; l.mods[kSplit] = 2; stripStride_ = 2; break;
                case M6::FxBurn: l.mods[kBurn] = 3; l.mods[kSplit] = 3; stripStride_ = 4; break;
                case M6::FxWell: l.riffs[riffIndex(kSingularity)] = 3; stripStride_ = static_cast<u32>(std::lround(0.5f * beatFrames)); break;
                case M6::FxHole: stripStride_ = static_cast<u32>(std::lround(beatFrames / 3.0f)); break;  // (taken at kM6At: it goes off at once)
                case M6::FxCoil: l.riffs[riffIndex(kStormCoil)] = 5; l.mods[kAegis] = 3; stripStride_ = 3; ring = true; break;
                case M6::FxAegis: l.mods[kAegis] = 3; stripStride_ = 2; crowd = false; break;
                case M6::FxMeteor: stripStride_ = 4; crowd = false; break;
                default: break;
            }
            // Bodies that live through it: a crowd ahead of the ship (it fires to the right), or a ring round it (Storm Coil).
            const u32 n = ring ? 10u : (crowd ? 18u : 0u);
            for (u32 k = 0; k < n; ++k) {
                Vec2 at;
                if (ring) {
                    const float a = kTau * static_cast<float>(k) / static_cast<float>(n);
                    at = Vec2{std::cos(a) * 230.0f, std::sin(a) * 180.0f};
                } else {
                    at = Vec2{380.0f + 90.0f * static_cast<float>(k % 3), -260.0f + 520.0f * static_cast<float>(k / 3) / 5.0f};
                }
                const i32 i = w.spawnEnemy(SpeciesTable::index(Family::Drifter, 1, kVariantA), at, angleOf(Vec2{} - at), 0.0);
                if (i >= 0) w.setEnemyHp(static_cast<u32>(i), 4000.0f);
            }
            break;
        }
        case M6::FxHzDeath:
            if (!setupFight(pic.fight, pic.arg)) return true;
            opt_.fightHold = true;
            break;
    }
    return true;
}

bool Play::scriptInputM6(StickInput& in) {
    if (m6Pic_ == 0xFFFF) return false;
    World& w = *world_;
    const M6Pic& pic = kM6Pics[m6Pic_];
    const u32 t = scriptTick_;
    const i64 now = static_cast<i64>(frameNo_);
    const Vec2 ship = controller_.pose().pos;
    switch (pic.kind) {
        case M6::Hint: {
            in = StickInput{};
            in.assist = true;  // (the assist's fire, as in a first run)
            if (pic.arg == 2 && t > 120) {
                // (the stream at the nearest body, as the assist would aim it: the line is about the ship firing by itself)
                const Enemies& e = w.enemies();
                i32 best = -1;
                float bd = 1e30f;
                for (u32 i = 0; i < e.count; ++i) {
                    if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping) continue;
                    const float dd = lengthSq(Vec2{e.x[i], e.y[i]} - ship);
                    if (dd < bd && dd > 200.0f * 200.0f) {  // (one off the ship, so the stream is seen)
                        bd = dd;
                        best = static_cast<i32>(i);
                    }
                }
                if (best >= 0) {
                    in.aim = Vec2{e.x[best], e.y[best]} - ship;
                    in.aimActive = true;
                }
            }
            if (t == 200 && pic.arg == 4) {
                const u8 forced[3] = {kSplit, kMagnet, kOrbitals};
                w.openTrioNow(0, music1_ + 4.0 * kTicksPerBar, forced, kTrioLevel);  // (a667116: the first level-up Trio)
            }
            if (t == 200 && pic.arg == 8) w.dropBoost(ship + Vec2{260.0f, -80.0f}, kPrism);
            if (t == 200 && pic.arg == 9) w.dropExtraLifeNow(ship + Vec2{260.0f, -80.0f}, 0);
            if (t == 200 && pic.arg == 10) w.placeStructureNow(StructKind::Vault, ship + Vec2{460.0f, 0.0f}, 0.0f);
            if (t == 200 && pic.arg == 5) {
                // (a real hit: the ring's moment)
                PlayerState& pl = w.playerMut(0);
                pl.invulS = pl.novaInvulS = 0.0f;
                w.rules().godMode = false;
                w.spawnBullet(pl.pos, Vec2{}, 0.0f);
            }
            if (t == 202) w.rules().godMode = true;
            if (captureFrom_ < 0 && t == kM6At + (pic.arg == 7 ? 60u : 0u)) captureFrom_ = now;
            return true;
        }
        case M6::PauseRestart:
        case M6::PauseQuit: {
            const double inBar = std::fmod(music1_, kTicksPerBar);
            in = screen_ == Screen::Run && trans_ == Trans::None ? bot_.think(w, 0, controller_, clock_.heardNow()) : StickInput{};
            if (screen_ == Screen::Run && trans_ == Trans::None && t >= kM6At && captureFrom_ < 0 && inBar >= kTicksPerBeat && inBar < kTicksPerBeat + 4.0) {
                pauseRun();
                placedAt_ = t;
            }
            if (screen_ == Screen::Paused && captureFrom_ < 0 && t >= placedAt_ + 30u) {
                endRun(false);
                if (pic.kind == M6::PauseRestart) dropInTap(Screen::Paused);
                else toTitle();
                captureFrom_ = now + static_cast<i64>(std::lround(1.25 * stripStride_));
            }
            return true;
        }
        case M6::DropTitle:
        case M6::DropRetry:
        case M6::Home: {
            // The tap on a bar's second beat (a quarter beat after it the strip's first frame, then a frame a beat).
            const double inBar = std::fmod(music1_, kTicksPerBar);
            const bool tapNow = captureFrom_ < 0 && inBar >= kTicksPerBeat && inBar < kTicksPerBeat + 4.0;
            if (pic.kind == M6::DropTitle) {
                in = trans_ == Trans::None ? bot_.think(w, 0, controller_, clock_.heardNow()) : StickInput{};
                if (trans_ == Trans::None) attractInput(in);  // (the title's demo, as in the game)
                if (t >= kM6At && tapNow) {
                    startRun();
                    captureFrom_ = now + static_cast<i64>(std::lround(1.25 * stripStride_));
                }
                return true;
            }
            in = screen_ == Screen::Run && trans_ == Trans::None ? bot_.think(w, 0, controller_, clock_.heardNow()) : StickInput{};
            if (t == kM6At) {
                // The last cell taken by a body, so the run ends here.
                PlayerState& pl = w.playerMut(0);
                pl.cells = 0;
                pl.novas = 0;
                pl.extraLives = 0;
                pl.invulS = pl.novaInvulS = 0.0f;
                w.rules().godMode = false;
                w.spawnEnemy(SpeciesTable::index(Family::Hound, 2, kVariantA), pl.pos, 0.0f, 0.0);
            }
            if (screen_ == Screen::GameOver && trans_ == Trans::None && overS_ > 2.0f && tapNow) {
                if (pic.kind == M6::DropRetry) dropInTap(Screen::GameOver);
                else toTitle();
                captureFrom_ = now + static_cast<i64>(std::lround(1.25 * stripStride_));
            }
            return true;
        }
        case M6::EndedBy:
        case M6::SetOver:
            in = bot_.think(w, 0, controller_, clock_.heardNow());
            if (t == kM6At) {
                if (pic.kind == M6::SetOver) {
                    runSetComplete_ = true;
                    setCompleteT_ = static_cast<float>(tickEnd_);
                setCompleteScore_ = w.player(0).score;
                }
                // The last cell taken by a body (a Hound on the ship), so the line names what took it.
                PlayerState& pl = w.playerMut(0);
                pl.cells = 0;
                pl.novas = 0;
                pl.extraLives = 0;
                pl.invulS = pl.novaInvulS = 0.0f;
                w.rules().godMode = false;
                w.spawnEnemy(SpeciesTable::index(Family::Hound, 2, kVariantA), pl.pos, 0.0f, 0.0);
            }
            if (t >= kM6At) in = StickInput{};
            if (screen_ == Screen::GameOver && captureFrom_ < 0 && overS_ > 2.6f) captureFrom_ = now;
            return true;
        case M6::SetCard: {
            in = bot_.think(w, 0, controller_, clock_.heardNow());
            if (t == kM6At) {
                runSetComplete_ = true;
                setCompleteT_ = static_cast<float>(tickEnd_);
                setCompleteScore_ = w.player(0).score;
            }
            if (captureFrom_ < 0 && t == kM6At + 60) captureFrom_ = now;  // (half a bar in: the card written)
            return true;
        }
        case M6::Credits:
            in = bot_.think(w, 0, controller_, clock_.heardNow());
            if (pic.arg == 1) creditsScroll_ = 1400.0f * (layoutH_ > 0.0f ? layoutH_ / 1080.0f : 1.0f);  // (into the licences)
            if (captureFrom_ < 0 && t == kM6At) captureFrom_ = now;
            return true;
        case M6::TitleStar:
            in = bot_.think(w, 0, controller_, clock_.heardNow());
            attractInput(in);
            if (captureFrom_ < 0 && t == kM6At) captureFrom_ = now;
            return true;
        case M6::T2Intro: {
            in = bot_.think(w, 0, controller_, clock_.heardNow());
            attractInput(in);
            if (t == kM6At) {
                startIntro();
                // (from the beat before its point)
                const double toPoint = (introAt_ - 2.0 * kTicksPerBeat - music1_) / static_cast<double>(w.ticksPerSecond()) * 60.0;
                captureFrom_ = now + std::max<i64>(0, static_cast<i64>(std::lround(toPoint)));
            }
            return true;
        }
        case M6::T2Drop: {
            const double inBar = std::fmod(music1_, kTicksPerBar);
            in = trans_ == Trans::None ? bot_.think(w, 0, controller_, clock_.heardNow()) : StickInput{};
            if (trans_ == Trans::None) attractInput(in);
            if (t >= kM6At && captureFrom_ < 0 && trans_ == Trans::None && inBar >= kTicksPerBeat && inBar < kTicksPerBeat + 4.0) {
                startRun();
                const double toCapture = (transLine_ - 2.0 * kTicksPerBeat - music1_) / static_cast<double>(w.ticksPerSecond()) * 60.0;
                captureFrom_ = now + std::max<i64>(0, static_cast<i64>(std::lround(toCapture)));
            }
            return true;
        }
        case M6::TitleIdle: {
            in = bot_.think(w, 0, controller_, clock_.heardNow());
            attractInput(in);  // (the title's demo, as in the game)
            const Vec2 play{0.5f * layoutW_, 759.0f * layoutH_ / 1080.0f};  // (PLAY's centre, screenTitle)
            if (captureFrom_ < 0 && titleHideLine_ >= 0.0 && titleWakeAt_ < 0.0) {
                const double beat = 3600.0 / std::fmax(w.tempoAt(music1_), 30.0f);  // frames a beat
                const double toLine = (titleHideLine_ - music1_) / static_cast<double>(w.ticksPerSecond()) * 60.0;
                stripStride_ = stripFrames_ > 8 ? 1u : static_cast<u32>(std::lround(beat));
                captureFrom_ = now + std::max<i64>(0, static_cast<i64>(std::lround(toLine - beat)));
                placedAt_ = t + static_cast<u32>(std::lround(toLine + 10.0 * beat));
                RF_LOG("play: picture '%s': the title leaves in %.0f frames, the tap %u frames after that", opt_.picture, toLine,
                       placedAt_ - t - static_cast<u32>(std::lround(toLine)));
            }
            if (captureFrom_ >= 0 && t == placedAt_) uiPointer(play, true);
            if (captureFrom_ >= 0 && t == placedAt_ + 6u) {
                uiPointer(play, false);
                RF_LOG("play: picture '%s': tapped PLAY's place; the screen is %s", opt_.picture, screen_ == Screen::Title && trans_ == Trans::None ? "still the title (right)" : "not the title (wrong)");
            }
            if (captureFrom_ >= 0 && t == placedAt_ + 200u)
                RF_LOG("play: picture '%s': 200 frames after the tap the screen is %s", opt_.picture, screen_ == Screen::Title && trans_ == Trans::None ? "the title (right)" : "not the title (wrong)");
            return true;
        }
        case M6::UiScale:
            in = bot_.think(w, 0, controller_, clock_.heardNow());
            if (!ffRunning_ && captureFrom_ < 0 && t == 24) captureFrom_ = now;
            return true;
        case M6::FxCrit:
        case M6::FxCresc:
        case M6::FxSync:
        case M6::FxBurn:
        case M6::FxWell:
        case M6::FxHole:
        case M6::FxCoil:
        case M6::FxAegis:
        case M6::FxMeteor: {
            in = StickInput{};
            in.assist = false;
            w.closeTrioNow(0);
            w.playerMut(0).levelsBanked = 0;
            const bool fires = pic.kind == M6::FxCrit || pic.kind == M6::FxCresc || pic.kind == M6::FxSync || pic.kind == M6::FxBurn;
            if (fires && t > 60) {
                in.aim = Vec2{1.0f, 0.0f};
                in.aimActive = true;
            }
            const auto upgradeFx = [&](u8 fx) {
                for (u32 k = 0; k < w.eventCount(); ++k)
                    if (w.events()[k].type == EventType::Upgrade && w.events()[k].b == fx && w.events()[k].a == 0) return true;
                return false;
            };
            if (pic.kind == M6::FxCresc) {
                if (t == kM6At - 1) w.playerMut(0).cleanBars = static_cast<u16>(pic.arg ? pic.arg : 14u);
                if (t == kM6At + 20 && !pic.arg) w.playerMut(0).cleanBars = 0;  // (a hit)
                if (captureFrom_ < 0 && t == kM6At + (pic.arg ? 2u : 18u)) captureFrom_ = now;
                return true;
            }
            if (pic.kind == M6::FxHole && t == kM6At) w.grantBoost(0, kBlackHole);
            if (pic.kind == M6::FxMeteor && t == 200) w.startEventNow(EventKind::MeteorRain);
            if (pic.kind == M6::FxAegis && t >= 200 && t % 20u == 0u) {
                // (into the arc: fast, so it has not turned away by the time they reach it)
                const Vec2 dv{std::cos(w.aegisAngle(0)), std::sin(w.aegisAngle(0))};
                w.spawnBullet(ship + dv * 200.0f, dv * -1000.0f, 0.0f);
            }
            if (captureFrom_ >= 0 || t < kM6At) return true;
            bool go = false;
            switch (pic.kind) {
                case M6::FxCrit:
                    for (u32 k = 0; k < w.eventCount() && !go; ++k) go = w.events()[k].type == EventType::CritBurst;
                    break;
                case M6::FxSync: {
                    const double inBeat = std::fmod(music1_, kTicksPerBeat);
                    go = inBeat >= 0.5 * kTicksPerBeat && inBeat < 0.5 * kTicksPerBeat + 4.0;
                    break;
                }
                case M6::FxBurn: go = t >= kM6At + 60; break;
                case M6::FxCoil: go = true; break;
                case M6::FxWell: go = upgradeFx(kFxWell); break;
                case M6::FxHole: go = upgradeFx(kFxHole); break;
                case M6::FxAegis: go = upgradeFx(kFxAegis); break;
                case M6::FxMeteor:
                    for (u32 k = 0; k < World::kArenaMeteors && !go; ++k) {
                        const ArenaMeteor& m = w.meteors()[k];
                        go = m.land > music1_ && m.land - music1_ <= 0.8 * kTicksPerBeat && lengthSq(m.at - ship) < 80.0f * 80.0f;
                    }
                    break;
                default: break;
            }
            if (go) {
                captureFrom_ = now + (pic.kind == M6::FxSync ? 3 : 0);
                RF_LOG("play: picture '%s' captures from frame %lld (music %.1f)", opt_.picture, static_cast<long long>(captureFrom_), music1_);
            }
            return true;
        }
        case M6::FxHzDeath: {
            in = StickInput{};
            in.assist = false;
            if (ffRunning_) return true;
            w.closeTrioNow(0);
            w.playerMut(0).levelsBanked = 0;
            const BossState& b = w.boss();
            const double inBeat = std::fmod(music1_, kTicksPerBeat);
            if (t >= 200 && b.active && !b.dying && inBeat < 5.0) w.killBossNextTick(0, true);
            if (captureFrom_ < 0 && b.dying) {
                stripStride_ = static_cast<u32>(std::lround(1800.0 / std::fmax(w.tempoAt(music1_), 30.0f)));
                captureFrom_ = now;
            }
            return true;
        }
        case M6::LwEntrance: {
            in = StickInput{};
            in.assist = false;
            if (t == 200) {
                // Longwave on the next bar line round the ship; the ship then stands between the wall it breaks in from and
                // the head's place on the path, so the whole entrance is in the frame.
                w.spawnBossNow((std::floor(music1_ / kTicksPerBar) + 1.0) * kTicksPerBar, ship, BossKind::Longwave);
                const BossState& b = w.boss();
                const Vec2 headAt = b.lw.anchor + w.species().longwave.pathAt(b.lw.p0);
                controller_.reset((b.lw.enterFrom + headAt) * 0.5f);
            }
            const BossState& b = w.boss();
            if (captureFrom_ < 0 && b.active && b.kind == BossKind::Longwave && music1_ >= b.enterTick) captureFrom_ = now;
            return true;
        }
        case M6::LwLunge:
        case M6::LwClose: {
            in = bot_.think(w, 0, controller_, clock_.heardNow());
            view_.setMute(kMuteEnemy | kMuteBullet | kMuteSparks | kMuteShard | kMuteWarp | kMuteFx | kMutePriority);
            if (ffRunning_) return true;
            // (the ship stands still a little off the body, firing nothing: the body moves only as the boss moves it)
            in = StickInput{};
            in.assist = false;
            w.closeTrioNow(0);
            w.playerMut(0).levelsBanked = 0;
            const BossState& b = w.boss();
            if (captureFrom_ >= 0 || !b.active || b.dying || b.kind != BossKind::Longwave) return true;
            // (the ship is put beside the body: once for the closing gap; for the lunge every second until the line shows, so
            // the head lunges toward a ship that sees it)
            const bool place = pic.kind == M6::LwClose ? !(scriptFlags_ & 0x40u) : (!b.lw.lungeArmed && t % 60u == 0u);
            if (place) {
                const Vec2 mid = b.alive ? b.body[1 + (pic.kind == M6::LwClose ? 3u : 0u)] : b.body[0];
                controller_.reset(mid + Vec2{0.0f, 260.0f});
                scriptFlags_ |= 0x40u;
                placedAt_ = t;
            }
            if (!(scriptFlags_ & 0x40u) || t < placedAt_ + 60u) return true;
            if (pic.kind == M6::LwLunge) {
                if (b.lw.dashTick >= 0.0 && music1_ >= b.lw.dashTick && music1_ < b.lw.dashTick + 8.0) captureFrom_ = now;
            } else if (t == placedAt_ + 60u) {
                // The segment nearest the ship (not the last) dies; the body behind closes the gap over a step.
                u32 best = 0xFFu, last = 0;
                float bd = 1e30f;
                for (u32 k = 0; k < w.species().longwave.segments; ++k) {
                    if (!(b.alive & (1u << k))) continue;
                    last = k;
                    const float dd = lengthSq(b.body[1 + k] - ship);
                    if (dd < bd) {
                        bd = dd;
                        best = k;
                    }
                }
                if (best == last && best > 0) --best;
                if (best != 0xFFu) {
                    w.killBossNextTick(1u << best, false);
                    captureFrom_ = now + 1;
                }
            }
            return true;
        }
        case M6::HzWave:
        case M6::HzTouch: {
            in = bot_.think(w, 0, controller_, clock_.heardNow());
            if (ffRunning_) return true;
            in = StickInput{};
            in.assist = false;
            w.closeTrioNow(0);
            w.playerMut(0).levelsBanked = 0;
            const BossState& b = w.boss();
            if (captureFrom_ >= 0 || !b.active || b.dying || b.kind != BossKind::EventHorizon || music1_ < b.fightTick + kTicksPerBar)
                return true;
            const Vec2 c = b.pos;
            stripStride_ = static_cast<u32>(std::lround(900.0 / std::fmax(w.tempoAt(music1_), 30.0f)));  // (a sixteenth's frames)
            if (pic.kind == M6::HzWave) {
                // The ship stands 380 wu out until a wave leaves; then it is left to the wave and the pull. The strip starts a
                // sixteenth before the ring reaches it.
                const HorizonState& h = b.hz;
                if (!(scriptFlags_ & 0x40u) || !(h.waveTick > static_cast<double>(placedAt_) && music1_ >= h.waveTick)) {
                    if (!(scriptFlags_ & 0x40u)) placedAt_ = static_cast<u32>(music1_);
                    scriptFlags_ |= 0x40u;
                    controller_.reset(c + Vec2{-380.0f, 0.0f});
                    return true;
                }
                const double reach = h.waveTick + 380.0 / prose::kHzWaveSpeed * static_cast<double>(w.ticksPerSecond());
                const double framesTo = (reach - music1_) / static_cast<double>(w.ticksPerSecond()) * 60.0;
                captureFrom_ = now + std::max<i64>(1, static_cast<i64>(std::lround(framesTo)) - static_cast<i64>(stripStride_));
                RF_LOG("play: picture '%s': the wave reaches the ship in %.1f frames", opt_.picture, framesTo);
                return true;
            }
            // The touch: put 320 wu out once, then flown straight at the Well until it is thrown; the strip starts as the ship
            // comes within 60 wu of the touch.
            if (!(scriptFlags_ & 0x40u)) {
                controller_.reset(c + Vec2{-320.0f, 0.0f});
                scriptFlags_ |= 0x40u;
                return true;
            }
            if (w.thrown(0, music1_)) scriptFlags_ |= 0x80u;
            if (!(scriptFlags_ & 0x80u)) in.move = normalizeOr(c - ship, Vec2{1.0f, 0.0f});
            if (length(ship - c) < w.species().horizon.horizonRadius + kPlayerHitRadius + 60.0f) captureFrom_ = now;
            return true;
        }
        case M6::FbRiffs: {
            in = bot_.think(w, 0, controller_, clock_.heardNow());
            if (ffRunning_) return true;
            const BossState& b = w.boss();
            if (!b.active || b.dying || b.kind != BossKind::Feedback) return true;
            // (held still 300 wu left of the Head, the assist firing: the Echo plays the shots and the Riffs on the right)
            in = StickInput{};
            in.assist = true;
            w.closeTrioNow(0);
            w.playerMut(0).levelsBanked = 0;
            if (!(scriptFlags_ & 0x40u)) {
                controller_.reset(b.pos + Vec2{-300.0f, 0.0f});
                scriptFlags_ |= 0x40u;
            }
            if (captureFrom_ < 0 && music1_ >= b.fightTick + 3.0 * kTicksPerBar) {
                stripStride_ = stripFrames_ > 8 ? 1u : 6u;
                captureFrom_ = now;
            }
            return true;
        }
        case M6::MiniStays:
        case M6::Flash: {
            in = bot_.think(w, 0, controller_, clock_.heardNow());
            // (the flash pictures: the horde, the bullets, sparks, Shards and effects not drawn, so the boss's white part reads)
            if (pic.kind == M6::Flash) view_.setMute(kMuteEnemy | kMuteBullet | kMuteSparks | kMuteShard | kMuteWarp | kMuteFx | kMutePriority);
            if (ffRunning_) return true;
            in.nova = false;
            w.closeTrioNow(0);
            w.playerMut(0).levelsBanked = 0;
            const BossState& b = w.boss();
            // The mini-boss 13 bars after its arrival (its Drop's own 8 have ended; the Drop is held for it to 16).
            if (pic.kind == M6::MiniStays && captureFrom_ < 0 && b.active && !b.dying && isMiniBoss(b.kind) &&
                music1_ >= b.enterTick + 13.0 * kTicksPerBar)
                captureFrom_ = now;
            return true;
        }
    }
    return true;
}

bool Play::scriptEventsM6() {
    if (m6Pic_ == 0xFFFF) return false;
    const M6Pic& pic = kM6Pics[m6Pic_];
    if (pic.kind != M6::Flash || captureFrom_ >= 0 || ffRunning_) return true;
    const World& w = *world_;
    const BossState& b = w.boss();
    if (!b.active || b.dying || b.kind != fightKind_) return true;
    // A shot on a weak point (its full share or more, not a plate) a bar into the fight, near the ship (the zoomed crop): the
    // part white in the next frame.
    const Vec2 ship = controller_.pose().pos;
    for (u32 k = 0; k < w.eventCount(); ++k) {
        const Event& e = w.events()[k];
        if (e.type == EventType::Impact && e.species == kBossSpecies && e.c != 1 && e.f >= 1.0f && w.music() > b.fightTick + kTicksPerBar &&
            scriptTick_ > 150 && std::fabs(e.x - ship.x) < 520.0f && std::fabs(e.y - ship.y) < 240.0f) {
            captureFrom_ = static_cast<i64>(frameNo_) + 1;
            RF_LOG("play: picture '%s' captures from frame %lld (part %u.%u)", opt_.picture, static_cast<long long>(captureFrom_), e.c, e.a);
            break;
        }
    }
    return true;
}

}  // namespace rf
