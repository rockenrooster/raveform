#include "audio/bake.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "audio/dsp.h"
#include "audio/fft.h"
#include "core/log.h"

namespace rf::audio {

namespace {

constexpr double kTau = 2.0 * kPiD;

float* alloc(u32 n) { return static_cast<float*>(std::calloc(n ? n : 1, sizeof(float))); }

void normPeak(float* x, u32 n, float peak = 1.0f) {
    float m = 0.0f;
    for (u32 i = 0; i < n; ++i) m = std::fabs(x[i]) > m ? std::fabs(x[i]) : m;
    if (m > 0.0f) {
        const float g = peak / m;
        for (u32 i = 0; i < n; ++i) x[i] *= g;
    }
}

// musicproto.fade_out: the last int(seconds * SR) samples times linspace(1, 0, n).
void fadeOut(float* x, u32 n, double seconds, double sr) {
    u32 m = framesFor(seconds, sr);
    if (m > n) m = n;
    for (u32 i = 0; i < m; ++i) x[n - m + i] *= m > 1 ? 1.0f - static_cast<float>(i) / static_cast<float>(m - 1) : 1.0f;
}

// sin(2 pi cumsum(f) / SR): the phase includes the current sample's frequency.
struct PhaseSine {
    double phase = 0.0;
    double next(double hz, double sr) {
        phase += hz / sr;
        phase -= std::floor(phase);
        return std::sin(kTau * phase);
    }
};

// sfxproto.sweep: a sine whose frequency glides exponentially from f0 to f1.
void addSweep(float* out, u32 n, double f0, double f1, double tau, double sr, float gain = 1.0f) {
    PhaseSine s;
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        out[i] += gain * static_cast<float>(s.next(f1 + (f0 - f1) * std::exp(-t / tau), sr));
    }
}

bool noise(float* out, u32 n, u64 seed, const SpectralShape& shape, double sr) {
    Random rng(seed);
    return filteredNoise(out, n, rng, shape, sr);
}

// sfxproto.lowpass_sweep: the prototype's SVF with an exponential cutoff glide (per-sample tan).
void lowpassSweep(float* x, u32 n, double f0, double f1, double tau, double q, double sr) {
    SvfLowpass f;
    const float k = static_cast<float>(1.0 / q);
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        x[i] = f.tick(x[i], svfG(static_cast<float>(f1 + (f0 - f1) * std::exp(-t / tau)), static_cast<float>(sr)), k);
    }
}

void addSaw(float* out, u32 n, double hz, double sr, float gain) {
    SawOsc o;
    o.dt = static_cast<float>(hz / sr);
    for (u32 i = 0; i < n; ++i) out[i] += gain * o.next();
}

// ---------------------------------------------------------------- musicproto one-shots

float* makeKick(const SoundParams& p, double sr, u32& n, u64 clickSeed = kKickClickSeed) {
    const KickPatch& k = p.kick;
    n = framesFor(0.42, sr);
    float* x = alloc(n);
    float* click = alloc(n);
    if (!x || !click || !noise(click, n, clickSeed, SpectralShape::bandpass(1800.0, 6000.0), sr)) {
        std::free(x);
        std::free(click);
        return nullptr;
    }
    PhaseSine s;
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        const double f = p.kickHz + k.sweepHz * std::exp(-t / k.sweepTau) + k.settleHz * std::exp(-t / k.settleTau);
        const double att = t / k.attack < 1.0 ? t / k.attack : 1.0;
        const double body = s.next(f, sr) * std::exp(-t / k.decay) * att;
        const double c = click[i] * std::exp(-t / k.clickDecay) * k.click;
        x[i] = static_cast<float>(std::tanh(k.drive * (body + c)));
    }
    std::free(click);
    normPeak(x, n);
    fadeOut(x, n, 0.008, sr);
    return x;
}

float* makeClap(double sr, u32& n) {
    n = framesFor(0.34, sr);
    float* x = alloc(n);
    if (!x || !noise(x, n, 2, SpectralShape::bandpass(750.0, 3600.0), sr)) {
        std::free(x);
        return nullptr;
    }
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        double env = 0.0;
        for (double d : {0.0, 0.011, 0.021})
            if (t >= d) env += std::exp(-(t - d) / 0.006) * 0.8;
        if (t >= 0.030) env += std::exp(-(t - 0.030) / 0.085);
        x[i] = static_cast<float>(x[i] * env);
    }
    normPeak(x, n);
    fadeOut(x, n, 0.008, sr);
    return x;
}

float* makeSnare(double sr, u32& n) {
    n = framesFor(0.24, sr);
    float* x = alloc(n);
    if (!x || !noise(x, n, 3, SpectralShape::bandpass(1800.0, 8500.0), sr)) {
        std::free(x);
        return nullptr;
    }
    PhaseSine s;
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        const double tone = s.next(185.0 + 140.0 * std::exp(-t / 0.012), sr) * std::exp(-t / 0.07);
        const double nz = x[i] * std::exp(-t / 0.09);
        x[i] = static_cast<float>(std::tanh(1.4 * (0.6 * tone + 0.9 * nz)));
    }
    normPeak(x, n);
    fadeOut(x, n, 0.008, sr);
    return x;
}

