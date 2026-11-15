// Co-op without a screen (M4; TECH_SPEC 8.4): part of raveform_headless.
//
//   raveform_headless --net soak [--minutes 10] [--players 4] [--netsim loss=0.05,jitter=80] [--seed 1] [--pause]
//       One host and three bots on the in-process loopback network, ten simulated minutes on a simulated
//       clock (each client's clock offset and rate differ). Measures and asserts: no disconnect, no stall, the
//       enemy sets agree (census), the on-screen enemy position error against the host (RMS), the song clock
//       offset, each client's bandwidth (down / up, the worst second), the largest datagram, pool overflows
//       and heap allocations in the steady state. Exit 0 = pass.
//   raveform_headless --net host [--players 4] [--bind 127.0.0.1] [--port 47778] [--minutes M] [--netsim ...]
//   raveform_headless --net join <ip> [--port 47778] [--minutes M] [--netsim ...] [--clock-offset-ms X]
//       [--clock-ppm P] [--same-clock] [--bind ip]
//       A bot peer over real UDP in real time. --same-clock (peers on one PC): the true song offset against
//       the host is measured from the shared monotonic clock (less the client's own --clock-offset-ms).
// Each prints a line every 10 s and one line tagged RFNET (JSON) at the end.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <thread>

#include "core/alloc_counter.h"
#include "core/log.h"
#include "net/peer.h"
#include "sim/species.h"
#include "sim/tuning.h"
#include "sim/tuning.h"

using namespace rf;
using namespace rf::net;
using namespace rf::sim;

namespace {

// Limits of the M4 stage A acceptance (TECH_SPEC 8.4, PLAN 7).
constexpr float kMaxErrRmsWu = 12.0f;
constexpr float kMaxSongOffsetMs = 10.0f;
constexpr float kMaxDownKbps = 500.0f, kMaxUpKbps = 120.0f;  // TECH_SPEC 8.4 (the lead's ruling of 2026-11-13; were 250 / 60)
constexpr u32 kMaxDatagramNoFrag = 1472;

struct Hist {
    // 0.25 wu buckets up to 256 wu.
    static constexpr u32 kN = 1024;
    u32 b[kN] = {};
    u64 n = 0;
    double sum2 = 0.0;
    float max = 0.0f;
    void add(float v) {
        u32 k = static_cast<u32>(v * 4.0f);
        if (k >= kN) k = kN - 1;
        ++b[k];
        ++n;
        sum2 += static_cast<double>(v) * v;
        if (v > max) max = v;
    }
    float rms() const { return n ? static_cast<float>(std::sqrt(sum2 / static_cast<double>(n))) : 0.0f; }
    float pct(double q) const {
        if (!n) return 0.0f;
        u64 want = static_cast<u64>(q * static_cast<double>(n)), seen = 0;
        for (u32 k = 0; k < kN; ++k) {
            seen += b[k];
            if (seen > want) return static_cast<float>(k + 1) * 0.25f;
        }
        return max;
    }
};

// The soak's true on-screen error, broken down (docs/PROGRESS_m4_net.md): where its squared sum comes from.
struct TrueDiag {
    NetDiagBin all, byFamily[16], byState[16], targetDiffers, stateDiffers, knocked, frozenDiffers, byAge[6], bySize[5], called;
    NetDiagBin shoalAge[6], otherAge[6];  // the age bins for Shoals (darts included) and for every other family
    NetDiagBin simPos;                    // the simulated position against the host's (without the drawn offset)
    NetDiagBin byTrack[16], byBoss[32];   // the host's Track then; the fight then (a boss or a mini-boss, by kind)
    NetDiagBin byTarget[4];  // the host's copy follows: 0 this client's player, 1 the host's, 2 another client's, 3 none
    NetDiagBin perp;
    NetDiagBin hzLane, hzOther, hzPhaseDiffers, hzPosDiffers;  // Event Horizon: lane walkers, the rest; the Well's phase or place differs
    NetDiagBin fbTarget, fbState, fbCalled, fbKnocked, fbFamily[16];  // Feedback's fight: the kinds and the families
    NetDiagBin elite, byTrait[kTraitCount], byEvent[kEventKinds];      // M5 part 5: elites, by trait; while an arena event runs
    NetDiagBin nearStruct[9];  // within a structure's radius + 150 wu (by StructKind 0-5; 6-8 the events' bodies)
    NetDiagBin statePair[16][16];  // host's state x the client copy's state, where they differ
    double hzPosSum2 = 0.0;
    u64 hzPosN = 0;
    double tauSum = 0.0, tau2Sum = 0.0;
    u64 tauN = 0;
    void print(const char* title) const {
        const auto b = [&](const char* name, const NetDiagBin& x) {
            if (!x.n) return;
            std::printf(" %s %.1f (%.0f%%e2 %.0f%%n)", name, static_cast<double>(x.rms()), all.sum2 > 0.0 ? 100.0 * x.sum2 / all.sum2 : 0.0,
                        all.n ? 100.0 * static_cast<double>(x.n) / static_cast<double>(all.n) : 0.0);
        };
        std::printf("[net]   %s: RMS %.2f wu (%llu); the simulated position alone (no drawn offset) %.2f\n[net]     kind:", title,
                    static_cast<double>(all.rms()), static_cast<unsigned long long>(all.n), static_cast<double>(simPos.rms()));
        b("target-differs", targetDiffers);
        b("state-differs", stateDiffers);
        b("knocked", knocked);
        b("frozen-differs", frozenDiffers);
        b("called", called);
        std::printf("\n[net]     host's target:");
        static const char* const kT[4] = {"self", "host", "other", "none"};
        for (u32 k = 0; k < 4; ++k) b(kT[k], byTarget[k]);
        std::printf("\n[net]     size:");
        static const char* const kS[5] = {"<5", "<20", "<50", "<96", ">=96"};
        for (u32 k = 0; k < 5; ++k) b(kS[k], bySize[k]);
        std::printf("\n[net]     since this copy's last correction:");
        static const char* const kA[6] = {"<.25s", "<.5s", "<1s", "<2s", "<4s", ">4s"};
        for (u32 k = 0; k < 6; ++k) b(kA[k], byAge[k]);
        std::printf("\n[net]       Shoals:");
        for (u32 k = 0; k < 6; ++k) b(kA[k], shoalAge[k]);
        std::printf("\n[net]       the rest:");
        for (u32 k = 0; k < 6; ++k) b(kA[k], otherAge[k]);
        const double tm = tauN ? tauSum / static_cast<double>(tauN) : 0.0, tr = tauN ? std::sqrt(tau2Sum / static_cast<double>(tauN)) : 0.0;
        std::printf("\n[net]     along the host's velocity: mean %+.1f ms, RMS %.1f ms; across it %.1f wu\n[net]     family:", tm * 1e3, tr * 1e3,
                    static_cast<double>(perp.rms()));
        char name[8];
        for (u32 k = 0; k < 16; ++k) {
            std::snprintf(name, sizeof name, "%u", k);
            b(name, byFamily[k]);
        }
        std::printf("\n[net]     Track (the host's):");
        for (u32 k = 0; k < 16; ++k) {
            if (!byTrack[k].n) continue;
            std::snprintf(name, sizeof name, "%u", k);
            b(name, byTrack[k]);
        }
        std::printf("\n[net]     fight:");
        for (u32 k = 0; k < 32; ++k)
            if (byBoss[k].n) b(bossId(static_cast<BossKind>(k)), byBoss[k]);
        std::printf("\n[net]     state:");
        for (u32 k = 0; k < 16; ++k) {
            std::snprintf(name, sizeof name, "%u", k);
            b(name, byState[k]);
        }
        std::printf("\n[net]     elites:");
        b("all", elite);
        for (u32 t = 0; t < kTraitCount; ++t) b(traitId(t), byTrait[t]);
        std::printf("\n[net]     state differs (host>client):");
        for (u32 x = 0; x < 16; ++x)
            for (u32 y = 0; y < 16; ++y)
                if (statePair[x][y].n) {
                    std::snprintf(name, sizeof name, "%u>%u", x, y);
                    b(name, statePair[x][y]);
                }
        std::printf("\n[net]     near a structure:");
        static const char* const kBodyNames[9] = {"vault", "pylon", "rift", "bumper", "current", "lens", "gold_vault", "comet", "carrier"};
        for (u32 k = 0; k < 9; ++k) b(kBodyNames[k], nearStruct[k]);
        std::printf("\n[net]     arena event running:");
        for (u32 k = 0; k < kEventKinds; ++k) b(eventId(static_cast<EventKind>(k)), byEvent[k]);
        std::printf("\n[net]     Event Horizon:");
        b("lane", hzLane);
        b("rest", hzOther);
        b("phase-differs", hzPhaseDiffers);
        b("well-off>10wu", hzPosDiffers);
        std::printf("; the Well's place against the host's RMS %.1f wu\n[net]     Feedback:", hzPosN ? std::sqrt(hzPosSum2 / static_cast<double>(hzPosN)) : 0.0);
        b("target-differs", fbTarget);
        b("state-differs", fbState);
        b("called", fbCalled);
        b("knocked", fbKnocked);
        for (u32 k = 0; k < 16; ++k) {
            std::snprintf(name, sizeof name, "f%u", k);
            b(name, fbFamily[k]);
        }
        std::printf("\n");
    }
};

// The remote ships as drawn (M4 net follow-up, the lead's point 2): each viewer's copy of another player against that
// player's own position at the same song time, and the largest step in one frame its own motion does not explain.
// Classes: 0 the host's view of a client, 1 a client's view of the host, 2 a client's view of a relayed client.
struct ShipTruth {
    static constexpr u32 kN = 256;
    double music[kN] = {};
    Vec2 pos[kN];
    float aim[kN] = {};
    u32 head = 0, n = 0;
    void add(double m, Vec2 p, float a) {
        if (n && music[(head + kN - 1) % kN] >= m) return;
        music[head] = m;
        pos[head] = p;
        aim[head] = a;
        head = (head + 1) % kN;
        if (n < kN) ++n;
    }
    bool at(double m, Vec2& out, float* aimOut = nullptr) const {
        for (u32 k = 1; k < n; ++k) {
            const u32 b = (head + kN - k) % kN, a = (head + kN - k - 1) % kN;
            if (music[a] <= m && m <= music[b]) {
                const double t = music[b] > music[a] ? (m - music[a]) / (music[b] - music[a]) : 0.0;
                out = pos[a] + (pos[b] - pos[a]) * static_cast<float>(t);
                if (aimOut) *aimOut = aim[static_cast<double>(t) < 0.5 ? a : b];
                return length(pos[b] - pos[a]) < 120.0f;  // (not across a teleport: a revive, a new run)
            }
        }
        return false;
    }
};
struct ShipView {
    Hist err;
    float maxStep = 0.0f;
    u64 steps = 0, stepsOverHalfHull = 0;
    double aimTurnSum2 = 0.0;  // a frame's turn of the drawn aim its own turn does not explain (radians)
    float aimTurnMax = 0.0f;
    u64 aimTurnsOver30 = 0;
};

struct PeerReport {
    float downMax = 0.0f, upMax = 0.0f;   // the worst second
    i64 downMaxAt = 0, upMaxAt = 0;       // when it ended
    u32 upPeers = 0;                      // peers connected then
    u32 upSilentMs = 0, upInFlight = 0;   // the longest silence of a peer then; reliable bytes unacknowledged
    float upRel = 0.0f, upUnrel = 0.0f, upCtrl = 0.0f;  // the worst up second's payloads by channel (kbit/s)
    u32 upDatagrams = 0;
    double downSum = 0.0, upSum = 0.0;    // for the mean
    u32 seconds = 0;
    float corrErrMax = 0.0f;
    double corrErrSum2 = 0.0;
    u64 corrN = 0;
};

const char* arg(int argc, char** argv, const char* name) {
    for (int i = 3; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], name) == 0) return argv[i + 1];
    return nullptr;
}
bool flag(int argc, char** argv, const char* name) {
    for (int i = 3; i < argc; ++i)
        if (std::strcmp(argv[i], name) == 0) return true;
    return false;
}

