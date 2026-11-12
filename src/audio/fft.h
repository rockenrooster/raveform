#pragma once
// Load-time spectral tools for baking one-shots (never used on the audio thread): a radix-2 complex
// FFT, Bluestein's algorithm for any length, and the prototype's zero-phase spectral filters
// (musicproto.fft_filter with hp / lp / bp magnitude responses).

#include "core/types.h"

namespace rf::audio {

class Random;

struct Complex {
    double re = 0.0;
    double im = 0.0;
};

// In place, n a power of two. inverse: e^(+i...) without the 1/n scale.
void fftPow2(Complex* data, u32 n, bool inverse);

// musicproto.hp / lp / bp: |H(f)| = 1 / sqrt(1 + (fc / f)^(2 order)) and so on.
struct SpectralShape {
    enum class Kind : u8 { Flat, Highpass, Lowpass, Bandpass } kind = Kind::Flat;
    double lo = 0.0;  // high-pass corner (Highpass, Bandpass)
    double hi = 0.0;  // low-pass corner (Lowpass, Bandpass)
    int order = 2;
    double gain(double f) const;

    static SpectralShape highpass(double fc, int order = 2) { return {Kind::Highpass, fc, 0.0, order}; }
    static SpectralShape lowpass(double fc, int order = 2) { return {Kind::Lowpass, 0.0, fc, order}; }
    static SpectralShape bandpass(double lo, double hi, int order = 2) { return {Kind::Bandpass, lo, hi, order}; }
};

// Exactly musicproto.fft_filter: the circular zero-phase filter of length n (any n; Bluestein when n
// is not a power of two). Allocates scratch with malloc (load time only). False when out of memory.
bool spectralFilter(float* x, u32 n, const SpectralShape& shape, double sampleRate);

// fft_filter(white(n), shape) in distribution: n samples of unit-variance Gaussian white noise shaped by
// the same zero-phase magnitude (the period is the next power of two at or above n).
bool filteredNoise(float* out, u32 n, Random& rng, const SpectralShape& shape, double sampleRate);

}  // namespace rf::audio
