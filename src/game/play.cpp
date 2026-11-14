#include "game/play.h"
#include "game/pending_sounds.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

#include "audio/music_data.h"
#include "audio/platform_audio.h"
#include "core/log.h"
#include "core/time.h"
#include "net/bots.h"
#include "net/lobby.h"
#include "sim/boss_prose.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
constexpr float kPadDeadZone = 0.20f;     // gamepad sticks (chosen; touch uses GAME_DESIGN 2's 8%)
constexpr float kPadAimActive = 0.30f;    // the right stick fires beyond this deflection
constexpr u32 kShardRunMax = 8;           // Shard notes waiting for their 1/32 step, at most
constexpr double kShardStepTicks = 12.0;  // one Shard note per 1/32 (the engine quantizes to it)
constexpr float kIntensityStep = 0.02f;   // the band hears a new intensity when it moved this much
// The band is told about the next bar this far into the heard bar: after the sim's decisions for that bar
// line (a boss death or a retry in the first half of a bar takes the next line), before its write head
// reaches the line (latency is far below half a bar).
constexpr double kPlanAtBar = 0.55;

bool is(const char* a, const char* b) { return std::strcmp(a, b) == 0; }
Vec2 deadZone(Vec2 v, float dz) {
    const float l = length(v);
    if (l <= dz) return Vec2{};
    const float m = clampf((l - dz) / (1.0f - dz), 0.0f, 1.0f);
    return v * (m / l);
}
}  // namespace

Play::~Play() {
    delete bots_;     // M4: the bot clients' worker first (their worlds read the species table)
    delete session_;
    delete world_;
    delete species_;
}

bool Play::init(const PlayOptions& opt, const Look& look, const Palettes& palettes, const Palette& palette, const GlyphSet& glyphs,
                const VectorFont& font, const TierSettings& tier, Band* band) {
    opt_ = opt;
    look_ = &look;
    palettes_ = &palettes;
    palette_ = &palette;
    basePalette_ = &palette;
    glyphs_ = &glyphs;
    font_ = &font;
    band_ = band;
    species_ = new (std::nothrow) SpeciesTable;
    world_ = new (std::nothrow) World;
    if (!species_ || !world_ || !species_->load()) return false;
    world_->init(species_);
    bandDrives_ = band && band->active() && band->gameMode() && !opt.fixedStep;

    // The Track's tempo (stage A plays Track 1): from the band, or from the music data when it is off.
    float bpm = 120.0f;
    u32 songs = sim::kSongs;
    // M5 (GAME_DESIGN 6.2, the lead's ruling after part 3): Kick Pulse rings on the Track's kicks (velocity 5 of 9 or more).
    const auto kicks = [&](const audio::MusicData& m) {
        for (u32 t = 0; t < m.trackCount && t < sim::kMaxSongs; ++t) {
            u16 mask = 0;
            for (u32 st = 0; st < 16; ++st)
                if (m.tracks[t].pattern[static_cast<u32>(audio::Pat::Kick)][st] >= 5.0f / 9.0f - 1e-3f) mask = static_cast<u16>(mask | (1u << st));
            world_->setKickMask(t, mask ? mask : 0x1111u);
        }
    };
    if (band && band->active()) {
        bpm = band->music().tracks[band->track()].bpm;
        songs = band->music().trackCount;
        kicks(band->music());
    } else if (auto* md = new (std::nothrow) audio::MusicData) {
        if (md->load() && md->trackCount) {
            bpm = md->tracks[0].bpm;
            songs = md->trackCount;
            kicks(*md);
        }
        delete md;
    }
    // M5 (GAME_DESIGN 4.2): the set list is every Track the music has that the game has content for (its own boss); the
    // loop begins after the last of them.
    world_->setSetList(songs < sim::kBossesBuilt ? songs : sim::kBossesBuilt);
    RF_LOG("play: the set list has %u Tracks (the music %u, the fights built %u)", world_->setList(), songs, sim::kBossesBuilt);
    clock_.reset(bpm, 0.0);

    const bool picture = opt.picture[0] != 0;
    Spawner::Mode mode = Spawner::Mode::Slice;
    if (opt.kind == PlayOptions::Kind::Stress) mode = Spawner::Mode::Stress;
    if (picture) mode = Spawner::Mode::Off;
    world_->spawner().mode = mode;
    world_->rules().godMode = opt.kind == PlayOptions::Kind::Stress || opt.noHits;
    world_->rules().novas = opt.kind != PlayOptions::Kind::Stress;
    world_->resetRun(opt.seed, 0.0, 1);
    controller_.reset(world_->player(0).pos);
    bot_.reset(opt.seed, opt.kind == PlayOptions::Kind::Stress ? Bot::Policy::Stress : Bot::Policy::Play);
    if (!view_.init(look, palettes, palette, glyphs, tier)) return false;
    // M3 stage B: the upgrade icons (design/icons.json) and the font for the Trio's words.
    if (!icons_.load()) return false;
    for (u8 u = 0; u < kUpgradeCount; ++u) upgradeIcons_[u] = icons_.find(upgradeInfo(u).icon);
    for (u8 b = 0; b < kBoostCount; ++b) boostIcons_[b] = icons_.find(boostInfo(b).icon);
    view_.setIcons(&icons_, upgradeIcons_, boostIcons_);
    view_.setFont(&font);
    // The screens around the run: interactive play only (benchmarks, pictures and --bot are the run alone; M4
    // stage A: co-op from the command line has no screens either). M4 stage B: --coop drives the screens (with --bot,
    // the bot plays each run).
    flow_ = opt.kind == PlayOptions::Kind::Play && (!opt.bot || opt.autoplay || opt.coopAuto) && !picture && !opt.fixedStep && !opt.net;
    if (flow_) {
        loadSettings(settings_);
        loadHighScores(scores_);
        screen_ = settings_.noticeSeen ? Screen::Title : Screen::Notice;
        applySettings();
    }
    markCount_ = 0;
    marks_[markCount_++] = Mark{0.0f, 0, 1};
    if (picture) scriptSetup();
    // M5 --fight (benchmarks; the m5_ pictures set it up themselves): the run plays on to the fight.
    if (opt.fight[0] && !picture && !setupFight(opt.fight, opt.fightPhase)) return false;
    if (opt.jumpTrack && !picture) {
        // M5 --drop <track>.<cycle> (benchmarks): that Track from its start, fast-forwarded to the cycle's Drop.
        world_->directorMut().track = static_cast<u8>(opt.jumpTrack <= world_->setList() ? opt.jumpTrack : world_->setList());
        world_->spawner().mode = Spawner::Mode::Slice;
        dropJump_ = true;
        scriptFastForwardS_ = std::fmax(scriptFastForwardS_, 600.0f);
        if (opt.holdDrop) world_->directorMut().holdDropCycle = opt.jumpCycle;  // M7: the thermal check's Drop never ends
    }
    if (bandDrives_) band->setIntensity(tuning::kIntensityFloor);

    // M4: co-op (the session takes the world over; the host starts the run once every seat is in).
    if (opt.net && !picture && opt.kind != PlayOptions::Kind::Stress && !netInit()) return false;
    // --fast-forward: the game runs on the simulated clock without drawing (pictures at 2 and 5 minutes).
    const float fastForward = coop() ? 0.0f : std::fmax(opt.fastForwardS, scriptFastForwardS_);
    if (fastForward > 0.0f) {
        const u32 ticks = static_cast<u32>(fastForward * 60.0f);
        const bool drives = bandDrives_;
        bandDrives_ = false;
        const PlayOptions saved = opt_;
        opt_.fixedStep = true;
        ffRunning_ = true;
        for (u32 k = 0; k < ticks; ++k) {
            frame(kDt);
            if (fightJump_ && fightReady()) break;  // M5 --fight: the fight is on (in its phase)
            if (dropJump_ && world_->phrase().cycle == opt.jumpCycle && world_->phrase().section == sim::Section::Drop && world_->phrase().sectionBar >= 1) break;
        }
        ffRunning_ = false;
        opt_ = saved;
        bandDrives_ = drives;
        view_.resetEffects();
        frameNo_ = 0;
        if (bandDrives_) {
            // The band's song starts now: the run goes on from where the fast-forward left it, against the
            // band's timeline from a bar line on (as after a pause, without the count-in rings).
            pauseMusic_ = music1_;
            realign_ = true;
            countIn_ = true;
            countInEnd_ = 1e18;
        }
        // Pictures time their moment from the first drawn frame.
        scriptTick_ = 0;
        captureFrom_ = -1;
        captureNow_ = pictureDone_ = false;
        RF_LOG("play: fast-forwarded %.1f s (%u ticks): %u enemies, %u kills, score %llu", fastForward, ticks,
               world_->enemies().count, world_->counters().kills, static_cast<unsigned long long>(world_->player(0).score));
    }
    RF_LOG("play: %s%s, seed %llu, %.0f BPM, %s clock, spawner %s", opt.kind == PlayOptions::Kind::Stress ? "bench_stress" : (opt.kind == PlayOptions::Kind::BenchPlay ? "bench_play" : "play"),
           opt.bot ? " (bot)" : "", static_cast<unsigned long long>(opt.seed), bpm, bandDrives_ ? "the band's heard" : "the simulated",
           mode == Spawner::Mode::Stress ? "stress" : (mode == Spawner::Mode::Off ? "off (picture script)" : "first slice"));
    return true;
}

void Play::setLayout(float w, float h, Vec2 insetMin, Vec2 insetMax) {
    layoutW_ = w;
    layoutH_ = h;
    insetMin_ = insetMin;
    insetMax_ = insetMax;
    touch_.layout(w, h, insetMin, insetMax, opt_.pxPerMm);
}

// ---------------------------------------------------------------------------------------------- frame

void Play::buildInput(StickInput& in) {
    in = StickInput{};
    if (opt_.picture[0]) {
        scriptInput(in);
        return;
    }
    // M6 (screens.md "Transitions"): the ship of a run being left (the title's in the drop-in's wait, a quit run on home's way)
    // is gone: it neither moves nor fires.
    if (flow_ && ((trans_ == Trans::Wait && transFrom_ == Screen::Title) || (trans_ == Trans::Home && screen_ != Screen::Title))) return;
    if (opt_.bot || opt_.autoplay || attract()) {
        // The bot plays (--bot, benchmarks, and the title's attract run).
        in = bot_.think(*world_, seat_, controller_, coop() ? netHeard_ : clock_.heardNow());
        return;
    }
    in.assist = settings_.autoAim || !flow_;
    // Keyboard: WASD or the arrows.
    Vec2 mv{static_cast<float>((keys_[3] || keys_[7]) - (keys_[1] || keys_[5])),
            static_cast<float>((keys_[2] || keys_[6]) - (keys_[0] || keys_[4]))};
    if (lengthSq(mv) > 0.0f) in.move = normalizeOr(mv, Vec2{});
    // Mouse: aim at the pointer, fire while the left button is held.
    if (mouseFire_ && mouseSeen_) {
        in.aim = view_.toWorld(mousePos_) - controller_.pose().pos;
        in.aimActive = lengthSq(in.aim) > 1.0f;
    }
    // Gamepad.
    const Vec2 pm = deadZone(padMove_, kPadDeadZone);
    if (lengthSq(pm) > lengthSq(in.move)) in.move = pm;
    if (length(padAim_) > kPadAimActive) {
        in.aim = padAim_;
        in.aimActive = true;
    }
    touch_.apply(in);
    in.nova = in.nova || keyNova_ || mouseNova_ || padNova_;
    in.dash = in.dash || keyDash_ || padDash_;
}