// --remote-poses interp|predict: where every peer of this process takes a remote player for a tick (net/session.h).
RemotePoses g_remotePoses = SessionConfig{}.remotePoses;
// --fight <id>: the soak starts in that fight's home Track and measures from the fight on, for --minutes.
bool g_fightOn = false;
// --away-at S --away-for D [--away-seat N] (pause-on-leave, 2026-11-14): seat N's app (default the last client; 0 the host)
// leaves the screen S s into the run: it says so, then its frames and its socket stop as Android stops the app; D s later it is
// back. The room pauses for it on one tick, waits (net::kAwayWaitMs) and counts in once it has settled.
float g_awayAtS = 0.0f, g_awayForS = 10.0f;
u32 g_awaySeat = 0xFFu;
// Census diagnostics: the host's enemies by serial (a ring), to name the deaths a client heard for enemies it never had.
struct HostSeen {
    u32 serial = 0;
    u16 species = 0, traits = 0, group = 0;
    u8 state = 0, flags = 0, track = 0, boss = 0xFF;
    double firstMusic = 0.0;
};
constexpr u32 kSeenRing = 1u << 16;
HostSeen* g_seen = nullptr;
u32 g_lastMismatch = 0;
BossKind g_fightKind = BossKind::Kaleidos;
// --net-tune down=..,famN=..: the host's correction budget and family weights (experiments, net/session.h).
const char* g_tune = nullptr;
void tune(SessionConfig& c) {
    if (g_tune) parseNetTune(g_tune, c);
}

u32 overflowOf(const World& w) {
    const WorldCounters& c = w.counters();
    return c.enemiesDropped + c.bulletsDropped + c.shotsDropped + c.shardsDropped + c.eventsDropped + c.netOpsDropped + c.netLogDropped;
}

void secondly(NetPeer& p, PeerReport& r, i64 when, i64& last) {
    // Each second the session's own figures (bandwidth with IP/UDP headers, correction errors).
    if (when - last < 1000000) return;
    last = when;
    const SessionStats& s = p.session().stats();
    if (s.downKbps > r.downMax) {
        r.downMax = s.downKbps;
        r.downMaxAt = when;
    }
    if (s.upKbps > r.upMax) {
        r.upMax = s.upKbps;
        r.upMaxAt = when;
        r.upPeers = s.peers;
        r.upSilentMs = s.silentMs;
        r.upInFlight = s.inFlight;
        r.upRel = s.upRelKbps;
        r.upUnrel = s.upUnrelKbps;
        r.upCtrl = s.upCtrlKbps;
        r.upDatagrams = s.upDatagrams;
    }
    r.downSum += s.downKbps;
    r.upSum += s.upKbps;
    ++r.seconds;
    if (s.errMaxWu > r.corrErrMax) r.corrErrMax = s.errMaxWu;
}

// ---------------------------------------------------------------------------------------------- soak

