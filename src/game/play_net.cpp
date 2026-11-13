// M4 stage A: the game in co-op (TECH_SPEC 8, GAME_DESIGN 10), from the command line: --host (with
// --loopback-bots N for bot clients in this process) or --join <ip>. No lobby screens: the host starts the run
// on a bar line once every seat is in.
//
// Time: every device runs the run on the host's song (net/sync.h). The host's song is its band's heard clock
// (the simulated clock without a band); its ticks follow that as in solo play. A client ticks at 60 Hz of its
// estimate of host time, each tick at the song's position then (as raveform_headless's peers), and steers its
// band toward the host's song: a rate correction of at most 0.3% on bar lines, or, when it is far off (at the
// start, after a stall), a hold on its next bar line until the host's song catches up (the re-seek).

#include <cmath>
#include <cstdio>
#include <new>

#include "core/log.h"
#include "core/time.h"
#include "game/play.h"
#include "net/bots.h"
#include "net/wire.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
constexpr i64 kTickUs = 16667;            // 1/60 s of host time
constexpr float kRateGainPerMs = 0.0003f; // the band's rate correction: 0.03% for each millisecond it is off ...
constexpr float kRateMax = 0.003f;        // ... at most 0.3% (TECH_SPEC 8.2)
constexpr float kReseekMs = 60.0f;        // farther off than this, the band re-seeks on a bar line (a stall)
}  // namespace

float Play::netTempoOf(void* self, double barTick) {
    // The host's song without a band: each bar at the director's tempo (Track 1's before the run).
    const Play& p = *static_cast<const Play*>(self);
    if (!p.netRunStarted_) return 120.0f;
    return p.world_->tempoAt(barTick - p.session_->runOffset() + 1.0);
}

bool Play::netInit() {
    session_ = new (std::nothrow) net::Session;
    if (!session_) return false;
    net::SessionConfig c;
    const bool host = opt_.net == 1;
    c.role = host ? NetRole::Host : NetRole::Client;
    c.seats = host ? (opt_.loopbackBots ? opt_.loopbackBots + 1u : opt_.players) : 2u;
    std::snprintf(c.bindIp, sizeof c.bindIp, "%s", opt_.bindIp);
    std::snprintf(c.joinIp, sizeof c.joinIp, "%s", opt_.joinIp);
    c.port = opt_.port;
    c.wire = opt_.loopbackBots ? net::WireKind::Loopback : net::WireKind::Udp;
    c.sim = opt_.netsim;
    std::snprintf(c.name, sizeof c.name, "%s", host ? "HOST" : "PLAYER");
    c.seed = opt_.seed;
    if (!session_->start(c, world_, net::contentHash())) {
        RF_ERR("net: could not %s %s port %u (address in use, or no network)", host ? "host on" : "join",
               host ? (c.bindIp[0] ? c.bindIp : "every address") : c.joinIp, c.port);
        return false;
    }
    session_->setClock([](void*) { return net::wireNowUs(); }, nullptr);
    if (host && opt_.loopbackBots) {
        bots_ = new (std::nothrow) net::LoopbackBots;
        if (!bots_ || !bots_->start(opt_.loopbackBots, opt_.port, species_, opt_.seed)) {
            RF_ERR("net: the loopback bots did not start");
            return false;
        }
    }
    RF_LOG("net: %s, %s port %u, %u seats%s; conditions delay %.0f ms jitter %.0f ms loss %.1f%%", host ? "host" : "client",
           host ? (c.bindIp[0] ? c.bindIp : "every address") : c.joinIp, c.port, c.seats,
           opt_.loopbackBots ? " (bots in this process, loopback network)" : "", c.sim.delayMs, c.sim.jitterMs, c.sim.loss * 100.0f);
    return true;
}