void Play::frame(float realDt) {
    ++frameNo_;
    transitionFrame();  // M6 (screens.md "Transitions")
    if (coop()) {
        netFrame(realDt);  // M4: the run on the host's song (game/play_net.cpp)
        return;
    }
    const bool fixed = opt_.fixedStep;
    frameDt_ = fixed ? kDt : (realDt > 0.1f ? 0.1f : realDt);
    // M4 stage B: --coop (host or join from the command line) and --coop-shot's moments before a session.
    if (flow_ && opt_.coopAuto && !coopAutoDone_) {
        coopAutoStep();
        if (coop()) return;  // a session began: the next frame runs it
    }
    if (flow_ && opt_.coopShot[0]) coopScreenShot();
    if (flow_ && (screen_ == Screen::CoopJoin || screen_ == Screen::NetPermission)) {
        // M4 stage B (screens.md "Co-op (M4)"): nothing of a run behind the co-op setup screens. The world was emptied
        // as they opened and stands still; the band plays on.
        stats_.ticksLastFrame = 0;
        alpha_ = 1.0f;
        bandFrame();
        return;
    }
    const BeatView* bv = (bandDrives_ && band_) ? &band_->view() : nullptr;
    // M3 stage B: pause freezes the run; the count-in ends on the resume point (game/screens.cpp).
    if (countIn_) {
        if (realign_ && bv && bv->running) {
            realign_ = false;
            beginCountIn();
        }
        countInClockS_ += static_cast<double>(frameDt_) * clock_.ticksPerSecond();
        if (!realign_ && countInNow() >= countInEnd_) {
            clock_.resume(pauseMusic_, resumeOffset_);
            countIn_ = false;
            if (screen_ == Screen::Paused) {
                screen_ = Screen::Run;
                if (bandDrives_) band_->setHold(false);
                plannedLine_ = -1e9;
                applySettings();
            }
        }
    }
    const bool frozen = countIn_ || (flow_ && (screen_ == Screen::Paused || (screen_ == Screen::Settings && settingsBack_ == Screen::Paused)));
    paused_ = frozen;
    if (frozen) {
        stats_.ticksLastFrame = 0;
        bandFrame();
        if (opt_.picture[0]) {
            // Picture scripts keep their own clock through a pause (they script the count-in).
            StickInput unused;
            scriptInput(unused);
            pictureFrame();
        }
        return;
    }
    if (!clock_.fromBand()) clock_.setBpm(world_->tempoAt(music1_));
    clock_.beginFrame(bv, realDt, fixed);

    StickInput in;
    buildInput(in);
    controller_.update(in, frameDt_, clock_.heardNow(), clock_.bpm(), *world_, 0);
    if (bandDrives_) band_->setFireMode(controller_.pose().fire);

    stats_.ticksLastFrame = 0;
    // A hit-stop (a boss's death, look.md §5) holds the ticks and the view's clock while the song runs on; the
    // ticks it held run in the frames after it, three a frame, on the same musical positions.
    const bool held = clock_.simTime() < hitStopUntil_;
    if (held) {
        alpha_ = 1.0f;
    } else if (fixed && clock_.simTime() - tickEnd_ < 1.5 * kDt) {
        tickEnd_ = clock_.simTime();
        runTick(music1_, clock_.musicAt(tickEnd_), true);
        alpha_ = 1.0f;
    } else {
        u32 n = 0;
        while (clock_.simTime() - tickEnd_ >= static_cast<double>(kDt) - 1e-9 && n < 3) {
            tickEnd_ += kDt;
            runTick(music1_, clock_.musicAt(tickEnd_), n == 0);
            ++n;
        }
        // Beyond three ticks a frame, drop time rather than spiral (TECH_SPEC 4); not while catching up a hit-stop.
        if (clock_.simTime() - tickEnd_ > 3.0 * kDt && clock_.simTime() > hitStopUntil_ + 0.1) tickEnd_ = clock_.simTime() - 0.5 * kDt;
        alpha_ = clampf(static_cast<float>((clock_.simTime() - tickEnd_) / kDt), 0.0f, 1.0f);
    }
    bandFrame();
    if (flow_) {
        const PlayerState& me = world_->player(0);
        if (screen_ == Screen::Run && me.alive && !world_->restartPending()) runSeconds_ += frameDt_;
        if (opt_.autoplay && opt_.recklessAfter > 0.0f)
            bot_.setPolicy(screen_ == Screen::Run && runSeconds_ > opt_.recklessAfter ? Bot::Policy::Reckless : Bot::Policy::Play);
        if (screen_ == Screen::GameOver) overS_ += frameDt_;
    }

    pictureFrame();

    const World& w = *world_;
    const PlayerState& p = w.player(0);
    if (bandDrives_ && clock_.fromBand() && frameNo_ % 1200 == 0) {
        // Every 20 s (60 Hz): how the sim's musical clock sits against the band's heard one.
        RF_LOG("play: frame %llu: music %.1f, heard %.1f (sim ahead by %.1f ticks), %u band shots, %u fired, %u kills, "
               "%u enemies, intensity %.2f, latency %.1f ticks",
               static_cast<unsigned long long>(frameNo_), music1_, clock_.heardNow(), music1_ - clock_.heardNow(), bandShots_,
               p.shotsFired, w.counters().kills, w.enemies().count, intensity_, clock_.latencyTicks());
    }
    stats_.enemies = w.enemies().count;
    stats_.bullets = w.bullets().count;
    stats_.shots = w.shots().count;
    stats_.shards = w.shards().count;
    stats_.kills = w.counters().kills;
    stats_.deaths = w.counters().deaths;
    stats_.restarts = w.counters().restarts;
    stats_.score = p.score;
    stats_.chain = p.chain;
    stats_.bestChain = p.bestChain > stats_.bestChain ? p.bestChain : stats_.bestChain;
    const WorldCounters& c = w.counters();
    stats_.overflow = c.enemiesDropped + c.bulletsDropped + c.shotsDropped + c.shardsDropped + c.eventsDropped;
    stats_.ticks = w.tickCount();
    // M5 part 4: say when a pool overflows, where the run is (at most once a bar).
    const u32 over = stats_.overflow;
    if (over != lastOverflow_ && w.runBar() != lastOverflowBar_) {
        RF_WARN("play: pools overflowed: enemies %u, enemy bullets %u (%u alive), shots %u, Shards %u, events %u so far; Track %u cycle %u, run bar %u, %s",
                c.enemiesDropped, c.bulletsDropped, w.bullets().count, c.shotsDropped, c.shardsDropped, c.eventsDropped, w.phrase().track,
                w.phrase().cycle, w.runBar(), w.boss().active ? sim::bossId(w.boss().kind) : "no fight");
        lastOverflow_ = over;
        lastOverflowBar_ = w.runBar();
    }
}

void Play::pictureFrame() {
    // Picture runs: capture `stripFrames_` frames from the scripted moment.
    captureNow_ = false;
    if (!opt_.picture[0]) return;
    scriptEvents();
    ++scriptTick_;
    if (captureFrom_ >= 0 && static_cast<i64>(frameNo_) >= captureFrom_) {
        // (M5: a strip's frames can be stripStride_ apart, so eight of them cover a beat.)
        const u64 k = frameNo_ - static_cast<u64>(captureFrom_);
        if (k < static_cast<u64>(stripFrames_) * stripStride_) {
            if (k % stripStride_ == 0) {
                captureNow_ = true;
                captureIndex_ = static_cast<u32>(k / stripStride_);
            }
        } else {
            pictureDone_ = true;
        }
    }
    if (scriptTick_ > 60u * (m5Pic_ != 0xFFFF ? 75u : 30u)) pictureDone_ = true;  // a script that never triggers ends after 30 s (M5: 75)
}

void Play::runTick(double music0, double music1, bool firstOfFrame) {
    TickInput in;
    in.music0 = music0;
    in.music1 = music1;
    in.bpm = clock_.bpm();
    in.latencyTicks = clock_.latencyTicks();
    in.players[0] = controller_.pose();
    in.retry = retryRequest_;
    retryRequest_ = false;
    // While the band plays, the shots are the lead rule's heard steps (TECH_SPEC 6.3), each advanced by
    // the time since its step; without it, the sim fires on its own grid.
    in.bandShots = bandDrives_ && clock_.fromBand();
    if (in.bandShots && firstOfFrame) {
        const BeatView& v = band_->view();
        const float tps = clock_.ticksPerSecond();
        for (u32 k = 0; k < v.shotCount && in.shotCount < countOf(in.shots); ++k) {
            ShotRequest& r = in.shots[in.shotCount++];
            r.tick = v.shots[k].tick;
            r.player = 0;
            const float adv = static_cast<float>((music1 - v.shots[k].tick) / tps);
            r.advanceS = adv < 0.0f ? 0.0f : (adv > 0.1f ? 0.1f : adv);
        }
        bandShots_ += in.shotCount;
    }
    // M5 part 3: the song's bars against the run's (Singularity's and the Sweeper's phase follow the band's).
    world_->setSongBarShift(static_cast<i64>(std::llround((coop() ? netBandOffset_ : clock_.offset()) / kTicksPerBar)));
    const u64 t0 = nowNs();
    world_->tick(in);
    const float ms = static_cast<float>(nsToMs(nowNs() - t0));
    if (stats_.ticksLastFrame < countOf(stats_.tickMs)) stats_.tickMs[stats_.ticksLastFrame] = ms;
    ++stats_.ticksLastFrame;
    controller_.clearEdges();
    musicPrev_ = music1_;
    music1_ = music1;
    afterTick();
}

