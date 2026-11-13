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

using namespace rf;
using namespace rf::net;
using namespace rf::sim;

namespace {

// Limits of the M4 stage A acceptance (TECH_SPEC 8.4, PLAN 7).
constexpr float kMaxErrRmsWu = 12.0f;
constexpr float kMaxSongOffsetMs = 10.0f;
constexpr float kMaxDownKbps = 250.0f, kMaxUpKbps = 60.0f;
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
    if (!peers || !hist || !tdiag || !cdiag || !truth || !ships) return 2;
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
    PeerReport rep[kMaxPlayers];
    double songSum2 = 0.0, songMax = 0.0;
    u64 songN = 0;
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
            if (!(dropped && k == dropPeer)) wirePump(peers[k].session().link().socket());
        for (u32 k = 0; k < players; ++k) {
            if (dropped && k == dropPeer) continue;
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
        if (all && !runFrom && peers[0].world().runId() > 0 && peers[0].music() > peers[0].world().tickOfBar(0) + 1.0) runFrom = G;
        if (!runFrom) {
            if (G > 2000000000ll + 30000000ll) {
                std::printf("[net] the run did not start within 30 s\n");
                return 1;
            }
            continue;
        }
        const i64 t = G - runFrom;
        if (t >= runUs) break;
        if (dropAtS > 0.0f && !dropped && t >= static_cast<i64>(dropAtS * 1e6f)) {
            dropped = true;
            peers[dropPeer].session().linkMut().abandon();
            std::printf("[net] %5.1f s: peer %u's process dies (its link goes without a word)\n", static_cast<double>(t) * 1e-6, dropPeer);
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
        if (!measuring && t >= settleUs) {
            measuring = true;
            allocs0 = heapAllocCountThisThread();
            pool0 = enetPoolStats();
            for (u32 k = 1; k < players; ++k) {
                peers[k].session().resetScreenError();
                peers[k].session().resetDiag();
            }
        }
        for (u32 k = 1; k < players; ++k)
            if (!(dropped && k == dropPeer)) secondly(peers[k], rep[k], G, lastSecond[k]);
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
            if (dropped && k == dropPeer) continue;
            const double ms = (peers[k].songNow(G) - hs) / tps * 1000.0;
            songSum2 += ms * ms;
            ++songN;
            if (std::fabs(ms) > songMax) songMax = std::fabs(ms);
        }
        // On-screen enemies: each client's drawn position against the host's, at the host's musical time.
        if ((G / 100000) % 3 != 0) continue;
        const World& hw = peers[0].world();
        for (u32 k = 1; k < players; ++k) {
            if (dropped && k == dropPeer) continue;
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
                    g.simPos.add(length(Vec2{ce.x[i] + (ce.vx[i] + ce.kx[i]) * dx, ce.y[i] + (ce.vy[i] + ce.ky[i]) * dx} - Vec2{he.x[j], he.y[j]}));
                    g.byFamily[static_cast<u32>(cw.species().get(ce.species[i]).family) & 15u].add(err);
                    g.byState[ce.state[i] & 15u].add(err);
                    if (ce.target[i] != he.target[j]) g.targetDiffers.add(err);
                    if (ce.state[i] != he.state[j]) g.stateDiffers.add(err);
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
        if (dropped && k == dropPeer) continue;  // (the process that died)
        disconnects += s.disconnects + (p.session().state() == SessionState::Ended ? 1u : 0u);
        if (dropped && k == 0) expectedDrops = s.disconnects;  // the host's: the one that died
        stalls += p.stats().dropped;
        overflow += overflowOf(p.world()) + s.droppedOutgoing;
        if (overflowOf(p.world()) + s.droppedOutgoing) {
            const WorldCounters& oc = p.world().counters();
            std::printf("[net]   peer %u overflow: enemies %u, bullets %u, shots %u, Shards %u, events %u, net ops %u, net log %u, "
                        "outgoing %u\n",
                        k, oc.enemiesDropped, oc.bulletsDropped, oc.shotsDropped, oc.shardsDropped, oc.eventsDropped, oc.netOpsDropped,
                        oc.netLogDropped, s.droppedOutgoing);
        }
        const WireStats ws = p.session().link().wire();
        maxDg = std::max(maxDg, ws.maxDatagram);
        queueFull += ws.queueFull + ws.oversize;
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
    if (dropped) {
        std::printf("[net]   the host timed out the dead peer: %s (%u disconnects seen by the host)\n", expectedDrops == 1 ? "yes" : "NO",
                    expectedDrops);
        disconnects -= expectedDrops;
    }
    const bool ok = (!dropped || expectedDrops == 1) && disconnects == 0 && stalls == 0 && mismatches == 0 && census > 0 && errRms < kMaxErrRmsWu &&
                    songMax < kMaxSongOffsetMs && downMax <= kMaxDownKbps && upMax <= kMaxUpKbps && maxDg <= kMaxDatagramNoFrag &&
                    overflow == 0 && queueFull == 0 && poolFallbacks == 0 && allocs == 0 && pauseOk && pauseSame;
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
        if (!(dropped && k == dropPeer))
            std::printf("[net]   client %u's worst up second: %.1f kbit/s at %.0f s (reliable %.1f, unreliable %.1f, control %.1f payload; %u "
                        "datagrams), down worst %.1f at %.0f s\n",
                        k, rep[k].upMax, static_cast<double>(rep[k].upMaxAt - runFrom) * 1e-6, rep[k].upRel, rep[k].upUnrel, rep[k].upCtrl,
                        rep[k].upDatagrams, rep[k].downMax, static_cast<double>(rep[k].downMaxAt - runFrom) * 1e-6);
    {
        for (u32 k = 1; k < players; ++k)
            if (!(dropped && k == dropPeer)) mergeDiag(*cdiag, peers[k].session().diag());
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
    std::printf("[net]   on-screen enemy position error against the host: RMS %.2f wu, P95 %.2f, max %.1f (%llu samples; limit RMS %.0f)\n",
                errRms, errP95, hist->max, static_cast<unsigned long long>(hist->n), kMaxErrRmsWu);
    std::printf("[net]   of which a mini-boss's called bodies: RMS %.2f wu (%llu samples)\n", calledN ? std::sqrt(calledSum2 / static_cast<double>(calledN)) : 0.0,
                static_cast<unsigned long long>(calledN));
    std::printf("[net]   the same as each correction lands (drawn against the host's sample): RMS %.2f wu, max %.1f (%llu)\n", scrRms,
                scrMax, static_cast<unsigned long long>(scrN));
    std::printf("[net]   song clock offset against the host: RMS %.2f ms, max %.2f ms (limit %.0f)\n", songRms, songMax, kMaxSongOffsetMs);
    std::printf("[net]   per client: down %.1f kbit/s worst second (mean %.1f), up %.1f worst second (mean %.1f); limits %.0f / %.0f\n",
                downMax, downMean, upMax, upMean, kMaxDownKbps, kMaxUpKbps);
    std::printf("[net]   largest datagram %u bytes (no IP fragmentation: <= %u); overflow %u, queue full %u, ENet pool fallbacks %llu "
                "(peaks %u/%u/%u/%u/%u/%u); heap allocations in the steady state %llu\n",
                maxDg, kMaxDatagramNoFrag, overflow, queueFull, static_cast<unsigned long long>(poolFallbacks), pool.peak[0], pool.peak[1],
                pool.peak[2], pool.peak[3], pool.peak[4], pool.peak[5], static_cast<unsigned long long>(allocs));
    if (pause) std::printf("[net]   pause: %s, every peer on the same tick: %s\n", pauseOk ? "held and resumed" : "FAILED", pauseSame ? "yes" : "NO");
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
        if (!(dropped && k == dropPeer)) peers[k].stop();
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
        code = runSoak(*species, minutes, players, sim, seed, flag(argc, argv, "--pause"), dropAt);
    } else if (std::strcmp(mode, "host") == 0 || std::strcmp(mode, "join") == 0) {
        code = runPeer(*species, std::strcmp(mode, "host") == 0, argc, argv);
    } else {
        std::printf("usage: raveform_headless --net soak|host|join [<ip>] [--minutes M] [--players N] [--netsim loss=..,jitter=..] ...\n");
    }
    delete species;
    return code;
}
