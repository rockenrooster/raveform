#pragma once
// The co-op protocol (M4; TECH_SPEC 8.1, 8.3). An ENet packet carries one or more messages:
// [type u8][payload length u16][payload]. Every packet is at most 1,200 bytes (one datagram, never split).
// Parsing is defensive (TECH_SPEC 8.3): a packet is validated as a whole first (every length, count, id,
// enum and number checked; nothing is allocated from a size field) and only a packet that passes is handed
// on, message by message; a malformed packet is dropped whole.

#include "core/types.h"
#include "sim/world.h"

namespace rf::net {

constexpr u32 kProtocolVersion = 0x52460507u;  // 'R' 'F', M5 part 6 (the bosses of Tracks 8 to 10 and mini-bosses batch 3: new kinds, two new decisions); 0x...506 M5 part 5 (the arena's operations and claims on its bodies); 0x...505 M5 part 4 (the enemy states kLane, kCourt, kEgg; the fights' new decisions) and M4 net's follow-up (motion corrections, 0x...504 on its branch); 0x...503 M5 part 3
constexpr u32 kMaxPacketBytes = 1200;

enum class Msg : u8 {
    // control (channel 2)
    Hello = 1, Welcome, Refuse, Ping, Pong, Bye,
    // the game, reliable (channel 0)
    RunStart = 16, Spawns, Deaths, Claims, BossClaims, Boss, BoostDrop, Director, PlayerEvent, Census, Pause, Seats, Ready, Lobby, Bed,
    // the game, unreliable (channel 1)
    Pose = 48, Players, Corrections, Heartbeat, Stats, Attention, Motions,
};

// A motion correction (M4 net follow-up, docs/PROGRESS_m4_net.md): where an enemy is, its heading, the player it follows and
// its knockback; nothing of its musical state, its health or its random stream. A third of a full correction's bytes, for
// the bodies whose motion is all that diverges (a moving Shoal: boids drift apart fastest); the receiver keeps its own
// copy's other fields. Every few corrections of an enemy the host still sends a full one.
struct NetMotion {
    u32 serial = 0;
    float x = 0, y = 0, heading = 0, kx = 0, ky = 0;
    u8 target = 0xFF;
    bool engaged = false, frozen = false;
};

enum RefuseReason : u8 { kRefuseVersion = 1, kRefuseContent = 2, kRefuseFull = 3, kRefuseRunning = 4 };

struct MsgHello {
    u32 version = kProtocolVersion;
    u64 content = 0;
    char name[16] = {};
};
struct MsgWelcome {
    u32 version = kProtocolVersion;
    u64 content = 0;
    u8 seat = 0, seats = 1;
};
struct MsgRunStart {
    u16 within = 0;      // the run it is part of (a later run's start waits for it)
    u16 run = 0;         // the new run's id
    double bar = 0.0;    // run ticks: the bar line it begins on
    u64 seed = 0;
    u8 players = 1;
    double runOffset = 0.0;  // song ticks minus run ticks (whole bars)
    u8 mask = 0xF;           // the seats in the run (a seat left open in the lobby is not)
};
// The host's lobby, to every client on each change (M4 stage B; design/screens.md "Co-op lobby").
struct MsgLobby {
    u8 seats = 0;     // taken seats (bit per seat)
    u8 ready = 0;     // of them, ready (joined and its clock settled)
    u16 code = 0;     // the room code's 16 bits
    u8 state = 0;     // 0 lobby, 1 a run is going
    char names[4][16] = {};
};
// The bed's intensity for a bar (M4 stage B, GAME_DESIGN 10: the shared bed is identical on every device): the host
// decides it half a bar ahead; every device posts it to its band for that bar.
struct MsgBed {
    u16 run = 0;
    u32 bar = 0;      // the run's bar
    u8 level = 0;     // intensity x 255
};
struct MsgBoss {
    u16 run = 0;
    u8 kind = 0, pair = 0, killer = 0xFF;  // 0 enters, 1 a part dies (or a step its health decided), 2 the fight ends
    u8 boss = 0;                           // M5: which fight (sim::BossKind)
    float x = 0.0f, y = 0.0f;
    float x2 = 0.0f, y2 = 0.0f;            // M5: the Pen's centre
    double tick = 0.0;
    u32 changeBar = 0;
};
struct MsgBoostDrop {
    u16 run = 0;
    float x = 0.0f, y = 0.0f;
    u8 type = 0;
};
struct MsgDirector {
    u16 run = 0;
    sim::World::DirectorSync d;
};
struct MsgPlayerEvent {
    u16 run = 0;
    u8 player = 0, kind = 0, b = 0;
    u8 opts[sim::kMaxOrbs] = {};  // M5 part 3: Lucky Break's and a Fusion's orbs too
    float x = 0.0f, y = 0.0f, x2 = 0.0f, y2 = 0.0f;
    double tick = 0.0;
};
struct MsgCensus {
    u16 run = 0;
    u32 count = 0;
    u64 digest = 0;
};
enum PauseKind : u8 { kPauseAsk = 0, kResumeAsk = 1, kPauseAt = 2, kResumeAt = 3 };
struct MsgPause {
    u8 kind = kPauseAsk;
    double runTick = 0.0;    // kPauseAt: the run freezes on this tick (a beat)
    double songTick = 0.0;   // kResumeAt: the run goes on when the song reaches this
    double runOffset = 0.0;  // kResumeAt: song ticks minus run ticks from then on
};
struct PoseState {
    float x = 0, y = 0, vx = 0, vy = 0, aim = 0, heading = 0;
    u8 fire = 0, flags = 0;  // flags: 1 dashing, 2 alive, 4 down
};
struct MsgPose {
    u8 player = 0;
    i64 stampUs = 0;         // host time of `cur`
    PoseState cur;
    i16 prevAgeMs = -1;      // `prev` is this much older (< 0: none)
    PoseState prev;
};
// A player's numbers (its own device keeps them), 4 Hz.
struct MsgStats {
    u8 player = 0;
    u8 cells = 0, novas = 0;
    u32 chain = 1, kills = 0;
    u64 score = 0;
};
struct MsgHeartbeat {
    u16 run = 0;
    i64 hostUs = 0;
    double song = 0.0;       // song ticks heard at hostUs
    float tps = 192.0f;      // song ticks per second now ...
    double nextBar = 0.0;    // ... until this song tick (the next bar line) ...
    float tpsNext = 192.0f;  // ... and from there
    double runOffset = 0.0;
    u8 state = 0;            // 0 lobby, 1 running, 2 paused
    u8 bossOn = 0;
    u8 bossParts = 0;  // M5: the fight's parts with health (Kaleidos 3, Longwave 24, the Choir 8, the Carom King's Princes 3)
    float bossPart[sim::kBossParts] = {}, bossCore = 0.0f, bossX = 0.0f, bossY = 0.0f;
};

// The receiving side: one call per message (and per item of a list).
class PacketSink {
public:
    virtual ~PacketSink() = default;
    virtual void onHello(const MsgHello&) {}
    virtual void onWelcome(const MsgWelcome&) {}
    virtual void onRefuse(u8) {}
    virtual void onBye(u8) {}
    virtual void onPing(i64) {}
    virtual void onPong(i64, i64, i64) {}
    virtual void onRunStart(const MsgRunStart&) {}
    virtual void onSpawn(u16, const sim::NetSpawn&) {}
    virtual void onDeath(u16, const sim::NetDeath&) {}
    virtual void onClaim(u16, const sim::NetClaim&) {}
    virtual void onBossClaim(u16, const sim::NetBossClaim&) {}
    virtual void onBoss(const MsgBoss&) {}
    virtual void onBoostDrop(const MsgBoostDrop&) {}
    virtual void onDirector(const MsgDirector&) {}
    virtual void onPlayerEvent(const MsgPlayerEvent&) {}
    virtual void onCensus(const MsgCensus&) {}
    virtual void onPause(const MsgPause&) {}
    virtual void onSeats(u8) {}
    virtual void onReady(u8) {}
    virtual void onLobby(const MsgLobby&) {}
    virtual void onBed(const MsgBed&) {}
    virtual void onPose(const MsgPose&) {}
    virtual void onCorrections(u16, double) {}  // then onCorrection per enemy
    virtual void onCorrection(const sim::NetEnemyState&) {}
    virtual void onMotions(u16, double) {}  // then onMotion per enemy
    virtual void onMotion(const NetMotion&) {}
    virtual void onHeartbeat(const MsgHeartbeat&) {}
    virtual void onStats(const MsgStats&) {}
    // Client -> host: enemies on the client's screen that no correction has reached for a while.
    virtual void onAttention(u16, const u32*, u32) {}
};

// Validates the whole packet, then (only if it is well formed) hands every message to the sink. Returns
// false for a malformed packet (nothing handed on).
bool parsePacket(const u8* data, u32 size, PacketSink& sink);

// Builds packets of at most kMaxPacketBytes. Each put*() returns false when the message does not fit in what
// is left (send the packet, reset, put it again). List messages take as many items as fit and say how many.
class PacketWriter {
public:
    void reset() { size_ = 0; }
    const u8* data() const { return buf_; }
    u32 size() const { return size_; }
    bool empty() const { return size_ == 0; }
    u32 room() const { return kMaxPacketBytes - size_; }