void Play::afterTick() {
    const World& w = *world_;
    view_.onEvents(w, tickEnd_);
    holdFight();  // M5 --fight ...:hold (benchmarks)
    if (opt_.loadoutMax) maxLoadout();  // M5 --loadout max (benchmarks)
    // M5: the fights' held voices end on time, and all of them when no fight is on.
    if (bandDrives_)  // (M5 audio: the held voices follow the world: this device's seat, the band's offset)
        updateFightVoices(*band_, fightVoices_, band_->view().heardTick, !w.boss().active || w.boss().dying, &w, seat_,
                          coop() ? netBandOffset_ : clock_.offset());
    bossHintTick(w);
    for (u32 k = 0; k < w.eventCount(); ++k) {
        const Event& e = w.events()[k];
        switch (e.type) {
            case EventType::Impact:
                // M5 (screens.md "Boss hint"): this player's shots on the boss, armour (below x 1, or a plate) or not.
                if (e.species == kBossSpecies && e.b == seat_ && hintBar_ >= 0) {
                    if (e.c == 1 || e.f < 1.0f) hintArmour_ = true;
                    else hintFull_ = true;
                }
                // Play test 3 (GAME_DESIGN 7.6): a weak point's hit flashes its segment of the boss's health bar.
                if (e.species == kBossSpecies && e.c != 1 && e.f >= 1.0f - 1e-3f) {
                    bossHitT_ = static_cast<float>(tickEnd_ - e.age);
                    bossHitPart_ = e.c == 2 ? e.a : 0xFF;
                }
                break;
            case EventType::Death:
                // The bot taps retry (also in the bot_ pictures, C2: the real careful bot's run goes on).
                if (opt_.bot || opt_.kind != PlayOptions::Kind::Play || std::strncmp(opt_.picture, "bot_", 4) == 0) retryRequest_ = true;
                if (bandDrives_) band_->playSfx(audio::Sfx::GameOver);
                RF_LOG("play: %s (Track %u, bar %u, score %llu)", coop() ? "every ship is down: the run is over" : "the ship is lost",
                       w.phrase().track, w.phrase().bar, static_cast<unsigned long long>(w.player(seat_).score));
                if (flow_ && (screen_ == Screen::Run || (coop() && (screen_ == Screen::Paused || screen_ == Screen::Settings)))) {
                    // M4 stage B: in co-op the run ends when all are down at once (GAME_DESIGN 10); the host's RETRY
                    // starts the next one for everyone.
                    endRun(true);
                    coopOver_ = coop();
                    screen_ = Screen::GameOver;
                    overS_ = -0.6f;  // a breath before the score counts up
                } else if (flow_ && attract()) {
                    retryRequest_ = true;  // the title's bot just goes again
                }
                break;
            case EventType::Restart:
                controller_.reset(w.player(seat_).pos);
                touch_.reset();
                // M6 (screens.md "Transitions"): the drop-in's downbeat; home's title bar line.
                if (trans_ == Trans::Wait) dropInLands(e.tick);
                else if (trans_ == Trans::Home) homeArrives(e.tick);
                if (coopFlowActive_ && coopOver_) {
                    // M4 stage B: the host's RETRY: the next run, everyone's screens back to it.
                    coopOver_ = false;
                    if (screen_ == Screen::GameOver) screen_ = Screen::Run;
                    applySettings();
                }
                // A new run: its record starts (the set list's first Track).
                runSeconds_ = 0.0f;
                runTrackMax_ = 1;
                runSetComplete_ = false;  // M6
                runEndedBy_[0] = 0;
                markCount_ = 0;
                marks_[markCount_++] = Mark{0.0f, 0, 1};
                loadoutSig_ = ~0u;
                upgradeCues_.reset();
                break;
            case EventType::Section:
                if (e.f > 0.5f) {
                    if (e.c > runTrackMax_) runTrackMax_ = e.c;
                    if (markCount_ < kMarks) marks_[markCount_++] = Mark{runSeconds_, 0, e.c};
                }
                break;
            case EventType::Spotlight: {
                // GAME_DESIGN 8: the debut's name on screen. Its family and tier (and variant B as "B").
                const Species& s = w.species().get(e.species);
                static const char* const kRoman[6] = {"", "I", "II", "III", "IV", "V"};
                char fam[16];
                std::snprintf(fam, sizeof fam, "%s", familyName(s.family));
                for (char* c = fam; *c; ++c)
                    if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 32);
                std::snprintf(spotlightName_, sizeof spotlightName_, "%s %s%s", fam, kRoman[s.tier < 6 ? s.tier : 5],
                              s.variant == kVariantB ? "  B" : (s.variant == kVariantC ? "  C" : ""));
                spotlightT_ = static_cast<float>(tickEnd_);
                break;
            }
            case EventType::Kill:
                if (!e.c && e.a < 3) ++killsPending_[e.a];
                break;
            case EventType::Pickup:
                if (shardsPending_ < kShardRunMax) ++shardsPending_;
                break;
            case EventType::WarpStart:
                // One swell per landing beat, ending as the enemies become solid.
                if (bandDrives_ && e.tick != lastWarpTick_) {
                    const float pan = clampf((e.x - w.player(seat_).pos.x) / 800.0f, -1.0f, 1.0f) * 0.6f;
                    band_->warp(e.tick, pan);
                    lastWarpTick_ = e.tick;
                }
                break;
            case EventType::Telegraph:
                // The tell one beat ahead and, for a shooter, the shot's blip on the beat it resolves on
                // (sfxproto's telegraph tick and enemy shot), once per beat.
                if (bandDrives_ && e.tick != lastTellTick_) {
                    lastTellTick_ = e.tick;
                    band_->playSfx(audio::Sfx::Tick, 0.7f);
                    // (M5: a fight's own lines carry kBossSpecies, no species.)
                    if (e.species != kBossSpecies && w.species().get(e.species).ranged()) band_->sfxAt(audio::Sfx::EnemyShot, e.tick);
                }
                break;
            case EventType::Hit:
                // M4 stage B: the set list keeps whose hit it was (screens.md: each player's crosses in their colour);
                // this device sounds its own player's.
                if (markCount_ < kMarks) marks_[markCount_++] = Mark{runSeconds_, 3, e.a};
                if (e.a == seat_) hitMomentT_ = static_cast<float>(tickEnd_ - e.age);  // (M6: the hint's moment)
                if (e.a == seat_ && e.c == 1) pulse(1);  // M6: the guard ring breaking (a medium pulse)
                if (bandDrives_ && e.a == seat_) {
                    band_->playSfx(audio::Sfx::Hit);
                    upgradeCues_.add(e, 0.0f, 0.0);  // M5: Crescendo falls back
                }
                break;
            case EventType::NovaPress: {
                // M5: Black Hole's Nova is its own cue (the three-beat inhale and the blast with the sub drop).
                const PlayerState& me = w.player(seat_);
                const bool hole = me.holeFrom >= 0.0 && me.holeBlow == e.tick;
                if (bandDrives_ && e.a == seat_ && !hole) band_->nova(e.tick);
                break;
            }
            case EventType::Upgrade:
                // M5 part 3: the upgrades' cues (game/upgrade_sounds.cpp), this device's player's.
                if (bandDrives_ && e.a == seat_)
                    upgradeCues_.add(e, clampf((e.x - w.player(seat_).pos.x) / 900.0f, -1.0f, 1.0f) * 0.7f,
                                     coop() ? netBandOffset_ : clock_.offset());
                break;
            case EventType::Dash:
                if (bandDrives_ && e.a == seat_) band_->playSfx(e.b ? audio::Sfx::PerfectDash : audio::Sfx::Dash);
                break;
            case EventType::TrioUnlock:
                // Play test 1 (screens.md): the orbs light on the Break's third beat with a chime (a bell two octaves
                // over the Track's root, on that beat).
                if (bandDrives_ && e.a == seat_)
                    band_->leadNote(0, 4, 0.55f, 0.0f, e.tick + (coop() ? netBandOffset_ : clock_.offset()), static_cast<u8>(audio::Timbre::P3Bell));
                break;
            case EventType::TrioTake:
                if (e.a != seat_) break;  // M4: a teammate's pick is theirs (their device sounds it)
                RF_LOG("play: Trio: took %s (now %u) at bar %u", isBoostOrb(e.b) ? boostInfo(boostOfOrb(e.b)).name : (isFusionOrb(e.b) ? fusionInfo(fusionOfOrb(e.b)).name : upgradeInfo(e.b).name), e.c,
                       w.phrase().bar);
                if (markCount_ < kMarks) marks_[markCount_++] = Mark{runSeconds_, 2, e.b};
                takenT_ = static_cast<float>(tickEnd_);
                takenFrom_ = view_.toScreen(Vec2{e.x, e.y});
                // screens.md: the "upgrade taken" chord, on the next sixteenth.
                if (bandDrives_) band_->sfxAt(audio::Sfx::Upgrade, (std::floor(band_->view().heardTick / kTicksPerStep) + 1.0) * kTicksPerStep);
                break;
            case EventType::BoostTake:
                if (bandDrives_ && e.a == seat_) band_->playSfx(audio::Sfx::Boost);
                break;
            case EventType::LevelUp:
                // Play test 2 (look.md 6): the meter flashes white and the number steps with a bright two-note rise of the
                // player's lead (the fifth, then the octave, on the next two sixteenths).
                if (e.a != seat_) break;
                levelUpT_ = static_cast<float>(tickEnd_);
                RF_LOG("play: Shard level %.0f at %.1f s (bar %u)", static_cast<double>(e.f), static_cast<double>(runSeconds_), w.phrase().bar);
                if (bandDrives_) {
                    const double step = (std::floor(band_->view().heardTick / kTicksPerStep) + 1.0) * kTicksPerStep;
                    band_->leadNote(0, 1, 0.9f, 0.0f, step);
                    band_->leadNote(0, 2, 0.9f, 0.0f, step + kTicksPerStep);
                }
                break;
            case EventType::CellTake:
                // Play test 2 (look.md 6): a Cell taken plays a rising note of the player's lead (Regen's arc grows quietly).
                if (bandDrives_ && e.a == seat_ && e.c == 0) band_->leadNote(0, 2, 0.85f, 0.0f);
                break;
            case EventType::HaloBlock:
                if (bandDrives_ && e.a == seat_) band_->playSfx(audio::Sfx::Shield);
                break;
            case EventType::ExtraLife:
                // Play test 3 (M5 audio): an Extra Life picked up, or spent (its stab lands with the free Nova's blast).
                if (bandDrives_ && e.a == seat_) {
                    Event x = e;
                    if (e.b) x.tick = w.player(seat_).novaBlastTick;
                    upgradeCues_.add(x, 0.0f, coop() ? netBandOffset_ : clock_.offset());
                }
                if (e.a == seat_ && e.b) pulse(2);  // M6: an Extra Life spent (a heavy pulse)
                break;
            case EventType::Revive:
                if (e.a == seat_) controller_.reset(w.player(seat_).pos);  // M4: back where the beacon was
                // look.md 6: the revive's close is the octave of its fill's notes, in the revived player's lead.
                if (bandDrives_ && coop() && e.b)
                    band_->leadNote(bandSlot(e.a), 2, 0.8f, clampf((e.x - w.player(seat_).pos.x) / 800.0f, -1.0f, 1.0f) * 0.6f);
                break;
            case EventType::BossEnter:
                RF_LOG("play: %s enters (Track %u, bar %u)", bossName(static_cast<BossKind>(e.a)), w.phrase().track, w.phrase().bar);
                if (bandDrives_) band_->playSfx(audio::Sfx::Elite);
                if (isMiniBoss(static_cast<BossKind>(e.a))) pulse(1);  // M6: a mini-boss's arrival (a medium pulse)
                break;
            case EventType::BossPair:
                // kaleidos.md: that voice leaves the chord; a low growl (the elite-arrives patch) enters, an octave
                // down (M3 stage C). M5: a segment, a Voice (its own cue), a Prince.
                if (bandDrives_ && static_cast<BossKind>(e.c) != BossKind::Choir) band_->playSfx(audio::Sfx::Elite, 1.2f, 0.5f);
                break;
            case EventType::BossDeath: {
                const BossKind kind = static_cast<BossKind>(e.a);
                RF_LOG("play: %s falls (Track %u, run %.1f s)", bossName(kind), w.phrase().track, runSeconds_);
                if (bandDrives_) band_->kill(2, isMiniBoss(kind) ? 3 : 6);
                if (isMiniBoss(kind)) break;  // the Track goes on
                pulse(2);  // M6: a boss's death (a heavy pulse)
                if (markCount_ < kMarks) marks_[markCount_++] = Mark{runSeconds_, 1, 0};
                hitStopUntil_ = clock_.simTime() + static_cast<double>(look_->boom.bossHitStopMs) * 0.001;
                // M6 (screens.md "A finished run"): the set list's last boss has fallen: the run is won (SET COMPLETE for two bars,
                // then the loop's own card).
                if (w.phrase().track == w.setList() && !runSetComplete_) {
                    runSetComplete_ = true;
                    setCompleteT_ = static_cast<float>(tickEnd_ - e.age);
                    setCompleteScore_ = w.player(seat_).score;
                    RF_LOG("play: SET COMPLETE (run %.1f s)", runSeconds_);
                }
                break;
            }
            case EventType::BossGone:
                RF_LOG("play: %s warps out (Track %u, run %.1f s)", bossName(static_cast<BossKind>(e.a)), w.phrase().track, runSeconds_);
                break;
            case EventType::BossCue:
                if (bandDrives_) bossCue(e);
                break;
            default: break;
        }
    }
    // Play test 1 (screens.md, the Trio): holding an orb plays two rising notes of the player's lead, the root as the
    // fill starts and the fifth a beat later; leaving silences it (a new hold starts again from the root).
    {
        const TrioState& tr = w.trio(seat_);
        const i8 orb = tr.open ? tr.holdOrb : static_cast<i8>(-1);
        if (orb != holdOrbSeen_) {
            holdOrbSeen_ = orb;
            holdStage_ = 0;
        }
        const u8 stage = orb < 0 ? 0 : (tr.holdTicks >= kTicksPerBeat ? 2 : (tr.holdTicks > 0.0 ? 1 : 0));
        if (bandDrives_ && stage > holdStage_) band_->leadNote(0, stage == 1 ? 0 : 1, 0.8f, 0.0f);
        holdStage_ = stage;
    }
}