float* makeHat(double decay, u64 seed, double sr, u32& n) {
    n = framesFor(decay * 6.0 + 0.02, sr);
    float* metal = alloc(n);
    float* nz = alloc(n);
    if (!metal || !nz || !noise(nz, n, seed, SpectralShape::highpass(7000.0, 3), sr)) {
        std::free(metal);
        std::free(nz);
        return nullptr;
    }
    static const double kMetal[] = {3136.0, 4410.0, 5274.0, 6211.0, 7102.0, 8372.0};
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        double m = 0.0;
        for (double f : kMetal) m += std::fmod(f * t, 1.0) < 0.5 ? 1.0 : -1.0;
        metal[i] = static_cast<float>(m);
    }
    if (!spectralFilter(metal, n, SpectralShape::highpass(6500.0, 3), sr)) {
        std::free(metal);
        std::free(nz);
        return nullptr;
    }
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        metal[i] = static_cast<float>((0.6 * metal[i] / 3.0 + 0.4 * nz[i]) * std::exp(-t / decay));
    }
    std::free(nz);
    normPeak(metal, n);
    fadeOut(metal, n, 0.004, sr);
    return metal;
}

float* makeShaker(double sr, u32& n) {
    n = framesFor(0.09, sr);
    float* x = alloc(n);
    if (!x || !noise(x, n, 9, SpectralShape::bandpass(4500.0, 11000.0), sr)) {
        std::free(x);
        return nullptr;
    }
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        x[i] = static_cast<float>(x[i] * (t / 0.006 < 1.0 ? t / 0.006 : 1.0) * std::exp(-t / 0.022));
    }
    normPeak(x, n);
    fadeOut(x, n, 0.004, sr);
    return x;
}

float* makePulse(double sr, u32& n) {
    n = framesFor(0.36, sr);
    float* x = alloc(n);
    if (!x || !noise(x, n, 5, SpectralShape::bandpass(1500.0, 5000.0), sr)) {
        std::free(x);
        return nullptr;
    }
    PhaseSine s;
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        double v = s.next(170.0 + 650.0 * std::exp(-t / 0.045), sr) * std::exp(-t / 0.13);
        v += 0.25 * x[i] * std::exp(-t / 0.03);
        x[i] = static_cast<float>(std::tanh(1.5 * v));
    }
    normPeak(x, n);
    fadeOut(x, n, 0.008, sr);
    return x;
}

float* makeImpact(double sr, u32& n) {
    n = framesFor(2.4, sr);
    float* boom = alloc(n);
    float* crash = alloc(n);
    if (!boom || !crash || !noise(boom, n, 6, SpectralShape::bandpass(140.0, 650.0), sr) ||
        !noise(crash, n, 7, SpectralShape::bandpass(3500.0, 11000.0), sr)) {
        std::free(boom);
        std::free(crash);
        return nullptr;
    }
    PhaseSine s;
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        const double sub = s.next(38.0 + 82.0 * std::exp(-t / 0.25), sr) * std::exp(-t / 0.7);
        const double b = boom[i] * std::exp(-t / 0.28) * 0.55;
        const double c = crash[i] * std::exp(-t / 0.65) * 0.30;
        boom[i] = static_cast<float>(std::tanh(1.3 * (sub + b)) + c);
    }
    std::free(crash);
    normPeak(boom, n);
    fadeOut(boom, n, 0.05, sr);
    return boom;
}

float* makeSwell(double seconds, double sr, u32& n) {
    n = framesFor(seconds, sr);
    float* x = alloc(n);
    if (!x || !noise(x, n, 8, SpectralShape::highpass(2500.0), sr)) {
        std::free(x);
        return nullptr;
    }
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        x[i] = static_cast<float>(x[i] * std::exp((t - seconds) / (seconds * 0.28)));
    }
    normPeak(x, n);
    fadeOut(x, n, 0.01, sr);
    return x;
}

