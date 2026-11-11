#include "audio/test_tone.h"

#include <cmath>

#include "core/math.h"

namespace rf {

namespace {
constexpr float kGain = 0.25f;           // about -12 dBFS peak
constexpr float kAttackSec = 0.004f;
constexpr float kDecaySec = 0.12f;       // exponential time constant
constexpr float kHoldNoteSec = 0.25f;
constexpr float kArpeggio[4] = {880.0f, 1108.73f, 1318.51f, 1760.0f};  // A major, A5..A6
}  // namespace

void TestTone::trigger(float pitchHz) {
    triggerHz_.store(pitchHz, std::memory_order_relaxed);
    triggerSeq_.fetch_add(1, std::memory_order_release);
}

void TestTone::render(void* self, float* out, u32 frames, u32 channels, u32 sampleRate) {
    static_cast<TestTone*>(self)->renderImpl(out, frames, channels, sampleRate);
}

void TestTone::startNote(float hz, u32 sampleRate) {
    freq_ = hz;
    attackLeft_ = static_cast<u32>(kAttackSec * sampleRate);
    attackStep_ = attackLeft_ ? (1.0f - env_) / static_cast<float>(attackLeft_) : 0.0f;
    decay_ = std::exp(-1.0f / (kDecaySec * static_cast<float>(sampleRate)));
    notes_.store(notes_.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
}

void TestTone::renderImpl(float* out, u32 frames, u32 channels, u32 sampleRate) {
    const u32 seq = triggerSeq_.load(std::memory_order_acquire);
    if (seq != seenSeq_) {
        seenSeq_ = seq;
        startNote(triggerHz_.load(std::memory_order_relaxed), sampleRate);
    }
    const bool hold = hold_.load(std::memory_order_relaxed);
    const float invRate = 1.0f / static_cast<float>(sampleRate);
    for (u32 i = 0; i < frames; ++i) {
        if (hold) {
            if (holdLeft_ == 0) {
                startNote(kArpeggio[holdStep_], sampleRate);
                holdStep_ = (holdStep_ + 1) % 4;
                holdLeft_ = static_cast<u32>(kHoldNoteSec * sampleRate);
            }
            --holdLeft_;
        }
        if (attackLeft_) {
            env_ += attackStep_;
            --attackLeft_;
        } else {
            env_ *= decay_;
            if (env_ < 1e-6f) env_ = 0.0f;
        }
        // Fundamental plus an octave so phone speakers reproduce it.
        const float s = std::sin(kTau * phase_) + 0.3f * std::sin(2.0f * kTau * phase_);
        phase_ += freq_ * invRate;
        if (phase_ >= 1.0f) phase_ -= 1.0f;
        const float v = s * env_ * kGain;
        for (u32 c = 0; c < channels; ++c) out[i * channels + c] = v;
    }
}

}  // namespace rf