void Play::bossHintTick(const World& w) {
    // screens.md "Boss hint (HUD, M5)": the line comes back for two bars when this player's shots have met only armour for
    // two bars running, at most once every eight bars (the arrival's showing counts as the first).
    const BossState& b = w.boss();
    if (!b.active || b.dying || !bossHint(b.kind)) {  // (b1d4a2f: the mini-bosses with a hint line too)
        hintFight_ = -1.0;
        hintBar_ = -1;
        return;
    }
    if (b.fightTick != hintFight_) {
        hintFight_ = b.fightTick;
        hintBar_ = -1;
        hintArmour_ = hintFull_ = false;
        hintRun_ = 0;
        hintShowBar_ = -1;
        hintLastBar_ = 0;
    }
    if (w.music() < b.fightTick) return;
    const i64 fb = static_cast<i64>(std::floor((w.music() - b.fightTick) / kTicksPerBar));
    if (fb == hintBar_) return;
    if (hintBar_ >= 0) {
        hintRun_ = hintArmour_ && !hintFull_ ? static_cast<u8>(hintRun_ + 1) : 0;
        if (hintRun_ >= 2 && fb >= hintLastBar_ + 8) {
            hintShowBar_ = fb;
            hintLastBar_ = fb;
            hintRun_ = 0;
        }
    }
    hintBar_ = fb;
    hintArmour_ = hintFull_ = false;
}

void Play::bandFrame() {
    if (!bandDrives_) {
        killsPending_[0] = killsPending_[1] = killsPending_[2] = 0;
        shardsPending_ = 0;
        upgradeCues_.reset();
        return;
    }
    upgradeCues_.flush(*band_, *world_, seat_);  // M5 part 3
    // Kills: the engine aggregates them per 1/32 step (one cluster sound per size class).
    for (u8 s = 0; s < 3; ++s) {
        if (killsPending_[s]) band_->kill(s, killsPending_[s]);
        killsPending_[s] = 0;
    }
    // Shards: each pickup plays the next note of the run, at most one per 1/32 step, so a streak of
    // pickups is a rising run rather than a chord.
    const double heard = clock_.heardNow();
    if (shardsPending_ && heard >= lastShardTick_ + kShardStepTicks) {
        band_->shard();
        --shardsPending_;
        lastShardTick_ = heard;
    }
    conductBand();
    // The bed's intensity (Groove) follows the threat on screen. M4 stage B (GAME_DESIGN 10: the shared bed is
    // identical on every device): in a co-op run the host's, bar by bar (coopBed).
    const float target = clampf(tuning::kIntensityFloor + world_->threatInView() * tuning::kIntensityPerThreat, 0.0f, 1.0f);
    intensity_ += (target - intensity_) * (1.0f - std::exp(-frameDt_ / tuning::kIntensityTau));
    if (coop() && netRunStarted_) {
        coopBed();
    } else if (std::fabs(intensity_ - intensityPosted_) >= kIntensityStep) {
        band_->setIntensity(intensity_);
        intensityPosted_ = intensity_;
    }
}

void Play::conductBand() {
    // M3 stage B: the director is the one source of the form; the band follows it on bar lines. In the
    // second half of every heard bar, the band is told what the next bar is: a new run, the phrase (section
    // and bar in it), a Track change, the boss's bar. The engine applies each on that bar line.
    if (!bandDrives_ || !clock_.fromBand()) return;
    const World& w = *world_;
    const PlayerState& pl = w.player(seat_);
    // The local player's upgrades: every Mod, Riff, Boost and Fusion (M5, audio/cues.h); the band plays the lead's
    // changes, the Riffs' layers and the Boosts' and Fusions' from them.
    {
        const audio::PlayerLoadout l = bandLoadout(pl);
        const u32 sig = loadoutSignature(l);
        if (sig != loadoutSig_) {
            loadoutSig_ = sig;
            band_->setLoadout(0, l);
        }
    }
    const double heard = band_->view().heardTick;  // the band's own (absolute) song ticks
    const double offset = coop() ? netBandOffset_ : clock_.offset();  // the run's ticks = the band's - offset (whole bars)
    const double line = std::floor(heard / kTicksPerBar) * kTicksPerBar;
    if (heard - line < kPlanAtBar * kTicksPerBar) return;
    const double nextLine = line + kTicksPerBar;
    if (nextLine <= plannedLine_ + 1.0) return;
    plannedLine_ = nextLine;
    const auto off = [&] {
        if (bossPosted_) band_->setBoss(0, false, 0, 0);
        bossPosted_ = false;
    };
    if (w.restartPending() && std::fabs(w.restartTick() + offset - nextLine) < 1.0) {
        // A new run on that line: Track 1 at its tempo, Short form, its first Groove.
        band_->setHold(false);
        band_->changeTrack(0, bpmOfTrack(1, w.setList()), true);
        postedBpm_ = bpmOfTrack(1, w.setList());
        band_->setForm(audio::Form::Short);
        band_->setPhrase(audio::Section::Groove, 0, kSectionBars[0][0]);
        off();
        overHeld_ = false;
        return;
    }
    if (!pl.alive && !coop()) {
        // The run is over: the band holds a Break under the game-over screen until the next run (co-op: the run goes
        // on while anyone stands; the host starts the next one).
        if (!overHeld_) {
            band_->setPhrase(audio::Section::Break, 0, kSectionBars[0][3]);
            band_->setHold(true);
            off();
            overHeld_ = true;
        }
        return;
    }
    if (overHeld_ || paused_) return;
    const u32 bar = w.barOf(nextLine - offset + 1.0);
    const Phrase next = w.phraseOfBar(bar);
    if (next.trackChange && next.sectionBar == 0) {
        // The boss is dead: the Track-change Break (tempo ramp, key on its fifth bar) into the next Track.
        band_->setHold(false);
        band_->changeTrack(next.song, next.trackBpm, false);
        band_->setForm(audio::Form::Short);  // play test 3: every Track Short
        postedBpm_ = next.trackBpm;
    }
    if (!next.trackChange && (band_->track() != next.song || std::fabs(postedBpm_ - next.trackBpm) > 0.01f)) {
        // The band plays another Track or tempo than the run (a new run, a fast-forward, the loop after a
        // resync): it changes on the next bar line.
        band_->changeTrack(next.song, next.trackBpm, true);
        band_->setForm(audio::Form::Short);  // play test 3: every Track Short
    }
    postedBpm_ = next.trackBpm;
    if (holdPosted_) {
        band_->setHold(false);
        holdPosted_ = false;
    }
    band_->setPhrase(static_cast<audio::Section>(next.section), next.sectionBar, next.sectionBars);
    if (next.sectionBar == 0)
        RF_LOG("play: band told: run bar %u (band tick %.0f) %s, %u bars, Track %u cycle %u%s", next.bar, nextLine, sectionLabel(next.section),
               next.sectionBars, next.track, next.cycle, next.trackChange ? " (Track change)" : (next.bossDrop ? " (boss)" : ""));
    // The boss's bar: which pairs fire on which beats (their bells), the iris's gong, a beam's riser and stab (Kaleidos; the
    // other fights speak through their cues, bossCue).
    const BossState& b = w.boss();
    const double simLine = nextLine - offset;
    if (!b.active || b.dying || simLine < b.fightTick || b.kind != BossKind::Kaleidos) {
        off();
        return;
    }
    u8 mask = 0, beats = 0, flags = 8u | 1u;
    for (u32 k = 0; k < 4; ++k) {
        const u8 m = static_cast<u8>(w.bossFiringMask(simLine + k * kTicksPerBeat + 48.0) |
                                     w.bossFiringMask(simLine + k * kTicksPerBeat + 24.0) | w.bossFiringMask(simLine + k * kTicksPerBeat));
        if (m) beats = static_cast<u8>(beats | (1u << k));
        mask = static_cast<u8>(mask | m);
    }
    for (u32 p = 0; p < 3; ++p) {
        const u8 st = w.bossBeamState(p, simLine + 1.0);
        if (st == 1) flags = static_cast<u8>(flags | 2u);
        if (st == 2) flags = static_cast<u8>(flags | 4u);
    }
    band_->setBoss(mask, b.phase == 2, beats, flags);
    bossPosted_ = true;
}

