#include "net/session.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "sim/species.h"
#include "sim/tuning.h"

namespace rf::net {

using namespace rf::sim;

namespace {
constexpr i64 kPoseEveryUs = 33333;     // TECH_SPEC 8.1: poses at 30 Hz
constexpr i64 kCorrEveryUs = 66667;     // corrections 15 Hz per client
constexpr i64 kBeatEveryUs = 250000;    // the song clock heartbeat, 4 Hz
constexpr i64 kCensusEveryUs = 2000000;
constexpr i64 kPingFastUs = 50000, kPingUs = 250000;  // TECH_SPEC 8.2: 4 Hz (20 Hz until 16 samples)
constexpr i64 kInterpUs = 66000;        // remote players drawn ~66 ms behind
constexpr i64 kExtrapMaxUs = 100000;
// Predicted remote players (fillRemote): carried on from the newest pose for at most this long, a new pose's jump spread
// over this long, and a jump larger than this (a revive, a new run) taken at once.
constexpr i64 kPredictMaxUs = 150000;
constexpr float kPoseSmoothS = 0.1f;
constexpr float kPoseSnapWu = 120.0f;
// A pose stamped further than this from the receiver's host time (ahead of it, behind it) is not used (onPose).
constexpr i64 kPoseStampAheadUs = 1000000, kPoseStampBehindUs = 5000000;
constexpr u32 kCorrPerPacket = 32;      // 32 x 30 bytes + headers: under 1,000 bytes a packet (the limit is 1,200)
// Motion corrections (M4 net follow-up): at most this many in a packet (the packet stays under 1,200 bytes by bytes), and
// every kFullEvery-th correction of an enemy is a full one.
constexpr u32 kMotionsPerPacket = 96;
constexpr u8 kFullEvery = 4;
constexpr double kBar = 384.0, kBeat = 96.0;
// TECH_SPEC 8.4: at most 250 kbit/s down per client. Everything sent to a client is counted on the wire against
// 160 kbit/s with 1.5 KB saved at most; corrections fill what the rest leaves. The margin to 250 is for a reliable
// burst (a wave's spawns, a Break's dissolve) on top of a full second, and for jitter bunching arrivals. (Play test 1,
// second pass: 184 -> 160 kbit/s; the stronger build's runs reach the denser Tracks and kill faster, and its bursts
// reached 264 kbit/s.)
constexpr float kDownBurstBytes = 1500.0f, kDatagramOverhead = 40.0f;  // (the rate: SessionConfig::downBytesPerS, 20,000)
// The accumulator knows only the host's copies: a client asks (5 Hz, with its pose) for the enemies on its screen
// that no correction has reached for this long, and the host sends those (and their schools) next.
constexpr i64 kAttentionEveryUs = 200000;
constexpr double kAttentionAgeS = 0.75;
constexpr float kAttentionBoost = 1000.0f;
constexpr float kAttentionReach = 1.5f;  // the screen and half again around it (an enemy is asked for before it shows)
// A predicted kill's wait: the frames a claim and its verdict spend queued (a pose each way, a tick each way), and the
// longest it waits however bad the link.
constexpr float kVerdictSlackS = 0.12f, kVerdictMaxS = 1.2f;

float lerpAngle(float a, float b, float t) {
    float d = b - a;
    while (d > kPi) d -= kTau;
    while (d < -kPi) d += kTau;
    return a + d * t;
}
}  // namespace

u64 contentHash() {
    return (static_cast<u64>(kProtocolVersion) * 0x9E3779B97F4A7C15ull) ^ speciesDataDigest() ^ (bossDataDigest() << 1);
}

bool parseNetTune(const char* spec, SessionConfig& c) {
    // "key=value,key=value": down (bytes a second), famN (family N's priority weight).
    const char* p = spec;
    while (p && *p) {
        char key[16] = {};
        u32 k = 0;
        while (*p && *p != '=' && k + 1 < sizeof key) key[k++] = *p++;
        if (*p != '=') return false;
        ++p;
        char* end = nullptr;
        const float v = std::strtof(p, &end);
        if (end == p || !(v >= 0.0f) || v > 1e6f) return false;
        p = end;
        if (std::strcmp(key, "down") == 0) {
            c.downBytesPerS = v;
        } else if (std::strcmp(key, "motion") == 0) {
            c.motionCorrections = v != 0.0f;
        } else if (std::strncmp(key, "fam", 3) == 0) {
            const int f = std::atoi(key + 3);
            if (f < 0 || f > 15) return false;
            c.familyWeight[f] = v;
        } else {
            return false;
        }
        if (*p == ',') ++p;
        else if (*p) return false;
    }
    return true;
}

float NetDiagBin::rms() const { return n ? static_cast<float>(std::sqrt(sum2 / static_cast<double>(n))) : 0.0f; }

namespace {
// "rms (share of all the squared error %, share of the samples %)"
int binText(char* out, u32 size, const NetDiagBin& b, const NetDiagBin& all) {
    const double e2 = all.sum2 > 0.0 ? 100.0 * b.sum2 / all.sum2 : 0.0;
    const double n = all.n ? 100.0 * static_cast<double>(b.n) / static_cast<double>(all.n) : 0.0;
    return std::snprintf(out, size, "%.1f (%.0f%%e2 %.0f%%n)", static_cast<double>(b.rms()), e2, n);
}
}  // namespace

void mergeDiag(NetDiag& into, const NetDiag& d) {
    const auto bin = [](NetDiagBin& a, const NetDiagBin& b) {
        a.sum2 += b.sum2;
        a.n += b.n;
    };
    bin(into.all, d.all);
    for (u32 k = 0; k < 4; ++k) bin(into.byTarget[k], d.byTarget[k]);
    bin(into.targetDiffers, d.targetDiffers);
    bin(into.stateDiffers, d.stateDiffers);
    for (u32 k = 0; k < 16; ++k) bin(into.byFamily[k], d.byFamily[k]);
    for (u32 k = 0; k < 6; ++k) bin(into.byAge[k], d.byAge[k]);
    bin(into.perp, d.perp);
    into.tauSum += d.tauSum;
    into.tau2Sum += d.tau2Sum;
    into.tauN += d.tauN;
    into.badPackets += d.badPackets;
    for (u32 s = 0; s < 4; ++s) {
        into.poses[s] += d.poses[s];
        into.posesHeard[s] += d.posesHeard[s];
        into.posesRelayed[s] += d.posesRelayed[s];
        into.posesMisstamped[s] += d.posesMisstamped[s];
        into.poseGapMaxMs[s] = std::fmax(into.poseGapMaxMs[s], d.poseGapMaxMs[s]);
        into.poseGapsOver66[s] += d.poseGapsOver66[s];
        into.poseGapsOver100[s] += d.poseGapsOver100[s];
        into.poseAgeSumMs[s] += d.poseAgeSumMs[s];
        into.poseAgeMinMs[s] = std::fmin(into.poseAgeMinMs[s], d.poseAgeMinMs[s]);
        into.poseAgeMaxMs[s] = std::fmax(into.poseAgeMaxMs[s], d.poseAgeMaxMs[s]);
        into.fills[s] += d.fills[s];
        into.extrapolated[s] += d.extrapolated[s];
        into.extrapSumMs[s] += d.extrapSumMs[s];
        into.extrapMaxMs[s] = std::fmax(into.extrapMaxMs[s], d.extrapMaxMs[s]);
    }
    bin(into.songResidualMs, d.songResidualMs);
    into.songResidualMaxMs = std::fmax(into.songResidualMaxMs, d.songResidualMaxMs);
    into.corrGapMaxMs = std::fmax(into.corrGapMaxMs, d.corrGapMaxMs);
    into.corrPackets += d.corrPackets;
    into.frames += d.frames;
    into.frameMaxMs = std::fmax(into.frameMaxMs, d.frameMaxMs);
    for (u32 k = 0; k < 4; ++k) into.ticksPerFrame[k] += d.ticksPerFrame[k];
    into.ticksSkipped += d.ticksSkipped;
    into.timeDropped += d.timeDropped;
    into.poseSendGapMaxMs = std::fmax(into.poseSendGapMaxMs, d.poseSendGapMaxMs);
    into.poseSendGapsOver50 += d.poseSendGapsOver50;
    bin(into.anchorResidualMs, d.anchorResidualMs);
    into.anchorResidualMaxMs = std::fmax(into.anchorResidualMaxMs, d.anchorResidualMaxMs);
    into.anchorsBackwards += d.anchorsBackwards;
}

u32 formatDiagCorrections(const NetDiag& d, char* out, u32 size) {
    if (!size) return 0;
    u32 w = 0;
    const auto put = [&](int n) {
        if (n > 0) w = w + static_cast<u32>(n) < size ? w + static_cast<u32>(n) : size - 1;
    };
    put(std::snprintf(out + w, size - w, "corr %.1f wu (%llu); target self ", static_cast<double>(d.all.rms()),
                      static_cast<unsigned long long>(d.all.n)));
    put(binText(out + w, size - w, d.byTarget[0], d.all));
    put(std::snprintf(out + w, size - w, " host "));
    put(binText(out + w, size - w, d.byTarget[1], d.all));
    put(std::snprintf(out + w, size - w, " other "));
    put(binText(out + w, size - w, d.byTarget[2], d.all));
    put(std::snprintf(out + w, size - w, "; target differs "));
    put(binText(out + w, size - w, d.targetDiffers, d.all));
    put(std::snprintf(out + w, size - w, "; state differs "));
    put(binText(out + w, size - w, d.stateDiffers, d.all));
    put(std::snprintf(out + w, size - w, "; age"));
    static const char* const kAge[6] = {"<.25", "<.5", "<1", "<2", "<4", ">4"};
    for (u32 k = 0; k < 6; ++k) {
        put(std::snprintf(out + w, size - w, " %s:", kAge[k]));
        put(binText(out + w, size - w, d.byAge[k], d.all));
    }
    const double tm = d.tauN ? d.tauSum / static_cast<double>(d.tauN) : 0.0;
    const double tr = d.tauN ? std::sqrt(d.tau2Sum / static_cast<double>(d.tauN)) : 0.0;
    put(std::snprintf(out + w, size - w, "; along v: mean %+.1f ms, rms %.1f ms; across v %.1f wu; family", tm * 1e3, tr * 1e3,
                      static_cast<double>(d.perp.rms())));
    for (u32 f = 0; f < 16; ++f) {
        if (!d.byFamily[f].n) continue;
        put(std::snprintf(out + w, size - w, " %u:", f));
        put(binText(out + w, size - w, d.byFamily[f], d.all));
    }
    return w;
}

u32 formatDiagTicks(const NetDiag& d, char* out, u32 size) {
    if (!size) return 0;
    const int n = std::snprintf(out, size,
                                "frames %u, longest %.1f ms, ticks a frame 0/1/2/3: %u/%u/%u/%u, ticks skipped %u, time dropped %u; pose "
                                "sends: longest gap %.1f ms, %u over 50 ms; song anchors set here %.2f ms rms, max %.2f (%llu), %u backwards",
                                d.frames, static_cast<double>(d.frameMaxMs), d.ticksPerFrame[0], d.ticksPerFrame[1], d.ticksPerFrame[2],
                                d.ticksPerFrame[3], d.ticksSkipped, d.timeDropped, static_cast<double>(d.poseSendGapMaxMs), d.poseSendGapsOver50,
                                static_cast<double>(d.anchorResidualMs.rms()), static_cast<double>(d.anchorResidualMaxMs),
                                static_cast<unsigned long long>(d.anchorResidualMs.n), d.anchorsBackwards);
    return n < 0 ? 0u : (static_cast<u32>(n) < size ? static_cast<u32>(n) : size - 1);
}

u32 formatDiagPoses(const NetDiag& d, u32 seats, u32 self, char* out, u32 size) {
    if (!size) return 0;
    u32 w = 0;
    const auto put = [&](int n) {
        if (n > 0) w = w + static_cast<u32>(n) < size ? w + static_cast<u32>(n) : size - 1;
    };
    put(std::snprintf(out + w, size - w, "poses (bad packets %u)", d.badPackets));
    for (u32 s = 0; s < seats && s < 4; ++s) {
        if (s == self) continue;
        if (!d.poses[s]) {
            if (d.posesHeard[s] || d.posesRelayed[s] || d.fills[s])
                put(std::snprintf(out + w, size - w, " | seat %u: none new (heard %u, misstamped %u, relayed %u, ticks %u)", s, d.posesHeard[s],
                                  d.posesMisstamped[s], d.posesRelayed[s], d.fills[s]));
            continue;
        }
        put(std::snprintf(out + w, size - w,
                          " | seat %u: %u new of %u heard (misstamped %u, relayed %u), gap max %.0f ms, >66 %u, >100 %u, age %.0f/%.0f/%.0f ms, "
                          "past newest %.1f%% (mean %.0f, max %.0f ms)",
                          s, d.poses[s], d.posesHeard[s], d.posesMisstamped[s], d.posesRelayed[s], static_cast<double>(d.poseGapMaxMs[s]),
                          d.poseGapsOver66[s],
                          d.poseGapsOver100[s],
                          static_cast<double>(d.poseAgeMinMs[s]), d.poseAgeSumMs[s] / static_cast<double>(d.poses[s]),
                          static_cast<double>(d.poseAgeMaxMs[s]),
                          d.fills[s] ? 100.0 * static_cast<double>(d.extrapolated[s]) / static_cast<double>(d.fills[s]) : 0.0,
                          d.extrapolated[s] ? d.extrapSumMs[s] / static_cast<double>(d.extrapolated[s]) : 0.0,
                          static_cast<double>(d.extrapMaxMs[s])));
    }
    put(std::snprintf(out + w, size - w, " | song anchors %.2f ms rms, max %.2f (%llu) | correction packets %u, gap max %.0f ms",
                      static_cast<double>(d.songResidualMs.rms()), static_cast<double>(d.songResidualMaxMs),
                      static_cast<unsigned long long>(d.songResidualMs.n), d.corrPackets, static_cast<double>(d.corrGapMaxMs)));
    return w;
}

// ---------------------------------------------------------------------------------------------- set-up

bool Session::start(const SessionConfig& cfg, World* world, u64 content) {
    stop();
    if (!world || !netInit()) return false;
    cfg_ = cfg;
    world_ = world;
    content_ = content;
    stats_ = SessionStats{};
    diag_ = NetDiag{};
    for (i64& t : poseArrivedUs_) t = 0;
    corrArrivedUs_ = frameUs_ = poseSentUs_ = 0;
    clock_.reset();
    song_.reset();
    runOffset_ = 0.0;
    pauseAt_ = resumeAt_ = -1.0;
    announcedRun_ = 0;
    directorSent_ = false;
    directorRun_ = 0xFFFFFFFFu;
    rel_.reset();
    havePrev_ = false;
    for (PoseRing& r : poses_) r = PoseRing{};
    for (RemoteTrack& t : track_) t = RemoteTrack{};
    for (i32& s : peerSeat_) s = -1;
    for (i32& p : seatPeer_) p = -1;
    for (bool& w : welcomed_) w = false;
    for (u32 s = 0; s < kMaxPlayers; ++s)
        for (u32 i = 0; i < kMaxEnemies; ++i) {
            prio_[s][i] = 0.0f;
            prioSerial_[s][i] = 0;
            motionRun_[s][i] = 0;
            fullHp_[s][i] = 0.0f;
        }
    for (u32 i = 0; i < kMaxEnemies; ++i) {
        corrSerial_[i] = seenSerial_[i] = 0;
        corrStamp_[i] = seenStamp_[i] = 0.0;
    }
    nextAttentionUs_ = 0;
    pendClaimCount_ = 0;
    pendRun_ = 0;
    measureFrom_ = 0;
    lastWire_ = WireStats{};
    errSum_ = 0.0;
    errMax_ = 0.0f;
    errN_ = corrCount_ = 0;
    nextPoseUs_ = nextBeatUs_ = nextCensusUs_ = nextPingUs_ = 0;
    for (i64& t : nextCorrUs_) t = 0;
    for (float& b : downBudget_) b = 0.0f;
    for (float& b : pending_) b = 0.0f;
    for (u64& b : sentTo_) b = 0;
    budgetUs_ = lastPrioUs_ = 0;
    endReason_ = "";
    for (auto& n : names_) std::memset(n, 0, sizeof n);
    lobbySeats_ = lobbyReady_ = 0;
    startUs_ = 0;
    leaveQueued_ = 0;
    roomCode_ = 0;
    lobbyDirty_ = readySent_ = false;
    for (BedEntry& e : bed_) e = BedEntry{};
    bedHead_ = 0;
    if (host()) {
        if (!link_.listen(cfg_.bindIp, cfg_.port, kMaxLinkPeers, cfg_.wire, cfg_.sim)) {
            netShutdown();
            return false;
        }
        std::snprintf(names_[0], sizeof names_[0], "%s", cfg_.name);
        lobbySeats_ = lobbyReady_ = 1;
        // The room code: the low 16 bits of this device's address on the network (TECH_SPEC 8.3).
        roomCode_ = cfg_.wire == WireKind::Udp ? static_cast<u16>(preferredLocalIPv4() & 0xFFFFu) : static_cast<u16>(cfg_.port);
        if (cfg_.beacon) beacon_.start(cfg_.wire, cfg_.sim);
        seat_ = 0;
        seats_ = cfg_.seats < 1 ? 1 : (cfg_.seats > kMaxPlayers ? kMaxPlayers : cfg_.seats);
        seatMask_ = 1;
        welcomed_[0] = true;
        world_->setNet(NetRole::Host, 1);
        state_ = SessionState::Lobby;
    } else {
        if (!link_.connect(cfg_.joinIp, cfg_.port, cfg_.wire, cfg_.sim, kProtocolVersion, cfg_.bindIp)) {
            netShutdown();
            return false;
        }
        state_ = SessionState::Connecting;
    }
    return true;
}

void Session::stop() {
    beacon_.stop();
    if (state_ == SessionState::Idle) return;
    if (link_.open()) {
        // Say goodbye (the host's leaving ends the run for everyone, PLAN 10).
        scratch_.reset();
        scratch_.putBye(0);
        if (host()) {
            for (u32 s = 1; s < kMaxPlayers; ++s)
                if (welcomed_[s] && seatPeer_[s] >= 0) sendTo(static_cast<u32>(seatPeer_[s]), kChControl, scratch_, true);
        } else {
            sendTo(0, kChControl, scratch_, true);
        }
        link_.flush();
    }
    link_.close();
    netShutdown();
    state_ = SessionState::Idle;
}

// ---------------------------------------------------------------------------------------------- in

void Session::receive(i64 localUs) {
    // The frame's clocks. The link itself is serviced once a frame, in send(), after everything is queued (the
    // frame's data leaves in one datagram a peer; ENet acknowledges what arrives at once, in its own).
    localNow_ = localUs;
    if (!startUs_) startUs_ = localUs;
    hostNow_ = host() ? localUs : clock_.hostNowUs(localUs);
    if (!host() && state_ == SessionState::Connecting && localUs - startUs_ > kConnectTimeoutUs) {
        // M4 stage B: nobody answered at that address (a room code or an address typed wrong, a host gone): the join
        // screen says so after a few seconds instead of ENet's 5 to 30.
        state_ = SessionState::Ended;
        endReason_ = "no answer";
    }
}

i64 Session::nowLocal() const { return clockFn_ ? clockFn_(clockUser_) : localNow_; }

void Session::setHostSong(const SongAnchor& a) {
    if (song_.valid() && a.hostUs > song_.anchor().hostUs && a.tps > 0.0) {
        // Diagnostics: the new anchor against the model it replaces, at its own host time.
        const float r = static_cast<float>((a.song - song_.songAt(a.hostUs)) / a.tps * 1000.0);
        diag_.anchorResidualMs.add(r);
        if (std::fabs(r) > diag_.anchorResidualMaxMs) diag_.anchorResidualMaxMs = std::fabs(r);
        if (a.song < song_.songAt(song_.anchor().hostUs)) ++diag_.anchorsBackwards;
    }
    song_.set(a);
}

void Session::noteFrame(i64 localUs, u32 ticks) {
    ++diag_.frames;
    if (frameUs_) {
        const float ms = static_cast<float>(static_cast<double>(localUs - frameUs_) * 1e-3);
        if (ms > diag_.frameMaxMs) diag_.frameMaxMs = ms;
    }
    frameUs_ = localUs;
    ++diag_.ticksPerFrame[ticks < 3 ? ticks : 3];
}

void Session::service() {
    Link::Event ev;
    while (link_.poll(ev)) {
        switch (ev.type) {
            case Link::Event::Connect: handleConnect(ev.peer, ev.info); break;
            case Link::Event::Disconnect: handleDisconnect(ev.peer); break;
            case Link::Event::Receive:
                fromPeer_ = ev.peer;
                from_ = host() ? (ev.peer < kMaxLinkPeers ? peerSeat_[ev.peer] : -1) : 0;
                if (!parsePacket(ev.data, ev.size, *this)) {
                    ++stats_.badPackets;
                    ++diag_.badPackets;
                }
                from_ = -1;
                break;
            default: break;
        }
    }
    if (host()) maybeStart();
}

void Session::handleConnect(u32 peer, u32 info) {
    if (host()) {
        if (peer < kMaxLinkPeers) peerSeat_[peer] = -1;
        if (info != kProtocolVersion) {
            scratch_.reset();
            scratch_.putRefuse(kRefuseVersion);
            sendTo(peer, kChControl, scratch_, true);
            link_.disconnect(peer, kRefuseVersion);
            ++stats_.refused;
        }
        return;
    }
    MsgHello h;
    h.content = content_;
    std::snprintf(h.name, sizeof h.name, "%s", cfg_.name);
    scratch_.reset();
    scratch_.putHello(h);
    sendTo(0, kChControl, scratch_, true);
    link_.flush();
}

void Session::handleDisconnect(u32 peer) {
    ++stats_.disconnects;
    if (!host()) {
        state_ = SessionState::Ended;
        if (!*endReason_) endReason_ = "the host is gone";
        return;
    }
    const i32 s = peer < kMaxLinkPeers ? peerSeat_[peer] : -1;
    if (peer < kMaxLinkPeers) peerSeat_[peer] = -1;
    if (s <= 0) return;
    seatPeer_[s] = -1;
    welcomed_[s] = false;
    seatMask_ = static_cast<u8>(seatMask_ & ~(1u << s));
    lobbySeats_ = static_cast<u8>(lobbySeats_ & ~(1u << s));
    lobbyReady_ = static_cast<u8>(lobbyReady_ & ~(1u << s));
    std::memset(names_[s], 0, sizeof names_[s]);
    lobbyDirty_ = true;
    poses_[s] = PoseRing{};
    // Its player leaves the run on every device on the same tick, two beats from now (M4 stage B: the enemies that
    // followed it choose again at the same moment everywhere, so no copy drifts from the host's): the event first, in
    // the reliable stream's order, then the seats (a client's onSeats does not leave it again).
    MsgPlayerEvent m;
    m.run = world_->runId();
    m.player = static_cast<u8>(s);
    m.kind = kPeLeave;
    m.tick = world_->music() + 2.0 * sim::kTicksPerBeat;
    NetOp op;
    op.kind = NetOp::Player;
    op.player = m.player;
    op.a = kPeLeave;
    op.run = m.run;
    op.tick = m.tick;
    pushOp(op);
    if (state_ == SessionState::Running) relayEvent(m, static_cast<u32>(s));
    reliable([&](PacketWriter& w) { return w.putSeats(seatMask_); });
}

void Session::onHello(const MsgHello& m) {
    if (!host() || fromPeer_ >= kMaxLinkPeers || peerSeat_[fromPeer_] >= 0) return;
    const u32 peer = fromPeer_;
    u8 reason = 0;
    if (m.version != kProtocolVersion) reason = kRefuseVersion;
    else if (m.content != content_) reason = kRefuseContent;
    else if (state_ == SessionState::Running) reason = kRefuseRunning;  // joining happens in the lobby
    i32 seat = -1;
    for (u32 s = 1; s < seats_ && !reason; ++s)
        if (!welcomed_[s]) {
            seat = static_cast<i32>(s);
            break;
        }
    if (!reason && seat < 0) reason = kRefuseFull;
    if (reason) {
        scratch_.reset();
        scratch_.putRefuse(reason);
        sendTo(peer, kChControl, scratch_, true);
        link_.disconnect(peer, reason);
        ++stats_.refused;
        return;
    }
    peerSeat_[peer] = seat;
    seatPeer_[seat] = static_cast<i32>(peer);
    welcomed_[seat] = true;
    seatMask_ = static_cast<u8>(seatMask_ | (1u << seat));
    lobbySeats_ = static_cast<u8>(lobbySeats_ | (1u << seat));
    lobbyReady_ = static_cast<u8>(lobbyReady_ & ~(1u << seat));
    for (u32 k = 0; k < 15; ++k) {
        const char c = m.name[k];
        if (!c) break;
        names_[seat][k] = c >= 0x20 && c <= 0x7E ? c : '?';
    }
    names_[seat][15] = '\0';
    lobbyDirty_ = true;
    MsgWelcome w;
    w.content = content_;
    w.seat = static_cast<u8>(seat);
    w.seats = static_cast<u8>(seats_);
    scratch_.reset();
    scratch_.putWelcome(w);
    sendTo(peer, kChControl, scratch_, true);
    lastJoinUs_ = localNow_;
    nextCorrUs_[seat] = localNow_ + static_cast<i64>(seat) * 22000;  // the clients' corrections take turns
    reliable([&](PacketWriter& pw) { return pw.putSeats(seatMask_); });
}

void Session::onWelcome(const MsgWelcome& m) {
    if (host() || state_ != SessionState::Connecting) return;
    seat_ = m.seat;
    seats_ = m.seats;
    seatMask_ = static_cast<u8>((1u << seats_) - 1u);
    world_->setNet(NetRole::Client, static_cast<u8>(1u << seat_));
    state_ = SessionState::Lobby;
}

void Session::onRefuse(u8 reason) {
    if (host()) return;
    state_ = SessionState::Ended;
    endReason_ = reason == kRefuseVersion ? "another version of the game"
                 : reason == kRefuseContent ? "other game data"
                 : reason == kRefuseFull ? "the run is full"
                 : reason == kRefuseRunning ? "the run has started" : "refused";
    ++stats_.refused;
}

void Session::onBye(u8) {
    if (host()) return;
    state_ = SessionState::Ended;
    endReason_ = "the host left";
}

void Session::onPing(i64 t0) {
    if (!host() || from_ < 1) return;
    // Answered at once, and sent at once, so its send time is the moment it leaves.
    const i64 now = nowLocal();
    scratch_.reset();
    scratch_.putPong(t0, now, now);
    sendTo(fromPeer_, kChControl, scratch_, false);
    link_.flush();
}

void Session::onPong(i64 t0, i64 t1, i64 t2) {
    if (host()) return;
    clock_.addSample(t0, t1, t2, nowLocal());
}

void Session::onRunStart(const MsgRunStart& m) {
    if (host()) return;
    // Applied now: the run's bar line is at least half a bar away, after everything sent before this.
    world_->scheduleRun(m.bar, m.seed, m.players, m.run, m.mask);
    runOffset_ = m.runOffset;
    if (state_ == SessionState::Lobby) clock_.snap();  // the run starts on the best estimate; from now on it slews
    state_ = SessionState::Running;
}

bool Session::pushOp(const NetOp& op) {
    if (world_->netPush(op)) return true;
    return false;  // counted by the world (netOpsDropped)
}

void Session::onSpawn(u16 run, const NetSpawn& s) {
    if (host()) return;
    NetOp op;
    op.kind = NetOp::Spawn;
    op.run = run;
    op.serial = s.serial;
    op.species = s.species;
    op.group = s.group;
    op.x = s.x;
    op.y = s.y;
    op.f = s.heading;
    op.tick = s.land;
    op.a = s.flags;
    pushOp(op);
}

void Session::onDeath(u16 run, const NetDeath& d) {
    if (host()) return;
    NetOp op;
    op.kind = NetOp::Death;
    op.run = run;
    op.serial = d.serial;
    op.a = d.kind;
    op.b = d.killer;
    pushOp(op);
}

void Session::onClaim(u16 run, const NetClaim& c) {
    if (!host() || from_ < 1) return;
    NetOp op;
    op.kind = NetOp::Claim;
    op.run = run;
    op.player = static_cast<u8>(from_);
    op.serial = c.serial;
    op.f = c.damage;
    op.a = c.kind;
    pushOp(op);
}

void Session::onBossClaim(u16 run, const NetBossClaim& c) {
    if (!host() || from_ < 1) return;
    NetOp op;
    op.kind = NetOp::BossClaim;
    op.run = run;
    op.player = static_cast<u8>(from_);
    op.a = c.part;
    op.b = c.pair;
    op.f = c.damage;
    pushOp(op);
}

void Session::onBoss(const MsgBoss& m) {
    if (host()) return;
    NetOp op;
    op.kind = m.kind == 0 ? NetOp::BossEnter : (m.kind == 1 ? NetOp::BossPair : NetOp::BossDeath);
    op.run = m.run;
    op.a = m.pair;
    op.b = m.killer;
    op.x = m.x;
    op.y = m.y;
    op.x2 = m.x2;
    op.y2 = m.y2;
    op.c = m.boss;
    op.tick = m.tick;
    op.u = m.changeBar;
    if (m.kind == 2) op.b = m.killer;
    pushOp(op);
}

void Session::onBoostDrop(const MsgBoostDrop& m) {
    if (host()) return;
    NetOp op;
    op.kind = NetOp::BoostDrop;
    op.run = m.run;
    op.x = m.x;
    op.y = m.y;
    op.a = m.type;
    pushOp(op);
}

void Session::onDirector(const MsgDirector& m) {
    if (host()) return;
    NetOp op;
    op.kind = NetOp::Director;
    op.run = m.run;
    op.tick = m.d.startTick;
    op.u = m.d.trackBar;
    op.u2 = m.d.changeBar;
    op.a = m.d.track;
    op.b = m.d.bossSpawned ? 1 : 0;
    op.f = m.d.assist;
    op.c = m.d.midHold;
    op.opts[0] = m.d.midHolding ? 1 : 0;
    op.opts[1] = m.d.miniSpawned ? 1 : 0;
    op.opts[2] = m.d.lastMini;
    pushOp(op);
}

void Session::onPlayerEvent(const MsgPlayerEvent& m) {
    if (host()) {
        if (from_ < 1 || m.player != static_cast<u32>(from_)) return;  // a client speaks for its own player only
    } else if (m.player == seat_) {
        return;
    }
    NetOp op;
    op.kind = NetOp::Player;
    op.run = m.run;
    op.player = m.player;
    op.a = m.kind;
    op.b = m.b;
    for (u32 k = 0; k < kMaxOrbs; ++k) op.opts[k] = m.opts[k];
    op.x = m.x;
    op.y = m.y;
    op.x2 = m.x2;
    op.y2 = m.y2;
    op.tick = m.tick;
    pushOp(op);
    if (!host() && m.kind == kPeLeave) leaveQueued_ = static_cast<u8>(leaveQueued_ | (1u << (m.player & 3u)));
    if (host()) relayEvent(m, static_cast<u32>(from_));
}

void Session::onCensus(const MsgCensus& m) {
    if (host()) return;
    NetOp op;
    op.kind = NetOp::Census;
    op.run = m.run;
    op.u = m.count;
    op.serial = static_cast<u32>(m.digest);
    op.u2 = static_cast<u32>(m.digest >> 32);
    pushOp(op);
}

void Session::onPause(const MsgPause& m) {
    if (host()) {
        if (from_ < 1) return;
        if (m.kind == kPauseAsk) hostPause(true);
        if (m.kind == kResumeAsk) hostPause(false);
        return;
    }
    if (m.kind == kPauseAt) {
        pauseAt_ = m.runTick;
        resumeAt_ = -1.0;
    } else if (m.kind == kResumeAt) {
        resumeAt_ = m.songTick;
        nextOffset_ = m.runOffset;
    }
}

void Session::onSeats(u8 mask) {
    if (host()) return;
    // A seat that emptied: its player leaves the run (at once, unless the host's timed leave came first).
    for (u32 s = 0; s < seats_; ++s) {
        if (!(seatMask_ & (1u << s)) || (mask & (1u << s)) || s == seat_) continue;
        poses_[s] = PoseRing{};
        if ((leaveQueued_ >> s) & 1u) continue;
        NetOp op;
        op.kind = NetOp::Player;
        op.player = static_cast<u8>(s);
        op.a = kPeLeave;
        op.run = world_->runId();
        pushOp(op);
        poses_[s] = PoseRing{};
    }
    seatMask_ = mask;
}

void Session::onPose(const MsgPose& m) {
    if (host()) {
        if (from_ < 1 || m.player != static_cast<u32>(from_)) return;
    } else if (m.player == seat_) {
        return;
    }
    if (m.player >= kMaxPlayers) return;
    ++diag_.posesHeard[m.player];
    // A pose is stamped in host time. One stamped far from this device's host time now was made on another clock (a
    // client's pose before its clock had a sample: its own local time; a PC's clock was 6.5 days ahead of the Moto's) or
    // is stale: it is dropped. Kept, it sorted as the newest for good, and the host relayed it in place of every later
    // pose, so the other clients saw that player standing where it stood in the lobby for the whole run
    // (docs/PROGRESS_m4_net.md, the cause of the on-screen error with the Moto hosting four).
    if (!host() && !clock_.valid()) return;
    {
        const i64 ahead = m.stampUs - hostNow_;
        if (ahead > kPoseStampAheadUs || ahead < -kPoseStampBehindUs) {
            ++diag_.posesMisstamped[m.player];
            return;
        }
    }
    // Into the ring in time order (packets arrive in any order); a repeat (the redundant previous state) is
    // skipped, and when the ring is full the oldest goes.
    PoseRing& r = poses_[m.player];
    const auto add = [&](i64 stamp, const PoseState& s) {
        u32 at = r.count;  // insertion place among the r.count samples (oldest first)
        while (at > 0 && r.s[(r.head + at - 1) % kPoseRing].stampUs > stamp) --at;
        if (at > 0 && r.s[(r.head + at - 1) % kPoseRing].stampUs == stamp) return false;
        if (r.count == kPoseRing) {
            if (at == 0) return false;  // older than everything kept
            r.head = (r.head + 1) % kPoseRing;
            --r.count;
            --at;
        }
        for (u32 k = r.count; k > at; --k) r.s[(r.head + k) % kPoseRing] = r.s[(r.head + k - 1) % kPoseRing];
        MsgPose& d = r.s[(r.head + at) % kPoseRing];
        ++r.count;
        d = m;
        d.stampUs = stamp;
        d.cur = s;
        return true;
    };
    if (m.prevAgeMs > 0 && m.prevAgeMs <= 1000) add(m.stampUs - static_cast<i64>(m.prevAgeMs) * 1000, m.prev);
    if (add(m.stampUs, m.cur)) {
        // Diagnostics: a new sample of this player (a relayed pose the host had already sent is not one).
        const u32 p = m.player;
        const i64 local = nowLocal();
        const i64 hostNow = host() ? local : clock_.hostAt(local);
        if (poseArrivedUs_[p]) {
            const float gap = static_cast<float>(static_cast<double>(local - poseArrivedUs_[p]) * 1e-3);
            if (gap > diag_.poseGapMaxMs[p]) diag_.poseGapMaxMs[p] = gap;
            if (gap > 66.0f) ++diag_.poseGapsOver66[p];
            if (gap > 100.0f) ++diag_.poseGapsOver100[p];
        }
        poseArrivedUs_[p] = local;
        const float age = static_cast<float>(static_cast<double>(hostNow - m.stampUs) * 1e-3);
        ++diag_.poses[p];
        diag_.poseAgeSumMs[p] += age;
        if (age < diag_.poseAgeMinMs[p]) diag_.poseAgeMinMs[p] = age;
        if (age > diag_.poseAgeMaxMs[p]) diag_.poseAgeMaxMs[p] = age;
    }
}

void Session::onStats(const MsgStats& m) {
    // A player's numbers: its own device keeps them (cells, Novas, Chain, score, kills).
    if (host()) {
        if (from_ < 1 || m.player != static_cast<u32>(from_)) return;
    } else if (m.player == seat_) {
        return;
    }
    PlayerState& pl = world_->playerMut(m.player);
    pl.cells = m.cells;
    pl.novas = m.novas;
    pl.chain = m.chain;
    if (m.chain > pl.bestChain) pl.bestChain = m.chain;
    pl.score = m.score;
    pl.kills = m.kills;
}

void Session::onCorrections(u16 run, double hostMusic) {
    corrRun_ = run;
    corrMusic_ = hostMusic;
    const i64 local = nowLocal();
    if (corrArrivedUs_) {
        const float gap = static_cast<float>(static_cast<double>(local - corrArrivedUs_) * 1e-3);
        if (gap > diag_.corrGapMaxMs) diag_.corrGapMaxMs = gap;
    }
    corrArrivedUs_ = local;
    ++diag_.corrPackets;
}

void Session::onCorrection(const NetEnemyState& s) {
    if (host()) return;
    if (corrRun_ != world_->runId()) {
        ++stats_.corrOtherRun;
        return;
    }
    // Corrections arrive in any order: one older than the last applied to this enemy is stale.
    const i32 i = world_->indexOfSerial(s.serial);
    if (i < 0) {
        ++stats_.corrUnknown;
        return;
    }
    const Enemies& e = world_->enemies();
    const u32 slot = e.handle[i] & 0xFFFFu;
    if (corrSerial_[slot] == s.serial && corrMusic_ <= corrStamp_[slot]) return;
    const double prevStamp = corrSerial_[slot] == s.serial ? corrStamp_[slot] : -1.0;
    corrSerial_[slot] = s.serial;
    corrStamp_[slot] = corrMusic_;
    if (!(e.flags[i] & kFlagDead) && e.state[i] != kWarping && s.state != kWarping) {
        // What this screen showed against the host's sample, both carried to now: measured as each correction
        // lands (the moment the error is largest), for enemies on this player's screen.
        const Vec2 me = world_->player(seat_).pos;
        const float dx = e.x[i] + e.ox[i] - me.x, dy = e.y[i] + e.oy[i] - me.y;
        if (std::fabs(dx) <= tuning::kFrameHalfWWu && std::fabs(dy) <= tuning::kFrameHalfHWu) {
            const float tps = static_cast<float>(world_->ticksPerSecond());
            const float ds = clampf(static_cast<float>((world_->music() - corrMusic_) / static_cast<double>(tps)), -0.25f, 0.25f);
            const Vec2 v{s.vx + s.kx, s.vy + s.ky};
            const Vec2 h{s.x + v.x * ds, s.y + v.y * ds};
            const Vec2 dv = Vec2{e.x[i] + e.ox[i], e.y[i] + e.oy[i]} - h;
            const float d = length(dv);
            stats_.screenErrSum2 += static_cast<double>(d) * d;
            ++stats_.screenErrN;
            if (d > stats_.screenErrMax) stats_.screenErrMax = d;
            // Diagnostics: what kind of divergence it is.
            NetDiag& g = diag_;
            g.all.add(d);
            const u32 tc = s.target >= kMaxPlayers ? 3u : (s.target == seat_ ? 0u : (s.target == 0 ? 1u : 2u));
            g.byTarget[tc].add(d);
            if (e.target[i] != s.target) g.targetDiffers.add(d);
            if (e.state[i] != s.state) g.stateDiffers.add(d);
            g.byFamily[static_cast<u32>(world_->species().get(e.species[i]).family) & 15u].add(d);
            const float age = prevStamp < 0.0 ? 1e9f : static_cast<float>((corrMusic_ - prevStamp) / static_cast<double>(tps));
            g.byAge[age < 0.25f ? 0 : (age < 0.5f ? 1 : (age < 1.0f ? 2 : (age < 2.0f ? 3 : (age < 4.0f ? 4 : 5))))].add(d);
            const float v2 = v.x * v.x + v.y * v.y;
            if (v2 > 60.0f * 60.0f) {
                const float tau = (dv.x * v.x + dv.y * v.y) / v2;
                g.tauSum += tau;
                g.tau2Sum += static_cast<double>(tau) * tau;
                ++g.tauN;
                g.perp.add(length(dv - v * tau));
            }
        }
    }
    const float err = world_->netCorrect(s, corrMusic_);
    if (err < 0.0f) return;
    seenSerial_[slot] = s.serial;
    seenStamp_[slot] = world_->music();
    ++corrCount_;
    ++stats_.corrections;
    if (err > tuning::kCorrectionSnapWu) ++stats_.snaps;
    errSum_ += static_cast<double>(err) * err;
    ++errN_;
    if (err > errMax_) errMax_ = err;
}

void Session::onMotion(const NetMotion& m) {
    // A motion correction: the host's place, heading, target and knockback; the rest as this copy has it (a full
    // correction carries it every few times). Applied as a correction like any other (netCorrect).
    if (host() || corrRun_ != world_->runId()) {
        if (!host()) ++stats_.corrOtherRun;
        return;
    }
    const i32 i = world_->indexOfSerial(m.serial);
    if (i < 0) {
        ++stats_.corrUnknown;
        return;
    }
    const Enemies& e = world_->enemies();
    if (e.state[i] == kWarping || (e.flags[i] & kFlagDead)) return;  // (not landed here yet, or a death predicted here)
    NetEnemyState s;
    s.serial = m.serial;
    s.x = m.x;
    s.y = m.y;
    // Its velocity: the host's heading at this copy's speed (the same species, so the same speed).
    const Vec2 v = dirOf(m.heading) * std::sqrt(e.vx[i] * e.vx[i] + e.vy[i] * e.vy[i]);
    s.vx = v.x;
    s.vy = v.y;
    s.kx = m.kx;
    s.ky = m.ky;
    s.heading = m.heading;
    s.a = e.a[i];
    s.b = e.b[i];
    s.hp = e.hp[i];
    s.until = e.until[i];
    s.rng = e.rng[i];
    s.state = e.state[i];
    s.flags = static_cast<u8>((e.flags[i] & (kFlagArriving | kFlagCalled)) | (m.engaged ? kFlagEngaged : 0));
    s.target = m.target;
    s.far = e.far[i];
    s.step = e.step[i];
    s.frozen = m.frozen;
    ++stats_.motions;
    onCorrection(s);
}

void Session::onAttention(u16 run, const u32* serials, u32 n) {
    // A client's screen shows these and no correction has reached them lately: they go out next (with their schools).
    if (!host() || from_ < 1 || run != world_->runId()) return;
    const u32 seat = static_cast<u32>(from_);
    const Enemies& e = world_->enemies();
    for (u32 k = 0; k < n; ++k) {
        const i32 i = world_->indexOfSerial(serials[k]);
        if (i < 0 || (e.flags[i] & kFlagDead)) continue;
        const u32 slot = e.handle[i] & 0xFFFFu;
        if (prioSerial_[seat][slot] != serials[k]) {
            prioSerial_[seat][slot] = serials[k];
            prio_[seat][slot] = 0.0f;
        }
        prio_[seat][slot] += kAttentionBoost;
        ++stats_.attention;
    }
}

u32 Session::attention() {
    // The enemies drawn on and around this player's screen that no correction has reached for kAttentionAgeS
    // (counted from the last one applied, or from when this device first saw it there), the longest waiting first.
    const World& w = *world_;
    const Enemies& e = w.enemies();
    const Vec2 me = w.player(seat_).pos;
    const double now = w.music();
    const double stale = kAttentionAgeS * static_cast<double>(w.ticksPerSecond());
    const float hw = tuning::kFrameHalfWWu * kAttentionReach, hh = tuning::kFrameHalfHWu * kAttentionReach;
    double age[kMaxAttention];
    u32 n = 0;
    for (u32 i = 0; i < e.count; ++i) {
        if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping) continue;
        if (std::fabs(e.x[i] + e.ox[i] - me.x) > hw || std::fabs(e.y[i] + e.oy[i] - me.y) > hh) continue;
        const u32 slot = e.handle[i] & 0xFFFFu;
        if (seenSerial_[slot] != e.serial[i]) {
            seenSerial_[slot] = e.serial[i];
            seenStamp_[slot] = now;
            continue;
        }
        const double a = now - seenStamp_[slot];
        if (a < stale) continue;
        u32 at = n;
        if (n < kMaxAttention) {
            ++n;
        } else if (a > age[kMaxAttention - 1]) {
            at = kMaxAttention - 1;
        } else {
            continue;
        }
        while (at > 0 && age[at - 1] < a) {
            age[at] = age[at - 1];
            attention_[at] = attention_[at - 1];
            --at;
        }
        age[at] = a;
        attention_[at] = e.serial[i];
    }
    return n;
}