void Play::netFrame(float realDt) {
    frameDt_ = realDt > 0.1f ? 0.1f : (realDt < 0.0f ? 0.0f : realDt);
    if (coopFlowActive_) {
        // M4 stage B: the screens follow the session; an ended session goes back to the title (and solo play).
        coopFlow();
        if (!session_) return;
    }
    net::Session& s = *session_;
    World& w = *world_;
    const i64 localUs = net::wireNowUs();
    s.receive(localUs);
    seat_ = s.seat();
    const i64 hostNow = s.hostNow();
    const BeatView* bv = (bandDrives_ && band_ && band_->view().running) ? &band_->view() : nullptr;
    clock_.beginFrame(bv, realDt, false);  // the band bridge's bookkeeping (latency); the run follows the song model
    if (s.host()) {
        // The song every device follows: the band's heard clock (the simulated one without a band). With a band,
        // nothing is published until it plays (no run is scheduled on a clock that is about to change).
        if (bandDrives_) {
            if (bv && bv->bpm > 1.0) {
                net::SongAnchor a;
                a.hostUs = hostNow;
                a.song = bv->heardTick;
                a.tps = bv->bpm * 96.0 / 60.0;
                a.nextBar = (std::floor(bv->heardTick / kTicksPerBar) + 1.0) * kTicksPerBar;
                a.tpsNext = netRunStarted_ ? static_cast<double>(w.tempoAt(a.nextBar - s.runOffset() + 1.0)) * 96.0 / 60.0 : a.tps;
                s.setHostSong(a);
            }
        } else {
            if (!netSongStarted_) {
                netSongClock_.reset(hostNow, 0.0, 120.0f);
                netSongStarted_ = true;
            }
            netSongClock_.advanceTo(hostNow, &netTempoOf, this);
            s.setHostSong(netSongClock_.anchor(&netTempoOf, this));
        }
        netBandOffset_ = s.runOffset();
    }
    // A pause holds the band's section, as in solo play (the run stops on the pause's beat; the song goes on).
    const bool held = s.paused() || s.resuming();
    if (held != paused_) {
        paused_ = held;
        if (bandDrives_) band_->setHold(held);
        if (!held) plannedLine_ = -1e9;
    }
    const bool songOk = s.songValid();
    const double tpsNow = songOk ? s.tpsAt(hostNow) : 192.0;
    netHeard_ = songOk ? s.songAt(hostNow) - s.runOffset() : music1_;
    if (s.paused() && netHeard_ > s.pauseTick()) netHeard_ = s.pauseTick();

    // Input, once a frame (as solo play).
    StickInput in;
    buildInput(in);
    controller_.update(in, frameDt_, netHeard_, static_cast<float>(tpsNow * 60.0 / 96.0), w, seat_);
    if (bandDrives_) band_->setFireMode(controller_.pose().fire);

    // Ticks: 60 Hz steps of host time, each at the song's position at its end.
    stats_.ticksLastFrame = 0;
    if (s.running() && songOk) {
        if (!netRunStarted_) {
            const double run = s.songAt(hostNow) - s.runOffset();
            if (w.restartPending() && run >= w.restartTick()) {
                // The run begins on its bar line: the first tick crosses it.
                netRunStarted_ = true;
                music1_ = musicPrev_ = w.restartTick();
                netSimHostUs_ = hostNow - static_cast<i64>((run - w.restartTick()) / tpsNow * 1e6);
                controller_.reset(w.player(seat_).pos);
                RF_LOG("net: the run starts on bar line %.0f (seat %u of %u)", w.restartTick(), seat_, s.seats());
            }
        }
        if (netRunStarted_) {
            u32 n = 0;
            while (netSimHostUs_ + kTickUs <= hostNow && n < 3) {
                const i64 tEnd = netSimHostUs_ + kTickUs;
                netSimHostUs_ = tEnd;
                const double song = s.songAt(tEnd);
                if (s.resuming() && song >= s.resumeSong()) s.resumed();
                double m1 = song - s.runOffset();
                if (s.paused() && m1 > s.pauseTick()) m1 = s.pauseTick();
                if (m1 <= music1_) continue;  // paused: the run holds its place, the song goes on
                const float bpm = static_cast<float>(60.0 / 96.0 * (m1 - music1_) / (static_cast<double>(kTickUs) * 1e-6));
                tickEnd_ += kDt;
                netTick(m1, bpm > 1.0f ? bpm : 120.0f, n == 0);
                ++n;
            }
            if (hostNow - netSimHostUs_ > 3 * kTickUs) {
                netSimHostUs_ = hostNow - kTickUs / 2;  // TECH_SPEC 4: beyond three ticks a frame, drop time
                ++netDropped_;
            }
            alpha_ = clampf(static_cast<float>(static_cast<double>(hostNow - netSimHostUs_) / static_cast<double>(kTickUs)), 0.0f, 1.0f);
        }
    }
    if (!s.host()) netBandServo();
    {
        // A teammate who leaves the run (its ship fades where it was), logged as it happens (device checks).
        u8 present = 0;
        for (u32 q = 0; q < kMaxPlayers; ++q) present = static_cast<u8>(present | (w.player(q).present ? 1u << q : 0u));
        if (netRunStarted_)
            for (u32 q = 0; q < kMaxPlayers; ++q)
                if (((presentLogged_ >> q) & 1u) && !((present >> q) & 1u)) RF_LOG("net: seat %u left the run", q);
        presentLogged_ = present;
    }
    coopBand();  // M4 stage B: the teammates' leads, the beacons, the revive's notes (GAME_DESIGN 10)
    bandFrame();
    s.send(localUs, controller_.pose());

    const PlayerState& p = w.player(seat_);
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
    stats_.overflow = c.enemiesDropped + c.bulletsDropped + c.shotsDropped + c.shardsDropped + c.eventsDropped + c.netOpsDropped + c.netLogDropped;
    stats_.ticks = w.tickCount();
    if (s.state() == net::SessionState::Ended && frameNo_ % 600 == 0) RF_WARN("net: the session ended: %s", s.endReason());
    if (frameNo_ % 1200 == 0 && s.state() == net::SessionState::Running) {
        // Every 20 s (60 Hz): the link and, on a client, how its band sits against the host's song.
        const net::SessionStats& st = s.stats();
        RF_LOG("net: frame %llu: rtt %.1f ms, loss %.1f%%, up %.1f / down %.1f kbit/s, %.0f corrections/s (error %.1f wu RMS), song %+.2f ms, "
               "band %+.1f ms (rate %.4f, %u re-seeks), census %u/%u, %u frames dropped time",
               static_cast<unsigned long long>(frameNo_), st.rttMs, st.loss * 100.0f, st.upKbps, st.downKbps, st.correctionsPerS, st.errRmsWu,
               st.songOffsetMs, netBandErrMs_, netRatePosted_, netSeeks_, st.censusChecks, st.censusMismatches, netDropped_);
    }
}