void Play::bossCue(const sim::Event& e) {
    // M5: a fight's sound, as its page gives it, through the one table of pending sounds (game/pending_sounds.cpp): until the
    // engine has the page's voices, each cue plays the nearest effect the band has, on its own tick, panned to where it is.
    const World& w = *world_;
    const double offset = coop() ? netBandOffset_ : clock_.offset();
    const float pan = clampf((e.x - w.player(seat_).pos.x) / 900.0f, -1.0f, 1.0f) * 0.7f;
    playFightCue(*band_, fightVoices_, e, e.tick + offset, pan, postedBpm_);
}

// ---------------------------------------------------------------------------------------------- render

void Play::render(FrameDraw& d, FrameParams& params, Background& bg, const LookVariant& variant, float W, float H, Vec2 insetMin,
                  Vec2 insetMax) {
    const World& w = *world_;
    ViewInput vi;
    vi.world = &w;
    vi.alpha = alpha_;
    vi.time = tickEnd_ - (1.0 - static_cast<double>(alpha_)) * kDt;
    vi.dt = frameDt_;
    vi.music = musicPrev_ + (music1_ - musicPrev_) * static_cast<double>(alpha_);
    const float bpmNow = coop() && session_->songValid() ? static_cast<float>(session_->tpsAt(session_->hostNow()) * 60.0 / 96.0) : clock_.bpm();
    vi.bpm = bpmNow > 1.0f ? bpmNow : 120.0f;
    vi.beat = (bandDrives_ && band_ && clock_.fromBand()) ? &band_->view() : nullptr;
    const PlayerPose& pose = controller_.pose();
    vi.shipPos = pose.pos;
    vi.shipVel = pose.vel;
    vi.shipAim = pose.aim;
    vi.shipHeading = pose.heading;
    vi.shipFiring = pose.fire != 0;
    if (coop() && opt_.netViewSeat >= 0 && opt_.netViewSeat < static_cast<i32>(kMaxPlayers) && w.player(static_cast<u32>(opt_.netViewSeat)).present) {
        // M4 pictures: every peer's camera on one ship, so their views of one moment show the same place.
        const PlayerState& o = w.player(static_cast<u32>(opt_.netViewSeat));
        vi.camFixed = true;
        vi.camPos = o.prevPos + (o.pos - o.prevPos) * alpha_;
    }
    // M4 stage B (screens.md "Co-op (M4)"): nothing of a run behind the co-op setup screens (the world there is
    // empty and still), and no beacon behind the co-op game over.
    vi.noShips = flow_ && (screen_ == Screen::CoopLobby || screen_ == Screen::CoopJoin || screen_ == Screen::NetPermission);
    vi.noBeacons = coop() && coopOver_;
    vi.backgroundOnly = flow_ && !coop() && (screen_ == Screen::CoopJoin || screen_ == Screen::NetPermission);
    // M6 (screens.md "Transitions"): the run's own light as the transitions have it; its ship goes with it, and on the drop-in's
    // downbeat beat the screen draws the ship (the title's gliding in, or the ship drawing itself) until the run's takes over.
    if (flow_) {
        vi.runLight = transRunLight();
        if (vi.runLight < 0.7f || (trans_ == Trans::Arrive && transBeats(transLine_) < 1.0f)) vi.noShips = true;
    }
    hintsTick(vi);  // M6: the first-run hints
    {
        // The HUD's blocks this frame: edge chevrons slide 20 px clear of them (screens.md "In the run").
        const float s = H / 1080.0f * hudScale();  // (M6: the UI scale)
        const float m = std::fmax(132.0f * s, insetMin.x), mr = std::fmax(132.0f * s, insetMax.x), top = insetMin.y;
        HudKeepOut k;
        const bool runHud = !flow_ || screen_ == Screen::Run || screen_ == Screen::Paused;
        if (runHud) {
            const PlayerState& me = w.player(seat_);
            u64 score = me.score;
            u32 ranks = 0;
            if (coop()) {
                score = 0;
                for (u32 q = 0; q < kMaxPlayers; ++q)
                    if (w.player(q).present) {
                        score += w.player(q).score;
                        ++ranks;
                    }
            }
            char num[32];
            std::snprintf(num, sizeof num, "%llu,,,", static_cast<unsigned long long>(score));  // (room for the separators)
            const float scoreW = font_->measure(TextStyle{34.0f * s}, num);
            const float bottom = ranks > 1 ? top + (160.0f + 26.0f * static_cast<float>(ranks - 1) + 13.0f) * s : top + 144.0f * s;
            k.a[k.count] = {0.0f, 0.0f};
            k.b[k.count++] = {m + std::fmax(scoreW, 290.0f * s), bottom};
            // The Track line and the phrase bar with its "DROP IN n".
            k.a[k.count] = {0.5f * W - 280.0f * s, 0.0f};
            k.b[k.count++] = {0.5f * W + 380.0f * s, top + 70.0f * s};
            // The Nova stock and the Dash bar under it.
            const float novas = static_cast<float>(me.novaCap > 0 ? me.novaCap - 1 : 0);
            k.a[k.count] = {W - mr - 150.0f * s - 50.0f * s * novas - 16.0f * s, 0.0f};
            k.b[k.count++] = {W - mr - 40.0f * s, top + 122.0f * s};
            // The pause ring.
            float r = 0.0f;
            const Vec2 pc = pauseButtonCentre(W, H, insetMin, insetMax, r);
            k.a[k.count] = pc - Vec2{r, r};
            k.b[k.count++] = pc + Vec2{r, r};
        }
        view_.setHudKeepOut(k);
    }
    updatePalette(vi.music);
    view_.render(d, params, bg, W, H, vi);

    HudState hs;
    const PlayerState& p = w.player(seat_);
    hs.score = p.score;
    if (coop()) {
        // GAME_DESIGN 10: the team's score is the sum of the players' (each keeps its own Chain); M4 stage B
        // (screens.md): each player's own under it in their colour, ranked.
        hs.score = 0;
        hs.coopPlayers = w.playerCount();
        hs.me = static_cast<u8>(seat_);
        hs.rankCount = 0;
        for (u32 q = 0; q < kMaxPlayers; ++q)
            if (w.player(q).present) {
                hs.score += w.player(q).score;
                hs.playerScore[q] = w.player(q).score;
                u32 at = hs.rankCount++;
                while (at > 0 && hs.playerScore[hs.rank[at - 1]] < hs.playerScore[q]) {
                    hs.rank[at] = hs.rank[at - 1];
                    --at;
                }
                hs.rank[at] = static_cast<u8>(q);
            }
    }
    hs.chain = p.chain;
    hs.chainPulse = view_.chainPulse(vi.time);
    // Play test 2: the Shard level (this device's own player).
    hs.level = p.level;
    hs.levelFull = p.levelsBanked > 0;
    {
        const u32 need = levelShards(static_cast<u32>(p.level) + 1u);
        hs.levelFill = need ? static_cast<float>(p.levelShards) / static_cast<float>(need) : 0.0f;
        const float since = static_cast<float>(vi.time) - levelUpT_;
        hs.levelFlash = since >= 0.0f && since < 0.6f ? 1.0f - since / 0.6f : 0.0f;
    }
    hs.novas = p.novas;
    hs.novaCap = p.novaCap;
    hs.extraLife = p.extraLives > 0;  // play test 3
    hs.reduceFlashing = settings_.reduceFlashing || forcedReduceFlashing_;  // M6
    if (runSetComplete_) {  // M6: SET COMPLETE's card
        hs.setCompleteBars = static_cast<float>((vi.time - setCompleteT_) * static_cast<double>(vi.bpm) / 240.0);
        hs.setCompleteScore = setCompleteScore_;
    }
    hs.dashCharge = controller_.dashCharge(coop() ? netHeard_ : clock_.heardNow());
    hs.alive = p.alive;
    hs.retryPending = w.restartPending();
    hs.beat = view_.beat();
    // The run's HUD: in the run (and under pause); the screens draw their own.
    const bool runHud = !flow_ || screen_ == Screen::Run || screen_ == Screen::Paused;
    hs.showTouch = touch_.used() && runHud && p.alive;
    hs.world = &w;
    hs.music = vi.music;
    hs.icons = &icons_;
    hs.upgradeIcons = upgradeIcons_;
    hs.boostIcons = boostIcons_;
    hs.trackName = songName(w.phrase().song);
    hs.bpm = vi.bpm;
    hs.runHud = runHud;
    hs.pausePressed = pausePressed_ || screen_ == Screen::Paused;
    const float now = static_cast<float>(vi.time);
    if (now - takenT_ < 0.6f) {
        hs.takenAge = now - takenT_;
        hs.takenFrom = takenFrom_;
    }
    if (spotlightName_[0] && now - spotlightT_ < 3.0f) {
        hs.spotlight = spotlightName_;
        hs.spotlightAge = now - spotlightT_;
    }
    {
        // M5 (screens.md "Boss hint"): from the arrival through the fight's first four bars, the last fading; again for two
        // bars (the last fading) when this player's shots met only armour.
        const BossState& b = w.boss();
        if (b.active && !b.dying && bossHint(b.kind) && b.fightTick == hintFight_) {
            const double bars = (vi.music - b.fightTick) / kTicksPerBar;
            float level = 0.0f;
            if (vi.music >= b.enterTick && bars < 4.0)
                level = clampf(static_cast<float>((vi.music - b.enterTick) / kTicksPerBeat), 0.0f, 1.0f) * clampf(static_cast<float>(4.0 - bars), 0.0f, 1.0f);
            if (hintShowBar_ >= 0) {
                const double since = bars - static_cast<double>(hintShowBar_);
                if (since >= 0.0 && since < 2.0)
                    level = std::fmax(level, clampf(static_cast<float>(since * 4.0), 0.0f, 1.0f) * clampf(static_cast<float>(2.0 - since), 0.0f, 1.0f));
            }
            hs.bossHint = bossHint(b.kind);
            hs.bossHintLevel = level;
        }
        // Play test 3 (GAME_DESIGN 7.6): the hit's segment of the bar, white for 60 ms, then fading over as long again.
        const float since = now - bossHitT_;
        if (b.active && !b.dying && since >= 0.0f && since < 0.12f) {
            hs.bossFlash = since < 0.06f ? 1.0f : 1.0f - (since - 0.06f) / 0.06f;
            hs.bossFlashPart = bossHitPart_;
        }
    }
    if (runHud) {
        // M6 (screens.md "Transitions"): the drop-in brings the HUD in over its second beat (the Track name card writes itself as
        // always).
        setHudFade(flow_ && trans_ == Trans::Arrive ? clampf(transBeats(transLine_) - 1.0f, 0.0f, 1.0f) : 1.0f);
        drawHud(d, *font_, *look_, variant, *palettes_, *palette_, hs, touch_, W, H, insetMin, insetMax);
        setHudFade(1.0f);
    }
    if (flow_) renderScreens(d, params, variant, W, H, insetMin, insetMax);
}

