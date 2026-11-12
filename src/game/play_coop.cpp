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
        RF_LOG("net: back to the title: %s", message);
    }
    screen_ = Screen::Title;
    applySettings();
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
    if (s.state() == net::SessionState::Ended) {
        char msg[96];
        const char* why = s.endReason();
        if (std::strcmp(why, "the host left") == 0 || std::strcmp(why, "the host is gone") == 0)
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
        const bool wasRun = netRunStarted_;
        RF_LOG("net: the session ended (%s)", why);
        leaveCoop(msg);
        if (!wasRun) openCoop();  // a refusal or a lost lobby: back to the join screen, the message on it
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
        if (!held && (screen_ == Screen::Paused || (screen_ == Screen::Settings && settingsBack_ == Screen::Paused))) {
            screen_ = Screen::Run;
            applySettings();
        }
    }
    if (coopRunShown_ && screen_ == Screen::Run && !coopOver_) runSeconds_ += frameDt_;
    if (screen_ == Screen::GameOver) overS_ += frameDt_;
    // --coop-start [N] (tests, pictures, device runs): the host starts once N are seated and ready, and goes again
    // three seconds after all are down.
    if (opt_.coopStart && s.host()) {
        u32 seated = 0;
        for (u32 k = 0; k < kMaxPlayers; ++k) seated += (s.lobbySeats() >> k) & 1u;
        if (s.state() == net::SessionState::Lobby && seated >= opt_.coopStart && s.canStart()) s.requestStart();
        if (coopOver_ && overS_ > 3.0f) s.requestRetry();
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
        const net::BeaconInfo* b = browser_ ? browser_->byCode(low) : nullptr;
        const u32 own = net::preferredLocalIPv4();
        if (b) joinRoom(b->address, b->port);
        else if (own) joinRoom(net::roomCodeHost(low, own), net::kGamePort);
        else RF_WARN("net: --coop join:%s: no network address of this device", target);
        return;
    }
    const u32 ip = parseAddress(target);
    coopAutoDone_ = true;
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
            // This device's ship goes down: its cells gone, a bullet where it is (the world takes the hit as any).
            PlayerState& pl = w.playerMut(seat_);
            if (pl.alive) {
                pl.cells = 0;
                pl.invulS = pl.novaInvulS = 0.0f;
                w.spawnBullet(pl.pos, Vec2{}, 0.0f);
                RF_LOG("net: --coop-down-at %.1f s: this ship is downed", static_cast<double>(opt_.coopDownAt));
            }
            coopDowned_ = true;
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
    }
    if (is("beacon") || is("revive")) {
        for (u32 q = 0; q < kMaxPlayers; ++q) {
            const PlayerState& pl = w.player(q);
            if (!pl.present || !pl.down) continue;
            if (is("beacon") && q != seat_ && pl.reviveTicks <= 0.0) coopShotReady_ = true;
            if (is("revive") && pl.reviveTicks >= 0.45 * tuning::kReviveBeats * kTicksPerBeat) coopShotReady_ = true;
        }
    }
    if (is("pause") && s.resuming()) {
        // Two beats into the shared count-in.
        const double left = s.resumeSong() - s.songAt(s.hostNow());
        if (left < 2.0 * kTicksPerBeat) coopShotReady_ = true;
    }
    if (is("over") && screen_ == Screen::GameOver && overS_ > 2.6f) coopShotReady_ = true;
    if (is("hud") && netRunStarted_ && runSeconds_ > 30.0f) coopShotReady_ = true;
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
            const Loadout& l = pl.loadout;
            const u8 mods[5] = {l.mods[kSplit], l.mods[kDoubleTime], l.mods[kPierce], l.mods[kOvercharge], l.mods[kOnBeatCrit]};
            const bool prism = pl.boost(kPrism), hyper = pl.boost(kHyper);
            u32 sig = 1;
            for (u8 m : mods) sig = sig * 7u + m;
            for (u8 r : l.riffs) sig = sig * 7u + r;
            sig = sig * 4u + (prism ? 1u : 0u) + (hyper ? 2u : 0u);
            if (sig != bandLoadSig_[slot]) {
                band_->setPlayerLoadout(slot, mods, l.riffs, prism, hyper);
                bandLoadSig_[slot] = sig;
            }
        }
        // The beacon: its player's lead holding the Track's root an octave down, panned by where it is.
        const bool on = pl.present && pl.down;
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