// musicproto.make_riser: twelve noise bands from 300 Hz to 9 kHz (bp(fc 0.8, fc 1.25) each, independent
// noise per band and channel) fading in one after another over the Build, times u^1.6. A sum of
// independent band noises with slowly moving gains is synthesised here frame by frame in the frequency
// domain: each 4096-sample frame is a random spectrum whose per-bin variance is the gain-weighted sum of
// the bands' exact |H(f)|^2 (the prototype's magnitude, no bilinear warping), windowed with a sqrt-Hann
// and overlap-added at 50%, which keeps the noise stationary. Same statistics, a fraction of the cost.
float* makeRiser(double seconds, double sr, u32& n) {
    n = framesFor(seconds, sr);
    constexpr u32 kN = 4096, kHop = kN / 2, kBins = kN / 2;
    float* out = alloc(n * 2);
    auto* spec = static_cast<Complex*>(std::malloc(sizeof(Complex) * kN));
    auto* shape = static_cast<float*>(std::malloc(sizeof(float) * 12 * kBins));  // |H_k(f)|^2
    float window[kN];
    if (!out || !spec || !shape) {
        std::free(out);
        std::free(spec);
        std::free(shape);
        return nullptr;
    }
    for (u32 i = 0; i < kN; ++i) window[i] = static_cast<float>(std::sin(kPiD * (i + 0.5) / kN));  // sqrt of a periodic Hann
    for (u32 k = 0; k < 12; ++k) {
        const double fc = 300.0 * std::pow(9000.0 / 300.0, k / 11.0);
        const SpectralShape bp = SpectralShape::bandpass(fc * 0.8, fc * 1.25);
        for (u32 b = 0; b < kBins; ++b) {
            const double g = bp.gain(b * sr / kN);
            shape[k * kBins + b] = static_cast<float>(g * g);
        }
    }
    for (u32 ch = 0; ch < 2; ++ch) {
        Random rng(100 + ch);
        for (i64 start = -static_cast<i64>(kHop); start < static_cast<i64>(n); start += kHop) {
            const double centre = (static_cast<double>(start) + kN / 2) / sr / seconds;  // u at the frame centre
            double weight[12];
            for (u32 k = 0; k < 12; ++k) {
                double gate = (centre - k / 13.0) * 4.0;
                gate = gate < 0.0 ? 0.0 : (gate > 1.0 ? 1.0 : gate);
                const double a = gate * gate * (3.0 - 2.0 * gate);  // the band's amplitude weight
                weight[k] = a * a;
            }
            spec[0] = {0.0, 0.0};
            spec[kBins] = {0.0, 0.0};
            const double scale = std::sqrt(kN / 2.0);
            for (u32 b = 1; b < kBins; ++b) {
                double var = 0.0;
                for (u32 k = 0; k < 12; ++k) var += weight[k] * shape[k * kBins + b];
                const double amp = std::sqrt(var) * scale;
                spec[b] = {rng.gaussian() * amp, rng.gaussian() * amp};
                spec[kN - b] = {spec[b].re, -spec[b].im};
            }
            fftPow2(spec, kN, true);
            for (u32 i = 0; i < kN; ++i) {
                const i64 t = start + static_cast<i64>(i);
                if (t < 0 || t >= static_cast<i64>(n)) continue;
                // Windowed twice (analysis-free synthesis): sqrt-Hann^2 sums to one at 50% overlap.
                out[static_cast<size_t>(t) * 2 + ch] += static_cast<float>(spec[i].re / kN) * window[i];
            }
        }
        for (u32 i = 0; i < n; ++i) out[i * 2 + ch] *= static_cast<float>(std::pow((i / sr) / seconds, 1.6));
    }
    std::free(spec);
    std::free(shape);
    normPeak(out, n * 2);
    return out;
}
// ---------------------------------------------------------------- sfxproto effects

// sfxproto.bell (FM).
void addBell(float* out, u32 cap, u32 at, double hz, double dur, double ratio, double index, double sr, float gain) {
    const u32 n = framesFor(dur, sr);
    const u32 fade = framesFor(0.008, sr) < n ? framesFor(0.008, sr) : n;
    for (u32 i = 0; i < n && at + i < cap; ++i) {
        const double t = i / sr;
        const double mod = std::sin(kTau * hz * ratio * t) * index * std::exp(-t / 0.15);
        double x = std::sin(kTau * hz * t + mod) * std::exp(-t / (dur / 3.5)) * (t / 0.002 < 1.0 ? t / 0.002 : 1.0);
        if (i >= n - fade) x *= fade > 1 ? 1.0 - static_cast<double>(i - (n - fade)) / (fade - 1) : 1.0;
        out[at + i] += gain * static_cast<float>(x);
    }
}

float* sfxKillSmall(double hz, double sr, u32& n) {
    n = framesFor(0.13, sr);
    float* x = alloc(n);
    float* click = alloc(n);
    if (!x || !click || !noise(click, n, static_cast<u64>(hz), SpectralShape::bandpass(2000.0, 7000.0), sr)) {
        std::free(x);
        std::free(click);
        return nullptr;
    }
    addSweep(x, n, hz * 1.6, hz, 0.012, sr);
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        x[i] = static_cast<float>(std::tanh(1.3 * (x[i] * std::exp(-t / 0.045) + click[i] * std::exp(-t / 0.010) * 0.5)));
    }
    std::free(click);
    normPeak(x, n);
    fadeOut(x, n, 0.008, sr);
    return x;
}

float* sfxKillMedium(double hz, double sr, u32& n) {
    n = framesFor(0.30, sr);
    float* body = alloc(n);
    float* nz = alloc(n);
    if (!body || !nz || !noise(nz, n, static_cast<u64>(hz) + 1, SpectralShape::bandpass(700.0, 4500.0), sr)) {
        std::free(body);
        std::free(nz);
        return nullptr;
    }
    addSweep(body, n, hz * 2.0, hz, 0.02, sr);
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        const double b = body[i] * std::exp(-t / 0.09);
        const double ring = b * std::sin(kTau * hz * 2.76 * t) * 0.3;
        body[i] = static_cast<float>(std::tanh(1.4 * (b + ring + nz[i] * std::exp(-t / 0.06) * 0.6)));
    }
    std::free(nz);
    normPeak(body, n);
    fadeOut(body, n, 0.008, sr);
    return body;
}