void Play::renderProbeTitle(FrameDraw& d, FrameParams& params, const LookVariant& variant, float W, float H, Vec2 insetMin, Vec2 insetMax) {
    // The tier probe's stage runs behind the title at a third of its light; the title's controls are live.
    params.exposurePump = 1.0f;
    if (!flow_) return;
    renderScreens(d, params, variant, W, H, insetMin, insetMax);
}

void Play::updatePalette(double music) {
    // GAME_DESIGN 4.2: each Track has its palette; the Track-change Break cross-fades it.
    if (!paletteFollows_ || !basePalette_) return;
    const auto forSong = [&](u32 song) {
        const u32 want = song + 1u;  // palettes.json: ultraviolet, solar, reef for Tracks 1-3
        for (u32 i = 0; i < palettes_->count; ++i)
            if (palettes_->list[i].track == want) return &palettes_->list[i];
        return basePalette_;
    };
    const Phrase& ph = world_->phrase();
    const Palette* to = forSong(ph.song);
    if (homeFadeLine_ >= 0.0 && music >= homeFadeLine_ && music < homeFadeLine_ + kTicksPerBar) {
        // M6 (screens.md "Transitions", home): the palette the run ended in cross-fades to Track 1's over the title's bar.
        float u = clampf(static_cast<float>((music - homeFadeLine_) / kTicksPerBar), 0.0f, 1.0f);
        u = u * u * (3.0f - 2.0f * u);
        const auto mixh = [&](Vec3 a, Vec3 b) { return a + (b - a) * u; };
        const Palette& from = homeFrom_;
        mixed_ = *to;
        mixed_.bg0 = mixh(from.bg0, to->bg0);
        mixed_.bg1 = mixh(from.bg1, to->bg1);
        mixed_.grid = mixh(from.grid, to->grid);
        mixed_.gridHot = mixh(from.gridHot, to->gridHot);
        mixed_.accent = mixh(from.accent, to->accent);
        for (u32 k = 0; k < 3; ++k) {
            mixed_.hue[k] = mixh(from.hue[k], to->hue[k]);
            mixed_.partner[k] = mixh(from.partner[k], to->partner[k]);
        }
        palette_ = &mixed_;
        view_.setPalette(mixed_);
        return;
    }
    const sim::BossState& boss = world_->boss();
    if (!ph.trackChange && boss.active && boss.kind == sim::BossKind::Reprise && boss.rp.phase == 2 && boss.coreMax > 0.0f) {
        // M5 part 6 (reprise.md): over Last Light the palette cross-fades from Last Light to Ultraviolet (by the health left).
        const Palette* uv = forSong(0);
        float u = boss.dying ? 1.0f : clampf(1.0f - boss.coreHp / (sim::prose::kRpLastAt * boss.coreMax), 0.0f, 1.0f);
        u = u * u * (3.0f - 2.0f * u);
        const auto mixv = [&](Vec3 a, Vec3 b) { return a + (b - a) * u; };
        mixed_ = *to;
        mixed_.bg0 = mixv(to->bg0, uv->bg0);
        mixed_.bg1 = mixv(to->bg1, uv->bg1);
        mixed_.grid = mixv(to->grid, uv->grid);
        mixed_.gridHot = mixv(to->gridHot, uv->gridHot);
        mixed_.accent = mixv(to->accent, uv->accent);
        for (u32 k = 0; k < 3; ++k) {
            mixed_.hue[k] = mixv(to->hue[k], uv->hue[k]);
            mixed_.partner[k] = mixv(to->partner[k], uv->partner[k]);
        }
        palette_ = &mixed_;
        view_.setPalette(mixed_);
        return;
    }
    if (!ph.trackChange) {
        palette_ = to;
        view_.setPalette(*to);
        return;
    }
    const Palette* from = forSong((ph.song + world_->setList() - 1u) % world_->setList());
    const double inBar = (music - world_->tickOfBar(ph.bar)) / kTicksPerBar;
    float u = clampf((static_cast<float>(ph.sectionBar) + static_cast<float>(inBar)) / static_cast<float>(ph.sectionBars), 0.0f, 1.0f);
    u = u * u * (3.0f - 2.0f * u);
    mixed_ = *to;
    const auto mixv = [&](Vec3 a, Vec3 b) { return a + (b - a) * u; };
    mixed_.bg0 = mixv(from->bg0, to->bg0);
    mixed_.bg1 = mixv(from->bg1, to->bg1);
    mixed_.grid = mixv(from->grid, to->grid);
    mixed_.gridHot = mixv(from->gridHot, to->gridHot);
    mixed_.accent = mixv(from->accent, to->accent);
    for (u32 k = 0; k < 3; ++k) {
        mixed_.hue[k] = mixv(from->hue[k], to->hue[k]);
        mixed_.partner[k] = mixv(from->partner[k], to->partner[k]);
    }
    palette_ = &mixed_;
    view_.setPalette(mixed_);
}

// ---------------------------------------------------------------------------------------------- flow

void Play::applySettings() {
    // Settings apply at once (screens.md: every change is heard as it is made).
    view_.setReduceFlashing(settings_.reduceFlashing || forcedReduceFlashing_);
    touch_.setSwap(settings_.swapSides);
    {
        // M6 (SETTINGS, LOOK): the UI scale of HUD text and touch controls; the controls are laid out again at it.
        static const float kScales[4] = {0.9f, 1.0f, 1.15f, 1.3f};
        const float scale = kScales[settings_.uiScale < 4 ? settings_.uiScale : 1];
        if (scale != hudScale()) {
            setHudScale(scale);
            if (layoutW_ > 0.0f) touch_.layout(layoutW_, layoutH_, insetMin_, insetMax_, opt_.pxPerMm);
        }
    }
    world_->rules().autoNova = settings_.autoNova;  // play test 1, second pass: this device's ship
    if (band_ && band_->active()) {
        if (band_->variant() != settings_.variant) band_->setVariant(settings_.variant);
        // The music under pause (and its settings): dipped and behind the low-pass, until the count-in ends.
        const bool underPause = screen_ == Screen::Paused || (screen_ == Screen::Settings && settingsBack_ == Screen::Paused);
        const float pauseDip = underPause ? 0.45f : 1.0f;
        band_->setGains(settings_.music * settings_.master * pauseDip, settings_.effects * settings_.master);
        band_->setMuffle(underPause);
        band_->setHaptics(settings_.haptics > 0);
        band_->setHapticStrong(settings_.haptics == 2);  // M3 stage C: SOFT and STRONG differ
    }
}

void Play::startRun() {
    // PLAY: the drop-in (screens.md "Transitions"): a run on the first bar line at least two beats away, the title leaving
    // over the wait (game/play_transitions.cpp).
    dropInTap(Screen::Title);
}

void Play::restartRun() {
    world_->requestRestart();
    screen_ = Screen::Run;
    countIn_ = false;
    applySettings();  // RESTART from pause: the music comes out from under the pause
    if (band_) band_->playSfx(audio::Sfx::UiConfirm);
}

void Play::toTitle() {
    // HOME (and QUIT RUN): home (screens.md "Transitions"): the screen leaves in a beat, the title builds itself over the next
    // bar, the bot's new run behind it fades up over the bar after (game/play_transitions.cpp).
    homeTap();
}

void Play::openSettings(Screen back) {
    settingsBack_ = back;
    screen_ = Screen::Settings;
    if (band_) band_->playSfx(audio::Sfx::UiTick);
}

void Play::pauseRun() {
    if (coop()) {
        // M4 stage B (GAME_DESIGN 10): any player may pause; it holds for everyone on the next beat (even while down).
        if (flow_ && screen_ == Screen::Run) coopPause(true);
        return;
    }
    if (!flow_ || screen_ != Screen::Run || !world_->player(0).alive) return;
    screen_ = Screen::Paused;
    countIn_ = false;
    pauseMusic_ = music1_;
    touch_.reset();
    mouseFire_ = mouseNova_ = false;
    for (bool& k : keys_) k = false;
    if (bandDrives_) {
        band_->setHold(true);  // the band keeps the section; nothing in the run moves
        band_->playSfx(audio::Sfx::UiTick);
    }
    applySettings();
}

double Play::countInNow() const {
    if (bandDrives_ && band_->view().running) return band_->view().heardTick;
    return countInClockS_;
}

void Play::beginCountIn() {
    // RESUME: four beats counted in; the run goes on where it paused within its bar, so its bar lines stay on
    // the band's (the resume offset is a whole number of bars).
    countIn_ = true;
    const double barStart = std::floor(pauseMusic_ / kTicksPerBar) * kTicksPerBar;
    const double frac = pauseMusic_ - barStart;
    if (bandDrives_ && band_->view().running) {
        const double heard = band_->view().heardTick;
        const double line = std::ceil((heard + 4.0 * kTicksPerBeat - frac + 8.0) / kTicksPerBar) * kTicksPerBar;
        countInEnd_ = line + frac;
        resumeOffset_ = line - barStart;
    } else {
        countInClockS_ = 0.0;
        countInEnd_ = 4.0 * kTicksPerBeat;  // the simulated clock: four beats of its own
        resumeOffset_ = clock_.offset();
    }
    if (band_) band_->playSfx(audio::Sfx::UiConfirm);
}

void Play::endRun(bool died) {
    // The run's numbers go to the high scores (a quit run counts as well; it was played). M4: this device's player's.
    const PlayerState& p = world_->player(seat_);
    HighScore h;
    h.score = p.score;
    h.track = runTrackMax_;
    h.bestChain = p.bestChain;
    h.seconds = static_cast<u32>(runSeconds_);
    h.kills = p.kills;
    h.setComplete = runSetComplete_;  // M6: a gold star in the table
    const u64 before = scores_.best();
    finalScore_ = p.score;
    newBest_ = p.score > 0 && p.score > before;
    // (M6: pictures and fixed-step runs write nothing to the device's files: they would replace a player's scores and settings)
    const bool keep = !opt_.picture[0] && !opt_.fixedStep;
    if (p.score > 0 && scores_.add(h) >= 0 && keep) saveHighScores(scores_);
    // M6 (screens.md "What ended it"): what took the last cell, as the game names it; nothing for a run quit or finished.
    runEndedBy_[0] = 0;
    if (died) causeName(p.lastHitCause, runEndedBy_, sizeof runEndedBy_);
    if (settings_.runsEnded < 0xFFFFFFFFu) ++settings_.runsEnded;  // (the hints' co-op rule)
    if (keep) saveSettings(settings_);
}

void Play::returnFromSync() {
    // The sync screen restarted the band's song: the run (paused, or the title's) continues against the new
    // timeline from the next bar line on.
    if (!band_) return;
    band_->setMode(audio::Mode::Conductor);
    plannedLine_ = -1e9;
    holdPosted_ = true;
    if (screen_ == Screen::Settings && settingsBack_ == Screen::Paused) return;  // RESUME counts in against it
    // The title's attract run: re-anchored once the band plays again (frame()).
    pauseMusic_ = music1_;
    realign_ = true;
    countIn_ = true;
    countInEnd_ = 1e18;
}

