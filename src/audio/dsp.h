#pragma once
// DSP building blocks of the band, ported from the reference prototype design/tools/musicproto.py:
// PolyBLEP oscillators, the zero-delay state-variable low-pass (Cytomic / Simper form), Butterworth
// biquads (the causal stand-ins for the prototype's zero-phase FFT filters: same magnitude response),
// first-order filters and a small deterministic random generator. Real-time safe: no allocation.

#include <cmath>

#include "core/types.h"

namespace rf::audio {

constexpr double kPiD = 3.14159265358979323846;
constexpr float kPiF = 3.14159265358979323846f;

inline double midiHz(double m) { return 440.0 * std::exp2((m - 69.0) / 12.0); }

// musicproto.polyblep: t is the phase in [0, 1), dt the phase increment per sample.
inline float polyBlep(float t, float dt) {
    if (t < dt) {
        const float x = t / dt;
        return x + x - x * x - 1.0f;
    }
    if (t > 1.0f - dt) {
        const float x = (t - 1.0f) / dt;
        return x * x + x + x + 1.0f;
    }
    return 0.0f;
}

// musicproto.saw / square: the value at the current phase, then one sample on.
struct SawOsc {
    float phase = 0.0f;
    float dt = 0.0f;
    float next() {
        const float t = phase;
        const float y = 2.0f * t - 1.0f - polyBlep(t, dt);
        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;
        return y;
    }
};

struct SquareOsc {
    float phase = 0.0f;
    float dt = 0.0f;
    float next() {
        const float t = phase;
        float t2 = t + 0.5f;
        if (t2 >= 1.0f) t2 -= 1.0f;
        const float y = (t < 0.5f ? 1.0f : -1.0f) + polyBlep(t, dt) - polyBlep(t2, dt);
        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;
        return y;
    }
};

// musicproto.svf_lowpass: g = tan(pi fc / sr) with fc clipped to [20, 0.45 sr], k = 1 / q.
inline float svfG(float fcHz, float sampleRate) {
    const float hi = 0.45f * sampleRate;
    const float fc = fcHz < 20.0f ? 20.0f : (fcHz > hi ? hi : fcHz);
    return std::tan(kPiF * fc / sampleRate);
}

struct SvfCoefs {
    float a1, a2, a3;
};
inline SvfCoefs svfCoefs(float g, float k) {
    const float a1 = 1.0f / (1.0f + g * (g + k));
    return {a1, g * a1, g * g * a1};
}

struct SvfLowpass {
    float ic1 = 0.0f;
    float ic2 = 0.0f;
    float tick(float x, float g, float k) {
        const SvfCoefs c = svfCoefs(g, k);
        return tick(x, c.a1, c.a2, c.a3);
    }
    // With precomputed coefficients (the voices interpolate them between control-rate knots).
    float tick(float x, float a1, float a2, float a3) {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }
};

// Transposed direct form II biquad. Butterworth (Q = 1/sqrt 2) low- and high-pass by the bilinear
// transform with the cutoff pre-warped: |H| equals the prototype's 2nd-order lp() / hp() magnitude.
struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    float tick(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0.0f; }
    void setLowpass(double fc, double sampleRate, double q = 0.70710678118654752) {
        const double w = 2.0 * kPiD * fc / sampleRate, c = std::cos(w), alpha = std::sin(w) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = static_cast<float>((1.0 - c) * 0.5 / a0);
        b1 = static_cast<float>((1.0 - c) / a0);
        b2 = b0;
        a1 = static_cast<float>(-2.0 * c / a0);
        a2 = static_cast<float>((1.0 - alpha) / a0);
    }
    void setHighpass(double fc, double sampleRate, double q = 0.70710678118654752) {
        const double w = 2.0 * kPiD * fc / sampleRate, c = std::cos(w), alpha = std::sin(w) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = static_cast<float>((1.0 + c) * 0.5 / a0);
        b1 = static_cast<float>(-(1.0 + c) / a0);
        b2 = b0;
        a1 = static_cast<float>(-2.0 * c / a0);
        a2 = static_cast<float>((1.0 - alpha) / a0);
    }
};

// First-order (Butterworth) low- or high-pass, bilinear with pre-warping: the prototype's order-1
// lp() / hp() magnitude (the reverb's band limits).
struct OnePole {
    float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f;
    float x1 = 0.0f, y1 = 0.0f;
    float tick(float x) {
        const float y = b0 * x + b1 * x1 - a1 * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void reset() { x1 = y1 = 0.0f; }
    void setLowpass(double fc, double sampleRate) {
        const double k = std::tan(kPiD * fc / sampleRate);
        b0 = b1 = static_cast<float>(k / (1.0 + k));
        a1 = static_cast<float>((k - 1.0) / (k + 1.0));
    }
    void setHighpass(double fc, double sampleRate) {
        const double k = std::tan(kPiD * fc / sampleRate);
        b0 = static_cast<float>(1.0 / (1.0 + k));
        b1 = -b0;
        a1 = static_cast<float>((k - 1.0) / (k + 1.0));
    }
};

// Deterministic generator (SplitMix64 seeding a xorshift64*): the same seed gives the same sequence on
// every device (co-op beds must agree).
class Random {
public:
    explicit Random(u64 seed = 1) { reseed(seed); }
    void reseed(u64 seed) {
        u64 z = seed + 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        state_ = (z ^ (z >> 31)) | 1ull;
    }
    u64 nextU64() {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        return state_ * 0x2545F4914F6CDD1Dull;
    }
    // [0, 1)
    double uniform() { return static_cast<double>(nextU64() >> 11) * (1.0 / 9007199254740992.0); }
    u32 below(u32 n) { return n ? static_cast<u32>(uniform() * n) : 0u; }
    // Standard normal (Box-Muller).
    double gaussian() {
        if (haveSpare_) {
            haveSpare_ = false;
            return spare_;
        }
        double u = uniform();
        while (u <= 1e-300) u = uniform();
        const double v = uniform();
        const double r = std::sqrt(-2.0 * std::log(u));
        spare_ = r * std::sin(2.0 * kPiD * v);
        haveSpare_ = true;
        return r * std::cos(2.0 * kPiD * v);
    }

private:
    u64 state_ = 1;
    double spare_ = 0.0;
    bool haveSpare_ = false;
};

// musicproto.Mixer.place: a mono source at `pan` (-1..1) gets cos / sin of (pan + 1) pi / 4 times
// 1.4142 (unity at the centre).
inline void panGains(float pan, float gain, float& left, float& right) {
    const double a = (static_cast<double>(pan) + 1.0) * kPiD / 4.0;
    left = static_cast<float>(gain * std::cos(a) * 1.4142);
    right = static_cast<float>(gain * std::sin(a) * 1.4142);
}

// Per-sample multiplier of an exponential decay with time constant `tau` seconds.
inline double decayPerSample(double tauSeconds, double sampleRate) {
    return tauSeconds > 0.0 ? std::exp(-1.0 / (tauSeconds * sampleRate)) : 0.0;
}

}  // namespace rf::audio