void Session::onHeartbeat(const MsgHeartbeat& m) {
    if (host()) return;
    SongAnchor a;
    a.hostUs = m.hostUs;
    a.song = m.song;
    a.tps = m.tps;
    a.nextBar = m.nextBar;
    a.tpsNext = m.tpsNext;
    if (song_.valid() && a.hostUs > song_.anchor().hostUs && a.tps > 0.0) {
        // Diagnostics: the new anchor against the model it replaces, at its own host time.
        const float r = static_cast<float>((a.song - song_.songAt(a.hostUs)) / a.tps * 1000.0);
        diag_.songResidualMs.add(r);
        if (std::fabs(r) > diag_.songResidualMaxMs) diag_.songResidualMaxMs = std::fabs(r);
    }
    song_.set(a);
    if (m.bossOn && m.run == world_->runId()) world_->netBossState(m.bossPart, m.bossParts, m.bossCore, Vec2{m.bossX, m.bossY});
}

// ---------------------------------------------------------------------------------------------- the run

void Session::maybeStart() {
    if (!cfg_.autoStart || state_ != SessionState::Lobby || !song_.valid()) return;
    for (u32 s = 0; s < seats_; ++s)
        if (!welcomed_[s]) return;
    // Time for the clients' clocks to settle (16 samples at 20 Hz) before the run's bar line is chosen.
    if (seats_ > 1 && localNow_ - lastJoinUs_ < static_cast<i64>(cfg_.startLeadS * 1e6f)) return;
    const double runNow = song_.songAt(hostNow_) - runOffset_;
    const double bar = (std::floor((runNow + kBar) / kBar) + 1.0) * kBar;  // a bar or more away
    startRun(bar, cfg_.seed, static_cast<u16>(world_->runId() + 1));
}