// ---------------------------------------------------------------------------------------------- input

void Play::uiPointer(Vec2 pos, bool down) {
    uiPtr_.pos = pos;
    if (down && !uiPtr_.down) uiPtr_.pressed = true;
    if (!down && uiPtr_.down) uiPtr_.released = true;
    uiPtr_.down = down;
}

bool Play::key(int k, bool down) {
    const bool dead = !world_->player(seat_).alive;
    if (coop() && k == SDLK_P) {
        // M4: any player may pause; it holds for everyone on the next beat (GAME_DESIGN 10). P again resumes.
        if (down) session_->requestPause(!session_->paused() && !session_->resuming());
        return true;
    }
    if (flow_ && screen_ == Screen::CoopJoin && !session_) {
        // M4 stage B: the join screen takes a room code from a keyboard too (its sixteen symbols; P is one of them, so
        // only Escape and Back go back); the address mode's digits come as text (textInput). Backspace deletes.
        if (down) {
            if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) keyBack_ = true;
            else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) keyPrimary_ = true;
            else if (k == SDLK_BACKSPACE) {
                if (typingIp_) {
                    const size_t len = std::strlen(ipTyped_);
                    if (len) ipTyped_[len - 1] = 0;
                } else if (codeLen_) {
                    codeTyped_[--codeLen_] = 0;
                }
            } else if (!typingIp_ && codeLen_ < 4) {
                char c = 0;
                if (k >= static_cast<int>(SDLK_A) && k <= static_cast<int>(SDLK_Z)) c = static_cast<char>('A' + (k - static_cast<int>(SDLK_A)));
                if (k >= static_cast<int>(SDLK_0) && k <= static_cast<int>(SDLK_9)) c = static_cast<char>('0' + (k - static_cast<int>(SDLK_0)));
                for (u32 i = 0; c && i < 16; ++i)
                    if (net::kRoomAlphabet[i] == c) {
                        codeTyped_[codeLen_++] = c;
                        codeTyped_[codeLen_] = 0;
                        if (band_) band_->playSfx(audio::Sfx::UiTick);
                        break;
                    }
            }
        }
        return k != SDLK_F1;
    }
    if (flow_) {
        // The screens: Enter / Space is the obvious control, Escape (Back) goes back or pauses.
        const bool primary = k == SDLK_RETURN || k == SDLK_KP_ENTER;
        const bool back = k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_P;
        if (screen_ == Screen::Run) {
            if (down && back) {
                pauseRun();
                return true;
            }
        } else {
            if (down && primary) keyPrimary_ = true;
            if (down && back) keyBack_ = true;
            if (primary || back) return true;
            if (screen_ != Screen::Paused || countIn_) return k != SDLK_F1;
        }
        if (dead) return true;  // the game-over screen takes over
    }
    switch (k) {
        case SDLK_W: keys_[0] = down; return true;
        case SDLK_A: keys_[1] = down; return true;
        case SDLK_S: keys_[2] = down; return true;
        case SDLK_D: keys_[3] = down; return true;
        case SDLK_UP: keys_[4] = down; return true;
        case SDLK_LEFT: keys_[5] = down; return true;
        case SDLK_DOWN: keys_[6] = down; return true;
        case SDLK_RIGHT: keys_[7] = down; return true;
        case SDLK_E:
            keyNova_ = down;
            if (down && dead) retryRequest_ = true;
            return true;
        case SDLK_SPACE:
            keyDash_ = down;
            if (down && dead) retryRequest_ = true;
            return true;
        case SDLK_RETURN:
            if (down && dead) retryRequest_ = true;
            return true;
        default: return false;
    }
}

void Play::mouseButton(int button, bool down, Vec2 pos) {
    mousePos_ = pos;
    mouseSeen_ = true;
    if (flow_ && screen_ != Screen::Run) {
        if (button == SDL_BUTTON_LEFT) uiPointer(pos, down);
        mouseFire_ = mouseNova_ = false;
        return;
    }
    if (flow_ && down && button == SDL_BUTTON_LEFT) {
        float r = 0.0f;
        const Vec2 c = pauseButtonCentre(layoutW_, layoutH_, insetMin_, insetMax_, r);
        if (lengthSq(pos - c) <= 4.0f * r * r) {
            pauseRun();
            return;
        }
    }
    if (button == SDL_BUTTON_LEFT) mouseFire_ = down;
    if (button == SDL_BUTTON_RIGHT) mouseNova_ = down;
    if (!flow_ && down && !world_->player(seat_).alive) retryRequest_ = true;
}

void Play::mouseMove(Vec2 pos) {
    mousePos_ = pos;
    mouseSeen_ = true;
    if (flow_ && screen_ != Screen::Run) uiPointer(pos, uiPtr_.down);
}

void Play::touchDown(u64 id, Vec2 pos) {
    if (flow_ && screen_ != Screen::Run) {
        // The screens: the first finger is the pointer (nothing here is a stick).
        if (uiTouch_ == ~0ull) {
            uiTouch_ = id;
            uiPointer(pos, true);
        }
        return;
    }
    if (flow_) {
        float r = 0.0f;
        const Vec2 c = pauseButtonCentre(layoutW_, layoutH_, insetMin_, insetMax_, r);
        if (lengthSq(pos - c) <= 4.0f * r * r) {
            pausePressed_ = true;
            pauseRun();
            return;
        }
    }
    if (!world_->player(seat_).alive) {
        if (!flow_) retryRequest_ = true;  // "tap to retry" (benchmarks, --bot)
        return;
    }
    touch_.down(id, pos);
}

void Play::touchMove(u64 id, Vec2 pos) {
    if (id == uiTouch_) uiPointer(pos, true);
    touch_.move(id, pos);
}

void Play::touchUp(u64 id) {
    if (id == uiTouch_) {
        uiPointer(uiPtr_.pos, false);
        uiTouch_ = ~0ull;
    }
    pausePressed_ = false;
    touch_.up(id);
}

void Play::padAxis(int axis, float v) {
    switch (axis) {
        case SDL_GAMEPAD_AXIS_LEFTX: padMove_.x = v; break;
        case SDL_GAMEPAD_AXIS_LEFTY: padMove_.y = v; break;
        case SDL_GAMEPAD_AXIS_RIGHTX: padAim_.x = v; break;
        case SDL_GAMEPAD_AXIS_RIGHTY: padAim_.y = v; break;
        default: break;
    }
}

void Play::padButton(int button, bool down) {
    switch (button) {
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: padNova_ = down; break;  // GAME_DESIGN 2: RB / R1
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: padDash_ = down; break;   // LB / L1
        case SDL_GAMEPAD_BUTTON_SOUTH:
        case SDL_GAMEPAD_BUTTON_START:
            if (down && !world_->player(seat_).alive) retryRequest_ = true;
            break;
        default: break;
    }
}

void Play::padRemoved() {
    padMove_ = padAim_ = Vec2{};
    padNova_ = padDash_ = false;
}

// ---------------------------------------------------------------------------------------------- pictures

// Scripted scenes for the lead's review strips (docs/PROGRESS.md M3): fixed 1/60 s steps, no band, the
// spawner off; each places its enemies, drives the ship and names the moment the strip starts.
namespace {
enum ScriptFlag : u32 { kPressed = 1, kSpawned = 2 };
}

void Play::scriptSetup() {
    if (scriptSetupM6()) return;
    if (scriptSetupM5()) return;
    if (scriptSetupB()) return;
    World& w = *world_;
    const SpeciesTable& s = *species_;
    const char* p = opt_.picture;
    const auto put = [&](Family f, u32 tier, u32 variant, Vec2 at, float hp) {
        const i32 i = w.spawnEnemy(SpeciesTable::index(f, tier, variant), at, angleOf(Vec2{} - at), 0.0);
        if (i >= 0 && hp > 0.0f) w.setEnemyHp(static_cast<u32>(i), hp);
        return i;
    };
    if (is(p, "fire")) {
        for (u32 k = 0; k < 5; ++k) put(Family::Drifter, 2, kVariantA, Vec2{430.0f + 55.0f * k, -120.0f + 60.0f * k}, 60.0f);
    } else if (is(p, "kill_small")) {
        put(Family::Drifter, 1, kVariantA, Vec2{300.0f, 0.0f}, 0.0f);
    } else if (is(p, "kill_medium")) {
        put(Family::Drifter, 3, kVariantA, Vec2{300.0f, 0.0f}, 1.0f);
    } else if (is(p, "kill_large")) {
        put(Family::Ram, 5, kVariantA, Vec2{340.0f, 0.0f}, 1.0f);
    } else if (is(p, "shards")) {
        for (u32 k = 0; k < 44; ++k) {
            const float x = 110.0f + 300.0f * static_cast<float>(k % 11) / 10.0f;
            const float y = -70.0f + 140.0f * static_cast<float>(k / 11) / 3.0f;
            w.spawnShard(Vec2{x, y}, Vec2{}, 0);
        }
    } else if (is(p, "hit")) {
        put(Family::Hound, 1, kVariantA, Vec2{170.0f, 20.0f}, 0.0f);
        w.playerMut(0).chain = 24;  // so the halving shows
    } else if (is(p, "nova") || is(p, "nova_front")) {
        for (u32 k = 0; k < 26; ++k) {
            const float a = kTau * static_cast<float>(k) / 26.0f;
            const float r = 280.0f + 160.0f * static_cast<float>(k % 3) / 2.0f;
            put(k % 4 == 0 ? Family::Hound : Family::Drifter, 1 + k % 2, kVariantA, Vec2{std::cos(a) * r * 1.6f, std::sin(a) * r * 0.75f}, 0.0f);
        }
        put(Family::Sentry, 1, kVariantB, Vec2{-520.0f, 200.0f}, 100.0f);
        put(Family::Sentry, 1, kVariantB, Vec2{560.0f, -210.0f}, 100.0f);
    } else if (is(p, "sentry")) {
        put(Family::Sentry, 1, kVariantB, Vec2{380.0f, -90.0f}, 100.0f);
    }
    (void)s;
}

