// The review pictures of M3 stage B and C (docs/PROGRESS.md; scripts/review-m3b.ps1): scripted moments of the
// screens, the Trio, the HUD through the phrase cycle, each Riff and Boost, Kaleidos and the bot's run.
// Fixed 1/60 s steps and no band, like stage A's pictures.

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "core/log.h"
#include "game/play.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
bool is(const char* a, const char* b) { return std::strcmp(a, b) == 0; }
bool starts(const char* a, const char* b) { return std::strncmp(a, b, std::strlen(b)) == 0; }
}  // namespace

bool Play::scriptSetupB() {
    const char* p = opt_.picture;
    World& w = *world_;
    const auto ring = [&](u32 n, float r, Family f, u32 tier, float hp) {
        for (u32 k = 0; k < n; ++k) {
            const float a = kTau * static_cast<float>(k) / static_cast<float>(n);
            const Vec2 at{std::cos(a) * r * 1.5f, std::sin(a) * r * 0.75f};
            const i32 i = w.spawnEnemy(SpeciesTable::index(f, tier, kVariantA), at, angleOf(Vec2{} - at), 0.0);
            if (i >= 0 && hp > 0.0f) w.setEnemyHp(static_cast<u32>(i), hp);
        }
    };
    if (starts(p, "screen_")) {
        flow_ = true;
        w.spawner().mode = Spawner::Mode::Slice;
        if (is(p, "screen_notice")) screen_ = Screen::Notice;
        else if (is(p, "screen_title")) screen_ = Screen::Title;
        else if (starts(p, "screen_settings")) {
            screen_ = Screen::Settings;
            settingsBack_ = Screen::Title;
            settingsTab_ = is(p, "screen_settings_controls") ? 1 : (is(p, "screen_settings_look") ? 2 : 0);
        } else {
            screen_ = Screen::Run;  // pause and game over start from a run
        }
        return true;
    }
    if (starts(p, "hud_") || starts(p, "bot_")) {
        w.spawner().mode = Spawner::Mode::Slice;
        if (is(p, "hud_groove")) scriptFastForwardS_ = 8.0f;
        if (is(p, "hud_build")) scriptFastForwardS_ = 19.0f;
        // M3 stage C: the Drop with the real crowd. Play test 2: Track 1's third Drop is the boss's now, so
        // the second cycle's (bar 36, 60 near the ship x 1.6), at 72.71 s (each Build's last bar runs at 85%).
        if (is(p, "hud_drop")) scriptFastForwardS_ = 72.75f;
        if (is(p, "hud_break")) scriptFastForwardS_ = 40.25f;
        // bot_<minutes>: the careful bot's real run at that minute (hits count, so it may have died and
        // started again); bot_<minutes>_nohits: one run with hits costing nothing, so the Tracks come on time
        // (minute 15 is Track 3, Reef). M3 stage C2.
        if (starts(p, "bot_")) {
            scriptFastForwardS_ = 60.0f * static_cast<float>(std::atoi(p + 4));
            if (std::strstr(p, "_nohits")) w.rules().godMode = true;
        }
        if (is(p, "hud_drop")) w.rules().godMode = true;
        return true;
    }
    if (starts(p, "trio")) return true;
    if (starts(p, "pt2_")) {
        // Play test 2's pictures (captures\playtest2): a level-up Trio in a fight with the ship sheltered in an orb, a Cell
        // in a crowd, the halo at eight cells with one growing back, Overcharge 0-5 (one picture each), the HUD's level.
        if (is(p, "pt2_level_trio") || is(p, "pt2_cell_crowd")) {
            w.spawner().mode = Spawner::Mode::Slice;
            scriptFastForwardS_ = 26.0f;  // Track 1's first Drop (bar 12, 24.4 s): its Surge with the crowd
            w.rules().godMode = true;
            w.playerMut(0).novas = 0;  // (no Nova's inhale across the picture)
        } else if (is(p, "pt2_hud_level")) {
            w.spawner().mode = Spawner::Mode::Slice;
            scriptFastForwardS_ = 50.0f;
            w.rules().godMode = true;
        } else if (starts(p, "pt2_overcharge_")) {
            const u32 n = static_cast<u32>(std::atoi(p + 15));
            for (u32 k = 0; k < n && k < 5; ++k) w.grantUpgrade(0, kOvercharge);
            for (u32 k = 0; k < 5; ++k) {
                const Vec2 at{900.0f + 60.0f * static_cast<float>(k), -240.0f + 120.0f * static_cast<float>(k)};
                const i32 i = w.spawnEnemy(SpeciesTable::index(Family::Drifter, 2, kVariantA), at, kPi, 0.0);
                if (i >= 0) w.setEnemyHp(static_cast<u32>(i), 4000.0f);
            }
            w.rules().godMode = true;
        } else if (is(p, "pt2_halo8")) {
            for (u32 k = 0; k < 4; ++k) w.grantUpgrade(0, kExtraCell);
            w.grantUpgrade(0, kRegen);
            PlayerState& pl = w.playerMut(0);
            pl.cells = 7;
            pl.regenBars = 9;
        }
        return true;
    }
    if (is(p, "boost_prism") || is(p, "split7")) {
        // M3 stage C: the fan in open air, toward a few targets far to the right (play test 1: Split's seven lanes too).
        if (is(p, "split7")) for (u32 k = 0; k < 3; ++k) w.grantUpgrade(0, kSplit);
        for (u32 k = 0; k < 5; ++k) {
            const Vec2 at{900.0f + 60.0f * static_cast<float>(k), -240.0f + 120.0f * static_cast<float>(k)};
            const i32 i = w.spawnEnemy(SpeciesTable::index(Family::Drifter, 2, kVariantA), at, kPi, 0.0);
            if (i >= 0) w.setEnemyHp(static_cast<u32>(i), 4000.0f);
        }
        w.rules().godMode = true;
        return true;
    }
    if (starts(p, "riff_") || starts(p, "boost_")) {
        ring(14, 330.0f, Family::Drifter, 2, 400.0f);
        if (!is(p, "riff_sentinel")) ring(8, 210.0f, Family::Hound, 1, 400.0f);  // (the drone's shots need room to show)
        w.rules().godMode = true;
        Loadout& l = w.playerMut(0).loadout;
        (void)l;
        if (is(p, "riff_orbitals")) for (u32 k = 0; k < 3; ++k) w.grantUpgrade(0, kOrbitals);
        if (is(p, "riff_pulse")) for (u32 k = 0; k < 3; ++k) w.grantUpgrade(0, kPulseRing);
        if (is(p, "riff_sentinel")) for (u32 k = 0; k < 3; ++k) w.grantUpgrade(0, kSentinel);
        if (is(p, "boost_magnet_storm"))
            for (u32 k = 0; k < 120; ++k) {
                const float a = kTau * static_cast<float>(k) / 120.0f;
                w.spawnShard(Vec2{std::cos(a) * (250.0f + 3.0f * k), std::sin(a) * (150.0f + 1.5f * k)}, Vec2{}, 0);
            }
        return true;
    }
    if (is(p, "edge")) {
        w.spawnEnemy(SpeciesTable::index(Family::Sentry, 1, kVariantB), Vec2{860.0f, -60.0f}, 0.0f, 0.0);  // engaged, just off screen
        w.dropBoost(Vec2{-1500.0f, 260.0f}, kPrism);
        w.rules().godMode = true;
        return true;
    }
    if (starts(p, "boss_")) {
        w.rules().godMode = true;
        return true;
    }
    if (is(p, "track_change")) return true;
    if (is(p, "loop_encore") || is(p, "loop_burn")) {
        // GAME_DESIGN 8 (the lead's decisions after play test 2): the loop's first Track card (ENCORE, NO MORE REFILLS, HALO 6)
        // and the halo burning down on its first downbeat (a full eight-cell halo loses two arcs).
        w.directorMut().track = 3;
        for (u32 k = 0; k < 4; ++k) w.grantUpgrade(0, kExtraCell);
        w.rules().godMode = true;
        return true;
    }
    return false;
}

