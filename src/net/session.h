#pragma once
// A co-op session (M4; TECH_SPEC 8.1-8.3): one device's side of the game over a Link, bound to its World.
//
// Host: listens, seats the clients (seat 0 is its own), starts the run on a bar line when every seat is in,
// and runs the world's director. After each tick it sends what the tick decided: spawn batches (at least two
// beats before they land), verdicts on enemies, the boss, Boost drops, the director's state, its own player's
// events, and a census of its enemies every two seconds; every frame the players' poses (30 Hz), enemy
// corrections chosen by a priority accumulator (at most 1,200 bytes a packet, 15 Hz per client) and the song
// clock (4 Hz). Clients' hit claims and events reach its world as operations of the next tick.
//
// Client: joins, keeps its clock on the host's (net/sync.h), runs its world without a director (the host's
// spawns and verdicts arrive as operations), claims its own player's hits and predicts the kills, applies
// corrections, and sends its pose (30 Hz, the last two states) and its player's events.
//
// The driver (the game, a headless bot peer) calls, each frame: receive(), then per tick fillRemote() and
// world.tick() and afterTick(), then send(). Nothing here allocates after start().

#include "net/link.h"
#include "net/lobby.h"
#include "net/protocol.h"
#include "net/sync.h"
#include "sim/world.h"