void Session::startRun(double bar, u64 seed, u16 run) {
    // The seats taken now are the run's players (a seat left open is not present).
    u32 players = 1;
    for (u32 s = 0; s < kMaxPlayers; ++s)
        if (welcomed_[s]) players = s + 1;
    MsgRunStart m;
    m.within = world_->runId();
    m.run = run;
    m.bar = bar;
    m.seed = seed;
    m.players = static_cast<u8>(players);
    m.runOffset = runOffset_;
    m.mask = seatMask_ ? seatMask_ : 1;
    reliable([&](PacketWriter& w) { return w.putRunStart(m); });
    world_->scheduleRun(bar, seed, players, run, m.mask);
    announcedRun_ = run;
    state_ = SessionState::Running;
    lobbyDirty_ = true;
}

bool Session::canStart() const {
    if (!host() || state_ != SessionState::Lobby || !song_.valid()) return false;
    u32 seated = 0;
    for (u32 s = 0; s < kMaxPlayers; ++s) seated += (lobbySeats_ >> s) & 1u;
    return seated >= 2 && (lobbyReady_ & lobbySeats_) == lobbySeats_;
}

bool Session::requestStart() {
    if (!canStart()) return false;
    const double runNow = song_.songAt(hostNow_) - runOffset_;
    const double bar = (std::floor((runNow + kBar) / kBar) + 1.0) * kBar;  // a bar or more away
    startRun(bar, cfg_.seed, static_cast<u16>(world_->runId() + 1));
    return true;
}

