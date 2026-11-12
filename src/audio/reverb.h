#pragma once
// The "small reverb on sends" (TECH_SPEC 6.5). The prototype convolves each channel with 1.3 x rt60 of
// exponentially decaying white noise (10^(-3 t / rt60)), band-limited by first-order 320 Hz / 7 kHz
// filters, after a 15 ms pre-delay, normalised to unit energy, fed 0.6 x mono + 0.4 x the channel. A
// real-time engine cannot afford that convolution, so each channel is a feedback delay network with the
// same decay, band limits, pre-delay, input mix and energy: input pre-delay -> first-order high- and
// low-pass -> four allpass diffusers (whose output is also the early part) -> eight delay lines mixed by
// a Hadamard matrix with per-line losses set for rt60. The two channels use different line lengths, so
// they are decorrelated like the prototype's two noise IRs. The output gain is measured once per
// parameter set at load: the impulse response is rendered offline and scaled to unit energy.

#include "audio/music_data.h"
#include "core/types.h"

namespace rf::audio {

class FdnReverb {
public:
    static constexpr u32 kLines = 8;
    static constexpr u32 kDiffusers = 4;
    static constexpr u32 kMaxSets = MusicData::kMaxTracks * MusicData::kMaxVariants;

    FdnReverb() = default;
    FdnReverb(const FdnReverb&) = delete;
    FdnReverb& operator=(const FdnReverb&) = delete;
    ~FdnReverb();

    // Load time: allocates the lines and measures the normalisation of each parameter set.
    bool init(double sampleRate, const ReverbPatch* sets, u32 setCount);
    // Audio thread: switches to a prepared set (keeps the tail).
    void select(u32 set);
    void clear();
    // Adds the wet signal of n samples to outL / outR (prototype input mix applied here).
    void process(const float* inL, const float* inR, float* outL, float* outR, u32 n);
    // The unit-energy impulse response of channel `ch` for set `set` (tests, load time).
    bool impulseResponse(u32 set, u32 ch, float* out, u32 n);

private:
    struct Line {
        float* buf = nullptr;
        u32 mask = 0;
        u32 len = 0;
    };
    struct Channel {
        float* pre = nullptr;
        u32 preMask = 0;
        u32 preLen = 0;
        float hpX1 = 0, hpY1 = 0, lpX1 = 0, lpY1 = 0;
        Line diff[kDiffusers];
        Line line[kLines];
        u32 pos = 0;
    };
    struct Set {
        float gain[2][kLines];  // per-line loss for rt60
        float out[2];           // unit-energy scale
        float mix = 1.0f;
        float hpB0, hpB1, hpA1, lpB0, lpB1, lpA1;
        u32 preLen = 0;
    };

    float tick(Channel& c, const Set& s, u32 ch, float x);
    void configure(Set& s, const ReverbPatch& p);

    double rate_ = 48000.0;
    Channel ch_[2];
    Set sets_[kMaxSets];
    u32 setCount_ = 0;
    u32 active_ = 0;
    float* memory_ = nullptr;
};

}  // namespace rf::audio
