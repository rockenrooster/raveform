#include "net/bots.h"

#include <chrono>
#include <cstdio>
#include <new>

namespace rf::net {

using namespace rf::sim;

namespace {
constexpr i64 kFrameUs = 16667;
}

bool LoopbackBots::start(u32 count, u16 port, const SpeciesTable* species, u64 seed) {
    stop();
    if (!count || !species) return true;
    if (count > kMax) count = kMax;
    peers_ = new (std::nothrow) NetPeer[kMax];
    if (!peers_) return false;
    for (u32 k = 0; k < count; ++k) {
        PeerConfig c;
        c.session.role = NetRole::Client;
        c.session.wire = WireKind::Loopback;
        c.session.port = port;
        std::snprintf(c.session.joinIp, sizeof c.session.joinIp, "127.0.0.1");
        std::snprintf(c.session.name, sizeof c.session.name, "BOT%u", k + 1);
        c.session.seed = seed;
        c.botSeed = seed * 31 + k + 1;
        if (!peers_[k].start(c, species)) {
            for (u32 j = 0; j < k; ++j) peers_[j].stop();
            delete[] peers_;
            peers_ = nullptr;
            return false;
        }
    }
    count_ = count;
    resetTimes();
    quit_.store(false);
    thread_ = std::thread([this] { run(); });
    return true;
}

void LoopbackBots::stop() {
    if (thread_.joinable()) {
        quit_.store(true);
        thread_.join();
    }
    if (peers_) {
        for (u32 k = 0; k < count_; ++k) peers_[k].stop();
        delete[] peers_;
        peers_ = nullptr;
    }
    count_ = 0;
}

void LoopbackBots::run() {
    i64 next = wireNowUs();
    while (!quit_.load(std::memory_order_relaxed)) {
        const i64 now = wireNowUs();
        if (now < next) {
            const i64 wait = next - now;
            std::this_thread::sleep_for(std::chrono::microseconds(wait > 1500 ? wait - 1000 : 0));
            continue;
        }
        next += kFrameUs;
        if (now - next > 200000) next = now + kFrameUs;  // a long stall (the app in the background): start again
        if (reset_.exchange(false)) {
            for (std::atomic<u32>& b : hist_) b.store(0, std::memory_order_relaxed);
            maxUs_.store(0, std::memory_order_relaxed);
            frames_.store(0, std::memory_order_relaxed);
        }
        u32 connected = 0, dropped = 0;
        for (u32 k = 0; k < count_; ++k) {
            peers_[k].frame(now);
            connected += peers_[k].session().state() == SessionState::Running || peers_[k].session().state() == SessionState::Lobby ? 1u : 0u;
            dropped += peers_[k].stats().dropped;
        }
        dropped_.store(dropped, std::memory_order_relaxed);
        const i64 took = wireNowUs() - now;
        const u32 us = took < 0 ? 0u : static_cast<u32>(took);
        const u32 bin = us / 100u < kBins ? us / 100u : kBins - 1;
        hist_[bin].fetch_add(1, std::memory_order_relaxed);
        if (us > maxUs_.load(std::memory_order_relaxed)) maxUs_.store(us, std::memory_order_relaxed);
        frames_.fetch_add(1, std::memory_order_relaxed);
        connected_.store(connected, std::memory_order_relaxed);
    }
}

LoopbackBots::Times LoopbackBots::times() const {
    Times t;
    t.frames = frames_.load(std::memory_order_relaxed);
    t.max = static_cast<float>(maxUs_.load(std::memory_order_relaxed)) * 1e-3f;
    t.connected = connected_.load(std::memory_order_relaxed);
    u64 n = 0;
    for (const std::atomic<u32>& b : hist_) n += b.load(std::memory_order_relaxed);
    const auto pct = [&](double q) {
        const u64 want = static_cast<u64>(q * static_cast<double>(n));
        u64 seen = 0;
        for (u32 k = 0; k < kBins; ++k) {
            seen += hist_[k].load(std::memory_order_relaxed);
            if (seen > want) return static_cast<float>(k + 1) * 0.1f;
        }
        return t.max;
    };
    if (n) {
        t.p50 = pct(0.5);
        t.p95 = pct(0.95);
        if (t.p50 > t.max) t.p50 = t.max;  // the bins' upper edges
        if (t.p95 > t.max) t.p95 = t.max;
    }
    t.dropped = dropped_.load(std::memory_order_relaxed);
    return t;
}

void LoopbackBots::resetTimes() { reset_.store(true); }

}  // namespace rf::net
