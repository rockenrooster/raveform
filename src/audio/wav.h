#pragma once
// WAV files for the offline renderer and the tests (never on the audio thread).

#include "core/types.h"

namespace rf::audio {

// Interleaved stereo float samples. `pcm16`: 16-bit with TPDF dither (for listening); else 32-bit float
// (exact, for analysis).
bool writeWav(const char* path, const float* interleaved, u64 frames, u32 channels, u32 sampleRate, bool pcm16);

// ITU-R BS.1770 integrated loudness (K-weighting, 400 ms blocks with 100 ms hop, absolute and relative
// gates), the same procedure as musicproto.lufs. 48 kHz only. Returns -70 for silence.
double integratedLoudness(const float* interleaved, u64 frames, u32 channels, u32 sampleRate);

}  // namespace rf::audio