void Session::requestRetry() {
    // The run is over for everyone (every player down): a new one on the next bar line, the seats still taken.
    if (!host() || state_ != SessionState::Running || world_->restartPending()) return;
    world_->requestRestart();
}

void Session::onReady(u8 ready) {
    if (!host() || from_ < 1) return;
    const u8 bit = static_cast<u8>(1u << from_);
    lobbyReady_ = static_cast<u8>(ready ? (lobbyReady_ | bit) : (lobbyReady_ & ~bit));
    lobbyDirty_ = true;
}

void Session::sendBed(u32 bar, u8 level) {
    if (!host() || state_ != SessionState::Running) return;
    MsgBed m;
    m.run = world_->runId();
    m.bar = bar;
    m.level = level;
    reliable([&](PacketWriter& w) { return w.putBed(m); });
}

void Session::onBed(const MsgBed& m) {
    if (host()) return;
    bed_[bedHead_ % 8] = BedEntry{m.run, m.bar, m.level};
    ++bedHead_;
}

bool Session::bedLevel(u32 bar, u8& level) const {
    for (const BedEntry& e : bed_)
        if (e.bar == bar && e.run == world_->runId()) {
            level = e.level;
            return true;
        }
    return false;
}

void Session::onLobby(const MsgLobby& m) {
    if (host()) return;
    lobbySeats_ = m.seats;
    lobbyReady_ = m.ready;
    roomCode_ = m.code;
    for (u32 s = 0; s < kMaxPlayers; ++s) std::memcpy(names_[s], m.names[s], sizeof names_[s]);
}