float* sfxKillLarge(double hz, double sr, u32& n) {
    n = framesFor(0.75, sr);
    float* sub = alloc(n);
    float* zap = alloc(n);
    float* crash = alloc(n);
    if (!sub || !zap || !crash || !noise(crash, n, static_cast<u64>(hz) + 2, SpectralShape::bandpass(500.0, 6500.0), sr)) {
        std::free(sub);
        std::free(zap);
        std::free(crash);
        return nullptr;
    }
    addSweep(sub, n, 150.0, 55.0, 0.06, sr);
    addSaw(zap, n, hz, sr, 1.0f);
    lowpassSweep(zap, n, 3000.0, 300.0, 0.07, 1.4, sr);
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        const double v = sub[i] * std::exp(-t / 0.22) + 0.7 * zap[i] * std::exp(-t / 0.16) + crash[i] * std::exp(-t / 0.20) * 0.7;
        sub[i] = static_cast<float>(std::tanh(1.5 * v));
    }
    std::free(zap);
    std::free(crash);
    normPeak(sub, n);
    fadeOut(sub, n, 0.03, sr);
    return sub;
}

float* sfxShard(double hz, double sr, u32& n) {
    n = framesFor(0.10, sr);
    float* x = alloc(n);
    if (!x) return nullptr;
    addSweep(x, n, hz * 0.9, hz, 0.010, sr);
    addSweep(x, n, hz * 1.8, hz * 2.0, 0.010, sr, 0.3f);
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        x[i] = static_cast<float>(x[i] * std::exp(-t / 0.03) * (t / 0.001 < 1.0 ? t / 0.001 : 1.0));
    }
    normPeak(x, n);
    fadeOut(x, n, 0.004, sr);
    return x;
}

float* sfxBoost(double sr, u32& n) {
    n = framesFor(1.1, sr);
    float* x = alloc(n);
    if (!x) return nullptr;
    const int notes[] = {69, 76, 81, 84};
    for (int k = 0; k < 4; ++k)
        addBell(x, n, framesFor(k * 0.055, sr), midiHz(notes[k]), 0.55, 3.5, 4.0, sr, static_cast<float>(0.8 + 0.07 * k));
    addBell(x, n, framesFor(0.24, sr), midiHz(100), 0.5, 2.0, 1.5, sr, 0.25f);
    normPeak(x, n);
    return x;
}

float* sfxUpgrade(double sr, u32& n) {
    n = framesFor(1.3, sr);
    float* x = alloc(n);
    if (!x) return nullptr;
    const int chord[] = {57, 60, 64, 67, 71};
    for (int m : chord) {
        addSaw(x, n, midiHz(m), sr, 0.1f);
        addSaw(x, n, midiHz(m) * std::pow(2.0, 8.0 / 1200.0), sr, 0.1f);
    }
    lowpassSweep(x, n, 2600.0, 900.0, 0.5, 0.9, sr);
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        x[i] = static_cast<float>(x[i] * (t / 0.08 < 1.0 ? t / 0.08 : 1.0) * std::exp(-t / 0.45));
    }
    const int bells[] = {76, 79, 83, 88};
    for (int k = 0; k < 4; ++k) addBell(x, n, framesFor(0.10 + k * 0.07, sr), midiHz(bells[k]), 0.6, 3.5, 4.0, sr, 0.5f);
    normPeak(x, n);
    fadeOut(x, n, 0.05, sr);
    return x;
}

float* sfxDash(double sr, u32& n) {
    n = framesFor(0.24, sr);
    float* x = alloc(n);
    if (!x) return nullptr;
    Random rng(41);
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        x[i] = static_cast<float>(rng.gaussian() * (t / 0.010 < 1.0 ? t / 0.010 : 1.0) * std::exp(-t / 0.060));
    }
    SvfLowpass f;
    const float k = static_cast<float>(1.0 / 1.2);
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        x[i] = f.tick(x[i], svfG(static_cast<float>(600.0 + 5400.0 * (t / 0.11 < 1.0 ? t / 0.11 : 1.0)), static_cast<float>(sr)), k);
    }
    if (!spectralFilter(x, n, SpectralShape::highpass(400.0), sr)) {
        std::free(x);
        return nullptr;
    }
    normPeak(x, n);
    fadeOut(x, n, 0.008, sr);
    return x;
}

float* sfxPerfectDash(double sr, u32& n) {
    u32 nd = 0;
    float* d = sfxDash(sr, nd);
    if (!d) return nullptr;
    const u32 nb = framesFor(0.75, sr);
    n = nd > nb ? nd : nb;
    float* x = alloc(n);
    if (!x) {
        std::free(d);
        return nullptr;
    }
    for (u32 i = 0; i < nd; ++i) x[i] = d[i];
    std::free(d);
    addBell(x, n, 0, 1318.5, 0.75, 2.0, 2.0, sr, 0.6f);
    normPeak(x, n);
    return x;
}