void Play::netTick(double m1, float bpm, bool firstOfFrame) {
    (void)firstOfFrame;
    World& w = *world_;
    TickInput in;
    in.music0 = music1_;
    in.music1 = m1;
    in.bpm = bpm;
    in.latencyTicks = clock_.latencyTicks();
    in.players[seat_] = controller_.pose();
    in.retry = false;      // co-op: the host starts runs again (every player down)
    in.bandShots = false;  // the sim's own grid (the band, steered to the host's song, plays the same steps)
    retryRequest_ = false;
    session_->fillRemote(in, netSimHostUs_);
    const u64 t0 = nowNs();
    w.tick(in);
    const float ms = static_cast<float>(nsToMs(nowNs() - t0));
    if (stats_.ticksLastFrame < countOf(stats_.tickMs)) stats_.tickMs[stats_.ticksLastFrame] = ms;
    ++stats_.ticksLastFrame;
    controller_.clearEdges();
    musicPrev_ = music1_;
    music1_ = m1;
    session_->afterTick();
    afterTick();
}

void Play::netBandServo() {
    // A client's band follows the host's song (TECH_SPEC 8.2): its heard tick against the host's song at this
    // frame's host time, whole bars apart (the band's own count of bars is its own).
    const net::Session& s = *session_;
    if (!bandDrives_ || !band_ || !band_->view().running || !s.songValid()) return;
    const BeatView& v = band_->view();
    const i64 hostNow = s.hostNow();
    const double model = s.songAt(hostNow), tps = s.tpsAt(hostNow);
    const double diff = v.heardTick - model;
    const double bars = std::floor(diff / kTicksPerBar + 0.5);
    const double err = diff - bars * kTicksPerBar;  // within half a bar; > 0: the band is ahead
    netBandOffset_ = s.runOffset() + bars * kTicksPerBar;
    netBandErrMs_ = static_cast<float>(err / tps * 1000.0);
    if (netHoldUntil_ >= 0.0) {
        if (v.heardTick < netHoldUntil_) return;  // a re-seek under way
        netHoldUntil_ = -1.0;
    }
    if (std::fabs(netBandErrMs_) > kReseekMs) {
        // At the start, or after a stall: the band's next bar line waits until the host's song reaches one.
        const double hold = err > 0.0 ? err : err + kTicksPerBar;
        band_->setRate(1.0f);
        netRatePosted_ = 1.0f;
        band_->holdNextBar(static_cast<float>(hold / tps));
        netHoldUntil_ = (std::floor(v.heardTick / kTicksPerBar) + 2.0) * kTicksPerBar;
        ++netSeeks_;
        RF_LOG("net: the band re-seeks: %.1f ms off the host's song, holds its next bar line %.1f ms", netBandErrMs_, hold / tps * 1000.0);
        return;
    }
    const float rate = 1.0f - clampf(netBandErrMs_ * kRateGainPerMs, -kRateMax, kRateMax);
    if (std::fabs(rate - netRatePosted_) > 0.0002f) {
        band_->setRate(rate);
        netRatePosted_ = rate;
    }
}

