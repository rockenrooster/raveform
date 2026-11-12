#pragma once
// In-process bot clients (M4): up to three NetPeers on the loopback network, joining a host in the same
// process (the game started with --host --loopback-bots N; the phone's co-op benchmark). They run on a worker
// thread at 60 Hz of real time, each with its own world, bot and session, exactly as raveform_headless's peers.
// Nothing is allocated after start().

#include <atomic>
#include <thread>

#include "net/peer.h"

namespace rf::net {

class LoopbackBots {
public:
    static constexpr u32 kMax = 3;

    LoopbackBots() = default;
    LoopbackBots(const LoopbackBots&) = delete;
    LoopbackBots& operator=(const LoopbackBots&) = delete;
    ~LoopbackBots() { stop(); }

    // Starts `count` clients of the loopback host on `port` (it must be listening already).
    bool start(u32 count, u16 port, const sim::SpeciesTable* species, u64 seed);
    void stop();
    u32 count() const { return count_; }

    // The worker's frame time (all peers, one frame), milliseconds: read from any thread.
    struct Times {
        float p50 = 0.0f, p95 = 0.0f, max = 0.0f;
        u64 frames = 0;
        u32 dropped = 0;  // frames that had to drop time (a peer more than three ticks behind)
        u32 connected = 0;
    };
    Times times() const;
    void resetTimes();

private:
    void run();

    NetPeer* peers_ = nullptr;  // kMax of them
    u32 count_ = 0;
    std::thread thread_;
    std::atomic<bool> quit_{false};
    // The worker's frame times (a histogram in 0.1 ms bins up to 50 ms), written by the worker only.
    static constexpr u32 kBins = 500;
    std::atomic<u32> hist_[kBins] = {};
    std::atomic<u32> maxUs_{0};
    std::atomic<u64> frames_{0};
    std::atomic<u32> connected_{0}, dropped_{0};
    std::atomic<bool> reset_{false};
};

}  // namespace rf::net