void Session::requestPause(bool on) {
    if (state_ != SessionState::Running) return;
    if (host()) {
        hostPause(on);
        return;
    }
    MsgPause m;
    m.kind = on ? kPauseAsk : kResumeAsk;
    reliable([&](PacketWriter& w) { return w.putPause(m); });
}

void Session::hostPause(bool on) {
    // GAME_DESIGN 10: any player may pause; it takes hold for everyone on the next beat (at least 120 ms away,
    // so every client hears of it first). RESUME counts four beats in, as solo play does (screens.md).
    if (!song_.valid() || state_ != SessionState::Running) return;
    const double song = song_.songAt(hostNow_);
    const double tps = song_.tpsAt(hostNow_);
    if (on && !paused()) {
        const double runNow = song - runOffset_;
        // M4 net follow-up: far enough ahead that the word reaches every client even after a loss and a resend (the worst
        // client's resend timeout and a half); 120 ms alone let a client under 80 ms of jitter and 5% loss run past the
        // beat before it heard (the soak's "every peer on the same tick: NO").
        const double rto = (static_cast<double>(stats_.rttMs) + 4.0 * static_cast<double>(stats_.rttVarMs)) * 1e-3;
        const double lead = std::fmin(std::fmax(0.12 + 1.5 * rto, 0.12), 0.6);
        pauseAt_ = std::ceil((runNow + lead * tps) / kBeat) * kBeat;
        resumeAt_ = -1.0;
        MsgPause m;
        m.kind = kPauseAt;
        m.runTick = pauseAt_;
        sendPause(m);
    } else if (!on && paused() && !resuming()) {
        const double barStart = std::floor(pauseAt_ / kBar) * kBar;
        const double frac = pauseAt_ - barStart;
        const double line = std::ceil((song + 4.0 * kBeat - frac + 8.0) / kBar) * kBar;
        resumeAt_ = line + frac;
        nextOffset_ = line - barStart;
        MsgPause m;
        m.kind = kResumeAt;
        m.songTick = resumeAt_;
        m.runOffset = nextOffset_;
        sendPause(m);
    }
}

void Session::sendPause(const MsgPause& m) { reliable([&](PacketWriter& w) { return w.putPause(m); }); }

void Session::resumed() {
    runOffset_ = nextOffset_;
    pauseAt_ = resumeAt_ = -1.0;
}

void Session::fillRemote(TickInput& in, i64 hostUs) {
    // TECH_SPEC 8.1 draws a remote player about 66 ms behind its newest pose (interpolated). Predicted (M4 net follow-up,
    // docs/PROGRESS_m4_net.md): every device takes each remote player where it is at the tick's own host time, carried on
    // from its newest pose by its velocity, so the enemies following a player follow the same place on every device (66 ms
    // behind, the host's enemies chased a client's ship where it had been and the client's copies where it was). When a
    // new pose moves that estimate, the ship keeps its place through an offset that decays over kPoseSmoothS.
    const bool predict = cfg_.remotePoses == RemotePoses::Predict;
    const i64 at = predict ? hostUs : hostUs - kInterpUs;
    const i64 reach = predict ? kPredictMaxUs : kExtrapMaxUs;
    for (u32 s = 0; s < seats_ && s < kMaxPlayers; ++s) {
        if (s == seat_) continue;
        PlayerPose& p = in.players[s];
        const PoseRing& r = poses_[s];
        if (!(seatMask_ & (1u << s)) || !r.count) {
            p.present = false;
            track_[s].valid = false;
            continue;
        }
        // The two samples around `at`; past the newest, carried on by its velocity for at most 100 ms.
        const MsgPose* a = nullptr;
        const MsgPose* b = nullptr;
        for (u32 k = 0; k < r.count; ++k) {
            const MsgPose& q = r.s[(r.head + k) % kPoseRing];
            if (q.stampUs <= at) a = &q;
            else if (!b) b = &q;
        }
        PoseState st;
        ++diag_.fills[s];
        if (a && !b) {
            // Diagnostics: past the newest sample (carried on by its velocity).
            const float ahead = static_cast<float>(static_cast<double>(at - a->stampUs) * 1e-3);
            ++diag_.extrapolated[s];
            diag_.extrapSumMs[s] += ahead;
            if (ahead > diag_.extrapMaxMs[s]) diag_.extrapMaxMs[s] = ahead;
        }
        if (a && b) {
            const float t = static_cast<float>(static_cast<double>(at - a->stampUs) / static_cast<double>(b->stampUs - a->stampUs));
            st = b->cur;
            st.x = a->cur.x + (b->cur.x - a->cur.x) * t;
            st.y = a->cur.y + (b->cur.y - a->cur.y) * t;
            st.vx = a->cur.vx + (b->cur.vx - a->cur.vx) * t;
            st.vy = a->cur.vy + (b->cur.vy - a->cur.vy) * t;
            st.aim = lerpAngle(a->cur.aim, b->cur.aim, t);
            st.heading = lerpAngle(a->cur.heading, b->cur.heading, t);
        } else if (a) {
            st = a->cur;
            const i64 ahead = at - a->stampUs < reach ? at - a->stampUs : reach;
            const float dt = static_cast<float>(static_cast<double>(ahead) * 1e-6);
            st.x += st.vx * dt;
            st.y += st.vy * dt;
        } else {
            st = b->cur;
        }
        if (predict) {
            RemoteTrack& t = track_[s];
            const MsgPose& n = r.s[(r.head + r.count - 1) % kPoseRing];
            if (!t.valid) {
                t.off = Vec2{};
                t.lastAt = at;
            } else {
                if (at > t.lastAt) t.off = t.off * std::exp(-static_cast<float>(static_cast<double>(at - t.lastAt) * 1e-6) / kPoseSmoothS);
                t.lastAt = at;
                if (n.stampUs != t.stamp) {
                    // What the newest pose before this one said about this moment, against what this one says.
                    const i64 ahead = at - t.stamp < 0 ? 0 : (at - t.stamp < reach ? at - t.stamp : reach);
                    const float dt = static_cast<float>(static_cast<double>(ahead) * 1e-6);
                    const Vec2 jump = Vec2{st.x, st.y} - (t.pos + t.vel * dt);
                    t.off = lengthSq(jump) > kPoseSnapWu * kPoseSnapWu ? Vec2{} : t.off - jump;
                }
            }
            t.valid = true;
            t.stamp = n.stampUs;
            t.pos = Vec2{n.cur.x, n.cur.y};
            t.vel = Vec2{n.cur.vx, n.cur.vy};
            st.x += t.off.x;
            st.y += t.off.y;
        }
        p.present = true;
        p.pos = Vec2{st.x, st.y};
        p.vel = Vec2{st.vx, st.vy};
        p.aim = dirOf(st.aim);
        p.heading = st.heading;
        p.fire = st.fire;
        p.dashing = (st.flags & 1u) != 0;
    }
}

