#include "audio/fft.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "audio/dsp.h"

namespace rf::audio {

namespace {

inline Complex mul(Complex a, Complex b) { return {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re}; }

u32 nextPow2(u32 n) {
    u32 p = 1;
    while (p < n) p <<= 1;
    return p;
}

// Spectral gain for bin k of an n-point DFT (Hermitian: bins above n/2 mirror those below).
double binGain(const SpectralShape& s, u32 k, u32 n, double sampleRate) {
    const u32 m = k <= n / 2 ? k : n - k;
    return s.gain(static_cast<double>(m) * sampleRate / static_cast<double>(n));
}

// Forward DFT of any length (Bluestein). `scratch` holds 3 * m complex values, m = nextPow2(2n - 1).
void dftAny(Complex* x, u32 n, Complex* scratch) {
    const u32 m = nextPow2(2 * n - 1);
    Complex* a = scratch;
    Complex* b = scratch + m;
    Complex* w = scratch + 2 * m;  // chirp, n values
    for (u32 k = 0; k < n; ++k) {
        // k^2 mod 2n keeps the angle exact for large k.
        const u64 kk = (static_cast<u64>(k) * k) % (2ull * n);
        const double ang = -kPiD * static_cast<double>(kk) / static_cast<double>(n);
        w[k] = {std::cos(ang), std::sin(ang)};
    }
    std::memset(a, 0, sizeof(Complex) * m);
    std::memset(b, 0, sizeof(Complex) * m);
    for (u32 k = 0; k < n; ++k) {
        a[k] = mul(x[k], w[k]);
        const Complex c{w[k].re, -w[k].im};
        b[k] = c;
        if (k) b[m - k] = c;
    }
    fftPow2(a, m, false);
    fftPow2(b, m, false);
    for (u32 k = 0; k < m; ++k) a[k] = mul(a[k], b[k]);
    fftPow2(a, m, true);
    const double inv = 1.0 / static_cast<double>(m);
    for (u32 k = 0; k < n; ++k) x[k] = mul({a[k].re * inv, a[k].im * inv}, w[k]);
}

}  // namespace

void fftPow2(Complex* a, u32 n, bool inverse) {
    if (n < 2) return;
    for (u32 i = 1, j = 0; i < n; ++i) {
        u32 bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            const Complex t = a[i];
            a[i] = a[j];
            a[j] = t;
        }
    }
    // Twiddles once per transform (exact cos / sin, no recurrence drift); each stage strides through them.
    const u32 halfN = n >> 1;
    auto* tw = static_cast<Complex*>(std::malloc(sizeof(Complex) * halfN));
    const double sign = inverse ? 1.0 : -1.0;
    for (u32 k = 0; k < halfN; ++k) {
        const double ang = sign * 2.0 * kPiD * static_cast<double>(k) / static_cast<double>(n);
        tw[k] = {std::cos(ang), std::sin(ang)};
    }
    for (u32 len = 2; len <= n; len <<= 1) {
        const u32 half = len >> 1, stride = n / len;
        for (u32 i = 0; i < n; i += len) {
            Complex* lo = a + i;
            Complex* hi = a + i + half;
            for (u32 k = 0; k < half; ++k) {
                const Complex v = mul(hi[k], tw[k * stride]);
                const Complex u = lo[k];
                lo[k] = {u.re + v.re, u.im + v.im};
                hi[k] = {u.re - v.re, u.im - v.im};
            }
        }
    }
    std::free(tw);
}
double SpectralShape::gain(double f) const {
    const double fp = f > 1e-6 ? f : 1e-6;  // np.maximum(f, 1e-6) in musicproto.hp
    const double o2 = 2.0 * order;
    switch (kind) {
        case Kind::Flat: return 1.0;
        case Kind::Highpass: return 1.0 / std::sqrt(1.0 + std::pow(lo / fp, o2));
        case Kind::Lowpass: return 1.0 / std::sqrt(1.0 + std::pow(f / hi, o2));
        case Kind::Bandpass:
            return 1.0 / std::sqrt(1.0 + std::pow(lo / fp, o2)) / std::sqrt(1.0 + std::pow(f / hi, o2));
    }
    return 1.0;
}

bool spectralFilter(float* x, u32 n, const SpectralShape& shape, double sampleRate) {
    if (n == 0) return true;
    const bool pow2 = (n & (n - 1)) == 0;
    const u32 m = pow2 ? n : nextPow2(2 * n - 1);
    auto* data = static_cast<Complex*>(std::malloc(sizeof(Complex) * n));
    auto* scratch = pow2 ? nullptr : static_cast<Complex*>(std::malloc(sizeof(Complex) * (2 * m + n)));
    if (!data || (!pow2 && !scratch)) {
        std::free(data);
        std::free(scratch);
        return false;
    }
    for (u32 i = 0; i < n; ++i) data[i] = {static_cast<double>(x[i]), 0.0};
    if (pow2) {
        fftPow2(data, n, false);
    } else {
        dftAny(data, n, scratch);
    }
    for (u32 k = 0; k < n; ++k) {
        const double g = binGain(shape, k, n, sampleRate);
        data[k].re *= g;
        data[k].im *= g;
    }
    // Inverse DFT by conjugation: x = conj(DFT(conj(X))) / n.
    for (u32 k = 0; k < n; ++k) data[k].im = -data[k].im;
    if (pow2) {
        fftPow2(data, n, false);
    } else {
        dftAny(data, n, scratch);
    }
    const double inv = 1.0 / static_cast<double>(n);
    for (u32 i = 0; i < n; ++i) x[i] = static_cast<float>(data[i].re * inv);
    std::free(data);
    std::free(scratch);
    return true;
}

bool filteredNoise(float* out, u32 n, Random& rng, const SpectralShape& shape, double sampleRate) {
    if (n == 0) return true;
    const u32 m = nextPow2(n);
    auto* data = static_cast<Complex*>(std::malloc(sizeof(Complex) * m));
    if (!data) return false;
    for (u32 i = 0; i < m; ++i) data[i] = {rng.gaussian(), 0.0};
    fftPow2(data, m, false);
    for (u32 k = 0; k < m; ++k) {
        const double g = binGain(shape, k, m, sampleRate);
        data[k].re *= g;
        data[k].im *= g;
    }
    fftPow2(data, m, true);
    const double inv = 1.0 / static_cast<double>(m);
    for (u32 i = 0; i < n; ++i) out[i] = static_cast<float>(data[i].re * inv);
    std::free(data);
    return true;
}

}  // namespace rf::audio
