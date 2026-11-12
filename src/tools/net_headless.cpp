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

struct PeerReport {
    float downMax = 0.0f, upMax = 0.0f;   // the worst second
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

u32 overflowOf(const World& w) {
    const WorldCounters& c = w.counters();
    return c.enemiesDropped + c.bulletsDropped + c.shotsDropped + c.shardsDropped + c.eventsDropped + c.netOpsDropped + c.netLogDropped;
}

void secondly(NetPeer& p, PeerReport& r, i64 when, i64& last) {
    // Each second the session's own figures (bandwidth with IP/UDP headers, correction errors).
    if (when - last < 1000000) return;
    last = when;
    const SessionStats& s = p.session().stats();
    r.downMax = std::fmax(r.downMax, s.downKbps);
    r.upMax = std::fmax(r.upMax, s.upKbps);
    r.downSum += s.downKbps;
    r.upSum += s.upKbps;
    ++r.seconds;
    if (s.errMaxWu > r.corrErrMax) r.corrErrMax = s.errMaxWu;
}

// ---------------------------------------------------------------------------------------------- soak

int runSoak(const SpeciesTable& species, float minutes, u32 players, const NetSim& sim, u64 seed, bool pause) {
    if (players < 2) players = 2;
    if (players > kMaxPlayers) players = kMaxPlayers;
    wireUseSimClock(true);
    i64 G = 2000000000ll;  // true time, microseconds
    wireSetSimTimeUs(G);
    auto* peers = new (std::nothrow) NetPeer[kMaxPlayers];
    auto* hist = new (std::nothrow) Hist;
    if (!peers || !hist) return 2;
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
    for (;;) {
        G += 1000;
        wireSetSimTimeUs(G);
        for (u32 k = 0; k < players; ++k) wirePump(peers[k].session().link().socket());
        for (u32 k = 0; k < players; ++k) {
            if (G < nextFrame[k]) continue;
            peers[k].frame(G);
            jitterRng ^= jitterRng << 13;
            jitterRng ^= jitterRng >> 17;
            jitterRng ^= jitterRng << 5;
            nominal[k] += period[k];
            nextFrame[k] = static_cast<i64>(nominal[k]) + static_cast<i64>(jitterRng % 1001u) - 500;  // vsync +-0.5 ms
        }
        bool all = true;
        for (u32 k = 0; k < players; ++k) all = all && peers[k].session().running();
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
        if (!measuring && t >= settleUs) {
            measuring = true;
            allocs0 = heapAllocCountThisThread();
            pool0 = enetPoolStats();
            for (u32 k = 1; k < players; ++k) peers[k].session().resetScreenError();
        }
        for (u32 k = 1; k < players; ++k) secondly(peers[k], rep[k], G, lastSecond[k]);
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
            const double ms = (peers[k].songNow(G) - hs) / tps * 1000.0;
            songSum2 += ms * ms;
            ++songN;
            if (std::fabs(ms) > songMax) songMax = std::fabs(ms);
        }
        // On-screen enemies: each client's drawn position against the host's, at the host's musical time.
        if ((G / 100000) % 3 != 0) continue;
        const World& hw = peers[0].world();
        for (u32 k = 1; k < players; ++k) {
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
    for (u32 k = 0; k < players; ++k) {
        const NetPeer& p = peers[k];
        const SessionStats& s = p.session().stats();
        disconnects += s.disconnects + (p.session().state() == SessionState::Ended ? 1u : 0u);
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
    const bool ok = disconnects == 0 && stalls == 0 && mismatches == 0 && census > 0 && errRms < kMaxErrRmsWu &&
                    songMax < kMaxSongOffsetMs && downMax <= kMaxDownKbps && upMax <= kMaxUpKbps && maxDg <= kMaxDatagramNoFrag &&
                    overflow == 0 && queueFull == 0 && poolFallbacks == 0 && allocs == 0 && pauseOk && pauseSame;
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
    for (u32 k = 0; k < players; ++k) peers[k].stop();
    delete[] peers;
    delete hist;
    wireUseSimClock(false);
    return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------- one peer over UDP

int runPeer(const SpeciesTable& species, bool host, int argc, char** argv) {
    PeerConfig c;
    c.session.role = host ? NetRole::Host : NetRole::Client;
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
    bool measuring = false;
    EnetPoolStats pool0;
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
        if (t >= static_cast<i64>(minutes * 60e6f)) break;
        if (!measuring && t >= settleUs) {
            measuring = true;
            peer->session().resetScreenError();
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
            std::printf("[net] %5.0f s  Track %u bar %4u  enemies %4u  rtt %.0f ms  loss %.1f%%  down %.1f up %.1f kbit/s  corr %.0f/s err "
                        "%.1f wu  clock %.2f ms (rtt %.1f)  census %u/%u  peers %u\n",
                        static_cast<double>(t) * 1e-6, peer->world().phrase().track, peer->world().phrase().bar, peer->world().enemies().count,
                        st.rttMs, st.loss * 100.0f, st.downKbps, st.upKbps, st.correctionsPerS, st.errRmsWu, st.clockOffsetMs, st.clockRttMs,
                        st.censusChecks, st.censusMismatches, st.peers);
        }
    }
    const SessionStats& st = peer->session().stats();
    const u64 allocs = peer->stats().heapAllocs - allocs0;
    const EnetPoolStats pool = enetPoolStats();
    const u64 fallbacks = pool.fallbacks - pool0.fallbacks;
    const WorldCounters& wc = peer->world().counters();
    const u32 overflow = overflowOf(peer->world()) + st.droppedOutgoing;
    const WireStats ws = peer->session().link().wire();
    const float songRms = songN ? static_cast<float>(std::sqrt(songSum2 / static_cast<double>(songN))) : 0.0f;
    const float scrRms = st.screenErrN ? static_cast<float>(std::sqrt(st.screenErrSum2 / static_cast<double>(st.screenErrN))) : 0.0f;
    const float downMean = rep.seconds ? static_cast<float>(rep.downSum / rep.seconds) : 0.0f;
    const float upMean = rep.seconds ? static_cast<float>(rep.upSum / rep.seconds) : 0.0f;
    const bool clientOk = host || (rep.downMax <= kMaxDownKbps && rep.upMax <= kMaxUpKbps && wc.censusMismatches == 0 &&
                                   wc.censusChecks > 0 && scrRms < kMaxErrRmsWu && (!sameClock || songMax < kMaxSongOffsetMs));
    const bool ok = runFrom && st.disconnects == 0 && peer->stats().dropped == 0 && overflow == 0 && ws.maxDatagram <= kMaxDatagramNoFrag &&
                    fallbacks == 0 && allocs == 0 && clientOk;
    std::printf("[net] %s: %.1f minutes run; disconnects %u, stalls %u; census %u/%u mismatches; corrections' error (all of them) "
                "max %.1f wu\n",
                host ? "host" : "client", runFrom ? static_cast<double>(wireNowUs() - runFrom) / 60e6 : 0.0, st.disconnects,
                peer->stats().dropped, wc.censusChecks, wc.censusMismatches, rep.corrErrMax);
    std::printf("[net]   down %.1f kbit/s worst second (mean %.1f), up %.1f (mean %.1f); largest datagram %u; overflow %u; pool fallbacks "
                "%llu; heap allocations in the steady state %llu\n",
                rep.downMax, downMean, rep.upMax, upMean, ws.maxDatagram, overflow, static_cast<unsigned long long>(fallbacks),
                static_cast<unsigned long long>(allocs));
    if (!host)
        std::printf("[net]   on-screen enemies as each correction lands, drawn against the host's sample: RMS %.2f wu, max %.1f (%llu)\n",
                    scrRms, st.screenErrMax, static_cast<unsigned long long>(st.screenErrN));
    if (!host && sameClock) std::printf("[net]   true song clock offset: RMS %.2f ms, max %.2f ms\n", songRms, songMax);
    std::printf("[net] %s %s\n", host ? "host" : "client", ok ? "PASS" : "FAIL");
    logTagged("RFNET",
              "{\"test\":\"%s\",\"minutes\":%.2f,\"disconnects\":%u,\"stalls\":%u,\"census\":%u,\"census_mismatches\":%u,\"song_rms_ms\":%.3f,"
              "\"song_max_ms\":%.3f,\"down_kbps_max\":%.1f,\"up_kbps_max\":%.1f,\"down_kbps_mean\":%.1f,\"up_kbps_mean\":%.1f,"
              "\"corr_err_max_wu\":%.2f,\"max_datagram\":%u,\"overflow\":%u,\"pool_fallbacks\":%llu,\"heap_allocs\":%llu,\"kills\":%u,"
              "\"err_at_corr_rms_wu\":%.3f,\"err_at_corr_max_wu\":%.2f,\"pass\":%s}",
              host ? "udp_host" : "udp_client", runFrom ? static_cast<double>(wireNowUs() - runFrom) / 60e6 : 0.0, st.disconnects,
              peer->stats().dropped, wc.censusChecks, wc.censusMismatches, songRms, songMax, rep.downMax, rep.upMax, downMean, upMean,
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
        code = runSoak(*species, minutes, players, sim, seed, flag(argc, argv, "--pause"));
    } else if (std::strcmp(mode, "host") == 0 || std::strcmp(mode, "join") == 0) {
        code = runPeer(*species, std::strcmp(mode, "host") == 0, argc, argv);
    } else {
        std::printf("usage: raveform_headless --net soak|host|join [<ip>] [--minutes M] [--players N] [--netsim loss=..,jitter=..] ...\n");
    }
    delete species;
    return code;
}