    bool putHello(const MsgHello& m);
    bool putWelcome(const MsgWelcome& m);
    bool putRefuse(u8 reason);
    bool putBye(u8 reason);
    bool putPing(i64 t0);
    bool putPong(i64 t0, i64 t1, i64 t2);
    bool putRunStart(const MsgRunStart& m);
    u32 putSpawns(u16 run, double base, const sim::NetSpawn* items, u32 n);
    u32 putDeaths(u16 run, const sim::NetDeath* items, u32 n);
    u32 putClaims(u16 run, const sim::NetClaim* items, u32 n);
    u32 putBossClaims(u16 run, const sim::NetBossClaim* items, u32 n);
    bool putBoss(const MsgBoss& m);
    bool putBoostDrop(const MsgBoostDrop& m);
    bool putDirector(const MsgDirector& m);
    bool putPlayerEvent(const MsgPlayerEvent& m);
    bool putCensus(const MsgCensus& m);
    bool putPause(const MsgPause& m);
    bool putSeats(u8 mask);
    bool putReady(u8 ready);
    bool putLobby(const MsgLobby& m);
    bool putBed(const MsgBed& m);
    bool putPose(const MsgPose& m);  // as Msg::Pose (client -> host) ...
    // ... or the host's relay of every player's pose in one message.
    bool putPlayers(const MsgPose* poses, u32 n);
    u32 putCorrections(u16 run, double hostMusic, const sim::NetEnemyState* items, u32 n);
    // Motion corrections, in increasing serial order (each serial a varint step from the one before): 8 to 12 bytes each.
    u32 putMotions(u16 run, double hostMusic, const NetMotion* items, u32 n);
    bool putHeartbeat(const MsgHeartbeat& m);
    bool putStats(const MsgStats* s, u32 n);
    bool putAttention(u16 run, const u32* serials, u32 n);