u32 Play::netOverlayLines(char (*lines)[96], u32 max) const {
    if (!session_ || !max) return 0;
    const net::Session& s = *session_;
    const net::SessionStats& st = s.stats();
    static const char* const kState[] = {"IDLE", "CONNECTING", "LOBBY", "RUNNING", "ENDED"};
    u32 n = 0;
    const auto line = [&](const char* fmt, auto... a) {
        if (n < max) std::snprintf(lines[n++], 96, fmt, a...);
    };
    line("NET %s  SEAT %u OF %u  %s%s  PEERS %u", s.host() ? "HOST" : "CLIENT", s.seat() + 1, s.seats(),
         kState[static_cast<u32>(s.state()) < 5 ? static_cast<u32>(s.state()) : 0], s.paused() ? " (PAUSED)" : "", st.peers);
    line("RTT %.1f MS (VAR %.1f)  LOSS %.1f%%", st.rttMs, st.rttVarMs, st.loss * 100.0f);
    line("UP %.1f KBIT/S  DOWN %.1f KBIT/S  LARGEST DATAGRAM %u B", st.upKbps, st.downKbps, st.maxDatagram);
    if (s.host()) {
        line("CORRECTIONS SENT %.0f/S (%u)  ASKED FOR %u", st.correctionsPerS, st.corrections, st.attention);
    } else {
        line("CORRECTIONS %.0f/S (%u, %u SNAPS)  POSITION ERROR %.1f WU RMS, MAX %.0f", st.correctionsPerS, st.corrections, st.snaps,
             st.errRmsWu, st.errMaxWu);
        const float scr = st.screenErrN ? static_cast<float>(std::sqrt(st.screenErrSum2 / static_cast<double>(st.screenErrN))) : 0.0f;
        line("ON SCREEN AT CORRECTION %.1f WU RMS  ASKED %u  UNKNOWN %u", scr, st.attention, st.corrUnknown);
        line("CLOCK %+.2f MS (RTT %.1f, DILATION %+.2f%%)  SONG %+.2f MS", st.clockOffsetMs, st.clockRttMs, st.dilation * 100.0f,
             st.songOffsetMs);
        line("BAND %+.1f MS FROM THE HOST'S SONG  RATE %.4f  RE-SEEKS %u", netBandErrMs_, netRatePosted_, netSeeks_);
    }
    line("CENSUS %u CHECKS, %u MISMATCHES  DROPPED FRAMES %u", st.censusChecks, st.censusMismatches, netDropped_);
    if (bots_) {
        const net::LoopbackBots::Times t = bots_->times();
        line("BOTS %u (%u IN)  WORKER FRAME P50 %.2f  P95 %.2f  MAX %.2f MS", bots_->count(), t.connected, t.p50, t.p95, t.max);
    }
    return n;
}

bool Play::botTimes(float& p50, float& p95, float& max, u32& dropped) const {
    if (!bots_) return false;
    const net::LoopbackBots::Times t = bots_->times();
    p50 = t.p50;
    p95 = t.p95;
    max = t.max;
    dropped = t.dropped;
    return true;
}

void Play::resetBotTimes() {
    if (bots_) bots_->resetTimes();
}

}  // namespace rf