float* sfxHit(double sr, u32& n) {
    n = framesFor(0.62, sr);
    float* growl = alloc(n);
    float* nz = alloc(n);
    if (!growl || !nz || !noise(nz, n, 43, SpectralShape::bandpass(300.0, 5000.0), sr)) {
        std::free(growl);
        std::free(nz);
        return nullptr;
    }
    double ph = 0.0;
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        ph += (55.0 + 165.0 * std::exp(-t / 0.10)) / sr;
        growl[i] = static_cast<float>(2.0 * (ph - std::floor(ph)) - 1.0);
    }
    lowpassSweep(growl, n, 2500.0, 200.0, 0.12, 1.6, sr);
    float* thud = alloc(n);
    if (!thud) {
        std::free(growl);
        std::free(nz);
        return nullptr;
    }
    addSweep(thud, n, 120.0, 45.0, 0.05, sr);
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        const double g = std::tanh(3.0 * growl[i]) * std::exp(-t / 0.20);
        growl[i] = static_cast<float>(0.7 * g + 0.6 * nz[i] * std::exp(-t / 0.08) + 0.8 * thud[i] * std::exp(-t / 0.18));
    }
    std::free(nz);
    std::free(thud);
    normPeak(growl, n);
    fadeOut(growl, n, 0.03, sr);
    return growl;
}

float* sfxInhale(double dur, double sr, u32& n) {
    n = framesFor(dur, sr);
    float* x = alloc(n);
    if (!x || !noise(x, n, 44, SpectralShape::highpass(1500.0), sr)) {
        std::free(x);
        return nullptr;
    }
    PhaseSine s;
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr, u = t / dur;
        const double rise = s.next(180.0 + 720.0 * u * u, sr) * u * u * 0.4;
        x[i] = static_cast<float>(x[i] * std::exp((t - dur) / (dur * 0.3)) + rise);
    }
    normPeak(x, n);
    fadeOut(x, n, 0.006, sr);
    return x;
}

float* sfxNovaBlast(double sr, u32& n) {
    float* imp = makeImpact(sr, n);
    float* burst = imp ? alloc(n) : nullptr;
    if (!imp || !burst || !noise(burst, n, 45, SpectralShape::bandpass(200.0, 8000.0), sr)) {
        std::free(imp);
        std::free(burst);
        return nullptr;
    }
    for (u32 i = 0; i < n; ++i) imp[i] += static_cast<float>(burst[i] * std::exp(-(i / sr) / 0.12) * 0.8);
    std::free(burst);
    normPeak(imp, n);
    return imp;
}

float* sfxSpawn(double hz, double sr, u32& n) {
    const double dur = 0.5;
    n = framesFor(dur, sr);
    float* x = alloc(n);
    if (!x) return nullptr;
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr, u = t / dur;
        x[i] = static_cast<float>((std::sin(kTau * hz * t) + 0.3 * std::sin(kTau * hz * 2.0 * t)) * u * u * u);
    }
    normPeak(x, n);
    fadeOut(x, n, 0.004, sr);
    return x;
}

float* sfxTone(double hz, double dur, double tau, double fade, double sr, u32& n) {
    n = framesFor(dur, sr);
    float* x = alloc(n);
    if (!x) return nullptr;
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        x[i] = static_cast<float>(std::sin(kTau * hz * t) * std::exp(-t / tau));
    }
    fadeOut(x, n, fade, sr);
    return x;
}

float* sfxEnemyShot(double sr, u32& n) {
    n = framesFor(0.09, sr);
    float* x = alloc(n);
    if (!x) return nullptr;
    SquareOsc o;
    o.dt = static_cast<float>(180.0 / sr);
    for (u32 i = 0; i < n; ++i) x[i] = o.next();
    if (!spectralFilter(x, n, SpectralShape::lowpass(1500.0), sr)) {
        std::free(x);
        return nullptr;
    }
    for (u32 i = 0; i < n; ++i) x[i] = static_cast<float>(x[i] * std::exp(-(i / sr) / 0.03));
    normPeak(x, n);
    fadeOut(x, n, 0.004, sr);
    return x;
}

float* sfxElite(double sr, u32& n) {
    n = framesFor(1.3, sr);
    float* x = alloc(n);
    if (!x) return nullptr;
    for (int m : {45, 52}) {
        addSaw(x, n, midiHz(m), sr, 0.25f);
        addSaw(x, n, midiHz(m) * std::pow(2.0, 12.0 / 1200.0), sr, 0.25f);
    }
    lowpassSweep(x, n, 1400.0, 500.0, 0.3, 1.2, sr);
    for (u32 i = 0; i < n; ++i) {
        const double t = i / sr;
        double v = x[i] * (t / 0.02 < 1.0 ? t / 0.02 : 1.0) * std::exp(-t / 0.5);
        v += 0.5 * std::sin(kTau * midiHz(33) * t) * std::exp(-t / 0.4);
        x[i] = static_cast<float>(std::tanh(1.6 * v));
    }
    normPeak(x, n);
    fadeOut(x, n, 0.05, sr);
    return x;
}