    // Raw access for tests (the fuzzer builds broken packets).
    u8* raw() { return buf_; }
    void setSize(u32 n) { size_ = n < kMaxPacketBytes ? n : kMaxPacketBytes; }

private:
    bool begin(Msg type, u32 payload);
    void u8v(u8 v) { buf_[size_++] = v; }
    void u16v(u16 v);
    void u32v(u32 v);
    void u64v(u64 v);
    void f32v(float v);
    void f64v(double v);
    void pose(const PoseState& s);
    void poseMsg(const MsgPose& m);
    u8 buf_[kMaxPacketBytes] = {};
    u32 size_ = 0;
};

// Item sizes on the wire (bytes). Play test 2: a spawn is 7 to 17 bytes (kSpawnBytes the most: only what differs from
// the item before is sent), a death 1 to 6 (a varint).
constexpr u32 kSpawnBytes = 17, kDeathBytes = 6, kBossClaimBytes = 6, kCorrectionBytes = 31, kPoseBytes = 39, kStatsBytes = 19;
constexpr u32 kMotionBytesMax = 13;  // a motion correction: serial step 1-5, position 4, heading 2, a byte of bits, knockback 2
constexpr u32 kCorrectionsHeader = 3 + 11;  // a corrections or motions message: type and length, run, host music, count
// Claims are variable (1-7 bytes an item, see putClaims); one claim's damage is at most this.
constexpr float kMaxClaimDamage = 16383.0f / 64.0f;
constexpr u32 kMaxAttention = 8;  // serials in one attention message

}  // namespace rf::net
