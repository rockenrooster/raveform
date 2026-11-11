#pragma once
// One-shots synthesised once at load (TECH_SPEC 6.5): the drum kit, risers, swells and impact of
// design/tools/musicproto.py and the sound effects of design/tools/sfxproto.py, rendered with the same
// recipes into float buffers that the sampler plays. Load time only: allocates with malloc.

#include "audio/music_data.h"
#include "core/types.h"

namespace rf::audio {

struct Sample {
    float* data = nullptr;  // interleaved when stereo
    u32 frames = 0;
    u8 channels = 1;
    i16 midi = -1;          // pitched effects: the note it was synthesised at
    u16 tag = 0;            // effect-specific (Nova inhale: length in 1/32 steps; risers: bars)
    char name[24] = {};     // for dumps and logs
};

// Sound effects (sfxproto.py). Pitched ones are baked per note the Tracks use.
enum class Sfx : u8 {
    KillSmall, KillMedium, KillLarge, Shard, Boost, Upgrade, Dash, PerfectDash, Hit, NovaInhale, NovaBlast,
    Spawn, Tick, EnemyShot, Elite, UiTick, UiConfirm, UiBack, Shield, GameOver, Count
};
constexpr u32 kSfxCount = static_cast<u32>(Sfx::Count);
const char* sfxName(Sfx s);
// sfxproto.LEVEL: each effect's level in the gameplay mix (the interface sounds: the sheet's 0.5).
float sfxLevel(Sfx s);

struct DrumKit {
    i32 kick = -1, hat = -1, ohat = -1;
};

class SampleBank {
public:
    static constexpr u32 kMaxSamples = 400;
    SampleBank() = default;
    SampleBank(const SampleBank&) = delete;
    SampleBank& operator=(const SampleBank&) = delete;
    ~SampleBank();

    // Bakes everything the Tracks and variants of `music` need at `sampleRate`.
    bool bake(const MusicData& music, u32 sampleRate);
    void release();

    const Sample& sample(i32 id) const { return samples_[id]; }
    u32 count() const { return count_; }
    u64 bytes() const { return bytes_; }
    double bakeSeconds() const { return bakeSeconds_; }

    // Lookups (cheap, audio-thread safe: read-only after bake).
    const DrumKit& kit(u32 track, u32 variant) const { return kits_[track][variant]; }
    i32 clap() const { return clap_; }
    i32 snare() const { return snare_; }
    i32 shaker() const { return shaker_; }
    i32 pulse() const { return pulse_; }
    i32 impact() const { return impact_; }
    i32 riser(u32 track, u32 bars) const;            // -1 when none was baked for that length
    i32 swellHalfBar(u32 track) const { return swellHalf_[track]; }
    i32 swellBar(u32 track) const { return swellBar_[track]; }
    // The nearest baked note of a pitched effect (or the effect itself when unpitched); -1: none.
    i32 sfx(Sfx kind, i32 midi = -1) const;
    // Nova inhale lasting `steps32` thirty-second notes of `track`'s tempo (1..8).
    i32 inhale(u32 track, u32 steps32) const;
    // Shard scale of a Track (minor pentatonic from its tonic, sfxproto's shard run): 10 notes.
    const u8* shardScale(u32 track) const { return shardScale_[track]; }

private:
    i32 add(float* data, u32 frames, u8 channels, i16 midi, u16 tag, const char* name);

    Sample samples_[kMaxSamples];
    u32 count_ = 0;
    u64 bytes_ = 0;
    double bakeSeconds_ = 0.0;
    DrumKit kits_[MusicData::kMaxTracks][MusicData::kMaxVariants];
    i32 clap_ = -1, snare_ = -1, shaker_ = -1, pulse_ = -1, impact_ = -1;
    struct RiserRef {
        u32 track, bars;
        i32 id;
    } risers_[8];
    u32 riserCount_ = 0;
    i32 swellHalf_[MusicData::kMaxTracks] = {-1, -1, -1, -1};
    i32 swellBar_[MusicData::kMaxTracks] = {-1, -1, -1, -1};
    i32 sfxFirst_[kSfxCount] = {};
    u32 sfxCount_[kSfxCount] = {};
    i32 inhale_[MusicData::kMaxTracks][9] = {};
    u8 shardScale_[MusicData::kMaxTracks][10] = {};
};

// Frames in `seconds` the way the prototype counts them (int(seconds * SR)), robust to the float
// rounding of values that are whole numbers of frames.
inline u32 framesFor(double seconds, double sampleRate) {
    const double v = seconds * sampleRate + 1e-3;
    return v > 0.0 ? static_cast<u32>(v) : 0u;
}

}  // namespace rf::audio
