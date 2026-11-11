#pragma once
// M0 test tone: a short blip on tap/click, or a held arpeggio for the 60-second audio soak.
// The main thread talks to the audio thread through atomics only.

#include <atomic>

#include "core/types.h"

namespace rf {

class TestTone {
public:
    // Main thread.
    void trigger(float pitchHz);
    void setHold(bool on) { hold_.store(on, std::memory_order_relaxed); }
    bool holding() const { return hold_.load(std::memory_order_relaxed); }
    u32 notesStarted() const { return notes_.load(std::memory_order_relaxed); }

    // AudioRenderFn: `self` is the TestTone.
    static void render(void* self, float* out, u32 frames, u32 channels, u32 sampleRate);

private:
    void startNote(float hz, u32 sampleRate);
    void renderImpl(float* out, u32 frames, u32 channels, u32 sampleRate);

    std::atomic<u32> triggerSeq_{0};
    std::atomic<float> triggerHz_{880.0f};
    std::atomic<bool> hold_{false};
    std::atomic<u32> notes_{0};

    // Audio thread state.
    u32 seenSeq_ = 0;
    float phase_ = 0.0f;
    float freq_ = 880.0f;
    float env_ = 0.0f;
    float attackStep_ = 0.0f;
    u32 attackLeft_ = 0;
    float decay_ = 0.0f;
    u32 holdLeft_ = 0;
    u32 holdStep_ = 0;
};

}  // namespace rf