int runSoak(const SpeciesTable& species, float minutes, u32 players, const NetSim& sim, u64 seed, bool pause, float dropAtS) {
    if (players < 2) players = 2;
    if (players > kMaxPlayers) players = kMaxPlayers;
    wireUseSimClock(true);
    i64 G = 2000000000ll;  // true time, microseconds
    wireSetSimTimeUs(G);
    auto* peers = new (std::nothrow) NetPeer[kMaxPlayers];
    auto* hist = new (std::nothrow) Hist;
    auto* tdiag = new (std::nothrow) TrueDiag;
    auto* cdiag = new (std::nothrow) NetDiag;  // the clients' corrections and poses, merged
    auto* truth = new (std::nothrow) ShipTruth[kMaxPlayers];
    auto* ships = new (std::nothrow) ShipView[3];
    if (!g_seen) g_seen = new (std::nothrow) HostSeen[kSeenRing];
    if (!peers || !hist || !tdiag || !cdiag || !truth || !ships || !g_seen) return 2;
    double shipLast[kMaxPlayers][kMaxPlayers];
    Vec2 shipDrawn[kMaxPlayers][kMaxPlayers];
    float shipAim[kMaxPlayers][kMaxPlayers] = {};
    float ownAimLast[kMaxPlayers] = {};
    u64 ownAimFrames = 0, ownAimFlicks = 0;
    for (auto& row : shipLast)
        for (double& v : row) v = -1.0;
    static const i64 kOffsets[kMaxPlayers] = {0, 1234567, -777000, 3210};
    static const double kPpm[kMaxPlayers] = {0.0, 60.0, -45.0, 20.0};
    for (u32 k = 0; k < players; ++k) {
        PeerConfig c;
        c.session.role = k == 0 ? NetRole::Host : NetRole::Client;
        c.session.seats = players;
        c.session.wire = WireKind::Loopback;
        c.session.port = kGamePort;
        std::snprintf(c.session.bindIp, sizeof c.session.bindIp, "%s", k == 0 ? "127.0.0.1" : "");
        std::snprintf(c.session.joinIp, sizeof c.session.joinIp, "127.0.0.1");
        std::snprintf(c.session.name, sizeof c.session.name, k == 0 ? "HOST" : "BOT%u", k);
        c.session.sim = sim;
        c.session.sim.seed = sim.seed * 7919u + k * 104729u + 1u;
        c.session.remotePoses = g_remotePoses;
        tune(c.session);
        c.session.seed = seed;
        c.botSeed = seed * 31 + k;
        c.clockOffsetUs = kOffsets[k];
        c.clockPpm = kPpm[k];
        if (!peers[k].start(c, &species)) {
            std::printf("[net] peer %u did not start\n", k);
            return 2;
        }
    }
    // Each device's frames come at its own display's rate ("60 Hz" panels run between about 59.9 and 60.1 Hz)
    // with a little jitter, as on real devices, so they fall at changing moments against each other.
    static const double kRefresh[kMaxPlayers] = {60.0, 59.94, 60.05, 59.98};
    i64 nextFrame[kMaxPlayers];
    double nominal[kMaxPlayers], period[kMaxPlayers];
    u32 jitterRng = 0x5EED1234u;
    for (u32 k = 0; k < players; ++k) {
        nominal[k] = static_cast<double>(G + static_cast<i64>(k) * 4100);
        nextFrame[k] = static_cast<i64>(nominal[k]);
        period[k] = 1e6 / kRefresh[k] / (1.0 + kPpm[k] * 1e-6);
    }
    const i64 runUs = static_cast<i64>(static_cast<double>(minutes) * 60e6);
    const i64 settleUs = 20000000;  // the first 20 s of the run are not measured (clocks settle, allocations at start)
    i64 runFrom = 0, lastMeasure = 0, lastSecond[kMaxPlayers] = {};
    i64 fightFrom = 0;  // --fight: when the fight began (true time)
    PeerReport rep[kMaxPlayers];
    double songSum2 = 0.0, songMax = 0.0;
    u64 songN = 0;
    u64 thinChecks = 0, thinDiffers = 0, thinOwnerOn = 0;  // a copy's thinning against the owner's (GAME_DESIGN 6.6)
    double thinLiveSum2 = 0.0;
    u64 shStraight[2] = {}, shBent[2] = {}, shBentFirst[2] = {}, shCurve[2] = {};  // the host's screen: own 0, remote 1
    double scrBoss2[32] = {}, lastScr2[kMaxPlayers] = {};  // the clients' at-correction error by fight (31: none)
    u64 scrBossN[32] = {}, lastScrN[kMaxPlayers] = {};
    double bossHpSum2 = 0.0, bossHpMax = 0.0;  // a client's boss health against the host's (shares of the whole)
    u64 bossHpN = 0, bossHpAbove = 0, bossDeathLate = 0;
    u64 missing = 0;
    u64 allocs0 = 0;
    EnetPoolStats pool0;
    bool measuring = false;
    bool pausedSeen = false;
    double pauseTicks[kMaxPlayers] = {};
    u32 pauseState = 0;  // 0 none, 1 asked, 2 resume asked, 3 done
    i64 pauseAskAt = 0;
    const double wall0 = static_cast<double>(std::chrono::steady_clock::now().time_since_epoch().count());
    // --drop-at S (M4 stage B): the last client's process "dies" S s into the run (its link goes without a word, the
    // host times it out); the others play on. The on-screen error is reported for each 10 s around it.
    const u32 dropPeer = players - 1;
    bool dropped = false;
    // --away-at (pause-on-leave): the away peer, while away, is out of the loop as a dead one is (no frames, no socket).
    const u32 awayPeer = g_awaySeat < players ? g_awaySeat : players - 1;
    bool awayNow = false, awayDone = false, awayAllPaused = false, awaySameTick = true, awayResumed = false;
    i64 awayFrom = 0, awayBackAt = 0, awayResumedAt = 0;
    double awayHeld = 0.0;  // the room's pause tick
    // Away past the room's wait: the room drops it and plays on; back, it finds the run gone on without it ("away too long").
    const bool awayPastWait = static_cast<double>(g_awayForS) * 1000.0 > static_cast<double>(kAwayWaitMs);
    const auto off = [&](u32 k) { return (dropped && k == dropPeer) || ((awayNow || (awayDone && awayPastWait)) && k == awayPeer); };
    i64 windowFrom = 0;
    double winSum2 = 0.0;
    u64 winN = 0;
    double calledSum2 = 0.0;  // M5: the part of it a mini-boss's called bodies make (the pack, the darts)
    u64 calledN = 0;
    double stateSum2[16] = {}, famSum2[16] = {};  // the window's error by enemy state and family (diagnostics)
    u64 stateN[16] = {}, famN[16] = {};
    float winMax = 0.0f;
    char worst[200] = {};  // the window's worst sample (diagnostics)
    for (;;) {
        G += 1000;
        wireSetSimTimeUs(G);
        for (u32 k = 0; k < players; ++k)
            if (!(off(k))) wirePump(peers[k].session().link().socket());
        for (u32 k = 0; k < players; ++k) {
            if (off(k)) continue;
            if (G < nextFrame[k]) continue;
            peers[k].frame(G);
            {
                // The remote ships as this peer now has them, against each player's own record (point 2).
                const World& vw = peers[k].world();
                const u32 me = peers[k].session().seat();
                const double m = peers[k].music();
                if (vw.player(me).present) {
                    // The owner's own aim: how often it turns more than 30 degrees in one frame (the bot's aim flicking
                    // between targets: behaviour, the same in solo play).
                    const float a = angleOf(vw.player(me).aim);
                    if (measuring && vw.player(me).alive) {
                        ++ownAimFrames;
                        if (std::fabs(wrapAngle(a - ownAimLast[me])) > 30.0f * kPi / 180.0f) ++ownAimFlicks;
                    }
                    ownAimLast[me] = a;
                    truth[me].add(m, vw.player(me).pos, a);
                }
                for (u32 j = 0; j < players && measuring; ++j) {
                    if (j == me || !vw.player(j).present || !vw.player(j).alive || vw.runId() != peers[0].world().runId()) continue;
                    const u32 cls = k == 0 ? 0u : (j == 0 ? 1u : 2u);
                    Vec2 t;
                    if (!truth[j].at(m, t)) {
                        shipLast[k][j] = -1.0;
                        continue;
                    }
                    const Vec2 drawn = vw.player(j).pos;
                    ships[cls].err.add(length(drawn - t));
                    float aimT = 0.0f;
                    truth[j].at(m, t, &aimT);
                    const float aimD = angleOf(vw.player(j).aim);
                    if (shipLast[k][j] >= 0.0) {
                        Vec2 t0;
                        float aimT0 = 0.0f;
                        if (truth[j].at(shipLast[k][j], t0, &aimT0)) {
                            const float step = length((drawn - shipDrawn[k][j]) - (t - t0));
                            ++ships[cls].steps;
                            if (step > ships[cls].maxStep) ships[cls].maxStep = step;
                            if (step > kPlayerVisualRadius) ++ships[cls].stepsOverHalfHull;
                            // The drawn aim's turn this frame against the player's own (the stream of its shots swings by it).
                            const float turn = std::fabs(wrapAngle((aimD - shipAim[k][j]) - (aimT - aimT0)));
                            ships[cls].aimTurnSum2 += static_cast<double>(turn) * turn;
                            if (turn > ships[cls].aimTurnMax) ships[cls].aimTurnMax = turn;
                            if (turn > 30.0f * kPi / 180.0f) ++ships[cls].aimTurnsOver30;
                        }
                    }
                    shipLast[k][j] = m;
                    shipDrawn[k][j] = drawn;
                    shipAim[k][j] = aimD;
                }
            }
            jitterRng ^= jitterRng << 13;
            jitterRng ^= jitterRng >> 17;
            jitterRng ^= jitterRng << 5;
            nominal[k] += period[k];
            nextFrame[k] = static_cast<i64>(nominal[k]) + static_cast<i64>(jitterRng % 1001u) - 500;  // vsync +-0.5 ms
        }
        bool all = true;
        for (u32 k = 0; k < players; ++k) all = all && (dropped || peers[k].session().running());
        if (all && !runFrom && peers[0].world().runId() > 0 && peers[0].music() > peers[0].world().tickOfBar(0) + 1.0) {
            runFrom = G;
            if (g_fightOn) {
                // --fight: every world plays the fight's home Track from here (as the game's --fight does); the host draws
                // the mini-boss it names.
                const u32 home = bossHomeTrack(g_fightKind);
                for (u32 k = 0; k < players; ++k) {
                    World& w = peers[k].world();
                    if (home > w.setList()) w.setSetList(home);
                    w.directorMut().track = static_cast<u8>(home);
                    w.spawner().mode = Spawner::Mode::Slice;
                }
                if (isMiniBoss(g_fightKind)) peers[0].world().forceMini(g_fightKind);
                std::printf("[net] --fight %s: the run plays Track %u to it\n", bossId(g_fightKind), home);
            }
        }
        if (!runFrom) {
            if (G > 2000000000ll + 30000000ll) {
                std::printf("[net] the run did not start within 30 s\n");
                return 1;
            }
            continue;
        }
        const i64 t = G - runFrom;
        if (g_fightOn) {
            // Measured from the fight's start for --minutes (at most 15 simulated minutes to reach it).
            const BossState& hb = peers[0].world().boss();
            if (!fightFrom && hb.active && hb.kind == g_fightKind && peers[0].music() >= hb.fightTick) {
                fightFrom = G;
                std::printf("[net] %5.0f s: the fight begins\n", static_cast<double>(t) * 1e-6);
            }
            if (fightFrom ? G - fightFrom >= runUs : t >= 900000000ll) break;
        } else if (t >= runUs) {
            break;
        }
        if (dropAtS > 0.0f && !dropped && t >= static_cast<i64>(dropAtS * 1e6f)) {
            dropped = true;
            peers[dropPeer].session().linkMut().abandon();
            std::printf("[net] %5.1f s: peer %u's process dies (its link goes without a word)\n", static_cast<double>(t) * 1e-6, dropPeer);
        }
        if (g_awayAtS > 0.0f && !awayNow && !awayDone && t >= static_cast<i64>(g_awayAtS * 1e6f)) {
            peers[awayPeer].goAway(G);
            awayNow = true;
            awayFrom = G;
            std::printf("[net] %5.1f s: peer %u's app leaves the screen (it stops for %.0f s)\n", static_cast<double>(t) * 1e-6, awayPeer,
                        static_cast<double>(g_awayForS));
        }
        if (awayNow && G - awayFrom >= static_cast<i64>(static_cast<double>(g_awayForS) * 1e6)) {
            // What the room did meanwhile: every other peer held on one tick.
            awayAllPaused = true;
            awayHeld = -1.0;
            for (u32 k = 0; k < players; ++k) {
                if (k == awayPeer) continue;
                awayAllPaused = awayAllPaused && peers[k].session().paused() && !peers[k].session().resuming();
                if (awayHeld < 0.0) awayHeld = peers[k].music();
                awaySameTick = awaySameTick && std::fabs(peers[k].music() - awayHeld) < 0.5;
            }
            awayNow = false;
            awayDone = true;
            awayBackAt = G;
            peers[awayPeer].comeBack(G);
            std::printf("[net] %5.1f s: peer %u is back (the room %s on run tick %.0f, every peer on it: %s; the host: away mask 0x%x, %u "
                        "disconnects, seats 0x%x)\n",
                        static_cast<double>(t) * 1e-6, awayPeer, awayAllPaused ? "held" : "did NOT hold", awayHeld, awaySameTick ? "yes" : "NO",
                        peers[0].session().awayMask(), peers[0].session().stats().disconnects, peers[0].session().seatMask());
        }
        if (awayDone && !awayResumed) {
            // Back on: every peer past the pause tick (the count-in over), the one that left among them (past the wait: the others).
            bool on = true;
            for (u32 k = 0; k < players; ++k)
                if (!(awayPastWait && k == awayPeer)) on = on && !peers[k].session().paused() && peers[k].music() > awayHeld + 1.0;
            if (on) {
                awayResumed = true;
                awayResumedAt = G;
                std::printf("[net] %5.1f s: the room plays on (%.1f s after the return; the one that left on run tick %.0f, the host on %.0f)\n",
                            static_cast<double>(t) * 1e-6, static_cast<double>(G - awayBackAt) * 1e-6, peers[awayPeer].music(), peers[0].music());
            }
        }
        if (dropAtS > 0.0f && measuring && t - windowFrom >= 10000000) {
            const SessionStats& s1 = peers[1].session().stats();
            u32 present = 0;
            for (u32 q = 0; q < kMaxPlayers; ++q) present += peers[0].world().player(q).present ? 1u : 0u;
            const World& hw0 = peers[0].world();
            std::printf("[net] %5.0f s: on-screen error (true, against the host) RMS %.2f wu, max %.1f (%llu samples); client 1's "
                        "corrections' error %.1f wu RMS (last second); players present on the host %u; Track %u %s%s %s\n",
                        static_cast<double>(t) * 1e-6, winN ? std::sqrt(winSum2 / static_cast<double>(winN)) : 0.0, static_cast<double>(winMax),
                        static_cast<unsigned long long>(winN), static_cast<double>(s1.errRmsWu), present, hw0.phrase().track,
                        sectionLabel(hw0.phrase().section), hw0.phrase().held ? " (held)" : "",
                        hw0.boss().active ? bossId(hw0.boss().kind) : "");
            {
                u32 hc = 0, cc = 0, cf = 0;
                const Enemies& he0 = peers[0].world().enemies();
                const Enemies& ce1 = peers[1].world().enemies();
                for (u32 i = 0; i < he0.count; ++i) hc += !(he0.flags[i] & kFlagDead) && (he0.flags[i] & kFlagCalled) ? 1u : 0u;
                for (u32 i = 0; i < ce1.count; ++i) {
                    cc += !(ce1.flags[i] & kFlagDead) && (ce1.flags[i] & kFlagCalled) ? 1u : 0u;
                    cf += !(ce1.flags[i] & kFlagDead) && ce1.state[i] == kFormation ? 1u : 0u;
                }
                std::printf("[net]          called: host %u, client 1 %u (in formation %u)\n", hc, cc, cf);
            }
            std::printf("[net]          worst: %s\n", worst);
            worst[0] = 0;
            std::printf("[net]          by state:");
            for (u32 q = 0; q < 16; ++q)
                if (stateN[q]) std::printf(" %u: %.1f (%llu)", q, std::sqrt(stateSum2[q] / static_cast<double>(stateN[q])), static_cast<unsigned long long>(stateN[q]));
            std::printf("; by family:");
            for (u32 q = 0; q < 16; ++q)
                if (famN[q]) std::printf(" %u: %.1f (%llu)", q, std::sqrt(famSum2[q] / static_cast<double>(famN[q])), static_cast<unsigned long long>(famN[q]));
            std::printf("\n");
            for (u32 q = 0; q < 16; ++q) stateSum2[q] = famSum2[q] = 0.0, stateN[q] = famN[q] = 0;
            windowFrom = t;
            winSum2 = 0.0;
            winN = 0;
            winMax = 0.0f;
        }
        if (!measuring && (g_fightOn ? fightFrom != 0 : t >= settleUs)) {
            measuring = true;
            allocs0 = heapAllocCountThisThread();
            pool0 = enetPoolStats();
            for (u32 k = 1; k < players; ++k) {
                peers[k].session().resetScreenError();
                peers[k].session().resetDiag();
            }
        }
        for (u32 k = 1; k < players; ++k)
            if (!(off(k))) secondly(peers[k], rep[k], G, lastSecond[k]);
        // The pause, once (GAME_DESIGN 10: any player may pause; it holds for everyone on the next beat).
        if (pause) {
            if (pauseState == 0 && t > 180000000) {
                peers[1].session().requestPause(true);
                pauseState = 1;
                pauseAskAt = G;
            } else if (pauseState == 1 && G - pauseAskAt > 3000000) {
                bool allPaused = true;
                for (u32 k = 0; k < players; ++k) {
                    allPaused = allPaused && peers[k].session().paused();
                    pauseTicks[k] = peers[k].music();
                }
                pausedSeen = allPaused;
                peers[2 < players ? 2 : 1].session().requestPause(false);
                pauseState = 2;
            } else if (pauseState == 2) {
                bool any = false;
                for (u32 k = 0; k < players; ++k) any = any || peers[k].session().paused();
                if (!any) pauseState = 3;
            }
        }
        if (!measuring || G - lastMeasure < 100000) continue;
        lastMeasure = G;
        // The song clock: each client's song by its own clock against the host's at the same true moment.
        const double hs = peers[0].songNow(G);
        const double tps = peers[0].session().tpsAt(peers[0].localUs(G));
        for (u32 k = 1; k < players; ++k) {
            if (off(k) || peers[k].session().goneAway()) continue;  // (back from away: in step once the room has heard it is back)
            const double ms = (peers[k].songNow(G) - hs) / tps * 1000.0;
            songSum2 += ms * ms;
            ++songN;
            if (std::fabs(ms) > songMax) songMax = std::fabs(ms);
        }
        {
            // Player shots on the host's screen (the game's `net: diag shots`): straight shots whose last tick bent more than
            // a quarter from a straight step, the host's own player's against the remote players'.
            const Shots& sh = peers[0].world().shots();
            for (u32 i = 0; i < sh.count; ++i) {
                if (sh.dead[i]) continue;
                const u32 cls = sh.owner[i] == 0 ? 0u : 1u;
                const float want = sh.speed[i] * kDt;
                if (want < 1.0f) continue;
                if (sh.mods[i] & (kShotSeek | kShotWave | kShotWaveNeg | kShotHelix | kShotHelixIn)) {
                    ++shCurve[cls];
                    continue;
                }
                ++shStraight[cls];
                if (length(Vec2{sh.x[i] - sh.px[i], sh.y[i] - sh.py[i]} - Vec2{sh.dx[i], sh.dy[i]} * want) > 0.25f * want) {
                    ++shBent[cls];
                    if (sh.life0[i] - sh.life[i] < 1.5f * kDt) ++shBentFirst[cls];
                }
            }
        }
        {
            // The boss's health on each client against the host's (a share of its whole), during fights; and the client's
            // copy at the tick the host's boss starts to die (the host's verdict: the fight's end).
            const BossState& hb = peers[0].world().boss();
            for (u32 k = 1; k < players; ++k) {
                if (off(k)) continue;
                const BossState& cb = peers[k].world().boss();
                if (!hb.active || !cb.active || hb.kind != cb.kind || hb.coreMax <= 0.0f) continue;
                if (hb.dying) {
                    if (!cb.dying) ++bossDeathLate;
                    continue;
                }
                const double d = static_cast<double>(cb.coreHp - hb.coreHp) / static_cast<double>(hb.coreMax);
                bossHpSum2 += d * d;
                ++bossHpN;
                if (std::fabs(d) > bossHpMax) bossHpMax = std::fabs(d);
                if (d > 0.0) ++bossHpAbove;
            }
        }
        if (players > 1 && peers[1].world().counters().censusMismatches != g_lastMismatch) {
            // Census diagnostics: client 1 just counted otherwise than the host; the enemies the two sets disagree on now.
            g_lastMismatch = peers[1].world().counters().censusMismatches;
            const World& hw = peers[0].world();
            const World& cw = peers[1].world();
            const Enemies& he = hw.enemies();
            const Enemies& ce = cw.enemies();
            std::printf("[net] census mismatch %u (client 1 count - host %d), host music %.0f, last refused message type %u:", g_lastMismatch,
                        cw.counters().censusLastDiff, hw.music(), net::g_lastBadMessage);
            u32 shown = 0;
            for (u32 i = 0; i < ce.count && shown < 6; ++i) {
                const bool live = !(ce.flags[i] & kFlagDead) || (ce.flags[i] & kFlagPredicted);
                if (!live) continue;
                const i32 j = hw.indexOfSerial(ce.serial[i]);
                if (j >= 0 && !(he.flags[j] & kFlagDead)) continue;
                ++shown;
                std::printf(" [client has %u sp %u st %u fl 0x%02x; host %s]", ce.serial[i], ce.species[i], ce.state[i], ce.flags[i],
                            j < 0 ? "gone" : "dead");
            }
            for (u32 j = 0; j < he.count && shown < 12; ++j) {
                if (he.flags[j] & kFlagDead) continue;
                if (cw.indexOfSerial(he.serial[j]) >= 0) continue;
                ++shown;
                std::printf(" [host has %u sp %u st %u fl 0x%02x; client none]", he.serial[j], he.species[j], he.state[j], he.flags[j]);
            }
            std::printf("\n");
        }
        {
            // Census diagnostics: what each host enemy was (by serial, a ring), to name the deaths a client heard for enemies
            // it never had (printed at the end).
            const World& hw = peers[0].world();
            const Enemies& he = hw.enemies();
            for (u32 j = 0; j < he.count; ++j) {
                HostSeen& r = g_seen[he.serial[j] & (kSeenRing - 1u)];
                if (r.serial != he.serial[j]) {
                    r = HostSeen{};
                    r.serial = he.serial[j];
                    r.species = he.species[j];
                    r.track = hw.phrase().track;
                    r.boss = hw.boss().active ? static_cast<u8>(hw.boss().kind) : 0xFF;
                    r.firstMusic = hw.music();
                }
                r.state = he.state[j];
                r.flags = he.flags[j];
                r.traits = he.traits[j];
                r.group = he.group[j];
            }
        }
        {
            // The clients' error as each correction lands (the game's and the device runs' `scr`), by the host's fight then
            // (31: no fight), to set beside the true error by fight.
            const BossState& hb = peers[0].world().boss();
            const u32 bk = hb.active && static_cast<u32>(hb.kind) < 31u ? static_cast<u32>(hb.kind) : 31u;
            for (u32 k = 1; k < players; ++k) {
                if (off(k)) continue;
                const SessionStats& st = peers[k].session().stats();
                scrBoss2[bk] += st.screenErrSum2 - lastScr2[k];
                scrBossN[bk] += st.screenErrN - lastScrN[k];
                lastScr2[k] = st.screenErrSum2;
                lastScrN[k] = st.screenErrN;
            }
        }
        // Each viewer's copy of every other player's thinning (GAME_DESIGN 6.6, decided on a beat from the live shots this
        // world holds) against the owner's own, and the live shot counts.
        for (u32 v = 0; v < players; ++v) {
            if (off(v)) continue;
            for (u32 o = 0; o < players; ++o) {
                if (o == v || (off(o)) || peers[v].world().runId() != peers[o].world().runId()) continue;
                ++thinChecks;
                if (peers[v].world().thinning(o) != peers[o].world().thinning(o)) ++thinDiffers;
                if (peers[o].world().thinning(o)) ++thinOwnerOn;
                const double dl = static_cast<double>(peers[v].world().shotsLive(o)) - static_cast<double>(peers[o].world().shotsLive(o));
                thinLiveSum2 += dl * dl;
            }
        }
        // On-screen enemies: each client's drawn position against the host's, at the host's musical time.
        if ((G / 100000) % 3 != 0) continue;
        const World& hw = peers[0].world();
        for (u32 k = 1; k < players; ++k) {
            if (off(k)) continue;
            const World& cw = peers[k].world();
            if (cw.runId() != hw.runId()) continue;
            const PlayerState& me = cw.player(peers[k].session().seat());
            if (!me.alive) continue;
            const double ahead = (peers[0].music() - peers[k].music()) / tps;
            const Enemies& ce = cw.enemies();
            const Enemies& he = hw.enemies();
            for (u32 i = 0; i < ce.count; ++i) {
                if ((ce.flags[i] & kFlagDead) || ce.state[i] == kWarping) continue;
                const float dx = static_cast<float>(ahead);
                const Vec2 drawn{ce.x[i] + ce.ox[i] + (ce.vx[i] + ce.kx[i]) * dx, ce.y[i] + ce.oy[i] + (ce.vy[i] + ce.ky[i]) * dx};
                if (std::fabs(drawn.x - me.pos.x) > tuning::kFrameHalfWWu || std::fabs(drawn.y - me.pos.y) > tuning::kFrameHalfHWu) continue;
                const i32 j = hw.indexOfSerial(ce.serial[i]);
                if (j < 0 || (he.flags[j] & kFlagDead) || he.state[j] == kWarping) {
                    ++missing;
                    continue;
                }
                const float err = length(drawn - Vec2{he.x[j], he.y[j]});
                hist->add(err);
                {
                    TrueDiag& g = *tdiag;
                    g.all.add(err);
                    g.byTrack[hw.phrase().track & 15u].add(err);
                    if (hw.boss().active) g.byBoss[static_cast<u32>(hw.boss().kind) & 31u].add(err);
                    if (hw.boss().active && hw.boss().kind == BossKind::EventHorizon && cw.boss().active && cw.boss().kind == BossKind::EventHorizon) {
                        (ce.state[i] == kLane ? g.hzLane : g.hzOther).add(err);
                        if (cw.boss().hz.phase != hw.boss().hz.phase) g.hzPhaseDiffers.add(err);
                        const float wellOff = length(cw.boss().pos - hw.boss().pos);
                        if (wellOff > 10.0f) g.hzPosDiffers.add(err);
                        g.hzPosSum2 += static_cast<double>(wellOff) * wellOff;
                        ++g.hzPosN;
                    }
                    if (he.traits[j] & kEliteBit) {
                        g.elite.add(err);
                        for (u32 t = 0; t < kTraitCount; ++t)
                            if (hasTrait(he.traits[j], t)) g.byTrait[t].add(err);
                    }
                    if (hw.arenaEvent().kind < kEventKinds && hw.arenaEvent().stage == 1) g.byEvent[hw.arenaEvent().kind].add(err);
                    {
                        const ArenaBody* ab = hw.arenaBodies();
                        const Vec2 hp{he.x[j], he.y[j]};
                        for (u32 q = 0; q < World::kArenaBodies; ++q) {
                            if (ab[q].kind > 8 || !ab[q].alive) continue;
                            const float r = ab[q].kind < kStructKinds ? hw.species().arena.radius[ab[q].kind] : 120.0f;
                            if (length(hp - ab[q].pos) <= r + 150.0f) {
                                g.nearStruct[ab[q].kind].add(err);
                                break;
                            }
                        }
                    }
                    if (hw.boss().active && hw.boss().kind == BossKind::Feedback) {
                        if (ce.target[i] != he.target[j]) g.fbTarget.add(err);
                        if (ce.state[i] != he.state[j]) g.fbState.add(err);
                        if (ce.flags[i] & kFlagCalled) g.fbCalled.add(err);
                        if (std::fabs(ce.kx[i]) + std::fabs(ce.ky[i]) + std::fabs(he.kx[j]) + std::fabs(he.ky[j]) > 20.0f) g.fbKnocked.add(err);
                        g.fbFamily[static_cast<u32>(cw.species().get(ce.species[i]).family) & 15u].add(err);
                    }
                    g.simPos.add(length(Vec2{ce.x[i] + (ce.vx[i] + ce.kx[i]) * dx, ce.y[i] + (ce.vy[i] + ce.ky[i]) * dx} - Vec2{he.x[j], he.y[j]}));
                    g.byFamily[static_cast<u32>(cw.species().get(ce.species[i]).family) & 15u].add(err);
                    g.byState[ce.state[i] & 15u].add(err);
                    if (ce.target[i] != he.target[j]) g.targetDiffers.add(err);
                    if (ce.state[i] != he.state[j]) {
                        g.stateDiffers.add(err);
                        g.statePair[he.state[j] & 15u][ce.state[i] & 15u].add(err);
                    }
                    if (std::fabs(ce.kx[i]) + std::fabs(ce.ky[i]) + std::fabs(he.kx[j]) + std::fabs(he.ky[j]) > 20.0f) g.knocked.add(err);
                    if ((ce.frozenUntil[i] > cw.music()) != (he.frozenUntil[j] > hw.music())) g.frozenDiffers.add(err);
                    if (ce.flags[i] & kFlagCalled) g.called.add(err);
                    const u8 ht = he.target[j];
                    g.byTarget[ht >= kMaxPlayers ? 3u : (ht == peers[k].session().seat() ? 0u : (ht == 0 ? 1u : 2u))].add(err);
                    g.bySize[err < 5.0f ? 0 : (err < 20.0f ? 1 : (err < 50.0f ? 2 : (err < 96.0f ? 3 : 4)))].add(err);
                    const double last = peers[k].session().lastCorrection(ce.handle[i] & 0xFFFFu, ce.serial[i]);
                    const float age = last < 0.0 ? 1e9f : static_cast<float>((peers[0].music() - last) / tps);
                    const u32 ab = age < 0.25f ? 0 : (age < 0.5f ? 1 : (age < 1.0f ? 2 : (age < 2.0f ? 3 : (age < 4.0f ? 4 : 5))));
                    g.byAge[ab].add(err);
                    (cw.species().get(ce.species[i]).family == Family::Shoal ? g.shoalAge[ab] : g.otherAge[ab]).add(err);
                    const Vec2 v{he.vx[j] + he.kx[j], he.vy[j] + he.ky[j]};
                    const float v2 = v.x * v.x + v.y * v.y;
                    if (v2 > 60.0f * 60.0f) {
                        const Vec2 dv = drawn - Vec2{he.x[j], he.y[j]};
                        const float tau = (dv.x * v.x + dv.y * v.y) / v2;
                        g.tauSum += tau;
                        g.tau2Sum += static_cast<double>(tau) * tau;
                        ++g.tauN;
                        g.perp.add(length(dv - v * tau));
                    }
                }
                if (ce.flags[i] & kFlagCalled) {
                    calledSum2 += static_cast<double>(err) * err;
                    ++calledN;
                }
                stateSum2[ce.state[i] & 15u] += static_cast<double>(err) * err;
                ++stateN[ce.state[i] & 15u];
                const u32 fam = static_cast<u32>(cw.species().get(ce.species[i]).family) & 15u;
                famSum2[fam] += static_cast<double>(err) * err;
                ++famN[fam];
                winSum2 += static_cast<double>(err) * err;
                ++winN;
                if (err > winMax) {
                    winMax = err;
                    std::snprintf(worst, sizeof worst, "serial %u species %u state %u/%u flags %u/%u client (%.0f %.0f) host (%.0f %.0f)", ce.serial[i],
                                  ce.species[i], ce.state[i], he.state[j], ce.flags[i], he.flags[j], ce.x[i], ce.y[i], he.x[j], he.y[j]);
                }
            }
        }
    }
    const double wallS = (static_cast<double>(std::chrono::steady_clock::now().time_since_epoch().count()) - wall0) * 1e-9;
    const u64 allocs = heapAllocCountThisThread() - allocs0;
    const EnetPoolStats pool = enetPoolStats();
    // The report.
    u32 disconnects = 0, stalls = 0, overflow = 0, census = 0, mismatches = 0, maxDg = 0, queueFull = 0;
    float downMax = 0.0f, upMax = 0.0f, downMean = 0.0f, upMean = 0.0f;
    u32 predicted = 0, confirmed = 0, undone = 0, late = 0, unknown = 0;
    double scrSum2 = 0.0;
    u64 scrN = 0;
    float scrMax = 0.0f;
    u32 expectedDrops = 0;
    for (u32 k = 0; k < players; ++k) {
        const NetPeer& p = peers[k];
        const SessionStats& s = p.session().stats();
        if (off(k)) {  // (the process that died; one away past the wait: its session ended "away too long")
            if (awayPastWait && k == awayPeer)
                std::printf("[net]   peer %u (away past the wait): its session %s (%s)\n", k,
                            p.session().state() == SessionState::Ended ? "ended" : "did NOT end", p.session().endReason());
            continue;
        }
        disconnects += s.disconnects + (p.session().state() == SessionState::Ended ? 1u : 0u);
        if ((dropped || (awayDone && awayPastWait)) && k == 0) expectedDrops = s.disconnects;  // the host's: the one that died or stayed away
        stalls += p.stats().dropped;
        overflow += overflowOf(p.world()) + s.droppedOutgoing;
    {
        const WorldCounters& bc = p.world().counters();
        std::printf("[net]   peer %u enemy bullets: %u fired (the fights %u), %u held by the crowd above %.0f%% of the pool, %u in the crowd's place, %u refused\n", k,
                    bc.bulletsAll, bc.bulletsBoss, bc.bulletsHeld, 100.0 * sim::tuning::kBulletPoolCrowd, bc.bulletsEvicted, bc.bulletsDropped);
    }
        if (overflowOf(p.world()) + s.droppedOutgoing) {
            const WorldCounters& oc = p.world().counters();
            std::printf("[net]   peer %u overflow: enemies %u, bullets %u, shots %u, Shards %u, events %u, net ops %u, net log %u, "
                        "outgoing %u\n",
                        k, oc.enemiesDropped, oc.bulletsDropped, oc.shotsDropped, oc.shardsDropped, oc.eventsDropped, oc.netOpsDropped,
                        oc.netLogDropped, s.droppedOutgoing);

        }
        const WireStats ws = p.session().link().wire();
        maxDg = std::max(maxDg, ws.maxDatagram);
        if (awayDone && k == awayPeer) {
            // (its inbox filled while its app was stopped, as a phone's socket buffer would: not the network's fault)
            std::printf("[net]   peer %u (away) datagrams that found its inbox full: %u\n", k, ws.queueFull);
            queueFull += ws.oversize;
        } else {
            queueFull += ws.queueFull + ws.oversize;
        }
        const WorldCounters& c = p.world().counters();
        if (k > 0) {
            census += c.censusChecks;
            mismatches += c.censusMismatches;
            downMax = std::fmax(downMax, rep[k].downMax);
            upMax = std::fmax(upMax, rep[k].upMax);
            if (rep[k].seconds) {
                downMean = std::fmax(downMean, static_cast<float>(rep[k].downSum / rep[k].seconds));
                upMean = std::fmax(upMean, static_cast<float>(rep[k].upSum / rep[k].seconds));
            }
            scrSum2 += s.screenErrSum2;
            scrN += s.screenErrN;
            scrMax = std::fmax(scrMax, s.screenErrMax);
            predicted += c.predicted;
            confirmed += c.confirmed;
            undone += c.undone;
            late += c.lateSpawns;
            unknown += c.unknownIds;
        }
    }
    const u64 poolFallbacks = pool.fallbacks - pool0.fallbacks;
    const float errRms = hist->rms(), errP95 = hist->pct(0.95);
    const float scrRms = scrN ? static_cast<float>(std::sqrt(scrSum2 / static_cast<double>(scrN))) : 0.0f;
    const float songRms = songN ? static_cast<float>(std::sqrt(songSum2 / static_cast<double>(songN))) : 0.0f;
    const World& hw = peers[0].world();
    const WorldCounters& hc = hw.counters();
    const bool pauseOk = !pause || (pausedSeen && pauseState == 3);
    bool pauseSame = true;
    for (u32 k = 1; k < players && pause; ++k) pauseSame = pauseSame && std::fabs(pauseTicks[k] - pauseTicks[0]) < 0.5;
    // The host away past the wait: every client gives up on it ("the host is gone") and the host's own session ends on its return.
    const bool hostGone = awayDone && awayPastWait && awayPeer == 0;
    bool clientsGaveUp = hostGone;
    for (u32 k = 1; k < players && hostGone; ++k) {
        const bool ended = peers[k].session().state() == SessionState::Ended && std::strcmp(peers[k].session().endReason(), "the host is gone") == 0;
        clientsGaveUp = clientsGaveUp && ended;
        disconnects -= peers[k].session().stats().disconnects + (peers[k].session().state() == SessionState::Ended ? 1u : 0u);
    }
    if (hostGone) std::printf("[net]   the clients gave up on the long-away host: %s\n", clientsGaveUp ? "yes" : "NO");
    const bool expectDrop = dropped || (awayDone && awayPastWait && !hostGone);
    if (expectDrop) {
        std::printf("[net]   the host dropped the %s peer: %s (%u disconnects seen by the host)\n", dropped ? "dead" : "long-away",
                    expectedDrops == 1 ? "yes" : "NO", expectedDrops);
        disconnects -= expectedDrops;
    }
    const bool awayEnded = !(awayDone && awayPastWait) || peers[awayPeer].session().state() == SessionState::Ended;
    const bool ok = (!expectDrop || expectedDrops == 1) && awayEnded && disconnects == 0 && stalls == 0 && mismatches == 0 && census > 0 && errRms < kMaxErrRmsWu &&
                    songMax < kMaxSongOffsetMs && downMax <= kMaxDownKbps && upMax <= kMaxUpKbps && maxDg <= kMaxDatagramNoFrag &&
                    overflow == 0 && queueFull == 0 && poolFallbacks == 0 && allocs == 0 && pauseOk && pauseSame &&
                    (g_awayAtS <= 0.0f || (hostGone && clientsGaveUp) ||
                     (awayDone && awayResumed && (awayPastWait || (awayAllPaused && awaySameTick && awayResumedAt - awayBackAt < 15000000))));
    // Diagnostics (docs/PROGRESS_m4_net.md), before the summary: the true error broken down, the clients' corrections and poses.
    tdiag->print("the true on-screen error broken down");
    {
        static const char* const kCls[3] = {"the host's view of a client", "a client's view of the host", "a client's view of a relayed client"};
        for (u32 c = 0; c < 3; ++c)
            std::printf("[net]   remote ships, %s: drawn against the truth RMS %.2f wu, P95 %.2f, max %.1f (%llu); the largest step its motion does "
                        "not explain %.1f wu, %llu of %llu frames over half a hull (%.0f wu)\n",
                        kCls[c], static_cast<double>(ships[c].err.rms()), static_cast<double>(ships[c].err.pct(0.95)), static_cast<double>(ships[c].err.max),
                        static_cast<unsigned long long>(ships[c].err.n), static_cast<double>(ships[c].maxStep),
                        static_cast<unsigned long long>(ships[c].stepsOverHalfHull), static_cast<unsigned long long>(ships[c].steps),
                        static_cast<double>(kPlayerVisualRadius));
        for (u32 c = 0; c < 3; ++c)
            std::printf("[net]   remote aims, %s: the turn in a frame its own turn does not explain RMS %.1f deg, max %.0f deg, %llu frames over 30 deg\n",
                        kCls[c], ships[c].steps ? std::sqrt(ships[c].aimTurnSum2 / static_cast<double>(ships[c].steps)) * 180.0 / kPi : 0.0,
                        static_cast<double>(ships[c].aimTurnMax) * 180.0 / kPi, static_cast<unsigned long long>(ships[c].aimTurnsOver30));
        std::printf("[net]   the players' own aims (each on its own device): %llu of %llu frames turn more than 30 deg\n",
                    static_cast<unsigned long long>(ownAimFlicks), static_cast<unsigned long long>(ownAimFrames));
    }
    for (u32 k = 1; k < players; ++k)
        if (!(off(k)))
            std::printf("[net]   client %u's worst up second: %.1f kbit/s at %.0f s (reliable %.1f, unreliable %.1f, control %.1f payload; %u "
                        "datagrams), down worst %.1f at %.0f s\n",
                        k, rep[k].upMax, static_cast<double>(rep[k].upMaxAt - runFrom) * 1e-6, rep[k].upRel, rep[k].upUnrel, rep[k].upCtrl,
                        rep[k].upDatagrams, rep[k].downMax, static_cast<double>(rep[k].downMaxAt - runFrom) * 1e-6);
    for (u32 k = 1; k < players; ++k) {
        if (off(k)) continue;
        const SessionStats& st = peers[k].session().stats();
        std::printf("[net]   client %u's claims: %llu items sent, %llu hits, the damage changes %llu times in order; boss claims %llu "
                    "(at most %llu a tick); the most in one second: claims %u, boss claims %u\n", k,
                    static_cast<unsigned long long>(st.claimItems), static_cast<unsigned long long>(st.claimHits),
                    static_cast<unsigned long long>(st.claimDmgRuns), static_cast<unsigned long long>(st.bossClaimItems),
                    static_cast<unsigned long long>(st.bossClaimMax), st.claimSecMax, st.bossClaimSecMax);
    }
    {
        for (u32 k = 1; k < players; ++k)
            if (!(off(k))) mergeDiag(*cdiag, peers[k].session().diag());
        char line[1536];
        formatDiagCorrections(*cdiag, line, sizeof line);
        std::printf("[net]   the clients' %s\n", line);
        formatDiagPoses(*cdiag, players, 99, line, sizeof line);
        std::printf("[net]   the clients' %s\n", line);
    }
    std::printf("[net] soak: %u peers on the loopback network, %.1f simulated minutes in %.1f s; conditions: delay %.0f ms, "
                "jitter %.0f ms, loss %.1f%%, duplicates %.1f%%, reordered %.1f%%\n",
                players, minutes, wallS, sim.delayMs, sim.jitterMs, sim.loss * 100.0f, sim.duplicate * 100.0f, sim.reorder * 100.0f);
    std::printf("[net]   run: %u kills, %u downs, %u revives, %u restarts, Track %u, bar %u, %u enemies alive at the end\n", hc.kills,
                hc.downs, hc.revives, hc.restarts, hw.phrase().track, hw.phrase().bar, hw.enemies().count);
    std::printf("[net]   disconnects %u, stalls %u; census %u checks, %u mismatches; predicted kills %u (confirmed %u, undone %u); "
                "late spawns %u, unknown ids %u; enemies on a client's screen not on the host's %llu\n",
                disconnects, stalls, census, mismatches, predicted, confirmed, undone, late, unknown, static_cast<unsigned long long>(missing));
    for (u32 k = 1; k < players; ++k) {
        // Census diagnostics: the deaths client k heard for enemies it never had, as the host had them.
        const WorldCounters& c = peers[k].world().counters();
        for (u32 q = 0; q < c.unknownIds && q < 8u; ++q) {
            const u32 sn = c.unknownSerial[q];
            const HostSeen& r = g_seen[sn & (kSeenRing - 1u)];
            if (r.serial != sn) {
                std::printf("[net]   client %u heard the death of %u, never seen on the host either\n", k, sn);
                continue;
            }
            std::printf("[net]   client %u heard the death of %u, which it never had: species %u family %u state %u flags 0x%02x traits "
                        "0x%04x group %u, Track %u, fight %s, first seen at music %.0f\n", k, sn, r.species,
                        static_cast<u32>(peers[0].world().species().get(r.species).family), r.state, r.flags, r.traits, r.group, r.track,
                        r.boss != 0xFF ? bossId(static_cast<BossKind>(r.boss)) : "none", r.firstMusic);
        }
    }
    std::printf("[net]   on-screen enemy position error against the host: RMS %.2f wu, P95 %.2f, max %.1f (%llu samples; limit RMS %.0f)\n",
                errRms, errP95, hist->max, static_cast<unsigned long long>(hist->n), kMaxErrRmsWu);
    std::printf("[net]   of which a mini-boss's called bodies: RMS %.2f wu (%llu samples)\n", calledN ? std::sqrt(calledSum2 / static_cast<double>(calledN)) : 0.0,
                static_cast<unsigned long long>(calledN));
    std::printf("[net]   the same as each correction lands (drawn against the host's sample): RMS %.2f wu, max %.1f (%llu)\n", scrRms,
                scrMax, static_cast<unsigned long long>(scrN));
    std::printf("[net]   song clock offset against the host: RMS %.2f ms, max %.2f ms (limit %.0f)\n", songRms, songMax, kMaxSongOffsetMs);
    std::printf("[net]   thinning: a copy's against its owner's differs in %llu of %llu looks (owner thinned in %llu); live shots off by %.1f RMS\n",
                static_cast<unsigned long long>(thinDiffers), static_cast<unsigned long long>(thinChecks),
                static_cast<unsigned long long>(thinOwnerOn), thinChecks ? std::sqrt(thinLiveSum2 / static_cast<double>(thinChecks)) : 0.0);
    std::printf("[net]   at correction (the device runs' scr) by fight:");
    for (u32 k = 0; k < 32; ++k)
        if (scrBossN[k])
            std::printf(" %s %.1f (%llu)", k == 31 ? "none" : bossId(static_cast<BossKind>(k)),
                        std::sqrt(scrBoss2[k] / static_cast<double>(scrBossN[k])), static_cast<unsigned long long>(scrBossN[k]));
    std::printf("\n");
    std::printf("[net]   boss health, a client's against the host's: RMS %.2f%% of the whole, max %.2f%% (%llu looks, %llu above the "
                "host's); looks with the host's boss dying and the client's not yet: %llu\n",
                bossHpN ? 100.0 * std::sqrt(bossHpSum2 / static_cast<double>(bossHpN)) : 0.0, 100.0 * bossHpMax,
                static_cast<unsigned long long>(bossHpN), static_cast<unsigned long long>(bossHpAbove),
                static_cast<unsigned long long>(bossDeathLate));
    std::printf("[net]   shots on the host's screen, straight ones bent more than a quarter in their last tick: own %llu of %llu (first tick "
                "%llu), remote %llu of %llu (first tick %llu); curving own %llu, remote %llu\n",
                static_cast<unsigned long long>(shBent[0]), static_cast<unsigned long long>(shStraight[0]),
                static_cast<unsigned long long>(shBentFirst[0]), static_cast<unsigned long long>(shBent[1]),
                static_cast<unsigned long long>(shStraight[1]), static_cast<unsigned long long>(shBentFirst[1]),
                static_cast<unsigned long long>(shCurve[0]), static_cast<unsigned long long>(shCurve[1]));
    std::printf("[net]   per client: down %.1f kbit/s worst second (mean %.1f), up %.1f worst second (mean %.1f); limits %.0f / %.0f\n",
                downMax, downMean, upMax, upMean, kMaxDownKbps, kMaxUpKbps);
    std::printf("[net]   largest datagram %u bytes (no IP fragmentation: <= %u); overflow %u, queue full %u, ENet pool fallbacks %llu "
                "(peaks %u/%u/%u/%u/%u/%u); heap allocations in the steady state %llu\n",
                maxDg, kMaxDatagramNoFrag, overflow, queueFull, static_cast<unsigned long long>(poolFallbacks), pool.peak[0], pool.peak[1],
                pool.peak[2], pool.peak[3], pool.peak[4], pool.peak[5], static_cast<unsigned long long>(allocs));
    if (pause) std::printf("[net]   pause: %s, every peer on the same tick: %s\n", pauseOk ? "held and resumed" : "FAILED", pauseSame ? "yes" : "NO");
    if (g_awayAtS > 0.0f)
        std::printf("[net]   away (peer %u, %.0f s): the room held %s, on one tick %s, played on %s (%.1f s after the return)\n", awayPeer,
                    static_cast<double>(g_awayForS), awayAllPaused ? "yes" : "NO", awaySameTick ? "yes" : "NO", awayResumed ? "yes" : "NO",
                    awayResumed ? static_cast<double>(awayResumedAt - awayBackAt) * 1e-6 : 0.0);
    std::printf("[net] soak %s\n", ok ? "PASS" : "FAIL");
    logTagged("RFNET",
              "{\"test\":\"soak\",\"players\":%u,\"minutes\":%.1f,\"loss\":%.3f,\"jitter_ms\":%.0f,\"disconnects\":%u,\"stalls\":%u,"
              "\"census\":%u,\"census_mismatches\":%u,\"err_rms_wu\":%.3f,\"err_p95_wu\":%.3f,\"err_max_wu\":%.2f,\"song_rms_ms\":%.3f,"
              "\"song_max_ms\":%.3f,\"down_kbps_max\":%.1f,\"up_kbps_max\":%.1f,\"down_kbps_mean\":%.1f,\"up_kbps_mean\":%.1f,"
              "\"max_datagram\":%u,\"overflow\":%u,\"pool_fallbacks\":%llu,\"heap_allocs\":%llu,\"kills\":%u,\"predicted\":%u,"
              "\"undone\":%u,\"err_at_corr_rms_wu\":%.3f,\"err_at_corr_max_wu\":%.2f,\"pass\":%s}",
              players, minutes, sim.loss, sim.jitterMs, disconnects, stalls, census, mismatches, errRms, errP95, hist->max, songRms, songMax,
              downMax, upMax, downMean, upMean, maxDg, overflow, static_cast<unsigned long long>(poolFallbacks),
              static_cast<unsigned long long>(allocs), hc.kills, predicted, undone, scrRms, scrMax, ok ? "true" : "false");
    for (u32 k = 0; k < players; ++k)
        if (!(off(k))) peers[k].stop();
    delete[] peers;
    delete hist;
    delete tdiag;
    delete cdiag;
    delete[] truth;
    delete[] ships;
    wireUseSimClock(false);
    return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------- one peer over UDP

int runPeer(const SpeciesTable& species, bool host, int argc, char** argv) {
    PeerConfig c;
    c.session.role = host ? NetRole::Host : NetRole::Client;
    c.session.remotePoses = g_remotePoses;
    tune(c.session);
    const char* v = nullptr;
    c.session.seats = (v = arg(argc, argv, "--players")) ? static_cast<u32>(std::atoi(v)) : 4u;
    c.session.port = (v = arg(argc, argv, "--port")) ? static_cast<u16>(std::atoi(v)) : kGamePort;
    if ((v = arg(argc, argv, "--bind"))) std::snprintf(c.session.bindIp, sizeof c.session.bindIp, "%s", v);
    if (!host) {
        if (argc < 4) {
            std::printf("[net] join: the host's address is missing\n");
            return 2;
        }
        std::snprintf(c.session.joinIp, sizeof c.session.joinIp, "%s", argv[3]);
    }
    if ((v = arg(argc, argv, "--netsim")) && !parseNetSim(v, c.session.sim)) {
        std::printf("[net] bad --netsim '%s'\n", v);
        return 2;
    }
    c.session.seed = (v = arg(argc, argv, "--seed")) ? std::strtoull(v, nullptr, 10) : 1ull;
    std::snprintf(c.session.name, sizeof c.session.name, "%s", (v = arg(argc, argv, "--name")) ? v : (host ? "HOST" : "BOT"));
    c.botSeed = c.session.seed * 31 + static_cast<u64>(std::rand() % 1000);
    c.clockOffsetUs = (v = arg(argc, argv, "--clock-offset-ms")) ? static_cast<i64>(std::atof(v) * 1000.0) : 0;
    c.clockPpm = (v = arg(argc, argv, "--clock-ppm")) ? std::atof(v) : 0.0;
    const bool sameClock = flag(argc, argv, "--same-clock");
    const float minutes = (v = arg(argc, argv, "--minutes")) ? static_cast<float>(std::atof(v)) : 15.0f;
    // --wait S (join): a host that is not there yet is tried again for up to S seconds (a phone still starting).
    const float waitS = (v = arg(argc, argv, "--wait")) ? static_cast<float>(std::atof(v)) : 0.0f;
    const i64 waitUntil = wireNowUs() + static_cast<i64>(waitS * 1e6f);
    auto* peer = new (std::nothrow) NetPeer;
    if (!peer || !peer->start(c, &species)) {
        std::printf("[net] could not %s (address in use, or no network)\n", host ? "host" : "join");
        return 2;
    }
    std::printf("[net] %s: %s port %u, %u seats; conditions delay %.0f jitter %.0f loss %.3f\n", host ? "host" : "join",
                host ? (c.session.bindIp[0] ? c.session.bindIp : "every address") : c.session.joinIp, c.session.port, c.session.seats,
                c.session.sim.delayMs, c.session.sim.jitterMs, c.session.sim.loss);
    using clock = std::chrono::steady_clock;
    const i64 frameUs = 16667;
    i64 next = wireNowUs();
    i64 runFrom = 0, lastLine = 0, lastSecond = 0;
    const i64 settleUs = 20000000;
    PeerReport rep;
    double songSum2 = 0.0, songMax = 0.0;
    u64 songN = 0, allocs0 = 0;
    u64 lineScrN = 0;
    double lineScrSum2 = 0.0;
    bool measuring = false;
    EnetPoolStats pool0, poolEnd;
    bool ended = false;
    i64 endAt = 0;
    SessionStats st;
    NetPeer::Stats ps;
    WorldCounters wc;
    WireStats ws;
    u32 overflowEnd = 0;
    double endMinutes = 0.0;
    NetDiag diagTotal;
    for (;;) {
        const i64 now = wireNowUs();
        if (now < next) {
            std::this_thread::sleep_for(std::chrono::microseconds(next - now > 2000 ? next - now - 1000 : 0));
            continue;
        }
        next += frameUs;
        if (now - next > 200000) next = now + frameUs;  // a long hiccup: start counting again
        peer->frame(now);
        Session& s = peer->session();
        if (s.state() == SessionState::Ended) {
            if (!host && !runFrom && now < waitUntil) {
                peer->stop();
                if (!peer->start(c, &species)) break;
                continue;
            }
            std::printf("[net] the session ended: %s\n", s.endReason());
            break;
        }
        if (!runFrom && s.running() && peer->music() > 0.0 && peer->world().runId() > 0) runFrom = now;
        if (!runFrom) {
            if (now - lastLine > 2000000) {
                lastLine = now;
                std::printf("[net]   waiting: state %u, seats 0x%x, %u peers\n", static_cast<unsigned>(s.state()), s.seatMask(), s.stats().peers);
            }
            continue;
        }
        const i64 t = now - runFrom;
        if (t >= static_cast<i64>(minutes * 60e6f)) {
            // The measured window ends; the peer stays a few seconds more, so the others end theirs before it goes.
            if (!ended) {
                ended = true;
                endAt = now + 4000000;
                st = s.stats();
                ps = peer->stats();
                wc = peer->world().counters();
                overflowEnd = overflowOf(peer->world()) + st.droppedOutgoing;
                ws = s.link().wire();
                poolEnd = enetPoolStats();
                endMinutes = static_cast<double>(t) / 60e6;
            }
            if (now >= endAt) break;
            continue;
        }
        if (!measuring && t >= settleUs) {
            measuring = true;
            peer->session().resetScreenError();
            peer->session().resetDiag();
            peer->measureAllocs(true);
            allocs0 = peer->stats().heapAllocs;
            pool0 = enetPoolStats();
        }
        if (!measuring) continue;
        secondly(*peer, rep, now, lastSecond);
        if (!host && sameClock && s.songValid()) {
            // The true offset: this client's song against the host's at the true moment (one clock on one PC; the
            // host runs on it unshifted).
            const double mine = peer->songNow(now), theirs = s.songAt(now);
            const double ms = (mine - theirs) / s.tpsAt(now) * 1000.0;
            songSum2 += ms * ms;
            ++songN;
            if (std::fabs(ms) > songMax) songMax = std::fabs(ms);
        }
        if (now - lastLine > 10000000) {
            lastLine = now;
            const SessionStats& st = s.stats();
            // "err": every correction of the last second (off screen too); "scr": the enemies on this screen as each
            // correction landed since the last line (M4 stage B: what a player could see).
            const double scrN = static_cast<double>(st.screenErrN - lineScrN), scrRms = scrN > 0.0 ? std::sqrt((st.screenErrSum2 - lineScrSum2) / scrN) : 0.0;
            lineScrN = st.screenErrN;
            lineScrSum2 = st.screenErrSum2;
            std::printf("[net] %5.0f s  Track %u bar %4u  enemies %4u  rtt %.0f ms  loss %.1f%%  down %.1f up %.1f kbit/s  corr %.0f/s err "
                        "%.1f wu  scr %.1f wu (%.0f)  clock %.2f ms (rtt %.1f)  census %u/%u  peers %u  players %u\n",
                        static_cast<double>(t) * 1e-6, peer->world().phrase().track, peer->world().phrase().bar, peer->world().enemies().count,
                        st.rttMs, st.loss * 100.0f, st.downKbps, st.upKbps, st.correctionsPerS, st.errRmsWu, scrRms, scrN, st.clockOffsetMs,
                        st.clockRttMs, st.censusChecks, st.censusMismatches, st.peers,
                        static_cast<u32>(peer->world().player(0).present) + peer->world().player(1).present + peer->world().player(2).present +
                            peer->world().player(3).present);
            // Diagnostics (docs/PROGRESS_m4_net.md): the 10 s window's corrections and poses, then the next window.
            char line[1536];
            if (!host) {
                const WorldCounters& wc2 = peer->world().counters();
                std::printf("[net]        spawns landed late (arrived after their landing) %u in all; predicted kills %u, undone %u; motion "
                            "corrections %u in all\n",
                            wc2.lateSpawns, wc2.predicted, wc2.undone, s.stats().motions);
                formatDiagCorrections(s.diag(), line, sizeof line);
                std::printf("[net]        %s\n", line);
            }
            formatDiagPoses(s.diag(), s.seats(), s.seat(), line, sizeof line);
            std::printf("[net]        %s\n", line);
            formatDiagTicks(s.diag(), line, sizeof line);
            std::printf("[net]        %s\n", line);
            mergeDiag(diagTotal, s.diag());
            s.resetDiag();
        }
    }
    if (!ended) {  // the session ended early, or never ran
        st = peer->session().stats();
        ps = peer->stats();
        wc = peer->world().counters();
        overflowEnd = overflowOf(peer->world()) + st.droppedOutgoing;
        ws = peer->session().link().wire();
        poolEnd = enetPoolStats();
        endMinutes = runFrom ? static_cast<double>(wireNowUs() - runFrom) / 60e6 : 0.0;
    }
    const u64 allocs = ps.heapAllocs - allocs0;
    const u64 fallbacks = poolEnd.fallbacks - pool0.fallbacks;
    const u32 overflow = overflowEnd;
    const float songRms = songN ? static_cast<float>(std::sqrt(songSum2 / static_cast<double>(songN))) : 0.0f;
    const float scrRms = st.screenErrN ? static_cast<float>(std::sqrt(st.screenErrSum2 / static_cast<double>(st.screenErrN))) : 0.0f;
    const float downMean = rep.seconds ? static_cast<float>(rep.downSum / rep.seconds) : 0.0f;
    const float upMean = rep.seconds ? static_cast<float>(rep.upSum / rep.seconds) : 0.0f;
    const bool clientOk = host || (rep.downMax <= kMaxDownKbps && rep.upMax <= kMaxUpKbps && wc.censusMismatches == 0 &&
                                   wc.censusChecks > 0 && scrRms < kMaxErrRmsWu && (!sameClock || songMax < kMaxSongOffsetMs));
    const bool ok = runFrom && st.disconnects == 0 && ps.dropped == 0 && overflow == 0 && ws.maxDatagram <= kMaxDatagramNoFrag &&
                    fallbacks == 0 && allocs == 0 && clientOk;
    std::printf("[net] %s: %.1f minutes run; disconnects %u, stalls %u; census %u/%u mismatches; corrections' error (all of them) "
                "max %.1f wu\n",
                host ? "host" : "client", endMinutes, st.disconnects,
                ps.dropped, wc.censusChecks, wc.censusMismatches, rep.corrErrMax);
    std::printf("[net]   down %.1f kbit/s worst second (mean %.1f), up %.1f (mean %.1f); largest datagram %u; overflow %u; pool fallbacks "
                "%llu; heap allocations in the steady state %llu\n",
                rep.downMax, downMean, rep.upMax, upMean, ws.maxDatagram, overflow, static_cast<unsigned long long>(fallbacks),
                static_cast<unsigned long long>(allocs));
    if (runFrom)
        std::printf("[net]   the worst second down ended %.1f s into the run, up %.1f s (%u peers connected then, the longest silent %u ms, "
                    "%u reliable bytes unacknowledged); the run's window ended %.1f s in\n",
                    static_cast<double>(rep.downMaxAt - runFrom) * 1e-6, static_cast<double>(rep.upMaxAt - runFrom) * 1e-6, rep.upPeers,
                    rep.upSilentMs, rep.upInFlight, endMinutes * 60.0);
    if (!host)
        std::printf("[net]   on-screen enemies as each correction lands, drawn against the host's sample: RMS %.2f wu, max %.1f (%llu)\n",
                    scrRms, st.screenErrMax, static_cast<unsigned long long>(st.screenErrN));
    if (!host && sameClock) std::printf("[net]   true song clock offset: RMS %.2f ms, max %.2f ms\n", songRms, songMax);
    {
        char line[1536];
        if (!host) {
            formatDiagCorrections(diagTotal, line, sizeof line);
            std::printf("[net]   the whole run's %s\n", line);
        }
        formatDiagPoses(diagTotal, peer->session().seats(), peer->session().seat(), line, sizeof line);
        std::printf("[net]   the whole run's %s\n", line);
        formatDiagTicks(diagTotal, line, sizeof line);
        std::printf("[net]   the whole run's %s\n", line);
    }
    std::printf("[net] %s %s\n", host ? "host" : "client", ok ? "PASS" : "FAIL");
    logTagged("RFNET",
              "{\"test\":\"%s\",\"minutes\":%.2f,\"disconnects\":%u,\"stalls\":%u,\"census\":%u,\"census_mismatches\":%u,\"song_rms_ms\":%.3f,"
              "\"song_max_ms\":%.3f,\"down_kbps_max\":%.1f,\"up_kbps_max\":%.1f,\"down_kbps_mean\":%.1f,\"up_kbps_mean\":%.1f,"
              "\"corr_err_max_wu\":%.2f,\"max_datagram\":%u,\"overflow\":%u,\"pool_fallbacks\":%llu,\"heap_allocs\":%llu,\"kills\":%u,"
              "\"err_at_corr_rms_wu\":%.3f,\"err_at_corr_max_wu\":%.2f,\"pass\":%s}",
              host ? "udp_host" : "udp_client", endMinutes, st.disconnects,
              ps.dropped, wc.censusChecks, wc.censusMismatches, songRms, songMax, rep.downMax, rep.upMax, downMean, upMean,
              rep.corrErrMax, ws.maxDatagram, overflow, static_cast<unsigned long long>(fallbacks), static_cast<unsigned long long>(allocs),
              wc.kills, scrRms, st.screenErrMax, ok ? "true" : "false");
    peer->stop();
    delete peer;
    (void)clock::now();
    return ok ? 0 : 1;
}

}  // namespace