float* sfxTwoNote(double a, double b, double da, double db, bool soft, double sr, u32& n) {
    const u32 na = framesFor(da, sr), nb = framesFor(db, sr);
    n = na + nb;
    float* x = alloc(n);
    if (!x) return nullptr;
    for (u32 i = 0; i < na; ++i) x[i] = static_cast<float>(std::sin(kTau * a * (i / sr)) * std::exp(-(i / sr) / (da * 0.6)));
    for (u32 i = 0; i < nb; ++i) x[na + i] = static_cast<float>(std::sin(kTau * b * (i / sr)) * std::exp(-(i / sr) / (db * 0.5)));
    fadeOut(x, na, 0.004, sr);
    fadeOut(x + na, nb, 0.006, sr);
    if (soft) {
        for (u32 i = 0; i < na; ++i)
            x[i] += static_cast<float>(0.25 * std::sin(kTau * 2.0 * a * (i / sr)) * std::exp(-(i / sr) / (da * 0.4)));
        for (u32 i = 0; i < nb; ++i)
            x[na + i] += static_cast<float>(0.25 * std::sin(kTau * 2.0 * b * (i / sr)) * std::exp(-(i / sr) / (db * 0.4)));
    }
    normPeak(x, n);
    return x;
}

float* sfxGameOver(double sr, u32& n) {
    n = framesFor(2.6, sr);
    float* x = alloc(n);
    if (!x) return nullptr;
    const int notes[] = {81, 76, 72, 69, 64, 57};
    for (int k = 0; k < 6; ++k)
        addBell(x, n, framesFor(k * 0.13, sr), midiHz(notes[k]), 0.9, 2.0, 2.5, sr, static_cast<float>(1.0 - 0.08 * k));
    const u32 nb = framesFor(1.4, sr), at = framesFor(0.78, sr);
    PhaseSine s;
    for (u32 i = 0; i < nb && at + i < n; ++i) {
        const double t = i / sr;
        x[at + i] += static_cast<float>(s.next(35.0 + 55.0 * std::exp(-t / 0.12), sr) * std::exp(-t / 0.45) * 0.9);
    }
    normPeak(x, n);
    fadeOut(x, n, 0.1, sr);
    return x;
}

}  // namespace

float* bakeKickWithSeed(const SoundParams& params, double sampleRate, u64 seed, u32& frames) {
    return makeKick(params, sampleRate, frames, seed);
}

const char* sfxName(Sfx s) {
    static const char* const kNames[] = {"kill_small", "kill_medium", "kill_large", "shard",   "boost",      "upgrade",
                                         "dash",       "perfect",     "hit",        "inhale",  "blast",      "spawn",
                                         "tick",       "enemy_shot",  "elite",      "ui_tick", "ui_confirm", "ui_back",
                                         "shield",     "game_over"};
    const u32 i = static_cast<u32>(s);
    return i < countOf(kNames) ? kNames[i] : "?";
}

float sfxLevel(Sfx s) {
    // sfxproto.LEVEL; the interface sounds and game over are not in it: the effect sheet plays them at 0.5.
    static const float kLevels[] = {0.13f, 0.22f, 0.40f, 0.085f, 0.30f, 0.34f, 0.22f, 0.20f, 0.55f, 0.30f,
                                    0.70f, 0.10f, 0.05f, 0.07f,  0.34f, 0.5f,  0.5f,  0.5f,  0.5f,  0.6f};
    const u32 i = static_cast<u32>(s);
    return i < countOf(kLevels) ? kLevels[i] : 0.0f;
}

SampleBank::~SampleBank() { release(); }

void SampleBank::release() {
    for (u32 i = 0; i < count_; ++i) std::free(samples_[i].data);
    count_ = 0;
    bytes_ = 0;
    riserCount_ = 0;
}

i32 SampleBank::add(float* data, u32 frames, u8 channels, i16 midi, u16 tag, const char* name) {
    if (!data || count_ >= kMaxSamples) {
        std::free(data);
        return -1;
    }
    Sample& s = samples_[count_];
    s.data = data;
    s.frames = frames;
    s.channels = channels;
    s.midi = midi;
    s.tag = tag;
    std::snprintf(s.name, sizeof s.name, "%s", name);
    bytes_ += static_cast<u64>(frames) * channels * sizeof(float);
    return static_cast<i32>(count_++);
}

i32 SampleBank::riser(u32 track, u32 bars) const {
    // The exact length, else the nearest one baked for the Track (the caller stretches it).
    i32 best = -1;
    u32 bestDist = ~0u;
    for (u32 i = 0; i < riserCount_; ++i) {
        if (risers_[i].track != track) continue;
        const u32 d = risers_[i].bars > bars ? risers_[i].bars - bars : bars - risers_[i].bars;
        if (d < bestDist) {
            bestDist = d;
            best = risers_[i].id;
        }
    }
    return best;
}

