// M4 stage B: co-op from the title (design/screens.md "Co-op lobby", GAME_DESIGN 10, TECH_SPEC 8.3). CO-OP opens
// the join screen: the rooms heard on the network, the room code's pad, an address typed in, and HOST A ROOM. A
// room is a session the title's world joins (it leaves the attract run); the lobby shows the seats until the host
// starts; the run, the shared pause and the game over follow the session; LEAVE and HOME go back to the title and
// the attract run. The host leaving ends the run on every device (the scores are kept); a client leaving opens its
// seat and the run goes on. The band in co-op (GAME_DESIGN 10): the bed identical on every device, this device's
// player loudest and the others quieter by distance in their fixed timbres, the beacons and the revive's notes.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

#include "core/log.h"
#include "game/play.h"
#include "net/bots.h"
#include "net/lobby.h"
#include "net/wire.h"
#include "sim/tuning.h"
#if defined(__ANDROID__)
#include "platform/android/android_glue.h"
#endif

namespace rf {

using namespace sim;

namespace {
// A dotted-quad address (host byte order; 0 when it is not one).
u32 parseAddress(const char* s) {
    u32 parts[4] = {};
    u32 n = 0, v = 0, digits = 0;
    for (const char* c = s;; ++c) {
        if (*c >= '0' && *c <= '9') {
            v = v * 10u + static_cast<u32>(*c - '0');
            if (++digits > 3 || v > 255u) return 0;
        } else if (*c == '.' || *c == '\0') {
            if (!digits || n >= 4) return 0;
            parts[n++] = v;
            v = digits = 0;
            if (*c == '\0') break;
        } else {
            return 0;
        }
    }
    return n == 4 ? (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3] : 0u;
}
}  // namespace

void Play::openCoop() {
    // TECH_SPEC 8.3 / Android 17: the local network may need the user's leave; one plain screen says why first.
#if defined(__ANDROID__)
    const int state = android::localNetworkState();
#else
    const int state = 0;
#endif
    if (!session_ && screen_ != Screen::CoopJoin && screen_ != Screen::NetPermission) {
        // screens.md "Co-op (M4)": nothing of a run behind the setup screens: the attract run ends here (the world empty
        // and still until the lobby's run or the title's next attract run).
        world_->resetRun(opt_.seed, music1_, 1);
        view_.resetEffects();
        controller_.reset(world_->player(0).pos);
    }
    if ((state == 1 || state == 2 || opt_.coopPermissionScreen) && !permissionAsked_) {
        screen_ = Screen::NetPermission;
        return;
    }
    if (!browser_) browser_ = new (std::nothrow) net::BeaconBrowser;
    if (browser_ && !browser_->open() && !browser_->start(net::WireKind::Udp))
        RF_WARN("net: cannot listen for rooms on UDP %u (error %d)", net::kBeaconUdpPort, browser_->lastError());
#if defined(__ANDROID__)
    android::setMulticastLock(true);  // TECH_SPEC 8.3: while browsing
#endif
    codeLen_ = 0;
    codeTyped_[0] = '\0';
    typingIp_ = false;
    ipTyped_[0] = '\0';
    screen_ = Screen::CoopJoin;
}

static void closeBrowser(net::BeaconBrowser* b) {
    if (b) b->stop();
#if defined(__ANDROID__)
    android::setMulticastLock(false);
#endif
}

bool Play::hostRoom() {
    if (session_) return false;
    closeBrowser(browser_);
    session_ = new (std::nothrow) net::Session;
    if (!session_) return false;
    // The title's world becomes the room's: the attract run ends, the host's director waits for START.
    world_->resetRun(opt_.seed, music1_, 1);
    net::SessionConfig c;
    c.role = NetRole::Host;
    c.seats = kMaxPlayers;
    c.port = net::kGamePort;
    c.wire = net::WireKind::Udp;
    c.sim = opt_.netsim;
    std::snprintf(c.name, sizeof c.name, "%s", opt_.coopName[0] ? opt_.coopName : "P1");
    c.seed = opt_.seed ^ static_cast<u64>(net::wireNowUs());
    c.autoStart = false;
    c.autoRestart = false;
    c.beacon = true;
    if (!session_->start(c, world_, net::contentHash() ^ opt_.contentSalt)) {
        delete session_;
        session_ = nullptr;
        world_->setNet(NetRole::Solo, 1);
        world_->resetRun(opt_.seed, clock_.heardNow(), 1);
        world_->requestRestart();
        std::snprintf(coopMessage_, sizeof coopMessage_, "CANNOT HOST: THE GAME PORT IS IN USE, OR THERE IS NO NETWORK");
        coopMessageT_ = 0.0f;
        RF_WARN("net: could not host a room on port %u", c.port);
        openCoop();
        return false;
    }
    session_->setClock([](void*) { return net::wireNowUs(); }, nullptr);
    if (!opt_.coopMode[0]) roomSetup_ = sim::RunSetup{};  // M8: a room opens in THE SET, TEAM (--coop-mode: as it says)
    session_->setRoomSetup(roomSetup_);
    roomSeen_ = roomSetup_;
    coopFlowActive_ = true;
    coopRunShown_ = coopOver_ = false;
    netRunStarted_ = netSongStarted_ = false;
    seat_ = 0;
    coopMessage_[0] = '\0';
    screen_ = Screen::CoopLobby;
    char code[5];
    net::roomCodeEncode(session_->roomCode(), code);
    RF_LOG("net: hosting a room: code %s (%u.%u.x.x network), port %u", code, net::preferredLocalIPv4() >> 24,
           (net::preferredLocalIPv4() >> 16) & 255u, c.port);
    return true;
}

bool Play::joinRoom(u32 ip, u16 port) {
    if (session_ || !ip) return false;
    closeBrowser(browser_);
    session_ = new (std::nothrow) net::Session;
    if (!session_) return false;
    world_->resetRun(opt_.seed, music1_, 1);
    net::SessionConfig c;
    c.role = NetRole::Client;
    c.port = port ? port : net::kGamePort;
    c.wire = net::WireKind::Udp;
    c.sim = opt_.netsim;
    std::snprintf(c.joinIp, sizeof c.joinIp, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255u, (ip >> 8) & 255u, ip & 255u);
    std::snprintf(c.name, sizeof c.name, "%s", opt_.coopName[0] ? opt_.coopName : "PLAYER");
    c.autoStart = false;
    c.autoRestart = false;
    if (!session_->start(c, world_, net::contentHash() ^ opt_.contentSalt)) {
        delete session_;
        session_ = nullptr;
        world_->setNet(NetRole::Solo, 1);
        world_->resetRun(opt_.seed, clock_.heardNow(), 1);
        world_->requestRestart();
        std::snprintf(coopMessage_, sizeof coopMessage_, "CANNOT REACH %s", c.joinIp);
        coopMessageT_ = 0.0f;
        openCoop();
        return false;
    }
    session_->setClock([](void*) { return net::wireNowUs(); }, nullptr);
    coopFlowActive_ = true;
    coopRunShown_ = coopOver_ = false;
    netRunStarted_ = netSongStarted_ = false;
    coopMessage_[0] = '\0';
    screen_ = Screen::CoopLobby;
    RF_LOG("net: joining %s port %u", c.joinIp, c.port);
    return true;
}

void Play::coopRecordScore() {
    // The run's numbers go to this device's high scores (GAME_DESIGN 10: scores are kept when the host leaves).
    if (!session_ || !netRunStarted_ || coopOver_) return;
    endRun(false);
}

void Play::leaveCoop(const char* message) {
    // Back to the title and its attract run, with a message when the session ended on its own.
    if (session_) {
        if (!coopOver_) coopRecordScore();
        session_->stop();
        delete session_;
        session_ = nullptr;
    }
    delete bots_;
    bots_ = nullptr;
    closeBrowser(browser_);
    world_->setNet(NetRole::Solo, 1);
    world_->resetRun(opt_.seed, clock_.heardNow(), 1);
    world_->requestRestart();
    seat_ = 0;
    netRunStarted_ = netSongStarted_ = false;
    coopFlowActive_ = coopRunShown_ = coopOver_ = false;
    paused_ = false;
    // (audit 2026-11-15: a host that left during its band's re-align published no song in its next room for 12 s)
    hostRealign_ = false;
    hostBandShift_ = 0.0;
    hostRealignOkS_ = hostRealignS_ = 0.0f;
    netServoFromUs_ = netServoAt_ = 0;
    if (bandDrives_ && band_) {
        band_->setRate(1.0f);
        band_->setHold(false);
        // The band back to one player in P1's timbre, no beacon, the bed back.
        for (u8 k = 0; k < kMaxPlayers; ++k) band_->setBeacon(k, false, 0.0f);
        band_->setPlayers(1);
        band_->setPlayerTimbre(0, 0);
        band_->setHostOnly(false);
    }
    bandPlayers_ = 1;
    for (u32 k = 0; k < kMaxPlayers; ++k) {
        bandTimbre_[k] = k == 0 ? 0 : 0xFF;
        bandFire_[k] = 0;
        bandBeacon_[k] = false;
        bandLoadSig_[k] = 0;
        reviveStage_[k] = 0;
    }
    hostOnlyPosted_ = false;
    bedSentBar_ = bedPostedBar_ = 0xFFFFFFFFu;
    netRatePosted_ = 1.0f;
    netHoldUntil_ = -1.0;
    plannedLine_ = -1e9;
    controller_.reset(world_->player(0).pos);
    view_.resetEffects();
    coopMessage_[0] = '\0';
    if (message && *message) {
        std::snprintf(coopMessage_, sizeof coopMessage_, "%s", message);
        coopMessageT_ = 0.0f;
        RF_LOG("net: the session is over: %s", message);
    }
    coopSeated_ = false;
    screen_ = Screen::Title;
    applySettings();
}

void Play::appBackground() {
    // Pause-on-leave (2026-11-14): this app leaves the screen. In a co-op run the room pauses on the next sixteenth and waits
    // for this player (what the session sends now leaves before the main loop stops). Solo (audit 2026-11-15): the run pauses,
    // so a call or a trip home no longer drops the player back into a fight in full swing.
    if (!session_) {
        if (flow_ && screen_ == Screen::Run) pauseRun();
        return;
    }
    if (!session_->running()) return;
    awayAtUs_ = net::wireNowUs();
    session_->goAway(awayAtUs_);
    RF_LOG("net: away: this app left the screen at run bar %u; the room pauses and waits up to %u s", world_->phrase().bar, net::kAwayWaitMs / 1000u);
}

void Play::appForeground() {
    updateWanted_ = true;  // (2026-11-16, the owner: the updater asks again when the app comes back; on the title's next frame)
    if (!session_ || !session_->goneAway()) return;
    session_->comeBack(net::wireNowUs());
    netServoFromUs_ = net::wireNowUs() + 1000000;  // (the band's heard clock settles on the reopened stream's timestamps first)
    RF_LOG("net: back after %.1f s%s", static_cast<double>(session_->lastAwayMs()) * 1e-3,
           session_->state() == net::SessionState::Ended ? ": away too long, the run went on without this device"
                                                          : (session_->host() ? "; the band is brought onto the room's song" : "; the clock settles"));
    touch_.reset();
    mouseFire_ = mouseNova_ = false;
    for (bool& k : keys_) k = false;
    if (session_->host() && session_->state() == net::SessionState::Running) {
        if (bandDrives_ && band_) {
            hostRealign_ = true;
            hostRealignOkS_ = hostRealignS_ = 0.0f;
            netHoldUntil_ = -1.0;
        } else {
            session_->hostReturned();
        }
    }
}

void Play::coopPause(bool on) {
    // GAME_DESIGN 10: any player may pause; it takes hold on the next beat for everyone, the count-in is shared.
    if (!session_ || !netRunStarted_ || coopOver_) return;
    session_->requestPause(on);
    if (band_) band_->playSfx(on ? audio::Sfx::UiTick : audio::Sfx::UiConfirm);
}

void Play::coopFlow() {
    // The screens follow the session (game/play_net.cpp runs it).
    net::Session& s = *session_;
    coopMessageT_ += frameDt_;
    if (s.state() == net::SessionState::Lobby || s.state() == net::SessionState::Running) coopSeated_ = true;
    if (s.state() == net::SessionState::Ended) {
        char msg[96];
        const char* why = s.endReason();
        const bool gone = std::strcmp(why, "the host left") == 0 || std::strcmp(why, "the host is gone") == 0 || std::strcmp(why, "no answer") == 0;
        if (gone && !coopSeated_)
            // Never seated: nobody answered at that address (screens.md's join messages).
            std::snprintf(msg, sizeof msg, "%s", coopJoinBy_ == 1 ? "NO ROOM WITH THAT CODE" : (coopJoinBy_ == 2 ? "NO ROOM AT THAT ADDRESS" : "THAT ROOM IS GONE"));
        else if (gone)
            std::snprintf(msg, sizeof msg, "%s", netRunStarted_ ? "THE HOST LEFT: THE RUN IS OVER. YOUR SCORE IS KEPT" : "THE HOST CLOSED THE ROOM");
        else if (std::strcmp(why, "another version of the game") == 0)
            std::snprintf(msg, sizeof msg, "THAT ROOM RUNS ANOTHER VERSION OF RAVEFORM: UPDATE BOTH PHONES");
        else if (std::strcmp(why, "other game data") == 0)
            std::snprintf(msg, sizeof msg, "THAT ROOM'S GAME DATA DIFFERS FROM THIS ONE'S: UPDATE BOTH PHONES");
        else if (std::strcmp(why, "the run is full") == 0)
            std::snprintf(msg, sizeof msg, "THAT ROOM IS FULL");
        else if (std::strcmp(why, "the run has started") == 0)
            std::snprintf(msg, sizeof msg, "THAT ROOM'S RUN HAS STARTED: JOIN WHEN IT IS BACK IN ITS LOBBY");
        else
            std::snprintf(msg, sizeof msg, "THE CONNECTION WAS LOST");
        RF_LOG("net: the session ended (%s)", why);
        if (std::strcmp(why, "away too long") == 0) {
            // Pause-on-leave: this app was off the screen past the room's wait (net::kAwayWaitMs): the others went on.
            homeLeaveCoop(s.host() ? "AWAY TOO LONG: THE ROOM CLOSED" : "AWAY TOO LONG: THE RUN WENT ON WITHOUT YOU");
            return;
        }
        if (gone && coopSeated_) {
            // c9218dc (screens.md "Every other way in or out of a run"): a peer whose host has left goes home, the host's message
            // under PLAY for two bars (its score kept: leaveCoop).
            homeLeaveCoop("THE HOST LEFT");
            return;
        }
        leaveCoop(msg);
        openCoop();  // back to the join screen, the message on it (screens.md: the join screen's message line)
        return;
    }
    // M8 (design/modes/screens.md "The podium": LOBBY): the host took the room back to its lobby: every device's run ends there,
    // the room standing, and its next run starts from it as the first did.
    if (coopRunShown_ && s.state() == net::SessionState::Lobby) {
        RF_LOG("net: the room is back in its lobby (seats, crowns and mode kept): crowns %u %u %u %u", s.crowns(0), s.crowns(1), s.crowns(2), s.crowns(3));
        coopRunShown_ = coopOver_ = false;
        netRunStarted_ = false;
        resultPending_ = false;
        backFromRun_ = true;
        coopAutoT_ = 0.0f;
        screen_ = Screen::CoopLobby;
        trans_ = Trans::None;
        if (band_) band_->playSfx(audio::Sfx::UiBack);
        return;
    }
    // The run began (the lobby gives way); a new one after all were down (the host's RETRY) comes as a Restart.
    if (netRunStarted_ && !coopRunShown_) {
        coopRunShown_ = true;
        coopOver_ = false;
        screen_ = Screen::Run;
        runSeconds_ = 0.0f;
        if (band_) band_->playSfx(audio::Sfx::UiConfirm);
    }
    // The shared pause (GAME_DESIGN 10): it holds on the next beat for everyone; the count-in is shared.
    if (coopRunShown_ && !coopOver_) {
        const bool held = s.paused() || s.resuming();
        if (held && screen_ == Screen::Run) {
            screen_ = Screen::Paused;
            touch_.reset();
            mouseFire_ = mouseNova_ = false;
            for (bool& k : keys_) k = false;
            applySettings();
        }
        // The review's fixes (H6, 2026-11-15; screens.md "Nobody is dropped into a fight without the count"): while the shared
        // count-in runs, a device in SETTINGS or CREDITS opened from the pause is brought back to the pause screen (its settings
        // saved, as DONE does), so it sees the 3-2-1.
        if (s.resuming() && screen_ != Screen::Paused && underPauseMenu()) {
            if (screen_ == Screen::Settings) saveSettings(settings_);
            screen_ = Screen::Paused;
            applySettings();
        }
        if (!held && underPauseMenu()) {
            screen_ = Screen::Run;
            applySettings();
        }
    }
    // (audit 2026-11-15) Every ship down while a pause was coming (or one a client asked for after): the game over is up and the
    // run is held on the pause's tick, which the host's RETRY could never get past (its drop-in waited for good, HOME disabled).
    // The host lifts it; its count-in runs behind the game over.
    if (coopOver_ && s.host() && s.paused() && !s.resuming()) s.requestPause(false);
    if (coopRunShown_ && screen_ == Screen::Run && !coopOver_) runSeconds_ += frameDt_;
    if (screen_ == Screen::GameOver) overS_ += frameDt_;
    // --coop-start [N] (tests, pictures, device runs): the host starts once N are seated and ready, and goes again
    // three seconds after all are down.
    if (opt_.coopStart && s.host()) {
        u32 seated = 0;
        for (u32 k = 0; k < kMaxPlayers; ++k) seated += (s.lobbySeats() >> k) & 1u;
        if (s.state() == net::SessionState::Lobby && seated >= opt_.coopStart && s.canStart() && !backFromRun_) s.requestStart();  // (not again after LOBBY)
        // (M8 versus: a REMATCH once the podium has stood its two bars and four seconds; never with --coop-back-lobby)
        const float podiumWait = 2.0f * 240.0f / std::fmax(static_cast<float>(world_->bpm()), 30.0f) + 4.0f;
        if (coopOver_ && overS_ > (world_->setup().versus ? podiumWait : 3.0f) && !(opt_.coopBackLobby && matchesEnded_ >= 2)) s.requestRetry();
    }
    coopTestStep();
}

void Play::coopAutoStep() {
    // --coop host | join[:code|:ip|:list]: what a tap on the title would do, from the command line.
    if (!flow_ || !opt_.coopAuto || coopAutoDone_) return;
    if (screen_ == Screen::Notice) {
        screen_ = Screen::Title;  // (the notice is not saved as seen)
        return;
    }
    if (screen_ == Screen::Title && !session_) {
        if (opt_.coopAuto == 1 && !opt_.coopPermissionScreen) {
            hostRoom();
            coopAutoDone_ = true;
            return;
        }
        openCoop();
        if (screen_ == Screen::NetPermission || !opt_.coopTarget[0]) coopAutoDone_ = true;
        return;
    }
    if (screen_ != Screen::CoopJoin || session_) return;
    coopAutoT_ += frameDt_;
    const char* target = opt_.coopTarget;
    if (std::strcmp(target, "list") == 0) {
        // The first room heard that can be joined.
        for (u32 k = 0; browser_ && k < browser_->count(); ++k) {
            const net::BeaconInfo& b = browser_->room(k);
            if (!b.compatible || b.state != net::kBeaconLobby || b.players >= b.seats || coopAutoT_ < 1.5f) continue;
            RF_LOG("net: --coop join:list: joining the room heard from %u.%u.%u.%u", b.address >> 24, (b.address >> 16) & 255u,
                   (b.address >> 8) & 255u, b.address & 255u);
            coopAutoDone_ = true;
            coopJoinBy_ = 0;
            joinRoom(b.address, b.port);
            return;
        }
        if (coopAutoT_ > 20.0f) {
            RF_WARN("net: --coop join:list: no room heard in 20 s");
            coopAutoDone_ = true;
        }
        return;
    }
    u16 low = 0;
    if (net::roomCodeDecode(target, low)) {
        // Typed on the pad, one key a fifth of a second (the picture shows it), then JOIN as the button would.
        const u32 want = static_cast<u32>(coopAutoT_ / 0.2f);
        while (codeLen_ < 4 && codeLen_ < want) {
            codeTyped_[codeLen_] = static_cast<char>(target[codeLen_] >= 'a' && target[codeLen_] <= 'z' ? target[codeLen_] - 32 : target[codeLen_]);
            codeTyped_[++codeLen_] = '\0';
        }
        if (coopAutoT_ < 2.5f) return;
        coopAutoDone_ = true;
        coopJoinBy_ = 1;
        const net::BeaconInfo* b = browser_ ? browser_->byCode(low) : nullptr;
        const u32 own = net::preferredLocalIPv4();
        if (b) joinRoom(b->address, b->port);
        else if (own) joinRoom(net::roomCodeHost(low, own), net::kGamePort);
        else RF_WARN("net: --coop join:%s: no network address of this device", target);
        return;
    }
    if (std::strcmp(opt_.coopShot, "address") == 0) {
        coopAutoDone_ = true;  // (the picture types it: coopScreenShot)
        return;
    }
    const u32 ip = parseAddress(target);
    coopAutoDone_ = true;
    coopJoinBy_ = 2;
    if (ip) joinRoom(ip, net::kGamePort);
    else RF_WARN("net: --coop join:%s is neither a room code nor an address", target);
}

void Play::coopTestStep() {
    // --coop-down-at, --coop-pause-at (tests and pictures) and --coop-shot's moments.
    if (!session_) return;
    net::Session& s = *session_;
    World& w = *world_;
    if (netRunStarted_ && !coopOver_) {
        if (opt_.coopDownAt >= 0.0f && !coopDowned_ && runSeconds_ >= opt_.coopDownAt) {
            // This device's ship goes down as in play: a bullet where it is takes its last cell (a hit: a cross on the set
            // list), the next one, once it can be hurt again, downs it (the world takes both as any hit).
            PlayerState& pl = w.playerMut(seat_);
            if (!pl.alive || pl.down) {
                coopDowned_ = true;
            } else {
                // First a hit on the last cell, then (the cells kept at none, so Regen or a Cell cannot refill it) the hit
                // that downs it once it can be hurt again. Each is a hit inside the tick, as from a bullet.
                if (coopHitOnce_) pl.cells = 0;
                else if (pl.cells > 1) pl.cells = 1;
                if (!pl.invulnerable()) {
                    if (coopHitOnce_) {
                        RF_LOG("net: --coop-down-at %.1f s: this ship is downed", static_cast<double>(opt_.coopDownAt));
                        coopDowned_ = true;
                    }
                    w.hitNextTick(seat_);
                    coopHitOnce_ = true;
                }
            }
        }
        if (opt_.coopQuitAt >= 0.0f && runSeconds_ >= opt_.coopQuitAt) {
            RF_LOG("net: --coop-quit-at %.1f s: this device leaves the room", static_cast<double>(opt_.coopQuitAt));
            opt_.coopQuitAt = -1.0f;
            leaveCoop("");
            return;
        }
        if (opt_.coopPauseAt >= 0.0f && !coopPaused_ && runSeconds_ >= opt_.coopPauseAt) {
            coopPause(true);
            coopPaused_ = true;
            coopShotT_ = 0.0f;
        }
        if (coopPaused_ && !coopResumed_ && s.paused() && !s.resuming()) {
            coopShotT_ += frameDt_;
            if (coopShotT_ > 3.0f) {
                coopPause(false);
                coopResumed_ = true;
            }
        }
    }
    if (!opt_.coopShot[0] || coopShotReady_) return;
    const char* shot = opt_.coopShot;
    const auto is = [&](const char* name) { return std::strcmp(shot, name) == 0; };
    u32 seated = 0, ready = 0;
    for (u32 k = 0; k < kMaxPlayers; ++k) {
        seated += (s.lobbySeats() >> k) & 1u;
        ready += (s.lobbyReady() >> k) & 1u;
    }
    if (screen_ == Screen::CoopLobby) {
        coopAutoT_ += frameDt_;
        const u32 want = is("lobby") ? 1u : (is("lobby2") ? 2u : (is("lobby3") ? 3u : (is("lobby4") ? 4u : 99u)));
        if (want <= 4 && ready >= want && seated >= want && coopAutoT_ > 2.0f) coopShotReady_ = true;
        // M8: the room's MODES (the host's MODE pill) once two are seated and ready; the picture when it is at rest.
        if (is("modes") && s.host() && ready >= 2 && seated >= 2 && coopAutoT_ > 2.0f && trans_ == Trans::None) openModes(Screen::CoopLobby);
    }
    if (is("modes") && screen_ == Screen::Modes && trans_ == Trans::None) {
        shotWaitS_ += frameDt_;
        if (shotWaitS_ > 1.0f) coopShotReady_ = true;
    }
    if (is("beacon") || is("revive")) {
        // The beacon: a teammate's, on this screen (well inside the frame around this ship) once the shatter of its ship
        // has gone, a second after it went down, and before anyone revives it. The revive: half filled.
        const PlayerState& me = w.player(seat_);
        bool down = false;
        for (u32 q = 0; q < kMaxPlayers; ++q) {
            const PlayerState& pl = w.player(q);
            if (!pl.present || !pl.down) continue;
            if (is("beacon") && q != seat_ && pl.reviveTicks <= 0.0) {
                down = true;
                if (beaconDownS_ > 1.0f && std::fabs(pl.pos.x - me.pos.x) < 0.6f * tuning::kFrameHalfWWu &&
                    std::fabs(pl.pos.y - me.pos.y) < 0.6f * tuning::kFrameHalfHWu)
                    coopShotReady_ = true;
            }
            if (is("revive") && pl.reviveTicks >= 0.45 * tuning::kReviveBeats * kTicksPerBeat) coopShotReady_ = true;
        }
        beaconDownS_ = down ? beaconDownS_ + frameDt_ : 0.0f;
    }
    if (is("beacon_off")) {
        // A downed teammate's beacon off this screen (its chevron with the ring at the tip), half a second after it went
        // down, before anyone reaches it.
        const PlayerState& me = w.player(seat_);
        bool down = false;
        for (u32 q = 0; q < kMaxPlayers; ++q) {
            const PlayerState& pl = w.player(q);
            if (q == seat_ || !pl.present || !pl.down || pl.reviveTicks > 0.0) continue;
            down = true;
            if (beaconDownS_ > 0.5f && me.alive &&
                (std::fabs(pl.beacon.x - me.pos.x) > 1.15f * tuning::kFrameHalfWWu || std::fabs(pl.beacon.y - me.pos.y) > 1.15f * tuning::kFrameHalfHWu))
                coopShotReady_ = true;
        }
        beaconDownS_ = down ? beaconDownS_ + frameDt_ : 0.0f;
    }
    if (is("pause_menu")) {
        // The shared pause, a second and a half after it held (before anyone resumes).
        shotWaitS_ = s.paused() && !s.resuming() && screen_ == Screen::Paused ? shotWaitS_ + frameDt_ : 0.0f;
        if (shotWaitS_ > 1.5f) coopShotReady_ = true;
    }
    if (is("pause") && s.resuming()) {
        // Two beats into the shared count-in.
        const double left = s.resumeSong() - s.songAt(s.hostNow());
        if (left < 2.0 * kTicksPerBeat) coopShotReady_ = true;
    }
    if ((is("over") || is("over_client")) && screen_ == Screen::GameOver && overS_ > 2.6f) coopShotReady_ = true;
    {
        // M8: the podium (its table counting, half way through its first bar; and at rest, its controls up) and the lobby after LOBBY.
        const float barS = 240.0f / std::fmax(static_cast<float>(w.bpm()), 30.0f);
        const bool podium = screen_ == Screen::GameOver && w.setup().versus && runCleared_;
        if (is("podium_table") && podium && podiumS_ > 0.55f * barS) coopShotReady_ = true;
        if ((is("podium") || is("podium_draw")) && podium && podiumS_ > 1.6f * barS) coopShotReady_ = true;
        if (is("lobby_back") && backFromRun_ && screen_ == Screen::CoopLobby && coopAutoT_ > 1.5f) coopShotReady_ = true;
        // HEADLINERS versus: the race bar eight seconds into a fight from act 2 on; ACT n: Pn half a bar after it came.
        const sim::BossState& b = w.boss();
        if (is("race") && w.setup().mode == sim::Mode::Headliners && w.phrase().track >= 2 && b.active && !b.dying && !sim::isMiniBoss(b.kind) &&
            netRunStarted_ && w.music() > b.fightTick + 8.0 * w.ticksPerSecond())
            coopShotReady_ = true;
        if (is("act_won") && actWonSeat_ < kMaxPlayers && static_cast<float>(tickEnd_) - actWonT_ > 0.5f * barS) coopShotReady_ = true;
    }
    if (is("hud") && netRunStarted_ && runSeconds_ > 30.0f) coopShotReady_ = true;
}

void Play::coopScreenShot() {
    // --coop-shot's moments on the screens before a session: the join screen with a room heard (or after 6 s), the code
    // typed, the local network's screen, a refusal's message.
    if (!opt_.coopShot[0] || coopShotReady_ || session_) return;
    const char* shot = opt_.coopShot;
    const auto is = [&](const char* name) { return std::strcmp(shot, name) == 0; };
    // c9218dc: a peer whose host has gone comes home, THE HOST LEFT under PLAY (the picture a beat after it shows).
    if (is("message") && screen_ == Screen::Title && titleNote_[0] && titleNoteAt_ >= 0.0 && transNow() > titleNoteAt_ + kTicksPerBeat) {
        coopShotReady_ = true;
        return;
    }
    if (screen_ == Screen::CoopJoin || screen_ == Screen::NetPermission) {
        coopShotT_ += frameDt_;
    } else {
        coopShotT_ = 0.0f;
        return;
    }
    if (is("join") && screen_ == Screen::CoopJoin && ((browser_ && browser_->count() && coopShotT_ > 2.0f) || coopShotT_ > 6.0f)) coopShotReady_ = true;
    if (is("code") && screen_ == Screen::CoopJoin && codeLen_ == 4 && coopShotT_ > 0.5f) coopShotReady_ = true;
    if (is("permission") && screen_ == Screen::NetPermission && coopShotT_ > 1.0f) coopShotReady_ = true;
    if (is("message") && screen_ == Screen::CoopJoin && coopMessage_[0] && coopMessageT_ > 1.0f) coopShotReady_ = true;
    if (is("address") && screen_ == Screen::CoopJoin) {
        // The address typed on the keyboard (a key every tenth of a second), then the picture.
        typingIp_ = true;
        const u32 want = static_cast<u32>(coopShotT_ / 0.1f), len = static_cast<u32>(std::strlen(ipTyped_));
        const u32 total = static_cast<u32>(std::strlen(opt_.coopTarget));
        if (len < want && len < total && len + 1 < sizeof ipTyped_) {
            ipTyped_[len] = opt_.coopTarget[len];
            ipTyped_[len + 1] = '\0';
        }
        if (len >= total && coopShotT_ > 0.1f * static_cast<float>(total) + 0.6f) coopShotReady_ = true;
    }
}

void Play::coopBed() {
    // GAME_DESIGN 10: the shared bed is identical on every device. The host decides each bar's intensity a quarter
    // into the bar before it and sends it; every device posts it to its band half a bar ahead (the band plans a bar
    // when its write head reaches the line), a client keeping the last it had when one is late.
    if (!bandDrives_ || !band_ || !session_ || !band_->view().running) return;
    const double heard = band_->view().heardTick;
    const double line = std::floor(heard / kTicksPerBar) * kTicksPerBar;
    const double inBar = (heard - line) / kTicksPerBar;
    const u32 nextBar = world_->barOf(line + kTicksPerBar - netBandOffset_ + 1.0);
    if (session_->host() && inBar >= 0.25 && bedSentBar_ != nextBar) {
        bedLevel_ = static_cast<u8>(clampf(intensity_, 0.0f, 1.0f) * 255.0f + 0.5f);
        session_->sendBed(nextBar, bedLevel_);
        bedSentBar_ = nextBar;
    }
    if (inBar >= 0.5 && bedPostedBar_ != nextBar) {
        u8 level = bedLevel_;
        if (!session_->host() && session_->bedLevel(nextBar, level)) bedLevel_ = level;
        band_->setIntensity(static_cast<float>(bedLevel_) / 255.0f);
        intensityPosted_ = static_cast<float>(bedLevel_) / 255.0f;
        bedPostedBar_ = nextBar;
    }
}

// ---------------------------------------------------------------------------------------------- the band

void Play::coopBand() {
    // GAME_DESIGN 10: this device's player loudest, the others quieter by distance, each in its fixed timbre (P1 saw
    // pluck, P2 square pluck, P3 bell, P4 stab); look.md 6: a downed player's beacon and the revive's notes. Slot 0 of
    // the band is this device's player; the others follow in seat order. Posted only when something changed.
    if (!bandDrives_ || !band_ || !session_) return;
    const World& w = *world_;
    const u8 count = static_cast<u8>(w.playerCount() < 1 ? 1 : (w.playerCount() > kMaxPlayers ? kMaxPlayers : w.playerCount()));
    if (count != bandPlayers_) {
        band_->setPlayers(count);
        bandPlayers_ = count;
    }
    u8 slotOf[kMaxPlayers] = {};
    {
        u8 n = 1;
        for (u32 q = 0; q < kMaxPlayers; ++q) slotOf[q] = q == seat_ ? 0 : n++;
    }
    const PlayerState& me = w.player(seat_);
    for (u32 q = 0; q < kMaxPlayers; ++q) {
        const u8 slot = slotOf[q];
        const PlayerState& pl = w.player(q);
        if (slot < count && bandTimbre_[slot] != q) {
            band_->setPlayerTimbre(slot, static_cast<u8>(q));
            bandTimbre_[slot] = static_cast<u8>(q);
        }
        if (q != seat_ && slot < count) {
            // A teammate's lead: its fire (from its pose), its distance, its Mods and Riffs.
            const u8 fire = pl.present && pl.alive ? pl.fire : 0;
            if (fire != bandFire_[slot]) {
                band_->setPlayerFire(slot, fire);
                bandFire_[slot] = fire;
            }
            const float dist = pl.present ? length(pl.pos - me.pos) : 1e6f;
            if (std::fabs(dist - bandDist_[slot]) > 40.0f) {
                band_->setPlayerDistance(slot, dist);
                bandDist_[slot] = dist;
            }
            // M5: a teammate's whole loadout (audio/cues.h), as this device's own.
            const audio::PlayerLoadout l = bandLoadout(pl);
            const u32 sig = loadoutSignature(l);
            if (sig != bandLoadSig_[slot]) {
                band_->setLoadout(slot, l);
                bandLoadSig_[slot] = sig;
            }
        }
        // The beacon: its player's lead holding the Track's root an octave down, panned by where it is.
        const bool on = pl.present && pl.down && !coopOver_;  // (the game over has no beacons)
        const float pan = clampf((pl.beacon.x - me.pos.x) / 800.0f, -1.0f, 1.0f) * 0.6f;
        if (slot < count && (on != bandBeacon_[slot] || (on && std::fabs(pan - bandBeaconPan_[slot]) > 0.1f))) {
            band_->setBeacon(slot, on, pan);
            bandBeacon_[slot] = on;
            bandBeaconPan_[slot] = pan;
        }
        // The revive's fill: two rising notes, the root then the fifth, on its two beats (the octave as it closes:
        // the Revive event, game/play.cpp).
        const u8 stage = on ? (pl.reviveTicks >= kTicksPerBeat ? 2 : (pl.reviveTicks > 0.0 ? 1 : 0)) : 0;
        if (slot < count && stage > reviveStage_[q]) band_->leadNote(slot, stage == 1 ? 0 : 1, 0.7f, pan);
        reviveStage_[q] = stage;
    }
    // The SOUND setting "music from host only": a client's bed and its teammates' leads are the host's phone's.
    const bool hostOnly = settings_.musicHostOnly && !session_->host();
    if (hostOnly != hostOnlyPosted_) {
        band_->setHostOnly(hostOnly);
        hostOnlyPosted_ = hostOnly;
    }
}

}  // namespace rf