namespace rf::net {

enum class SessionState : u8 { Idle, Connecting, Lobby, Running, Ended };
// Where a remote player is taken for a tick (fillRemote): ~66 ms behind its newest pose, interpolated (TECH_SPEC 8.1 as
// written), or at the tick's own time, carried on from its newest pose (docs/PROGRESS_m4_net.md).
enum class RemotePoses : u8 { Interpolate, Predict };

struct SessionConfig {
    sim::NetRole role = sim::NetRole::Host;
    u32 seats = 2;              // host: players in the run, itself included; it starts when all are in
    char bindIp[48] = {};       // host: the address to listen on ("" every address); client: to send from
    char joinIp[48] = {};       // client: the host's address
    u16 port = kGamePort;
    WireKind wire = WireKind::Udp;
    NetSim sim;
    char name[16] = "PLAYER";
    u64 seed = 1;
    float startLeadS = 3.0f;    // host: the run's first bar line at least this far after the last seat joined
    // M4 stage B. Headless peers, bots and the soak start when every seat is in and start again when all are down;
    // the game's lobby starts on START and again on RETRY, and the host's beacon is heard on the network.
    bool autoStart = true;
    bool autoRestart = true;
    bool beacon = false;
    RemotePoses remotePoses = RemotePoses::Interpolate;
    // Host: the corrections' share of each client's downlink (bytes a second, every byte to the client counted) and how
    // fast each enemy family gains priority (experiments: raveform_headless --net-tune).
    float downBytesPerS = 20000.0f;
    float familyWeight[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
};
// "down=20000,fam2=3,..." onto a config (raveform_headless --net-tune); false on a malformed spec.
bool parseNetTune(const char* spec, SessionConfig& c);

// What the net overlay and the soak report show (per second where it says so).
struct SessionStats {
    float rttMs = 0.0f, rttVarMs = 0.0f, loss = 0.0f;  // client: to the host; host: the worst client
    u32 silentMs = 0, inFlight = 0;                     // the longest a peer has been silent; reliable bytes unacknowledged
    float upKbps = 0.0f, downKbps = 0.0f;               // this device, last second, with IP/UDP headers
    float correctionsPerS = 0.0f;                       // client: applied; host: sent
    float errRmsWu = 0.0f, errMaxWu = 0.0f;             // client: position error the corrections found, last second
    float songOffsetMs = 0.0f;                          // client: its song against the host's, by its clock estimate
    float clockOffsetMs = 0.0f, clockRttMs = 0.0f;      // client: host - local (ms), the sample's round trip
    float dilation = 0.0f;                              // client: the clock's steering (rate - 1)
    float predictUndoMs = 0.0f;                         // client: how long a predicted kill waits for its verdict
    u32 snaps = 0, corrections = 0;                     // totals
    u32 corrUnknown = 0, corrOtherRun = 0;              // client: corrections for no enemy here / another run
    // Client: enemies on this player's screen, drawn position against the host's sample as each correction lands.
    double screenErrSum2 = 0.0;
    u64 screenErrN = 0;
    float screenErrMax = 0.0f;
    u32 peers = 0;                                      // connected
    u32 disconnects = 0, refused = 0, badPackets = 0;
    u32 censusChecks = 0, censusMismatches = 0;
    u32 attention = 0;                                  // client: enemies asked for; host: asks received (totals)
    u32 maxDatagram = 0;                                // the largest UDP payload this device sent
    u32 droppedOutgoing = 0;                            // reliable data that could not be queued (must stay 0)
    double totalUpBytes = 0.0, totalDownBytes = 0.0;    // since start, with headers
};

// Diagnostics (docs/PROGRESS_m4_net.md): where a client's error at correction comes from, how the other players' poses
// arrive and how the host's song anchors sit on the model they replace. Sums over a window that the reader resets
// (resetDiag); nothing here changes what the session does.
struct NetDiagBin {
    double sum2 = 0.0;
    u64 n = 0;
    void add(float e) {
        sum2 += static_cast<double>(e) * e;
        ++n;
    }
    float rms() const;
};
struct NetDiag {
    // Corrections landing on enemies on this player's screen (the error at correction, as SessionStats::screenErr*).
    NetDiagBin all;
    NetDiagBin byTarget[4];    // the host's copy follows: 0 this device's player, 1 the host's player, 2 another client's, 3 none
    NetDiagBin targetDiffers;  // this copy followed another player than the host's copy did
    NetDiagBin stateDiffers;   // in another state (moving, tell, dash, rest ...) than the host's copy
    NetDiagBin byFamily[16];
    NetDiagBin byAge[6];       // since the last correction applied to it here: < 0.25, 0.5, 1, 2, 4 s, longer or none
    NetDiagBin perp;           // moving enemies: the error square to the host's velocity, ...
    double tauSum = 0.0, tau2Sum = 0.0;  // ... and along it as a time offset (s; > 0: this copy is ahead)
    u64 tauN = 0;
    // The other players' poses (by seat): arrivals, the gap between them, their age on arrival (host time then less the
    // stamp: one-way delay, the frame it waited for and the clock estimate's error), and how far fillRemote reached past
    // the newest sample (extrapolated).
    u32 poses[4] = {};
    u32 posesHeard[4] = {};    // every pose of that seat parsed, repeats and the redundant previous state included
    u32 posesRelayed[4] = {};  // host: poses of that seat relayed to the other clients
    u32 posesMisstamped[4] = {};  // dropped: stamped too far from this device's host time (another clock, or stale)
    u32 badPackets = 0;
    float poseGapMaxMs[4] = {};
    u32 poseGapsOver66[4] = {}, poseGapsOver100[4] = {};
    double poseAgeSumMs[4] = {};
    float poseAgeMinMs[4] = {1e9f, 1e9f, 1e9f, 1e9f}, poseAgeMaxMs[4] = {-1e9f, -1e9f, -1e9f, -1e9f};
    u32 fills[4] = {}, extrapolated[4] = {};
    double extrapSumMs[4] = {};
    float extrapMaxMs[4] = {};
    // The host's song anchors: each against the model it replaces, at its own host time (ms of song).
    NetDiagBin songResidualMs;
    float songResidualMaxMs = 0.0f;
    // Correction packets: the longest gap between two arrivals (ms).
    float corrGapMaxMs = 0.0f;
    u32 corrPackets = 0;
    // This device's own frames and ticks (the driver reports them): the longest frame, ticks a frame (0, 1, 2, 3), ticks
    // skipped because the song had not moved past the last one, time dropped (more than three ticks due); its pose sends
    // (the longest gap between two); on the host, each new song anchor (the band's heard clock) against the model it
    // replaces, at its own host time (ms of song), and how many went backwards.
    u32 frames = 0;
    float frameMaxMs = 0.0f;
    u32 ticksPerFrame[4] = {};
    u32 ticksSkipped = 0, timeDropped = 0;
    float poseSendGapMaxMs = 0.0f;
    u32 poseSendGapsOver50 = 0;
    NetDiagBin anchorResidualMs;
    float anchorResidualMaxMs = 0.0f;
    u32 anchorsBackwards = 0;
};

void mergeDiag(NetDiag& into, const NetDiag& d);
// The diagnostics as two lines of text ("corr ..." and "poses ..."), for logs. Returns the characters written.
u32 formatDiagCorrections(const NetDiag& d, char* out, u32 size);
u32 formatDiagPoses(const NetDiag& d, u32 seats, u32 self, char* out, u32 size);
u32 formatDiagTicks(const NetDiag& d, char* out, u32 size);

constexpr i64 kConnectTimeoutUs = 6000000;  // M4 stage B: a join nobody answers ends after 6 s ("no answer")

class Session : public PacketSink {
public:
    Session() = default;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session() override { stop(); }