i32 SampleBank::sfx(Sfx kind, i32 midi) const {
    const u32 k = static_cast<u32>(kind);
    if (k >= kSfxCount || !sfxCount_[k]) return -1;
    i32 best = sfxFirst_[k];
    if (midi < 0) return best;
    i32 bestDist = 1 << 30;
    for (u32 i = 0; i < sfxCount_[k]; ++i) {
        const i32 id = sfxFirst_[k] + static_cast<i32>(i);
        const i32 d = samples_[id].midi > midi ? samples_[id].midi - midi : midi - samples_[id].midi;
        if (d < bestDist) {
            bestDist = d;
            best = id;
        }
    }
    return best;
}

i32 SampleBank::inhale(u32 track, u32 steps32) const {
    if (track >= MusicData::kMaxTracks) return -1;
    if (steps32 < 1) steps32 = 1;
    if (steps32 > 8) steps32 = 8;
    return inhale_[track][steps32];
}

bool SampleBank::bake(const MusicData& music, u32 sampleRate) {
    release();
    const auto t0 = std::chrono::steady_clock::now();
    const double sr = sampleRate;
    u32 n = 0;
    bool ok = true;
    char label[24] = "?";
    const auto put = [&](float* data, u8 channels = 1, i16 midi = -1, u16 tag = 0) {
        const i32 id = add(data, n, channels, midi, tag, label);
        if (id < 0) ok = false;
        return id;
    };

    for (u32 t = 0; t < music.trackCount; ++t) {
        for (u32 v = 0; v < music.variantCount; ++v) {
            const SoundParams& p = music.sound[t][v];
            DrumKit& kit = kits_[t][v];
            std::snprintf(label, sizeof label, "kick_%u_%u", t, v);
            kit.kick = put(makeKick(p, sr, n));
            std::snprintf(label, sizeof label, "hat_%u_%u", t, v);
            kit.hat = put(makeHat(p.hat.closedDecay, 21, sr, n));
            std::snprintf(label, sizeof label, "ohat_%u_%u", t, v);
            kit.ohat = put(makeHat(p.hat.openDecay, 22, sr, n));
        }
    }
    std::snprintf(label, sizeof label, "clap");
    clap_ = put(makeClap(sr, n));
    std::snprintf(label, sizeof label, "snare");
    snare_ = put(makeSnare(sr, n));
    std::snprintf(label, sizeof label, "shaker");
    shaker_ = put(makeShaker(sr, n));
    std::snprintf(label, sizeof label, "pulse");
    pulse_ = put(makePulse(sr, n));
    std::snprintf(label, sizeof label, "impact");
    impact_ = put(makeImpact(sr, n));

    for (u32 t = 0; t < music.trackCount; ++t) {
        const Track& tr = music.tracks[t];
        const double bar = 240.0 / tr.bpm;
        std::snprintf(label, sizeof label, "swellhalf_%u", t);
        swellHalf_[t] = put(makeSwell(bar * 0.5, sr, n));
        std::snprintf(label, sizeof label, "swellbar_%u", t);
        swellBar_[t] = put(makeSwell(bar, sr, n));
        // Risers for every Build length the staged arrangement and the phrase cycle use (4 or 8 bars).
        u32 lengths[4];
        u32 nl = 0;
        const auto want = [&](u32 bars) {
            for (u32 i = 0; i < nl; ++i)
                if (lengths[i] == bars) return;
            if (nl < 4) lengths[nl++] = bars;
        };
        // The staged arrangement's Builds and the phrase cycle's (Short form for Track 1, Standard after).
        for (u32 i = 0; i < tr.partCount; ++i)
            if (tr.parts[i].section == Section::Build) want(tr.parts[i].bars);
        want(t == 0 ? 4 : 8);
        for (u32 i = 0; i < nl && riserCount_ < countOf(risers_); ++i) {
            std::snprintf(label, sizeof label, "riser_%u_%u", t, lengths[i]);
            float* r = makeRiser(lengths[i] * bar, sr, n);
            const i32 id = put(r, 2, -1, static_cast<u16>(lengths[i]));
            risers_[riserCount_++] = {t, lengths[i], id};
        }
        // Shard scale: minor pentatonic from the tonic (the first chord's bass) three octaves up.
        static const u8 kPenta[] = {0, 3, 5, 7, 10};
        const u8 tonic = static_cast<u8>(tr.chords[tr.progression[0]].bass + 36);
        for (u32 i = 0; i < 10; ++i) shardScale_[t][i] = static_cast<u8>(tonic + 12 * (i / 5) + kPenta[i % 5]);
    }

    // Pitched effects: every note the Tracks' chords ask for.
    const auto bakePitched = [&](Sfx kind, auto&& notesOf, auto&& make) {
        sfxFirst_[static_cast<u32>(kind)] = static_cast<i32>(count_);
        i16 done[64];
        u32 nd = 0;
        for (u32 t = 0; t < music.trackCount; ++t) {
            i16 notes[32];
            const u32 nn = notesOf(music.tracks[t], t, notes);
            for (u32 i = 0; i < nn; ++i) {
                bool dup = false;
                for (u32 j = 0; j < nd; ++j) dup = dup || done[j] == notes[i];
                if (dup || nd >= countOf(done)) continue;
                done[nd++] = notes[i];
                std::snprintf(label, sizeof label, "%s_%d", sfxName(kind), notes[i]);
                put(make(midiHz(notes[i])), 1, notes[i]);
                ++sfxCount_[static_cast<u32>(kind)];
            }
        }
    };
    const auto stabTones = [](const Track& tr, u32, i16* out, int offset) {
        u32 k = 0;
        for (u32 c = 0; c < tr.chordCount; ++c)
            for (u32 s = 0; s < tr.chords[c].stabCount && k < 32; ++s) out[k++] = static_cast<i16>(tr.chords[c].stab[s] + offset);
        return k;
    };
    bakePitched(Sfx::KillSmall, [&](const Track& tr, u32 t, i16* o) { return stabTones(tr, t, o, 12); },
                [&](double hz) { return sfxKillSmall(hz, sr, n); });
    bakePitched(Sfx::KillMedium, [&](const Track& tr, u32 t, i16* o) { return stabTones(tr, t, o, 0); },
                [&](double hz) { return sfxKillMedium(hz, sr, n); });
    bakePitched(Sfx::KillLarge,
                [&](const Track& tr, u32, i16* o) {
                    u32 k = 0;
                    for (u32 c = 0; c < tr.chordCount && k < 32; ++c) o[k++] = static_cast<i16>(tr.chords[c].stab[0] - 12);
                    return k;
                },
                [&](double hz) { return sfxKillLarge(hz, sr, n); });
    bakePitched(Sfx::Shard,
                [&](const Track&, u32 t, i16* o) {
                    for (u32 i = 0; i < 10; ++i) o[i] = shardScale_[t][i];
                    return 10u;
                },
                [&](double hz) { return sfxShard(hz, sr, n); });
    bakePitched(Sfx::Spawn,
                [&](const Track& tr, u32, i16* o) {
                    u32 k = 0;
                    for (u32 c = 0; c < tr.chordCount && k < 32; ++c) o[k++] = static_cast<i16>(tr.chords[c].bass + 12);
                    return k;
                },
                [&](double hz) { return sfxSpawn(hz, sr, n); });

    const auto single = [&](Sfx kind, float* data) {
        std::snprintf(label, sizeof label, "%s", sfxName(kind));
        sfxFirst_[static_cast<u32>(kind)] = put(data);
        sfxCount_[static_cast<u32>(kind)] = 1;
    };
    single(Sfx::Boost, sfxBoost(sr, n));
    single(Sfx::Upgrade, sfxUpgrade(sr, n));
    single(Sfx::Dash, sfxDash(sr, n));
    single(Sfx::PerfectDash, sfxPerfectDash(sr, n));
    single(Sfx::Hit, sfxHit(sr, n));
    single(Sfx::NovaBlast, sfxNovaBlast(sr, n));
    single(Sfx::Tick, sfxTone(3000.0, 0.03, 0.006, 0.003, sr, n));
    single(Sfx::EnemyShot, sfxEnemyShot(sr, n));
    single(Sfx::Elite, sfxElite(sr, n));
    single(Sfx::UiTick, sfxTone(1800.0, 0.03, 0.008, 0.003, sr, n));
    single(Sfx::UiConfirm, sfxTwoNote(1318.5, 1760.0, 0.04, 0.09, false, sr, n));
    single(Sfx::UiBack, sfxTwoNote(1760.0, 1318.5, 0.04, 0.09, false, sr, n));
    single(Sfx::Shield, sfxTwoNote(659.3, 880.0, 0.07, 0.16, true, sr, n));
    single(Sfx::GameOver, sfxGameOver(sr, n));
    // Nova inhale: from the press to the next beat, 1..8 thirty-seconds of each Track's tempo.
    sfxFirst_[static_cast<u32>(Sfx::NovaInhale)] = static_cast<i32>(count_);
    for (u32 t = 0; t < music.trackCount; ++t) {
        const double step32 = 240.0 / music.tracks[t].bpm / 32.0;
        for (u32 s = 1; s <= 8; ++s) {
            std::snprintf(label, sizeof label, "inhale_%u_%u", t, s);
            inhale_[t][s] = put(sfxInhale(s * step32, sr, n), 1, -1, static_cast<u16>(s));
            ++sfxCount_[static_cast<u32>(Sfx::NovaInhale)];
        }
    }

    bakeSeconds_ = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (!ok) {
        RF_ERR("audio: baking the one-shots failed (out of memory or too many samples)");
        return false;
    }
    RF_LOG("audio: baked %u one-shots (%.1f MB) in %.0f ms", count_, bytes_ / 1048576.0, bakeSeconds_ * 1000.0);
    return true;
}

}  // namespace rf::audio