int netHeadlessMain(int argc, char** argv) {
    const char* mode = argc > 2 ? argv[2] : "";
    auto* species = new (std::nothrow) SpeciesTable;
    if (!species || !species->load()) return 2;
    int code = 2;
    if ((g_tune = arg(argc, argv, "--net-tune"))) {
        SessionConfig probe;
        if (!parseNetTune(g_tune, probe)) {
            std::printf("[net] bad --net-tune '%s' (down=B,famN=W,...)\n", g_tune);
            delete species;
            return 2;
        }
    }
    if (const char* fid = arg(argc, argv, "--fight")) {
        // The soak started at a fight (the lead's short runs): the run begins in its home Track, measuring from the fight.
        if (!bossFromId(fid, g_fightKind) || !fightBuilt(g_fightKind)) {
            std::printf("[net] --fight '%s': no such fight built\n", fid);
            delete species;
            return 2;
        }
        g_fightOn = true;
    }
    if (const char* rp = arg(argc, argv, "--remote-poses")) {
        if (std::strcmp(rp, "predict") == 0) g_remotePoses = RemotePoses::Predict;
        else if (std::strcmp(rp, "direct") == 0) g_remotePoses = RemotePoses::Direct;
        else if (std::strcmp(rp, "interp") == 0) g_remotePoses = RemotePoses::Interpolate;
        else {
            std::printf("[net] --remote-poses interp|predict|direct\n");
            delete species;
            return 2;
        }
    }
    if (std::strcmp(mode, "soak") == 0) {
        const char* v = nullptr;
        const float minutes = (v = arg(argc, argv, "--minutes")) ? static_cast<float>(std::atof(v)) : 10.0f;
        const u32 players = (v = arg(argc, argv, "--players")) ? static_cast<u32>(std::atoi(v)) : 4u;
        const u64 seed = (v = arg(argc, argv, "--seed")) ? std::strtoull(v, nullptr, 10) : 1ull;
        NetSim sim;
        if ((v = arg(argc, argv, "--netsim")) && !parseNetSim(v, sim)) {
            std::printf("[net] bad --netsim '%s'\n", v);
            delete species;
            return 2;
        }
        const float dropAt = (v = arg(argc, argv, "--drop-at")) ? static_cast<float>(std::atof(v)) : 0.0f;
        if ((v = arg(argc, argv, "--away-at"))) g_awayAtS = static_cast<float>(std::atof(v));
        if ((v = arg(argc, argv, "--away-for"))) g_awayForS = static_cast<float>(std::atof(v));
        if ((v = arg(argc, argv, "--away-seat"))) g_awaySeat = static_cast<u32>(std::atoi(v));
        code = runSoak(*species, minutes, players, sim, seed, flag(argc, argv, "--pause"), dropAt);
    } else if (std::strcmp(mode, "host") == 0 || std::strcmp(mode, "join") == 0) {
        code = runPeer(*species, std::strcmp(mode, "host") == 0, argc, argv);
    } else {
        std::printf("usage: raveform_headless --net soak|host|join [<ip>] [--minutes M] [--players N] [--netsim loss=..,jitter=..] ...\n");
    }
    delete species;
    return code;
}