    bool start(const SessionConfig& cfg, sim::World* world, u64 contentHash);
    void stop();
    SessionState state() const { return state_; }
    sim::NetRole role() const { return cfg_.role; }
    bool host() const { return cfg_.role == sim::NetRole::Host; }
    u32 seat() const { return seat_; }
    u32 seats() const { return seats_; }
    u8 seatMask() const { return seatMask_; }
    const char* endReason() const { return endReason_; }

    // ---- the frame ----
    void receive(i64 localUs);
    i64 hostNow() const { return hostNow_; }
    // Host: its song as heard now (the band's, or a SongClock's) and the tempo after the next bar line.
    void setHostSong(const SongAnchor& a);
    // Diagnostics from the driver: a frame (with the ticks it ran), a tick skipped (the song had not moved on), time dropped.
    void noteFrame(i64 localUs, u32 ticks);
    void noteTickSkipped() { ++diag_.ticksSkipped; }
    void noteTimeDropped() { ++diag_.timeDropped; }
    double songAt(i64 hostUs) const { return song_.songAt(hostUs); }
    double tpsAt(i64 hostUs) const { return song_.tpsAt(hostUs); }
    bool songValid() const { return song_.valid(); }
    double runOffset() const { return runOffset_; }
    // The run: scheduled (the world resets on its bar line) and not paused. While paused the run stops on
    // pauseTick() (run ticks); it goes on when the song reaches resumeSong().
    bool running() const { return state_ == SessionState::Running; }
    bool paused() const { return pauseAt_ >= 0.0; }
    double pauseTick() const { return pauseAt_; }
    bool resuming() const { return resumeAt_ >= 0.0; }
    double resumeSong() const { return resumeAt_; }
    void resumed();  // the driver passed resumeSong(): the run goes on with the new offset
    void requestPause(bool on);
    // M4 stage B: the lobby (design/screens.md). A seat is taken once its player is welcomed and ready once its
    // clock has settled; the host starts when every seated player is ready and at least two are seated.
    u8 lobbySeats() const { return lobbySeats_; }
    u8 lobbyReady() const { return lobbyReady_; }
    const char* seatName(u32 s) const { return s < sim::kMaxPlayers ? names_[s] : ""; }
    u16 roomCode() const { return roomCode_; }
    bool canStart() const;
    bool requestStart();  // host: the run starts on a bar line for everyone
    void requestRetry();  // host: after all were down, a new run for everyone on the next bar line
    bool synced() const { return host() || clock_.samples() >= ClockSync::kSamples; }
    // M4 stage B (GAME_DESIGN 10: the shared bed is identical on every device): the bed's intensity for a run bar,
    // decided by the host half a bar ahead and posted by every device to its band for that bar.
    void sendBed(u32 bar, u8 level);          // host
    bool bedLevel(u32 bar, u8& level) const;  // client: the host's level for that bar, when it has come
    // The remote players for a tick ending at host time `hostUs` (drawn ~66 ms behind, TECH_SPEC 8.1).
    void fillRemote(sim::TickInput& in, i64 hostUs);
    void afterTick();
    void send(i64 localUs, const sim::PlayerPose& pose);
    // The device's clock at this moment (microseconds), for the time-sync stamps; without it the frame's time.
    using ClockFn = i64 (*)(void* user);
    void setClock(ClockFn fn, void* user) {
        clockFn_ = fn;
        clockUser_ = user;
    }