bool Play::scriptInputB(StickInput& in) {
    const char* p = opt_.picture;
    World& w = *world_;
    const u32 t = scriptTick_;
    const double music = music1_;
    const i64 now = static_cast<i64>(frameNo_);
    const Vec2 ship = controller_.pose().pos;
    if (starts(p, "screen_")) {
        // The attract run (bot); the scripted screens.
        in = bot_.think(w, 0, controller_, clock_.heardNow());
        if (is(p, "screen_pause")) {
            if (t == 240) pauseRun();
            if (t == 300) beginCountIn();
            if (t >= 240) in = StickInput{};
            if (t == 300 + 96 && captureFrom_ < 0) captureFrom_ = now;  // two beats into the count-in
        } else if (is(p, "screen_game_over")) {
            if (t == 300) {
                PlayerState& pl = w.playerMut(0);
                pl.cells = 0;
                pl.novas = 0;  // (play test 1, second pass: Auto Nova would fire one instead)
                pl.invulS = pl.novaInvulS = 0.0f;
                w.rules().godMode = false;
                w.spawnBullet(pl.pos, Vec2{}, 0.0f);
            }
            if (screen_ == Screen::GameOver && captureFrom_ < 0 && overS_ > 2.6f) captureFrom_ = now;
        } else if (captureFrom_ < 0 && t == (is(p, "screen_title") ? 300u : 150u)) {
            captureFrom_ = now;
        }
        return true;
    }
    if (starts(p, "hud_") || starts(p, "bot_")) {
        in = bot_.think(w, 0, controller_, clock_.heardNow());
        if (captureFrom_ < 0 && t == 24) captureFrom_ = now;
        return true;
    }
    if (starts(p, "trio")) {
        const u8 forced[3] = {kSplit, kMagnet, kOrbitals};
        // Play test 1: the Trio opens after the run's Track card has gone (2.25 bars), so the card does not cover it.
        constexpr u32 kOpen = 300;
        if (t == kOpen) w.openTrioNow(0, music + 4.0 * kTicksPerBar, forced);
        const TrioState& tr = w.trio(0);
        if (is(p, "trio")) {
            if (t == kOpen + 80 && captureFrom_ < 0) captureFrom_ = now;  // lit (the third beat), the ship off the orbs
        } else if (is(p, "trio_locked")) {
            if (t == kOpen + 52 && captureFrom_ < 0) captureFrom_ = now;  // play test 1: under two beats in, the orbs still outlines
        } else if (tr.open) {
            const Vec2 orb = tr.orbPos[2];
            const Vec2 d = orb - ship;
            const float dist = length(d);
            const float stop = is(p, "trio_answer") ? 92.0f : 0.0f;
            if (t > kOpen + 20 && dist > stop + 6.0f) in.move = d * (0.55f / std::fmax(dist, 1.0f));
            // (Play test 1: an orb answers once the Trio is lit, after its two locked beats.)
            if (is(p, "trio_answer") && t > kOpen + 20 && dist <= stop + 8.0f && music > tr.openTick + (tuning::kTrioLockBeats + 0.25) * kTicksPerBeat &&
                captureFrom_ < 0)
                captureFrom_ = now + 6;
            // Play test 1: the hold half filled.
            if (is(p, "trio_hold") && tr.holdOrb >= 0 && tr.holdTicks >= 0.5 * tuning::kTrioHoldBeats * kTicksPerBeat && captureFrom_ < 0)
                captureFrom_ = now;
        }
        return true;
    }
    if (starts(p, "pt2_")) {
        // Play test 2. The fights: the bot plays through the fast-forward and a moment after it, then the script.
        const Enemies& e = w.enemies();
        const auto aimNearest = [&] {
            i32 best = -1;
            float bestD = 900.0f * 900.0f;
            for (u32 i = 0; i < e.count; ++i) {
                if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping) continue;
                const float dd = lengthSq(Vec2{e.x[i], e.y[i]} - ship);
                if (dd < bestD) {
                    bestD = dd;
                    best = static_cast<i32>(i);
                }
            }
            if (best >= 0) {
                in.aim = Vec2{e.x[best], e.y[best]} - ship;
                in.aimActive = true;
            }
        };
        if (is(p, "pt2_hud_level")) {
            in = bot_.think(w, 0, controller_, clock_.heardNow());
            if (!ffRunning_ && captureFrom_ < 0 && t == 24) captureFrom_ = now;
            return true;
        }
        if (is(p, "pt2_level_trio") || is(p, "pt2_cell_crowd")) {
            if (ffRunning_ || t < 20) {
                in = bot_.think(w, 0, controller_, clock_.heardNow());
                return true;
            }
            in = StickInput{};
            aimNearest();
            if (is(p, "pt2_level_trio")) {
                if (t == 20) {
                    PlayerState& pl = w.playerMut(0);
                    pl.level = 7;
                    pl.levelsBanked = 0;
                    pl.levelShards = levelShards(8) / 3;
                    w.openTrioNow(0, music + 4.0 * kTicksPerBar, nullptr, kTrioLevel);
                }
                const TrioState& tr = w.trio(0);
                if (tr.open) {
                    const Vec2 d = tr.orbPos[0] - ship;
                    const float dist = length(d);
                    if (dist > 3.0f) in.move = d * (std::fmin(dist / 60.0f, 1.0f) / std::fmax(dist, 1.0f));
                }
                if (captureFrom_ < 0 && tr.open && tr.holdOrb == 0 && tr.holdTicks >= 0.35 * tuning::kTrioHoldBeats * kTicksPerBeat) captureFrom_ = now;
            } else {
                if (t == 20) {
                    w.playerMut(0).cells = 1;  // on the last cell: its Cells off screen get the chevron with the plus
                    w.dropCellNow(ship + Vec2{230.0f, -110.0f}, 0);
                    w.dropCellNow(ship + Vec2{-1500.0f, 320.0f}, 0);
                }
                if (captureFrom_ < 0 && t == 56) captureFrom_ = now;
            }
            return true;
        }
        if (starts(p, "pt2_overcharge_")) {
            if (t >= 40 && e.count) {
                in.aim = Vec2{e.x[0], e.y[0]} - ship;
                in.aimActive = true;
            }
            if (captureFrom_ < 0 && t >= 290 && (w.shots().count >= 14 || t >= 360)) captureFrom_ = now;
            return true;
        }
        if (is(p, "pt2_halo8") && captureFrom_ < 0 && t == 300) captureFrom_ = now;  // after the run's Track card
        return true;
    }
    if (starts(p, "riff_") || starts(p, "boost_") || is(p, "split7")) {
        const Enemies& e = w.enemies();
        if (t == 30 && is(p, "boost_prism")) w.grantBoost(0, kPrism);
        if (t == 30 && is(p, "boost_hyper")) w.grantBoost(0, kHyper);
        if (t == 30 && is(p, "boost_halo")) w.grantBoost(0, kHalo);
        if (t == 60 && is(p, "boost_magnet_storm")) w.grantBoost(0, kMagnetStorm);
        const bool fires = is(p, "boost_prism") || is(p, "boost_hyper") || is(p, "split7");
        if (fires && t >= 40 && e.count) {
            in.aim = Vec2{e.x[0], e.y[0]} - ship;
            in.aimActive = true;
        }
        if (is(p, "boost_prism") || is(p, "split7")) {
            // M3 stage C: while the fan fires, after the run's Track card has gone (2.25 bars): once the shots in
            // the air make a fan, or at the latest at 6 s (play test 1, Split: seven lanes in the air).
            const u32 enough = is(p, "split7") ? 30u : 14u;
            if (captureFrom_ < 0 && t >= 290 && (w.shots().count >= enough || t >= 360)) captureFrom_ = now;
            return true;
        }
        // (Play test 1: Orbitals after the run's Track card, so the card does not cover the shards.)
        const u32 at = is(p, "boost_magnet_storm") ? 72u : (is(p, "riff_pulse") ? 96u : (is(p, "riff_orbitals") ? 330u : 110u));
        if (is(p, "riff_sentinel")) {
            // Just after an off-beat, while the burst is in the air.
            const double inBeat = std::fmod(music, kTicksPerBeat);
            if (t >= at && inBeat > 48.0 && inBeat < 51.5 && captureFrom_ < 0) captureFrom_ = now;
        } else if (t == at && captureFrom_ < 0) {
            captureFrom_ = now;
        }
        return true;
    }
    if (is(p, "edge")) {
        // M3 stage C: Kaleidos off screen too (beyond the camera's 1,800 wu lean), its double chevron caught
        // just after a beat, while it swells.
        if (t == 2) w.spawnBossNow((std::floor(music / kTicksPerBar) + 1.0) * kTicksPerBar, Vec2{-1900.0f, -700.0f});
        if (captureFrom_ < 0 && w.boss().active && t > 290) {  // after the run's Track card
            const Enemies& e = w.enemies();
            bool engaged = false;
            for (u32 i = 0; i < e.count; ++i) engaged = engaged || e.state[i] == kGlint || e.state[i] == kFiring;
            if (engaged && std::fmod(music, kTicksPerBeat) < 10.0) captureFrom_ = now + 1;
        }
        return true;
    }
    if (starts(p, "boss_")) {
        // The boss on the right, the ship on the left; pairs killed as the picture needs.
        if (t == 2) {
            const double line = (std::floor(music / kTicksPerBar) + 1.0) * kTicksPerBar;
            w.spawnBossNow(line, Vec2{230.0f, -10.0f});
        }
        const BossState& b = w.boss();
        in.move = (Vec2{-250.0f, 40.0f} - ship) * 0.004f;
        if (!b.active) return true;
        const double fight = b.fightTick;
        const auto killPair = [&](u32 pair) {
            if (!(b.alive & (1u << pair))) return;
            const Vec2 dv = dirOf(w.bossArmAngle(pair));
            w.shootBoss(b.pos + dv * 400.0f, b.pos + dv * 180.0f, 1e6f);
        };
        u32 pairsToKill = 0;
        if (is(p, "boss_refraction") || is(p, "boss_beam_tell") || is(p, "boss_beam") || is(p, "boss_iris")) pairsToKill = 1;
        if (is(p, "boss_spiral")) pairsToKill = 2;
        if (is(p, "boss_naked") || is(p, "boss_death")) pairsToKill = 3;
        if (music > fight + 8.0) {
            for (u32 k = 0; k < pairsToKill; ++k) killPair(k);
            if (is(p, "boss_pair_death") && music > fight + 1.5 * kTicksPerBar && (b.alive & 2u) && captureFrom_ < 0) {
                w.killBossNextTick(2u, false);  // inside the tick, so the view sees it as in play
                captureFrom_ = now + 1;
            }
        }
        if (is(p, "boss_death") && b.alive == 0 && music > fight + 2.2 * kTicksPerBar && !b.dying && captureFrom_ < 0) {
            w.killBossNextTick(0, true);
            captureFrom_ = now + 1;
        }
        if (captureFrom_ >= 0) return true;
        const double barIn = std::fmod(music, kTicksPerBar);
        if ((is(p, "boss_bloom") || is(p, "boss_refraction") || is(p, "boss_spiral") || is(p, "boss_naked")) &&
            music > fight + 1.6 * kTicksPerBar && barIn > 150.0 && barIn < 200.0)
            captureFrom_ = now;
        if (is(p, "boss_beam_tell") && music > fight + 1.0 * kTicksPerBar) {
            for (u32 pr = 0; pr < 3; ++pr)
                if (w.bossBeamState(pr, music) == 1 && barIn > 200.0) captureFrom_ = now;
        }
        if (is(p, "boss_beam") && music > fight + 1.0 * kTicksPerBar) {
            for (u32 pr = 0; pr < 3; ++pr)
                if (w.bossBeamState(pr, music) == 2 && barIn > 30.0) captureFrom_ = now;
        }
        if (is(p, "boss_iris") && music > fight + 1.0 * kTicksPerBar && barIn > 20.0 && barIn < 60.0) captureFrom_ = now;
        return true;
    }
    if (is(p, "track_change") || is(p, "loop_encore") || is(p, "loop_burn")) {
        if (t == 10) {
            // The boss has just fallen: the Track-change Break begins on the next bar line.
            DirectorState& d = w.directorMut();
            d.changeBar = w.barOf(music) + 1;
        }
        // M3 stage C: the new Track's name card, written and holding, half a bar into the new Track.
        const DirectorState& dir = w.director();
        if (captureFrom_ < 0 && !is(p, "loop_burn") && w.phrase().track >= (is(p, "loop_encore") ? 4u : 2u) && !w.phrase().trackChange && dir.trackBar != kNoBar &&
            music >= w.tickOfBar(dir.trackBar) + 0.5 * kTicksPerBar)
            captureFrom_ = now;
        return true;
    }
    return false;
}

