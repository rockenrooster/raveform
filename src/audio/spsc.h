#pragma once
// Lock-free single-producer / single-consumer ring for the audio thread (TECH_SPEC 6.1): MixState
// commands main -> audio, beat events and clock data audio -> main. Fixed capacity (a power of two),
// storage owned by the ring, wait-free on both sides, no allocation after construction. A full ring
// drops the new item and counts it; nothing ever blocks.

#include <atomic>

#include "core/types.h"

namespace rf::audio {

template <typename T, u32 N>
class SpscRing {
    static_assert(N >= 2 && (N & (N - 1)) == 0, "capacity must be a power of two");

public:
    // Producer side.
    bool push(const T& v) {
        const u32 head = head_.load(std::memory_order_relaxed);
        const u32 tail = tail_.load(std::memory_order_acquire);
        if (head - tail >= N) {
            dropped_.store(dropped_.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
            return false;
        }
        items_[head & (N - 1)] = v;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }
    // Consumer side.
    bool pop(T& out) {
        const u32 tail = tail_.load(std::memory_order_relaxed);
        const u32 head = head_.load(std::memory_order_acquire);
        if (head == tail) return false;
        out = items_[tail & (N - 1)];
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }
    // Consumer side: the oldest item without removing it.
    const T* peek() const {
        const u32 tail = tail_.load(std::memory_order_relaxed);
        const u32 head = head_.load(std::memory_order_acquire);
        return head == tail ? nullptr : &items_[tail & (N - 1)];
    }
    u32 size() const { return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire); }
    static constexpr u32 capacity() { return N; }
    u32 dropped() const { return dropped_.load(std::memory_order_relaxed); }

private:
    alignas(64) std::atomic<u32> head_{0};
    alignas(64) std::atomic<u32> tail_{0};
    std::atomic<u32> dropped_{0};
    T items_[N];
};

// The newest value of a small struct, published by one writer and read by any thread without tearing
// (sequence lock: the writer never waits; a reader retries while a write is in progress).
template <typename T>
class SeqLatest {
public:
    void publish(const T& v) {
        const u32 s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        copyIn(v);
        seq_.store(s + 2, std::memory_order_release);
    }
    // False when nothing was published yet or the writer kept interfering.
    bool read(T& out) const {
        for (int attempt = 0; attempt < 64; ++attempt) {
            const u32 s0 = seq_.load(std::memory_order_acquire);
            if (s0 == 0) return false;
            if (s0 & 1u) continue;
            copyOut(out);
            std::atomic_thread_fence(std::memory_order_acquire);
            if (seq_.load(std::memory_order_relaxed) == s0) return true;
        }
        return false;
    }

private:
    // Word-wise relaxed atomic copies keep the data race formally defined.
    static constexpr u32 kWords = (sizeof(T) + 7) / 8;
    void copyIn(const T& v) {
        u64 tmp[kWords] = {};
        __builtin_memcpy(tmp, &v, sizeof(T));
        for (u32 i = 0; i < kWords; ++i) words_[i].store(tmp[i], std::memory_order_relaxed);
    }
    void copyOut(T& out) const {
        u64 tmp[kWords];
        for (u32 i = 0; i < kWords; ++i) tmp[i] = words_[i].load(std::memory_order_relaxed);
        __builtin_memcpy(&out, tmp, sizeof(T));
    }
    std::atomic<u32> seq_{0};
    std::atomic<u64> words_[kWords];  // value-initialised (C++20)
};

}  // namespace rf::audio