    const SessionStats& stats() const { return stats_; }
    void resetScreenError() {
        stats_.screenErrSum2 = 0.0;
        stats_.screenErrN = 0;
        stats_.screenErrMax = 0.0f;
    }
    const NetDiag& diag() const { return diag_; }
    void resetDiag() { diag_ = NetDiag{}; }
    // The host music of the newest correction applied to this enemy (diagnostics; < 0: none yet).
    double lastCorrection(u32 slot, u32 serial) const {
        return slot < sim::kMaxEnemies && corrSerial_[slot] == serial ? corrStamp_[slot] : -1.0;
    }
    const Link& link() const { return link_; }
    Link& linkMut() { return link_; }  // (tests: a peer that dies)
    // Ground truth for tests: the clock and song as this client steers them.
    const ClockSync& clock() const { return clock_; }

private:
    // PacketSink (the sender's seat is in `from_` while a packet is parsed).
    void onHello(const MsgHello& m) override;
    void onWelcome(const MsgWelcome& m) override;
    void onRefuse(u8 reason) override;
    void onBye(u8 reason) override;
    void onPing(i64 t0) override;
    void onPong(i64 t0, i64 t1, i64 t2) override;
    void onRunStart(const MsgRunStart& m) override;
    void onSpawn(u16 run, const sim::NetSpawn& s) override;
    void onDeath(u16 run, const sim::NetDeath& d) override;
    void onClaim(u16 run, const sim::NetClaim& c) override;
    void onBossClaim(u16 run, const sim::NetBossClaim& c) override;
    void onBoss(const MsgBoss& m) override;
    void onBoostDrop(const MsgBoostDrop& m) override;
    void onDirector(const MsgDirector& m) override;
    void onPlayerEvent(const MsgPlayerEvent& m) override;
    void onCensus(const MsgCensus& m) override;
    void onPause(const MsgPause& m) override;
    void onSeats(u8 mask) override;
    void onPose(const MsgPose& m) override;
    void onCorrections(u16 run, double hostMusic) override;
    void onCorrection(const sim::NetEnemyState& s) override;
    void onHeartbeat(const MsgHeartbeat& m) override;
    void onStats(const MsgStats& m) override;
    void onAttention(u16 run, const u32* serials, u32 n) override;
    void onReady(u8 ready) override;
    void onLobby(const MsgLobby& m) override;
    void onBed(const MsgBed& m) override;

    void handleConnect(u32 peer, u32 info);
    void service();
    i64 nowLocal() const;
    void handleDisconnect(u32 peer);
    void maybeStart();
    void startRun(double bar, u64 seed, u16 run);
    void hostPause(bool on);
    void sendPause(const MsgPause& m);
    // Outgoing: reliable messages to every client (host) or to the host (client), streamed out a packet at a time.
    template <typename F>
    void reliable(F&& put);
    void flushReliable();
    void writeClaims();
    void sendTo(u32 peer, u8 channel, const PacketWriter& w, bool rel);
    void relayEvent(const MsgPlayerEvent& m, u32 exceptSeat);
    void sendCorrections(u32 seat, double hostMusic);
    void refillBudgets(i64 localUs);
    u32 attention();
    void updatePriorities(float dt);
    void measure(i64 localUs);
    void collectLocalEvents();
    bool pushOp(const sim::NetOp& op);

    SessionConfig cfg_;
    sim::World* world_ = nullptr;
    Link link_;
    u64 content_ = 0;
    SessionState state_ = SessionState::Idle;
    const char* endReason_ = "";
    u32 seat_ = 0, seats_ = 1;
    u8 seatMask_ = 1;
    i32 peerSeat_[kMaxLinkPeers] = {-1, -1, -1, -1};  // host: ENet peer -> seat
    i32 seatPeer_[sim::kMaxPlayers] = {-1, -1, -1, -1};  // host: seat -> ENet peer
    bool welcomed_[sim::kMaxPlayers] = {};
    i32 from_ = -1;  // seat of the packet being parsed (host)
    u32 fromPeer_ = 0;  // ENet peer of the packet being parsed
    i64 localNow_ = 0, hostNow_ = 0, lastJoinUs_ = 0, startUs_ = 0;
    u8 leaveQueued_ = 0;  // client: seats whose timed leave came from the host (onSeats leaves them to it)
    ClockFn clockFn_ = nullptr;
    void* clockUser_ = nullptr;