bool Play::scriptEventsB() {
    const char* p = opt_.picture;
    const World& w = *world_;
    const i64 now = static_cast<i64>(frameNo_);
    if (captureFrom_ >= 0) return starts(p, "screen_") || starts(p, "hud_") || starts(p, "bot_") || starts(p, "trio") ||
                                   starts(p, "riff_") || starts(p, "boost_") || starts(p, "boss_") || is(p, "edge") || is(p, "track_change") ||
                                   is(p, "split7") || starts(p, "pt2_") || is(p, "loop_encore") || is(p, "loop_burn");
    for (u32 k = 0; k < w.eventCount(); ++k) {
        const Event& e = w.events()[k];
        if (is(p, "trio_take") && e.type == EventType::TrioTake) captureFrom_ = now;
        if (is(p, "boss_pair_death") && e.type == EventType::BossPair && e.a == 1) captureFrom_ = now;
        if (is(p, "boss_death") && e.type == EventType::BossDeath) captureFrom_ = now;
        if (is(p, "loop_burn") && e.type == EventType::HaloBurn) captureFrom_ = now + 3;  // the arcs a few frames into their flight
        if (captureFrom_ >= 0) {
            RF_LOG("play: picture '%s' captures from frame %lld", p, static_cast<long long>(captureFrom_));
            break;
        }
    }
    return starts(p, "screen_") || starts(p, "hud_") || starts(p, "bot_") || starts(p, "trio") || starts(p, "riff_") ||
           starts(p, "boost_") || starts(p, "boss_") || is(p, "edge") || is(p, "track_change") || is(p, "split7") || starts(p, "pt2_") ||
           is(p, "loop_encore") || is(p, "loop_burn");
}

}  // namespace rf