void Play::scriptInput(StickInput& in) {
    const World& w = *world_;
    const char* p = opt_.picture;
    const u32 t = scriptTick_;
    const double music = music1_;
    const float tpt = clock_.ticksPerSecond() * kDt;  // musical ticks per sim tick
    in.assist = false;
    if (scriptInputM6(in)) return;
    if (scriptInputM5(in)) return;
    if (scriptInputB(in)) return;
    if (is(p, "fire")) {
        // A stream into the middle one of the five (they take 60 shots each), so the shots land.
        const Enemies& e = w.enemies();
        if (t >= 10 && e.count >= 3) {
            in.aim = Vec2{e.x[2], e.y[2]} - controller_.pose().pos;
            in.aimActive = true;
        }
    } else if (is(p, "kill_small") || is(p, "kill_medium") || is(p, "kill_large")) {
        // Aim at the target where it is (it wanders); fire from tick 20.
        const Enemies& e = w.enemies();
        if (t >= 20 && e.count) {
            in.aim = Vec2{e.x[0], e.y[0]} - controller_.pose().pos;
            in.aimActive = true;
        }
    } else if (is(p, "shards")) {
        if (t >= 10) in.move = Vec2{1.0f, 0.0f};
    } else if (is(p, "nova") || is(p, "nova_front")) {
        // Press seven frames before a beat, once the Sentries are firing.
        const double toBeat = std::ceil(music / kTicksPerBeat) * kTicksPerBeat - music;
        if (t > 200 && !(scriptFlags_ & kPressed) && toBeat > 6.5 * tpt && toBeat <= 7.5 * tpt) {
            in.nova = true;
            scriptFlags_ |= kPressed;
        }
        in.move = Vec2{0.25f, -0.1f};
    } else if (is(p, "dash") || is(p, "perfect_dash")) {
        in.move = Vec2{0.8f, -0.6f};
        const double inBeat = std::fmod(music, kTicksPerBeat);
        const double off = inBeat < 48.0 ? inBeat : kTicksPerBeat - inBeat;
        const bool perfect = is(p, "perfect_dash");
        if (t > 60 && !(scriptFlags_ & kPressed) && (perfect ? off < 0.6 * tpt : off > 0.3 * kTicksPerBeat)) {
            in.dash = true;
            scriptFlags_ |= kPressed;
        }
    } else if (is(p, "warp") || is(p, "warp_land")) {
        // Spawn on a beat: a hound pack, a Ram and a Shoal school warp in for exactly two beats.
        if (t > 30 && !(scriptFlags_ & kSpawned) && crossed(musicPrev_, music, kTicksPerBeat)) {
            World& wm = *world_;
            const double beat = std::floor(music / kTicksPerBeat) * kTicksPerBeat;
            const double land = beat + kWarpBeats * kTicksPerBeat;
            const auto at = [&](Family f, u32 tier, Vec2 pos) { wm.spawnEnemy(SpeciesTable::index(f, tier, kVariantA), pos, angleOf(Vec2{} - pos), land); };
            for (u32 k = 0; k < 3; ++k) at(Family::Hound, 1, Vec2{360.0f + 40.0f * k, -150.0f + 30.0f * k});
            at(Family::Ram, 1, Vec2{-380.0f, 120.0f});
            for (u32 k = 0; k < 8; ++k) at(Family::Shoal, 1, Vec2{-200.0f + 18.0f * (k % 4), -200.0f + 18.0f * (k / 4)});
            at(Family::Drifter, 2, Vec2{120.0f, 230.0f});
            scriptFlags_ |= kSpawned;
            // "warp": the first eight frames (the rings at their largest); "warp_land": the last frames of the
            // warp-in and the landing, from five frames before it. (Spawned between ticks, so no WarpStart
            // event reaches scriptEvents(): the moment is known here.)
            const i64 toLand = static_cast<i64>(std::llround((land - music) / tpt));
            captureFrom_ = static_cast<i64>(frameNo_) + (is(p, "warp") ? 0 : 1 + toLand - 5);
        }
    }
    (void)w;
}

void Play::scriptEvents() {
    if (scriptEventsM6()) return;
    if (scriptEventsM5()) return;
    if (scriptEventsB()) return;
    if (captureFrom_ >= 0) return;
    const World& w = *world_;
    const char* p = opt_.picture;
    const float tpt = clock_.ticksPerSecond() * kDt;
    const i64 now = static_cast<i64>(frameNo_);
    if (is(p, "fire")) {
        if (scriptTick_ == 80) captureFrom_ = now + 1;
        return;
    }
    for (u32 k = 0; k < w.eventCount(); ++k) {
        const Event& e = w.events()[k];
        if ((is(p, "kill_small") || is(p, "kill_medium") || is(p, "kill_large")) && e.type == EventType::Kill) captureFrom_ = now;
        if (is(p, "shards") && e.type == EventType::Pickup) captureFrom_ = now;
        if (is(p, "hit") && e.type == EventType::Hit) captureFrom_ = now;
        if (is(p, "nova") && e.type == EventType::NovaPress) captureFrom_ = now;
        if (is(p, "nova_front") && e.type == EventType::NovaBlast) captureFrom_ = now;
        if ((is(p, "dash") || is(p, "perfect_dash")) && e.type == EventType::Dash) captureFrom_ = now;
        if (is(p, "sentry") && e.type == EventType::Telegraph) {
            // The end of the glint (one beat) and the first volley.
            const i64 toFire = static_cast<i64>(std::llround((e.tick - w.music()) / tpt));
            captureFrom_ = now + toFire - 4;
        }
        if (captureFrom_ >= 0) {
            RF_LOG("play: picture '%s' captures from frame %lld (event at frame %lld, tick %.1f)", p, static_cast<long long>(captureFrom_),
                   static_cast<long long>(now), w.music());
            return;
        }
    }
}

// ---------------------------------------------------------------------------------------------- M6: first-run hints

void Play::hintsTick(ViewInput& vi) {
    // screens.md "First-run hints (M6)": eleven lines, each once ever on a device, one at a time, at least four bars apart,
    // never in a fight, never over a Trio's words or a boss's hint line, never in co-op for a player who has played a run to
    // its end; a hint whose moment passes waits for the next such moment.
    static const char* const kHints[11] = {"LEFT THUMB MOVES",         "WASD OR LEFT STICK MOVES",    "IT AIMS AND FIRES BY ITSELF",
                                           "RIGHT THUMB AIMS IF YOU LIKE", "FLY INTO ONE AND STAY",   "YOUR RING IS YOUR HEALTH",
                                           "NOVA CLEARS THE SCREEN",   "DASH GOES THROUGH BULLETS",   "GOLD ORBS ARE SHORT POWERS",
                                           "EXTRA LIFE: ONE MORE TRY", "BREAK IT FOR A PRIZE"};
    const World& w = *world_;
    const float now = static_cast<float>(vi.time);
    const float barS = 240.0f / std::fmax(vi.bpm, 30.0f);
    if (forcedHint_ >= 0 && forcedHint_ < 11) {  // (pictures: m6_hint_<k>)
        vi.hint = kHints[forcedHint_];
        vi.hintLevel = 1.0f;
        return;
    }
    if (opt_.picture[0] || opt_.fixedStep) return;  // (pictures show only the hint they ask for, and save nothing)
    const bool running = flow_ && screen_ == Screen::Run;
    if (hintIdx_ >= 0) {
        const float u = (now - hintT_) / barS;  // two bars up, fading over the last beat
        if (u >= 2.0f || u < 0.0f) {
            hintIdx_ = -1;
        } else {
            if (running) {
                vi.hint = kHints[hintIdx_];
                vi.hintLevel = clampf((2.0f - u) * 4.0f, 0.0f, 1.0f);
            }
            return;
        }
    }
    if (!running) return;
    const PlayerState& me = w.player(seat_);
    if (!me.alive || (coop() && settings_.runsEnded > 0)) return;
    const i64 bar = static_cast<i64>(w.phrase().bar);
    if (bar < 1) return;  // M6 (screens.md "Transitions"): a hint due as the run drops in waits for its second bar
    if (bar < lastHintBar_) lastHintBar_ = -100;  // (a new run)
    if (bar < lastHintBar_ + 4 || w.boss().active || now - takenT_ < 1.5f) return;
    const float hw = tuning::kFrameHalfWWu, hh = tuning::kFrameHalfHWu;
    const auto inView = [&](Vec2 p) { return std::fabs(p.x - me.pos.x) <= hw && std::fabs(p.y - me.pos.y) <= hh; };
#if defined(__ANDROID__)
    const bool touch = true;
#else
    const bool touch = touch_.used();
#endif
    const Enemies& e = w.enemies();
    const auto due = [&](u32 k) -> bool {
        switch (k) {
            case 0: return touch && bar <= 1;
            case 1: return !touch && bar <= 1;
            case 2:
                for (u32 i = 0; i < e.count; ++i)
                    if (!(e.flags[i] & kFlagDead) && e.state[i] != kWarping && inView(Vec2{e.x[i], e.y[i]})) return true;
                return false;
            case 3: return touch && (settings_.hintsSeen & (1u << 2)) && (hint2Bar_ < 0 || bar >= hint2Bar_ + 4);
            case 4: return w.trio(seat_).open;
            case 5: return now - hitMomentT_ < barS;
            case 6: {
                if (me.novas == 0) return false;
                u32 n = 0;
                for (u32 i = 0; i < e.count && n < 12; ++i)
                    if (!(e.flags[i] & kFlagDead) && e.state[i] != kWarping && lengthSq(Vec2{e.x[i], e.y[i]} - me.pos) < 300.0f * 300.0f) ++n;
                return n >= 12;
            }
            case 7: {
                const Bullets& b = w.bullets();
                for (u32 i = 0; i < b.count; ++i)
                    if (!b.dead[i] && inView(Vec2{b.x[i], b.y[i]})) return true;
                return false;
            }
            case 8:
            case 9: {
                const BoostPickups& bp = w.boostPickups();
                for (u32 i = 0; i < bp.count; ++i) {
                    if (bp.dead[i] || (coop() && bp.owner[i] != seat_)) continue;
                    if ((bp.type[i] == kPickupExtraLife) != (k == 9)) continue;
                    if (inView(Vec2{bp.x[i], bp.y[i]})) return true;
                }
                return false;
            }
            default: {
                const ArenaBody* bodies = w.arenaBodies();
                for (u32 i = 0; i < World::kArenaBodies; ++i)
                    if (bodies[i].alive && (bodies[i].kind == static_cast<u8>(StructKind::Vault) || bodies[i].kind == kBodyGoldVault) &&
                        inView(bodies[i].pos))
                        return true;
                return false;
            }
        }
    };
    for (u32 k = 0; k < 11; ++k) {
        if (settings_.hintsSeen & (1u << k)) continue;
        if (!due(k)) continue;
        hintIdx_ = static_cast<i32>(k);
        hintT_ = now;
        lastHintBar_ = bar;
        if (k == 2) hint2Bar_ = bar;
        settings_.hintsSeen |= 1u << k;
        saveSettings(settings_);
        RF_LOG("play: hint %u \"%s\"", k, kHints[k]);
        vi.hint = kHints[k];
        vi.hintLevel = 1.0f;
        return;
    }
}
void Play::pulse(int kind) {
    // M6: a haptic pulse for what M5 added (the ring breaking, an Extra Life spent, a boss's death, a mini-boss's arrival),
    // at the HAPTICS setting's strength; never for a weak point's hit (it would buzz all fight).
    if (!bandDrives_ || !band_ || !band_->haptics() || settings_.haptics == 0) return;
    platformHaptic(kind >= 2 ? HapticKind::Heavy : (kind == 1 ? HapticKind::Medium : HapticKind::Light), settings_.haptics == 2);
}
}  // namespace rf