    // Time and song.
    ClockSync clock_;
    SongModel song_;
    double runOffset_ = 0.0;
    double pauseAt_ = -1.0, resumeAt_ = -1.0, nextOffset_ = 0.0;
    i64 nextPingUs_ = 0;
    u16 announcedRun_ = 0;

    // Remote players' poses (by seat): the newest few, for interpolation.
    static constexpr u32 kPoseRing = 8;
    struct PoseRing {
        MsgPose s[kPoseRing];
        u32 head = 0, count = 0;
    } poses_[sim::kMaxPlayers];
    MsgPose ownPrev_;
    bool havePrev_ = false;
    // Predicted remote players: the newest pose each estimate came from and the offset that spreads a new pose's jump.
    struct RemoteTrack {
        bool valid = false;
        i64 stamp = 0, lastAt = 0;
        Vec2 pos, vel, off;
    } track_[sim::kMaxPlayers];

    // Outgoing.
    PacketWriter rel_;      // reliable, being filled
    PacketWriter scratch_;  // one-off packets
    i64 nextPoseUs_ = 0, nextBeatUs_ = 0, nextCensusUs_ = 0, nextCorrUs_[sim::kMaxPlayers] = {};
    sim::World::DirectorSync sentDirector_;
    bool directorSent_ = false;
    u32 directorRun_ = 0xFFFFFFFFu;
    // Host: correction priorities per seat and enemy slot.
    float prio_[sim::kMaxPlayers][sim::kMaxEnemies] = {};
    u32 prioSerial_[sim::kMaxPlayers][sim::kMaxEnemies] = {};
    struct Pick {
        float p;
        u32 i;
    };
    Pick picks_[sim::kMaxEnemies];
    sim::NetEnemyState states_[64];
    i64 lastPrioUs_ = 0;
    // Host: each client's downlink budget (bytes, with headers): corrections take what the rest leaves.
    float downBudget_[sim::kMaxPlayers] = {};
    float pending_[kMaxLinkPeers] = {};  // estimates of what was queued to each peer since the last refill
    u64 sentTo_[kMaxLinkPeers] = {};     // the wire's count of bytes to each peer at the last refill
    i64 budgetUs_ = 0;

    // Client: corrections being applied, and the newest applied to each enemy slot (they arrive in any order).
    u16 corrRun_ = 0;
    double corrMusic_ = 0.0;
    u32 corrSerial_[sim::kMaxEnemies] = {};
    double corrStamp_[sim::kMaxEnemies] = {};
    // Client: when each enemy slot last heard from the host (a correction applied) or was first seen on this
    // screen (music ticks), and the attention list being sent.
    u32 seenSerial_[sim::kMaxEnemies] = {};
    double seenStamp_[sim::kMaxEnemies] = {};
    u32 attention_[kMaxAttention] = {};
    i64 nextAttentionUs_ = 0;
    // M4 stage B: the lobby.
    char names_[sim::kMaxPlayers][16] = {};
    u8 lobbySeats_ = 0, lobbyReady_ = 0;
    u16 roomCode_ = 0;
    bool lobbyDirty_ = false, readySent_ = false;
    BeaconSender beacon_;
    struct BedEntry {
        u16 run = 0;
        u32 bar = 0xFFFFFFFFu;
        u8 level = 0;
    } bed_[8];
    u32 bedHead_ = 0;
    // Client: this player's claims since the last send (they leave with the pose, merged and sorted).
    static constexpr u32 kPendingClaims = 1024;
    sim::NetClaim pendClaims_[kPendingClaims];
    u32 pendClaimCount_ = 0;
    u16 pendRun_ = 0;

    // Measurement.
    SessionStats stats_;
    mutable NetDiag diag_;  // (fillRemote, const, counts its reach)
    i64 poseArrivedUs_[sim::kMaxPlayers] = {};
    i64 corrArrivedUs_ = 0, frameUs_ = 0, poseSentUs_ = 0;
    i64 measureFrom_ = 0;
    WireStats lastWire_;
    u32 corrCount_ = 0;
    double errSum_ = 0.0;
    float errMax_ = 0.0f;
    u32 errN_ = 0;
};

// The content the peers must share (the protocol and the embedded design data): a mismatch is refused.
u64 contentHash();

}  // namespace rf::net