void Session::afterTick() {
    World& w = *world_;
    NetLog& log = w.netLog();
    if (state_ != SessionState::Running) {
        log.clear();
        return;
    }
    const u16 run = w.runId();
    if (host()) {
        // Spawn batches (at least two beats before they land), verdicts, the boss, Boosts.
        const double base = w.music();
        for (u32 k = 0; k < log.spawnCount;) {
            u32 n = rel_.putSpawns(run, base, log.spawns + k, log.spawnCount - k);
            if (!n) {
                flushReliable();
                n = rel_.putSpawns(run, base, log.spawns + k, log.spawnCount - k);
                if (!n) break;
            }
            k += n;
        }
        // Play test 2: in serial order a death costs a byte or two (putDeaths); the order within a tick does not matter.
        std::sort(log.deaths, log.deaths + log.deathCount, [](const NetDeath& a, const NetDeath& b) { return a.serial < b.serial; });
        for (u32 k = 0; k < log.deathCount;) {
            u32 n = rel_.putDeaths(run, log.deaths + k, log.deathCount - k);
            if (!n) {
                flushReliable();
                n = rel_.putDeaths(run, log.deaths + k, log.deathCount - k);
                if (!n) break;
            }
            k += n;
        }
        for (u32 k = 0; k < log.bossCount; ++k) {
            MsgBoss m;
            m.run = run;
            m.kind = log.boss[k].kind;
            m.pair = log.boss[k].pair;
            m.killer = log.boss[k].killer;
            m.boss = log.boss[k].boss;
            m.x = log.boss[k].x;
            m.y = log.boss[k].y;
            m.x2 = log.boss[k].x2;
            m.y2 = log.boss[k].y2;
            m.tick = log.boss[k].tick;
            m.changeBar = log.boss[k].changeBar;
            reliable([&](PacketWriter& pw) { return pw.putBoss(m); });
        }
        for (u32 k = 0; k < log.boostCount; ++k) {
            MsgBoostDrop m;
            m.run = run;
            m.x = log.boosts[k].x;
            m.y = log.boosts[k].y;
            m.type = log.boosts[k].type;
            reliable([&](PacketWriter& pw) { return pw.putBoostDrop(m); });
        }
        // The director's state, on change (the phrase every client follows).
        const World::DirectorSync d = w.directorSync();
        if (!directorSent_ || !(d == sentDirector_) || directorRun_ != run) {
            MsgDirector m;
            m.run = run;
            m.d = d;
            reliable([&](PacketWriter& pw) { return pw.putDirector(m); });
            sentDirector_ = d;
            directorSent_ = true;
            directorRun_ = run;
        }
    } else {
        // This player's hits, claimed from the host (TECH_SPEC 8.1: reliable, batched; they leave with the pose).
        if (log.claimCount && pendRun_ != run) writeClaims();  // a new run: the last one's go first
        pendRun_ = run;
        for (u32 k = 0; k < log.claimCount; ++k) {
            if (pendClaimCount_ == kPendingClaims) writeClaims();
            pendClaims_[pendClaimCount_++] = log.claims[k];
        }
        for (u32 k = 0; k < log.bossClaimCount;) {
            u32 n = rel_.putBossClaims(run, log.bossClaims + k, log.bossClaimCount - k);
            if (!n) {
                flushReliable();
                n = rel_.putBossClaims(run, log.bossClaims + k, log.bossClaimCount - k);
                if (!n) break;
            }
            k += n;
        }
    }
    log.clear();
    collectLocalEvents();
    if (host()) {
        // Every player down at once: the run starts again on a bar line, everyone told now.
        if (w.restartPending() && w.restartRunId() != announcedRun_) {
            MsgRunStart m;
            m.within = run;
            m.run = w.restartRunId();
            m.bar = w.restartTick();
            m.seed = w.restartSeed();
            m.players = static_cast<u8>(w.restartPlayers());
            m.runOffset = runOffset_;
            m.mask = w.restartMask() ? w.restartMask() : 1;  // the seats still taken (one left during the run: not in it)
            reliable([&](PacketWriter& pw) { return pw.putRunStart(m); });
            announcedRun_ = m.run;
        }
        // The census: the host's enemies at this point of its stream.
        if (localNow_ >= nextCensusUs_) {
            MsgCensus c;
            c.run = run;
            c.digest = w.netCensus(c.count);
            reliable([&](PacketWriter& pw) { return pw.putCensus(c); });
            nextCensusUs_ = localNow_ + kCensusEveryUs;
        }
    }
}

void Session::collectLocalEvents() {
    World& w = *world_;
    for (u32 k = 0; k < w.eventCount(); ++k) {
        const Event& e = w.events()[k];
        if (host() && e.type == EventType::Death && e.a == 0xFF && !w.restartPending()) {
            // GAME_DESIGN 10: the run ends when all are down at once. Headless peers go again at once; in the game
            // the host's RETRY does (requestRetry).
            if (cfg_.autoRestart) w.requestRestart();
            continue;
        }
        if (e.a >= kMaxPlayers || !w.isLocal(e.a)) continue;
        MsgPlayerEvent m;
        m.run = w.runId();
        m.player = e.a;
        m.x = e.x;
        m.y = e.y;
        m.x2 = e.x2;
        m.y2 = e.y2;
        switch (e.type) {
            case EventType::NovaPress:
                m.kind = kPeNova;
                m.tick = e.tick;
                break;
            case EventType::Dash:
                m.kind = kPeDash;
                m.b = e.b;
                break;
            case EventType::Hit:
                m.kind = kPeHit;
                m.b = e.b;
                break;
            case EventType::Down: m.kind = kPeDown; break;
            case EventType::Revive:
                m.kind = kPeRevive;
                m.b = e.b;
                break;
            case EventType::TrioOpen: {
                const TrioState& tr = w.trio(e.a);
                m.kind = kPeTrioOpen;
                m.b = tr.orbs > kMaxOrbs ? static_cast<u8>(kMaxOrbs) : tr.orbs;
                for (u32 o = 0; o < m.b; ++o) m.opts[o] = tr.upgrade[o];
                m.tick = e.tick;
                break;
            }
            case EventType::TrioTake:
                m.kind = kPeTrioTake;
                m.b = e.b;
                break;
            case EventType::BoostTake:
                m.kind = kPeBoostTake;
                m.b = e.b;
                break;
            case EventType::HaloBlock:
                m.kind = kPeHalo;
                m.b = e.b;
                break;
            case EventType::Upgrade:
                // M5 part 3: a Singularity's well, where this device's player placed it (the others pull to the same place).
                if (e.b != kFxWell) continue;
                m.kind = kPeWell;
                m.b = e.c;
                m.tick = e.tick;
                break;
            default: continue;
        }
        reliable([&](PacketWriter& pw) { return pw.putPlayerEvent(m); });
    }
}

// ---------------------------------------------------------------------------------------------- out

template <typename F>
void Session::reliable(F&& put) {
    if (put(rel_)) return;
    flushReliable();
    if (!put(rel_)) ++stats_.droppedOutgoing;  // a message larger than a packet: cannot happen
}

void Session::writeClaims() {
    // The claims since the last send, one per enemy where they can be: hits on one enemy add up, a Nova's kill takes
    // the hits' place (the host's result is the same), sorted by serial (a byte or two an item, putClaims).
    if (!pendClaimCount_) return;
    std::sort(pendClaims_, pendClaims_ + pendClaimCount_, [](const NetClaim& a, const NetClaim& b) { return a.serial < b.serial; });
    u32 m = 0;
    for (u32 k = 0; k < pendClaimCount_; ++k) {
        const NetClaim& c = pendClaims_[k];
        if (m && pendClaims_[m - 1].serial == c.serial) {
            NetClaim& p = pendClaims_[m - 1];
            if (p.kind != kClaimHit) continue;  // dead by a kill already
            if (c.kind != kClaimHit) {
                p = c;
                continue;
            }
            if (p.damage + c.damage <= kMaxClaimDamage) {
                p.damage += c.damage;
                continue;
            }
        }
        pendClaims_[m++] = c;
    }
    for (u32 k = 0; k < m;) {
        u32 n = rel_.putClaims(pendRun_, pendClaims_ + k, m - k);
        if (!n) {
            flushReliable();
            n = rel_.putClaims(pendRun_, pendClaims_ + k, m - k);
            if (!n) break;
        }
        k += n;
    }
    pendClaimCount_ = 0;
}

void Session::flushReliable() {
    if (rel_.empty()) return;
    if (host()) {
        for (u32 s = 1; s < kMaxPlayers; ++s)
            if (welcomed_[s] && seatPeer_[s] >= 0) sendTo(static_cast<u32>(seatPeer_[s]), kChReliable, rel_, true);
    } else if (state_ != SessionState::Connecting && state_ != SessionState::Ended) {
        sendTo(0, kChReliable, rel_, true);
    }
    rel_.reset();
}

void Session::sendTo(u32 peer, u8 channel, const PacketWriter& w, bool rel) {
    if (w.empty()) return;
    if (!link_.send(peer, channel, w.data(), w.size(), rel) && rel) ++stats_.droppedOutgoing;
    if (channel < 3) chBytes_[channel] += static_cast<double>(w.size());
    if (host() && peer < kMaxLinkPeers) {
        // An estimate until it has left (refillBudgets() puts what the wire carried in its place).
        const float est = static_cast<float>(w.size()) + kDatagramOverhead;
        if (peerSeat_[peer] > 0) downBudget_[peerSeat_[peer]] -= est;
        pending_[peer] += est;
    }
}

void Session::refillBudgets(i64 localUs) {
    // Host: each client's downlink budget pays for what went to it on the wire (ENet's headers, acknowledgements
    // and resends included) in place of the estimates, then refills at kDownBytesPerS, at most kDownBurstBytes
    // saved. Every byte to a client is counted against it; corrections only take what is left.
    const float dt = budgetUs_ ? static_cast<float>(static_cast<double>(localUs - budgetUs_) * 1e-6) : 0.0f;
    budgetUs_ = localUs;
    for (u32 p = 0; p < kMaxLinkPeers; ++p) {
        const u64 sent = link_.sentTo(p);
        const float wire = static_cast<float>(sent >= sentTo_[p] ? sent - sentTo_[p] : sent);
        sentTo_[p] = sent;
        const i32 seat = peerSeat_[p];
        if (seat > 0) downBudget_[seat] += pending_[p] - wire;
        pending_[p] = 0.0f;
    }
    for (float& b : downBudget_) b = std::fmin(b + cfg_.downBytesPerS * (dt > 0.1f ? 0.1f : dt), kDownBurstBytes);
}

void Session::relayEvent(const MsgPlayerEvent& m, u32 exceptSeat) {
    // Another client's event: to every other client, in the reliable stream's order.
    flushReliable();
    for (u32 s = 1; s < kMaxPlayers; ++s) {
        if (!welcomed_[s] || s == exceptSeat || seatPeer_[s] < 0) continue;
        scratch_.reset();
        scratch_.putPlayerEvent(m);
        sendTo(static_cast<u32>(seatPeer_[s]), kChReliable, scratch_, true);
    }
}

