#pragma once
// Audio output device with one callback contract for both backends: Oboe on Android (low latency,
// float, stereo, reopened after a disconnect, closed in the background) and an SDL3 audio stream on
// Windows. Stream control happens on the main thread only.

#include <atomic>

#include "core/types.h"

namespace rf {

// Runs on the real-time audio thread: no allocation, locks, logging, file access or JNI.
// Write `frames` interleaved float frames of `channels` channels.
using AudioRenderFn = void (*)(void* user, float* out, u32 frames, u32 channels, u32 sampleRate);

struct AudioStats {
    bool open = false;
    const char* api = "none";
    u32 sampleRate = 0;
    u32 channels = 0;
    u32 framesPerBurst = 0;  // device period (Oboe burst / SDL device buffer)
    u32 bufferFrames = 0;
    u32 bufferCapacity = 0;
    i32 underruns = -1;      // -1 when the backend cannot report them
    u64 callbacks = 0;
    u64 framesRendered = 0;
    float worstCallbackMs = 0.0f;  // since the last resetPeaks()
    bool exclusive = false;
    bool lowLatency = false;
    bool mmap = false;
    u32 reopens = 0;
};

// Callback timing, written by the audio thread only.
struct CallbackMeter {
    std::atomic<u64> callbacks{0};
    std::atomic<u64> frames{0};
    std::atomic<u64> worstNs{0};
    std::atomic<u32> resetRequests{0};
    u32 resetsSeen = 0;  // audio thread only

    void record(u64 ns, u32 frameCount) {
        const u32 req = resetRequests.load(std::memory_order_relaxed);
        u64 worst = worstNs.load(std::memory_order_relaxed);
        if (req != resetsSeen) {
            resetsSeen = req;
            worst = 0;
        }
        if (ns > worst) worst = ns;
        worstNs.store(worst, std::memory_order_relaxed);
        callbacks.store(callbacks.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        frames.store(frames.load(std::memory_order_relaxed) + frameCount, std::memory_order_relaxed);
    }
};

// Flush denormals to zero on the calling thread (call at the top of every callback).
inline void flushDenormals() {
#if defined(__aarch64__)
    unsigned long fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    if (!(fpcr & (1ul << 24))) __asm__ volatile("msr fpcr, %0" : : "r"(fpcr | (1ul << 24)));
#elif defined(__x86_64__) || defined(__i386__)
    unsigned int csr;
    __asm__ volatile("stmxcsr %0" : "=m"(csr));
    if ((csr & 0x8040u) != 0x8040u) {
        csr |= 0x8040u;  // FTZ | DAZ
        __asm__ volatile("ldmxcsr %0" : : "m"(csr));
    }
#endif
}

class AudioDevice {
public:
    bool open(AudioRenderFn fn, void* user);  // opens and starts the stream
    void close();
    bool isOpen() const;
    // Main thread, every frame: reopens after a disconnect, grows the buffer after underruns.
    void update();
    AudioStats stats() const;
    void resetPeaks();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace rf
