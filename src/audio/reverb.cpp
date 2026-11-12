#include "audio/reverb.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "audio/dsp.h"
#include "audio/fft.h"

namespace rf::audio {

namespace {

// Prime lengths at 48 kHz (scaled with the rate). Left and right differ so the tails decorrelate.
constexpr u32 kLineLen[2][FdnReverb::kLines] = {{997, 1153, 1301, 1453, 1601, 1777, 1949, 2129},
                                                {1031, 1187, 1361, 1489, 1657, 1811, 2003, 2179}};
constexpr u32 kDiffLen[2][FdnReverb::kDiffusers] = {{211, 157, 557, 409}, {223, 167, 577, 431}};
constexpr float kDiffGain[FdnReverb::kDiffusers] = {0.75f, 0.75f, 0.625f, 0.625f};
constexpr float kInSign[FdnReverb::kLines] = {1, -1, 1, -1, 1, -1, 1, -1};
constexpr float kOutSign[FdnReverb::kLines] = {1, 1, -1, -1, 1, 1, -1, -1};
constexpr float kEarly = 0.5f;          // the diffused input also reaches the output (dense onset)
constexpr double kMaxPredelay = 0.1;    // seconds

u32 pow2Above(u32 n) {
    u32 p = 1;
    while (p <= n) p <<= 1;
    return p;
}

u32 scaled(u32 len48k, double rate) {
    const u32 v = static_cast<u32>(std::lround(len48k * rate / 48000.0));
    return v < 2 ? 2 : v;
}

}  // namespace

FdnReverb::~FdnReverb() { std::free(memory_); }

bool FdnReverb::init(double sampleRate, const ReverbPatch* sets, u32 setCount) {
    rate_ = sampleRate;
    std::free(memory_);
    memory_ = nullptr;
    // One block for every buffer.
    u32 total = 0;
    const u32 preCap = pow2Above(static_cast<u32>(kMaxPredelay * sampleRate) + 1);
    for (u32 c = 0; c < 2; ++c) {
        total += preCap;
        for (u32 d = 0; d < kDiffusers; ++d) total += pow2Above(scaled(kDiffLen[c][d], sampleRate));
        for (u32 l = 0; l < kLines; ++l) total += pow2Above(scaled(kLineLen[c][l], sampleRate));
    }
    memory_ = static_cast<float*>(std::calloc(total, sizeof(float)));
    if (!memory_) return false;
    float* p = memory_;
    for (u32 c = 0; c < 2; ++c) {
        Channel& ch = ch_[c];
        ch = Channel{};
        ch.pre = p;
        ch.preMask = preCap - 1;
        p += preCap;
        for (u32 d = 0; d < kDiffusers; ++d) {
            const u32 len = scaled(kDiffLen[c][d], sampleRate), cap = pow2Above(len);
            ch.diff[d] = {p, cap - 1, len};
            p += cap;
        }
        for (u32 l = 0; l < kLines; ++l) {
            const u32 len = scaled(kLineLen[c][l], sampleRate), cap = pow2Above(len);
            ch.line[l] = {p, cap - 1, len};
            p += cap;
        }
    }
    setCount_ = setCount < kMaxSets ? setCount : kMaxSets;
    for (u32 i = 0; i < setCount_; ++i) {
        // Identical parameters (most Track x variant slots) share one measurement.
        u32 same = i;
        for (u32 k = 0; k < i && same == i; ++k)
            if (std::memcmp(&sets[k], &sets[i], sizeof(ReverbPatch)) == 0) same = k;
        if (same != i) {
            sets_[i] = sets_[same];
            continue;
        }
        configure(sets_[i], sets[i]);
        // Normalisation, measured on the impulse response. The prototype's IR has unit energy and the
        // spectrum of its band limits; the network's modes tilt its spectrum a little, so the energy is
        // matched where the sends live (250 Hz - 6 kHz): the network's energy there equals the share the
        // prototype's IR has there. (Total energy: within a few tenths of a dB of one.)
        const u32 n = static_cast<u32>((2.0 * sets[i].rt60 + kMaxPredelay + 0.2) * sampleRate);
        const u32 m = 65536;  // the spectrum from the first 1.4 s (the rest holds well under 0.1% of the energy)
        auto* spec = static_cast<Complex*>(std::malloc(sizeof(Complex) * m));
        const SpectralShape shape = SpectralShape::bandpass(sets[i].highpass, sets[i].lowpass, 1);
        double expectIn = 0.0, expectAll = 0.0;
        for (u32 b = 1; b < m / 2; ++b) {
            const double f = static_cast<double>(b) * sampleRate / m, g = shape.gain(f);
            expectAll += g * g;
            if (f >= 250.0 && f < 6000.0) expectIn += g * g;
        }
        for (u32 c = 0; c < 2; ++c) {
            sets_[i].out[c] = 1.0f;
            clear();
            double e = 0.0;
            for (u32 k = 0; k < n; ++k) {
                const float y = tick(ch_[c], sets_[i], c, k == 0 ? 1.0f : 0.0f);
                e += static_cast<double>(y) * y;
                if (spec && k < m) spec[k] = {y, 0.0};
            }
            double scale = e > 0.0 ? 1.0 / std::sqrt(e) : 0.0;
            if (spec && e > 0.0 && expectAll > 0.0) {
                for (u32 k = n < m ? n : m; k < m; ++k) spec[k] = {0.0, 0.0};
                fftPow2(spec, m, false);
                double in = 0.0, all = 0.0;
                for (u32 b = 1; b < m / 2; ++b) {
                    const double f = static_cast<double>(b) * sampleRate / m;
                    const double p = spec[b].re * spec[b].re + spec[b].im * spec[b].im;
                    all += p;
                    if (f >= 250.0 && f < 6000.0) in += p;
                }
                if (in > 0.0 && all > 0.0) scale *= std::sqrt((expectIn / expectAll) / (in / all));
            }
            sets_[i].out[c] = static_cast<float>(scale);
        }
        std::free(spec);
    }
    clear();
    active_ = 0;
    return true;
}

void FdnReverb::configure(Set& s, const ReverbPatch& p) {
    for (u32 c = 0; c < 2; ++c)
        for (u32 l = 0; l < kLines; ++l)
            s.gain[c][l] = static_cast<float>(std::pow(10.0, -3.0 * ch_[c].line[l].len / (p.rt60 * rate_)));
    s.mix = p.mix;
    OnePole hp, lp;
    hp.setHighpass(p.highpass, rate_);
    lp.setLowpass(p.lowpass, rate_);
    s.hpB0 = hp.b0;
    s.hpB1 = hp.b1;
    s.hpA1 = hp.a1;
    s.lpB0 = lp.b0;
    s.lpB1 = lp.b1;
    s.lpA1 = lp.a1;
    double pre = p.predelay < 0.0 ? 0.0 : (p.predelay > kMaxPredelay ? kMaxPredelay : p.predelay);
    s.preLen = static_cast<u32>(pre * rate_);
}

void FdnReverb::select(u32 set) {
    if (set < setCount_) active_ = set;
}

void FdnReverb::clear() {
    for (Channel& c : ch_) {
        std::memset(c.pre, 0, sizeof(float) * (c.preMask + 1));
        for (Line& d : c.diff) std::memset(d.buf, 0, sizeof(float) * (d.mask + 1));
        for (Line& l : c.line) std::memset(l.buf, 0, sizeof(float) * (l.mask + 1));
        c.hpX1 = c.hpY1 = c.lpX1 = c.lpY1 = 0.0f;
        c.pos = 0;
    }
}

float FdnReverb::tick(Channel& c, const Set& s, u32 ch, float x) {
    const u32 pos = c.pos;
    c.pre[pos & c.preMask] = x;
    const float xd = c.pre[(pos - s.preLen) & c.preMask];
    const float hp = s.hpB0 * xd + s.hpB1 * c.hpX1 - s.hpA1 * c.hpY1;
    c.hpX1 = xd;
    c.hpY1 = hp;
    const float lp = s.lpB0 * hp + s.lpB1 * c.lpX1 - s.lpA1 * c.lpY1;
    c.lpX1 = hp;
    c.lpY1 = lp;
    float v = lp;
    for (u32 d = 0; d < kDiffusers; ++d) {
        Line& a = c.diff[d];
        const float delayed = a.buf[(pos - a.len) & a.mask];
        const float w = v + kDiffGain[d] * delayed;
        a.buf[pos & a.mask] = w;
        v = delayed - kDiffGain[d] * w;
    }
    float o[kLines], m[kLines];
    float out = 0.0f;
    for (u32 l = 0; l < kLines; ++l) {
        const Line& line = c.line[l];
        o[l] = line.buf[(pos - line.len) & line.mask];
        m[l] = o[l] * s.gain[ch][l];
        out += kOutSign[l] * o[l];
    }
    // Fast Walsh-Hadamard transform, scaled to stay lossless.
    for (u32 h = 1; h < kLines; h <<= 1)
        for (u32 i = 0; i < kLines; i += h << 1)
            for (u32 j = i; j < i + h; ++j) {
                const float a = m[j], b = m[j + h];
                m[j] = a + b;
                m[j + h] = a - b;
            }
    constexpr float kNorm = 0.35355339059f;  // 1 / sqrt(8)
    for (u32 l = 0; l < kLines; ++l) {
        Line& line = c.line[l];
        line.buf[pos & line.mask] = m[l] * kNorm + kInSign[l] * v * kNorm;
    }
    c.pos = pos + 1;
    return (out * kNorm + kEarly * v) * s.out[ch];
}

void FdnReverb::process(const float* inL, const float* inR, float* outL, float* outR, u32 n) {
    const Set& s = sets_[active_];
    for (u32 i = 0; i < n; ++i) {
        const float mono = 0.5f * (inL[i] + inR[i]);
        outL[i] += s.mix * tick(ch_[0], s, 0, 0.6f * mono + 0.4f * inL[i]);
        outR[i] += s.mix * tick(ch_[1], s, 1, 0.6f * mono + 0.4f * inR[i]);
    }
}

bool FdnReverb::impulseResponse(u32 set, u32 ch, float* out, u32 n) {
    if (set >= setCount_ || ch > 1) return false;
    clear();
    for (u32 k = 0; k < n; ++k) out[k] = tick(ch_[ch], sets_[set], ch, k == 0 ? 1.0f : 0.0f);
    clear();
    return true;
}

}  // namespace rf::audio
