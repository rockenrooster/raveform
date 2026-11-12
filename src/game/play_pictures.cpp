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
        // M3 stage C: the Drop with the real crowd: Track 1's third cycle (bar 60, 80 near the ship x 1.6), at
        // 121.06 s (each Build's last bar runs at 85%).
        if (is(p, "hud_drop")) scriptFastForwardS_ = 121.1f;
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
    if (is(p, "boost_prism")) {
        // M3 stage C: the fan in open air, toward a few targets far to the right.
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
        if (t == 20) w.openTrioNow(0, music + 4.0 * kTicksPerBar, forced);
        const TrioState& tr = w.trio(0);
        if (is(p, "trio")) {
            if (t == 80 && captureFrom_ < 0) captureFrom_ = now;
        } else if (tr.open) {
            const Vec2 orb = tr.orbPos[2];
            const Vec2 d = orb - ship;
            const float dist = length(d);
            const float stop = is(p, "trio_answer") ? 92.0f : 0.0f;
            if (t > 40 && dist > stop + 6.0f) in.move = d * (0.55f / std::fmax(dist, 1.0f));
            if (is(p, "trio_answer") && t > 40 && dist <= stop + 8.0f && captureFrom_ < 0) captureFrom_ = now + 6;
        }
        return true;
    }
    if (starts(p, "riff_") || starts(p, "boost_")) {
        const Enemies& e = w.enemies();
        if (t == 30 && is(p, "boost_prism")) w.grantBoost(0, kPrism);
        if (t == 30 && is(p, "boost_hyper")) w.grantBoost(0, kHyper);
        if (t == 30 && is(p, "boost_halo")) w.grantBoost(0, kHalo);
        if (t == 60 && is(p, "boost_magnet_storm")) w.grantBoost(0, kMagnetStorm);
        const bool fires = is(p, "boost_prism") || is(p, "boost_hyper");
        if (fires && t >= 40 && e.count) {
            in.aim = Vec2{e.x[0], e.y[0]} - ship;
            in.aimActive = true;
        }
        if (is(p, "boost_prism")) {
            // M3 stage C: while the fan fires, after the run's Track card has gone (2.25 bars): once the shots in
            // the air make a fan, or at the latest at 6 s.
            if (captureFrom_ < 0 && t >= 290 && (w.shots().count >= 14 || t >= 360)) captureFrom_ = now;
            return true;
        }
        const u32 at = is(p, "boost_magnet_storm") ? 72u : (is(p, "riff_pulse") ? 96u : 110u);
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
    if (is(p, "track_change")) {
        if (t == 10) {
            // The boss has just fallen: the Track-change Break begins on the next bar line.
            DirectorState& d = w.directorMut();
            d.changeBar = w.barOf(music) + 1;
        }
        // M3 stage C: the new Track's name card, written and holding, half a bar into the new Track.
        const DirectorState& dir = w.director();
        if (captureFrom_ < 0 && w.phrase().track >= 2 && !w.phrase().trackChange && dir.trackBar != kNoBar &&
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
                                   starts(p, "riff_") || starts(p, "boost_") || starts(p, "boss_") || is(p, "edge") || is(p, "track_change");
    for (u32 k = 0; k < w.eventCount(); ++k) {
        const Event& e = w.events()[k];
        if (is(p, "trio_take") && e.type == EventType::TrioTake) captureFrom_ = now;
        if (is(p, "boss_pair_death") && e.type == EventType::BossPair && e.a == 1) captureFrom_ = now;
        if (is(p, "boss_death") && e.type == EventType::BossDeath) captureFrom_ = now;
        if (captureFrom_ >= 0) {
            RF_LOG("play: picture '%s' captures from frame %lld", p, static_cast<long long>(captureFrom_));
            break;
        }
    }
    return starts(p, "screen_") || starts(p, "hud_") || starts(p, "bot_") || starts(p, "trio") || starts(p, "riff_") ||
           starts(p, "boost_") || starts(p, "boss_") || is(p, "edge") || is(p, "track_change");
}

}  // namespace rf