void Session::updatePriorities(float dt) {
    // The priority accumulator (TECH_SPEC 8.1): an enemy gains priority with time unsent, faster on and near the
    // client's screen and the faster it moves; a correction sends the highest and resets them.
    const World& w = *world_;
    const Enemies& e = w.enemies();
    for (u32 s = 1; s < seats_; ++s) {
        if (!welcomed_[s]) continue;
        const Vec2 c = w.player(s).pos;
        const double landedRecently = 2.0 * static_cast<double>(w.ticksPerSecond());  // musical ticks in 2 s
        for (u32 i = 0; i < e.count; ++i) {
            if (e.flags[i] & kFlagDead) continue;
            const u32 slot = e.handle[i] & 0xFFFFu;
            if (prioSerial_[s][slot] != e.serial[i]) {
                prioSerial_[s][slot] = e.serial[i];
                prio_[s][slot] = 0.0f;
            }
            // Every enemy gains (a copy far off here may be on the client's screen there), more on and near the
            // client's screen, more when it has just landed (a spawn that reached the client late landed there at
            // once, where it was spawned) and the faster it moves; one that has not landed hardly at all.
            const float dx = std::fabs(e.x[i] - c.x), dy = std::fabs(e.y[i] - c.y);
            float wgt = 0.6f;
            if (dx <= 1100.0f && dy <= 520.0f) wgt = 8.0f;        // on the screen (and a margin)
            else if (dx <= 1700.0f && dy <= 800.0f) wgt = 2.0f;   // about two frames
            if (e.state[i] == kWarping) {
                wgt *= 0.05f;
            } else if (e.state[i] == kMoving && w.music() - e.until[i] >= 0.0 && w.music() - e.until[i] < landedRecently) {
                wgt += 4.0f;
            }
            wgt *= cfg_.familyWeight[static_cast<u32>(w.species().get(e.species[i]).family) & 15u];
            const float sp = std::fabs(e.vx[i]) + std::fabs(e.vy[i]) + std::fabs(e.kx[i]) + std::fabs(e.ky[i]);
            prio_[s][slot] += wgt * (1.0f + sp * (1.0f / 300.0f)) * dt;
        }
    }
}

static_assert(static_cast<u32>(Family::Shoal) == 2, "SessionConfig::familyWeight's default doubles index 2, the Shoals");

bool Session::motionOnly(u32 seat, u32 i) const {
    // A moving Shoal's correction is its motion (M4 net follow-up): what diverges in a boid is where it is and where it
    // heads. Every kFullEvery-th correction of it, and the first after it took damage, is a full one.
    if (!cfg_.motionCorrections) return false;
    const World& w = *world_;
    const Enemies& e = w.enemies();
    const u32 slot = e.handle[i] & 0xFFFFu;
    if (e.state[i] != kMoving || (e.flags[i] & kFlagCalled) || w.species().get(e.species[i]).family != Family::Shoal) return false;
    return motionRun_[seat][slot] + 1u < kFullEvery && e.hp[i] >= fullHp_[seat][slot];
}

void Session::sendCorrections(u32 seat, double hostMusic) {
    const World& w = *world_;
    const Enemies& e = w.enemies();
    // As many as the client's downlink budget allows this time (a burst of spawns leaves fewer), by bytes: a full
    // correction costs kCorrectionBytes, a motion correction about a third of it.
    const float room = downBudget_[seat] - kDatagramOverhead - 2.0f * static_cast<float>(kCorrectionsHeader);
    if (room < 4.0f * static_cast<float>(kCorrectionBytes)) return;
    const u32 maxBytes = static_cast<u32>(room) < kMaxPacketBytes - 2u * kCorrectionsHeader ? static_cast<u32>(room)
                                                                                            : kMaxPacketBytes - 2u * kCorrectionsHeader;
    const u32 cap = cfg_.motionCorrections ? kCorrPerPacket + kMotionsPerPacket : kCorrPerPacket;
    u32 n = 0;
    for (u32 i = 0; i < e.count; ++i) {
        if (e.flags[i] & kFlagDead) continue;
        const u32 slot = e.handle[i] & 0xFFFFu;
        if (prioSerial_[seat][slot] != e.serial[i] || prio_[seat][slot] <= 0.0f) continue;
        picks_[n++] = Pick{prio_[seat][slot], i};
    }
    if (!n) return;
    const u32 k = n < cap ? n : cap;
    if (k < n) std::nth_element(picks_, picks_ + k, picks_ + n, [](const Pick& a, const Pick& b) { return a.p > b.p; });
    std::sort(picks_, picks_ + k, [](const Pick& a, const Pick& b) { return a.p > b.p; });
    // A school is corrected together (its cohesion would pull one corrected fish back into a school that went
    // its own way): the schools of the most urgent picks bring their members, the rest of the room goes in order.
    u16 groups[4];
    u32 ng = 0;
    for (u32 j = 0; j < k && ng < 4; ++j) {
        const u16 g = e.group[picks_[j].i];
        if (!g) continue;
        bool seen = false;
        for (u32 q = 0; q < ng; ++q) seen = seen || groups[q] == g;
        if (!seen) groups[ng++] = g;
    }
    u32 m = 0, mm = 0, bytes = 0;
    bool full = false;  // the room is spent
    const auto take = [&](u32 i) {
        const u32 slot = e.handle[i] & 0xFFFFu;
        if (full || prio_[seat][slot] < 0.0f) return;  // full, or taken already this time
        if (motionOnly(seat, i)) {
            // Its serial's step is at most 5 bytes; most are one or two (the bytes left over pay for that).
            const u32 cost = 10u + ((e.kx[i] != 0.0f || e.ky[i] != 0.0f) ? 2u : 0u);
            if (mm >= kMotionsPerPacket || bytes + cost > maxBytes) {
                full = mm >= kMotionsPerPacket || bytes + 8u > maxBytes;
                return;
            }
            NetMotion& o = motions_[mm];
            o.serial = e.serial[i];
            o.x = e.x[i];
            o.y = e.y[i];
            o.heading = e.heading[i];
            o.kx = e.kx[i];
            o.ky = e.ky[i];
            o.target = e.target[i];
            o.engaged = (e.flags[i] & kFlagEngaged) != 0;
            o.frozen = e.frozenUntil[i] > w.music();
            ++mm;
            bytes += cost;
            ++motionRun_[seat][slot];
        } else {
            if (m >= kCorrPerPacket || bytes + kCorrectionBytes > maxBytes) {
                full = m >= kCorrPerPacket && !cfg_.motionCorrections ? true : bytes + 8u > maxBytes;
                return;
            }
            if (!w.netSample(i, states_[m])) return;
            ++m;
            bytes += kCorrectionBytes;
            motionRun_[seat][slot] = 0;
            fullHp_[seat][slot] = e.hp[i];
        }
        prio_[seat][slot] = -1.0f;
    };
    for (u32 j = 0; j < k && !full; ++j) {
        take(picks_[j].i);
        const u16 g = e.group[picks_[j].i];
        bool school = false;
        for (u32 q = 0; q < ng; ++q) school = school || groups[q] == g;
        if (!g || !school) continue;
        for (u32 i = 0; i < e.count && !full; ++i)
            if (e.group[i] == g && !(e.flags[i] & kFlagDead) && prioSerial_[seat][e.handle[i] & 0xFFFFu] == e.serial[i]) take(i);
        for (u32 q = 0; q < ng; ++q)
            if (groups[q] == g) groups[q] = 0;  // its members are in
    }
    for (u32 j = 0; j < m; ++j) {
        const i32 i = w.indexOfSerial(states_[j].serial);
        if (i >= 0) prio_[seat][e.handle[i] & 0xFFFFu] = 0.0f;
    }
    for (u32 j = 0; j < mm; ++j) {
        const i32 i = w.indexOfSerial(motions_[j].serial);
        if (i >= 0) prio_[seat][e.handle[i] & 0xFFFFu] = 0.0f;
    }
    // Motion corrections go in serial order (each serial a step from the one before).
    std::sort(motions_, motions_ + mm, [](const NetMotion& a, const NetMotion& b) { return a.serial < b.serial; });
    scratch_.reset();
    if (m) scratch_.putCorrections(w.runId(), hostMusic, states_, m);
    u32 sentMotions = mm ? scratch_.putMotions(w.runId(), hostMusic, motions_, mm) : 0u;
    for (u32 j = sentMotions; j < mm; ++j) {
        // (did not fit: they keep a priority to go next time)
        const i32 i = w.indexOfSerial(motions_[j].serial);
        if (i >= 0) prio_[seat][e.handle[i] & 0xFFFFu] = 1000.0f;
    }
    sendTo(static_cast<u32>(seatPeer_[seat]), kChUnreliable, scratch_, false);
    corrCount_ += m + sentMotions;
    stats_.corrections += m + sentMotions;
    stats_.motions += sentMotions;
}

