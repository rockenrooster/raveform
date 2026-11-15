#pragma once
// A co-op peer without a screen (M4): its own world, the bot playing its player, and a session (net/session.h).
// raveform_headless --net runs one (a host or a client over UDP) or four (the loopback soak); the game runs
// three as in-process loopback bot clients of the phone's host (bench_play --loopback-bots 3).
//
// Time: frame() takes the true time; the peer's own clock is that time run at (1 + ppm) with an offset (the
// soak gives each client a different clock to steer). The host's song is a SongClock at the director's tempo
// (no band); a client's comes from the host's anchors. The sim ticks at 60 Hz of host time (a client's host
// time is its steered estimate), each tick's musical position the song's at its end, less the run offset.

#include "net/session.h"
#include "sim/bot.h"
#include "sim/player_controller.h"

namespace rf::net {

struct PeerConfig {
    SessionConfig session;
    sim::Bot::Policy policy = sim::Bot::Policy::Play;
    u64 botSeed = 1;
    i64 clockOffsetUs = 0;
    double clockPpm = 0.0;
    u64 contentSalt = 0;  // M4 stage B (tests, --net-content-salt): another content hash, so the host refuses it
};

class NetPeer {
public:
    NetPeer() = default;
    NetPeer(const NetPeer&) = delete;
    NetPeer& operator=(const NetPeer&) = delete;
    ~NetPeer() { stop(); }

    bool start(const PeerConfig& cfg, const sim::SpeciesTable* species);
    void stop();
    void frame(i64 trueUs);
    i64 localUs(i64 trueUs) const;
    // Pause-on-leave (tests, the soak's --away): this peer's app leaves the screen (the driver then stops calling frame() and
    // pumping its socket, as Android stops the app) and comes back. Without a band, a host is back at once.
    void goAway(i64 trueUs) { session_.goAway(localUs(trueUs)); }
    void comeBack(i64 trueUs) {
        session_.comeBack(localUs(trueUs));
        session_.hostReturned();
    }

    sim::World& world() { return *world_; }
    const sim::World& world() const { return *world_; }
    Session& session() { return session_; }
    const Session& session() const { return session_; }
    const sim::PlayerController& controller() const { return controller_; }
    double music() const { return music_; }  // the run's musical position at the newest tick's end
    i64 simHostUs() const { return simHostUs_; }
    // The song this peer is playing at true time `trueUs` (by its own clock).
    double songNow(i64 trueUs) const;

    struct Stats {
        u64 ticks = 0, frames = 0;
        u32 dropped = 0;        // frames that had to drop time (more than three ticks behind)
        u32 maxTicksInFrame = 0;
        u32 restarts = 0;
        u64 heapAllocs = 0;     // in frame(), on this thread, while measuring
    };
    const Stats& stats() const { return stats_; }
    void measureAllocs(bool on) { measureAllocs_ = on; }

private:
    static float tempoOf(void* self, double barTick);
    void tick(double m1, float bpm);

    PeerConfig cfg_;
    sim::World* world_ = nullptr;
    Session session_;
    sim::PlayerController controller_;
    sim::Bot bot_;
    SongClock songClock_;
    bool songStarted_ = false, runStarted_ = false;
    i64 trueStart_ = 0;
    i64 simHostUs_ = 0;
    double music_ = 0.0;
    Stats stats_;
    bool measureAllocs_ = false;
};

}  // namespace rf::net