void Session::send(i64 localUs, const PlayerPose& pose) {
    if (!link_.open()) return;
    localNow_ = localUs;
    if (host()) refillBudgets(localUs);
    const World& w = *world_;
    if (host() && (state_ == SessionState::Lobby || state_ == SessionState::Running)) {
        // M4 stage B: the lobby to every client on each change, and the beacon (twice a second) for the joiners.
        if (lobbyDirty_) {
            MsgLobby m;
            m.seats = lobbySeats_;
            m.ready = lobbyReady_;
            m.code = roomCode_;
            m.state = state_ == SessionState::Running ? 1 : 0;
            for (u32 s = 0; s < kMaxPlayers; ++s) std::memcpy(m.names[s], names_[s], sizeof m.names[s]);
            reliable([&](PacketWriter& pw) { return pw.putLobby(m); });
            lobbyDirty_ = false;
        }
        if (cfg_.beacon) {
            BeaconInfo b;
            b.version = kProtocolVersion;
            b.port = cfg_.port;
            b.code = roomCode_;
            for (u32 s = 0; s < kMaxPlayers; ++s) b.players = static_cast<u8>(b.players + ((lobbySeats_ >> s) & 1u));
            b.seats = static_cast<u8>(seats_);
            b.state = state_ == SessionState::Running ? kBeaconRunning : kBeaconLobby;
            std::snprintf(b.name, sizeof b.name, "%s", cfg_.name);
            beacon_.update(localUs, b);
        }
    }
    if (!host() && state_ == SessionState::Lobby && !readySent_ && synced()) {
        // Ready once this device's clock has settled on the host's (the seat stops "joining").
        reliable([&](PacketWriter& pw) { return pw.putReady(1); });
        readySent_ = true;
    }
    const bool live = state_ == SessionState::Running || state_ == SessionState::Lobby;
    const bool hostQuiet = !host() && link_.peerStats(0).silentMs > kLinkSilentMs;
    // Everything this device sends leaves with its pose (30 Hz): a frame sends one datagram to a peer, or none
    // (each datagram costs 32 bytes of headers). Acknowledgements are ENet's own.
    const bool poseFrame = live && localUs >= nextPoseUs_;
    if (poseFrame) {
        if (poseSentUs_) {
            const float gap = static_cast<float>(static_cast<double>(localUs - poseSentUs_) * 1e-3);
            if (gap > diag_.poseSendGapMaxMs) diag_.poseSendGapMaxMs = gap;
            if (gap > 50.0f) ++diag_.poseSendGapsOver50;
        }
        poseSentUs_ = localUs;
        // The own player: now and the last state (TECH_SPEC 8.1: unreliable, 30 Hz, two states).
        const PlayerState& me = w.player(seat_);
        MsgPose m;
        m.player = static_cast<u8>(seat_);
        m.stampUs = hostNow_;
        m.cur.x = pose.pos.x;
        m.cur.y = pose.pos.y;
        m.cur.vx = pose.vel.x;
        m.cur.vy = pose.vel.y;
        m.cur.aim = angleOf(pose.aim);
        m.cur.heading = pose.heading;
        m.cur.fire = me.alive ? pose.fire : 0;
        m.cur.flags = static_cast<u8>((pose.dashing ? 1u : 0u) | (me.alive ? 2u : 0u) | (me.down ? 4u : 0u));
        // A client stamps its pose in host time once its clock has settled on the host's (synced(): before that its
        // "host time" is its own clock, which may be days off); until then it sends none.
        // A client whose host has gone quiet (kLinkSilentMs: gone, or a Wi-Fi hole) holds its poses: nobody hears them.
        const bool stamped = host() || (synced() && !hostQuiet);
        const i64 prevAge = havePrev_ ? hostNow_ - ownPrev_.stampUs : -1;
        m.prevAgeMs = prevAge > 0 && prevAge <= 1000000 ? static_cast<i16>(prevAge / 1000) : -1;
        if (m.prevAgeMs >= 0) m.prev = ownPrev_.cur;
        if (stamped) {
            ownPrev_ = m;
            havePrev_ = true;
        }
        if (host()) {
            // The other players' newest poses, to each client (not its own back).
            MsgPose all[kMaxPlayers];
            for (u32 c = 1; c < kMaxPlayers; ++c) {
                if (!welcomed_[c] || seatPeer_[c] < 0) continue;
                u32 n = 0;
                all[n++] = m;
                for (u32 s = 1; s < seats_; ++s) {
                    const PoseRing& r = poses_[s];
                    if (s == c || !welcomed_[s] || !r.count) continue;
                    all[n++] = r.s[(r.head + r.count - 1) % kPoseRing];
                    ++diag_.posesRelayed[s];
                }
                scratch_.reset();
                scratch_.putPlayers(all, n);
                sendTo(static_cast<u32>(seatPeer_[c]), kChUnreliable, scratch_, false);
            }
        } else {
            scratch_.reset();
            if (stamped) scratch_.putPose(m);
            if (state_ == SessionState::Running && localUs >= nextAttentionUs_) {
                const u32 n = attention();
                if (n && scratch_.putAttention(w.runId(), attention_, n)) stats_.attention += n;
                nextAttentionUs_ = localUs + kAttentionEveryUs;
            }
            sendTo(0, kChUnreliable, scratch_, false);
        }
        nextPoseUs_ = localUs - nextPoseUs_ > kPoseEveryUs ? localUs + kPoseEveryUs : nextPoseUs_ + kPoseEveryUs;
    }
    if (!host() && poseFrame && localUs >= nextBeatUs_) {
        // This player's numbers, 4 Hz.
        const PlayerState& me = w.player(seat_);
        MsgStats st;
        st.player = static_cast<u8>(seat_);
        st.cells = me.cells;
        st.novas = me.novas;
        st.chain = me.chain;
        st.kills = me.kills;
        st.score = me.score;
        scratch_.reset();
        scratch_.putStats(&st, 1);
        sendTo(0, kChUnreliable, scratch_, false);
        nextBeatUs_ = localUs + kBeatEveryUs;
    }
    if (host() && poseFrame) {
        // The song clock (4 Hz) with the boss's health.
        if (localUs >= nextBeatUs_ && song_.valid()) {
            MsgHeartbeat m;
            m.run = w.runId();
            const SongAnchor& a = song_.anchor();
            m.hostUs = hostNow_;
            m.song = song_.songAt(hostNow_);
            m.tps = static_cast<float>(song_.tpsAt(hostNow_));
            m.nextBar = m.song < a.nextBar ? a.nextBar : (std::floor(m.song / kBar) + 1.0) * kBar;
            m.tpsNext = static_cast<float>(m.song < a.nextBar ? a.tpsNext : a.tpsNext);
            m.runOffset = runOffset_;
            m.state = paused() ? 2 : (state_ == SessionState::Running ? 1 : 0);
            const BossState& b = w.boss();
            m.bossOn = b.active && !b.dying ? 1 : 0;
            m.bossParts = static_cast<u8>(w.bossPartCount());
            for (u32 k = 0; k < m.bossParts; ++k) m.bossPart[k] = b.partHp[k];
            m.bossCore = b.coreHp;
            const Vec2 sync = w.bossSyncPoint();
            m.bossX = sync.x;
            m.bossY = sync.y;
            scratch_.reset();
            scratch_.putHeartbeat(m);
            // Every player's numbers ride with it.
            MsgStats st[kMaxPlayers];
            u32 n = 0;
            for (u32 s = 0; s < seats_; ++s) {
                if (!welcomed_[s]) continue;
                const PlayerState& pl = w.player(s);
                st[n].player = static_cast<u8>(s);
                st[n].cells = pl.cells;
                st[n].novas = pl.novas;
                st[n].chain = pl.chain;
                st[n].kills = pl.kills;
                st[n].score = pl.score;
                ++n;
            }
            if (n) scratch_.putStats(st, n);
            for (u32 s = 1; s < kMaxPlayers; ++s)
                if (welcomed_[s] && seatPeer_[s] >= 0) sendTo(static_cast<u32>(seatPeer_[s]), kChUnreliable, scratch_, false);
            nextBeatUs_ = localUs + kBeatEveryUs;
        }
        // Corrections, each client every other pose (15 Hz).
        if (state_ == SessionState::Running) {
            const float dt = lastPrioUs_ ? static_cast<float>(static_cast<double>(localUs - lastPrioUs_) * 1e-6) : 0.0f;
            lastPrioUs_ = localUs;
            updatePriorities(dt > 0.1f ? 0.1f : dt);
            for (u32 s = 1; s < seats_; ++s) {
                if (!welcomed_[s] || seatPeer_[s] < 0 || localUs < nextCorrUs_[s]) continue;
                sendCorrections(s, w.music());
                nextCorrUs_[s] = localUs - nextCorrUs_[s] > kCorrEveryUs ? localUs + kCorrEveryUs : nextCorrUs_[s] + kCorrEveryUs;
            }
        }
    }
    // Reliable data leaves with the pose (a packet that fills goes at once); before the lobby, every frame.
    if (poseFrame || !live) {
        if (!hostQuiet) writeClaims();  // (held while the host is quiet: kPendingClaims of them at most, then they go)
        flushReliable();
    }
    // A client's ping (TECH_SPEC 8.2: 4 Hz; 20 Hz until it has 16 samples), stamped as it leaves.
    if (!host() && poseFrame && localUs >= nextPingUs_) {
        scratch_.reset();
        scratch_.putPing(nowLocal());
        sendTo(0, kChControl, scratch_, false);
        nextPingUs_ = localUs + (clock_.samples() < ClockSync::kSamples ? kPingFastUs : kPingUs);
    }
    // The one service of the frame: what was queued leaves with the acknowledgements; what came is handled.
    service();
    link_.flush();
    measure(localUs);
}

void Session::measure(i64 localUs) {
    if (!measureFrom_) {
        measureFrom_ = localUs;
        lastWire_ = link_.wire();
        return;
    }
    const i64 span = localUs - measureFrom_;
    if (span < 1000000) return;
    const double s = static_cast<double>(span) * 1e-6;
    const WireStats now = link_.wire();
    const double up = static_cast<double>(now.sentBytes - lastWire_.sentBytes) +
                      static_cast<double>(now.sentDatagrams - lastWire_.sentDatagrams) * kIpUdpHeaderBytes;
    const double down = static_cast<double>(now.recvBytes - lastWire_.recvBytes) +
                        static_cast<double>(now.recvDatagrams - lastWire_.recvDatagrams) * kIpUdpHeaderBytes;
    stats_.upKbps = static_cast<float>(up * 8.0 / 1000.0 / s);
    stats_.downKbps = static_cast<float>(down * 8.0 / 1000.0 / s);
    stats_.upRelKbps = static_cast<float>(chBytes_[kChReliable] * 8.0 / 1000.0 / s);
    stats_.upUnrelKbps = static_cast<float>(chBytes_[kChUnreliable] * 8.0 / 1000.0 / s);
    stats_.upCtrlKbps = static_cast<float>(chBytes_[kChControl] * 8.0 / 1000.0 / s);
    stats_.upDatagrams = static_cast<u32>(now.sentDatagrams - lastWire_.sentDatagrams);
    chBytes_[0] = chBytes_[1] = chBytes_[2] = 0.0;
    stats_.totalUpBytes += up;
    stats_.totalDownBytes += down;
    stats_.maxDatagram = now.maxDatagram;
    lastWire_ = now;
    stats_.correctionsPerS = static_cast<float>(corrCount_ / s);
    stats_.errRmsWu = errN_ ? static_cast<float>(std::sqrt(errSum_ / errN_)) : 0.0f;
    stats_.errMaxWu = errMax_;
    corrCount_ = errN_ = 0;
    errSum_ = 0.0;
    errMax_ = 0.0f;
    measureFrom_ = localUs;
    // The link.
    stats_.peers = 0;
    stats_.rttMs = stats_.rttVarMs = stats_.loss = 0.0f;
    stats_.silentMs = stats_.inFlight = 0;
    for (u32 p = 0; p < kMaxLinkPeers; ++p) {
        const LinkPeerStats ps = link_.peerStats(p);
        if (!ps.connected) continue;
        ++stats_.peers;
        if (ps.silentMs > stats_.silentMs) stats_.silentMs = ps.silentMs;
        stats_.inFlight += ps.inFlight;
        if (static_cast<float>(ps.rttMs) > stats_.rttMs) stats_.rttMs = static_cast<float>(ps.rttMs);
        if (static_cast<float>(ps.rttVarMs) > stats_.rttVarMs) stats_.rttVarMs = static_cast<float>(ps.rttVarMs);
        if (ps.loss > stats_.loss) stats_.loss = ps.loss;
    }
    if (!host()) {
        // A predicted kill waits for its verdict as long as a claim and its answer take with one resend each way
        // (the round trip and its variance, twice) plus the frames between them; never less than TECH_SPEC 8.1's 300 ms.
        const float rto = (stats_.rttMs + 4.0f * stats_.rttVarMs) * 1e-3f;
        world_->setPredictUndoS(std::fmin(std::fmax(kVerdictSlackS + 2.0f * rto, tuning::kPredictUndoS), kVerdictMaxS));
        stats_.predictUndoMs = world_->predictUndoS() * 1e3f;
        stats_.clockOffsetMs = static_cast<float>(static_cast<double>(clock_.estimateUs()) * 1e-3);
        stats_.clockRttMs = static_cast<float>(static_cast<double>(clock_.rttUs()) * 1e-3);
        stats_.dilation = clock_.dilation();
        // The song by this device's steered clock against the song at its best estimate of host time.
        if (song_.valid()) {
            const double a = song_.songAt(hostNow_), b = song_.songAt(localUs + clock_.estimateUs());
            stats_.songOffsetMs = static_cast<float>((a - b) / song_.tpsAt(hostNow_) * 1000.0);
        }
    }
    stats_.censusChecks = world_->counters().censusChecks;
    stats_.censusMismatches = world_->counters().censusMismatches;
}

}  // namespace rf::net
