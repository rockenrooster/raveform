# RAVEFORM — M2 progress (audio engine and beat clock)

Branch `m2-audio` (worktree `Raveform-m2`), cut from `main` at `4ef7086`. Written by the M2 audio coder;
kept separate from `docs/PROGRESS.md` so the merge with M1 is mechanical. All device numbers come from
the **shared** Moto One 5G Ace (`ZY22BXMJG3`), taken under the phone lock, at the media volume already set.

## Status (2026-11-11)

| Done criterion (brief / PLAN §7 M2) | Status | Evidence |
|---|---|---|
| Offline match with the prototype, Track 1, Track 2, three variants | **Met** | Table below; `captures/m2/match_table.md`, `captures/m2/eng/*.match.json` |
| Every onset within one sample of its grid position | **Met** (0 samples, ~1250 onsets per render) | `scripts/audio_check.py onsets`; unit test for the conductor through the Build's 85% bar |
| True peak ≤ −1 dBFS, loudness −14 ± 2 LUFS | **Met** (TP −5.2 to −6.0; −14.00) | Same |
| Moto: callback within 25% of the burst period at P95, every layer playing | **Met, narrowly**: P95 23.7% | `captures/bench/ZY22BXMJG3-bench_idle-20261111-180352.json` |
| Moto: zero underruns over 10 minutes | See "Device" below (10-minute run) | |
| Moto: frame numbers with the full band show no cost | **Met** (0 missed on the panel, main thread +0.09 ms) | Two bench files below |
| Calibration screen works on desktop and Moto, flash drawn from heard time with the edited offset | **Built, screenshots taken**; "flash and click coincide" needs the user | `captures/m2/desktop_sync.png`, `captures/m2/moto_sync.png` |
| Desktop Debug: 0 validation messages with the audio scenes; 0 heap allocations in the steady frame and in the callback | **Met** | `run-desktop.ps1 -AppArgs '--scenario bench_idle --seconds 10 --warmup 2 --band full'`: validation 0, `heap_allocs` 0, other threads 0, `callback_heap_allocs` 0 |
| Unit tests for tempo map, sequencer timing, queues, run by a script | **Met** (11 tests, 2089 checks) | `scripts\test.ps1` |
| Renders for the user | **Done** | `captures/m2/*.wav` and `*.mp3` (list at the end) |

## How it is built

| Piece | Where | What |
|---|---|---|
| Music data | `src/audio/music_data.*` | Embeds and parses `design/music/{patches,variants,conductor,track01,track02}.json` (CMake, like the other design data). Resolves every Track × variant in the prototype's order: patches.json, then the variant, then the Track. conductor.json: structured numbers read directly; the numbers it states in sentences ("Level rises 0.35 -> 1.0", "85% speed", "steps 0, 6 and 12", ...) are extracted from those sentences with a check of how many there are, so an edit that changes a sentence's shape fails at start-up instead of being misread |
| One-shots | `src/audio/bake.*`, `fft.*` | Drum kit per Track × variant, clap, snare, shaker, pulse, impact, risers, swells and every sound effect of `sfxproto.py`, synthesised at load with the prototype's recipes (0.66 s on the desktop, 19.8 MB). Pitched effects are baked on the chord tones the Tracks use; others are played at a rate from the nearest |
| Engine (audio thread) | `src/audio/engine.*`, `dsp.h`, `reverb.*`, `tempo_map.*`, `spsc.h` | Tempo map (96 PPQ, segments start on exact ticks), sequencer scanning the grid every 2 ticks, three modes: **Staged** (the Track's `arrangement`, exactly the prototype's logic), **Conductor** (phrase cycle Groove/Build/Drop/Break with the Short/Standard forms, bed by intensity with hysteresis, fills, Build rules incl. the 85% last bar, Drop impact and Surge, Break echo, Riffs, four players' leads with Split/Double-Time/Pierce/Overcharge/On-Beat Crit/Prism/Hyper, Track change through an 8-bar Break with a tempo ramp and the key on bar 5), **Metronome** (the sync screen). Voices: P1–P4 leads, bass, stabs, pads, sampler; buses, sidechain, ping-pong delay, FDN reverb, section gain, master soft clip, output gain, effects ducks, limiter. Commands in / beat events out through lock-free SPSC rings. No allocation, lock, log, file or JNI in `render()`; denormals flushed by the device layer |
| Device layer | `src/audio/audio_device*.{h,cpp}` | Oboe: presentation timestamps (`getTimestamp(CLOCK_MONOTONIC)`), stream generations, reopen after a disconnect (route change) **or any other stream error** (retry every 0.5 s), underruns and buffer growth as before. SDL: no timestamps; a buffer estimate |
| Band (main thread) | `src/audio/band.*`, `platform_audio.*`, `audio_args.*` | Owns music data, bank, engine; the device callback publishes `{engine frame, stream frame, wall time}` (seqlock). **Heard time** for the frame being drawn: the device timestamp (else the buffer estimate + 10 ms), at the moment the frame reaches the panel (2 refresh periods ahead), minus the sync offset; slewed 10% per frame, snapped beyond 25 ms. Releases beat events at heard time, mirrors the tempo map, computes the lead rule's shots (TECH_SPEC 6.3), haptics (light tick on heard kicks and clicks, heavy on the Drop impact, ≥ 120 ms apart, through `RaveformActivity.haptic`, `performHapticFeedback` so the system setting applies), Bluetooth detection (`AudioDeviceInfo` type after each open), sync offset saved in `SDL_GetPrefPath()/audio_sync.txt` |
| Scenes | `src/game/audio_scene.*` | `--scenario audio_test` and `--scenario sync` (design/screens.md) |
| Offline renderer | `src/tools/audio_render.cpp` → `raveform_headless` | Same engine against a simulated device (fixed or random bursts) and scripted MixState timelines; WAV + event log + stems; loudness calibration; dev seed search |
| Tests | `tests/audio_tests.cpp` → `raveform_tests` | Queues (incl. two-thread stress), seqlock, tempo map (constant, changes, capacity, 126 BPM rounding = prototype), staged render bit-identical with 192-frame and random 1–1000-frame bursts, every onset on the grid, conductor grid through the 85% bar, lead follows fire, variant on the bar line, no allocation in any mode, music data (override order, conductor numbers, calibration current) |

## Match against the prototype (staged arrangement of each Track, `scripts\render-audio.ps1`)

Prototype: `design/tools/musicproto.py` run as a library by `scripts/audio_ref.py` (references in
`captures/m2/ref/`). Engine: `raveform_headless --render staged`. Same measurement code for both
(`scripts/audio_metrics.py`, which calls musicproto's `lufs` and `band_report`). Band and section
columns: the largest absolute difference over the six bands / the eight sections.

| Render | LUFS engine | LUFS prototype | True peak dBFS | Worst band diff dB | Worst section diff dB | Onsets checked | Worst onset error (samples) | Verdict |
|---|---|---|---|---|---|---|---|---|
| track01_club | -14.00 | -14.00 | -5.19 | 0.18 | 0.58 | 1226 | 0 | pass |
| track01_arcade | -14.00 | -14.00 | -5.20 | 0.19 | 0.60 | 1226 | 0 | pass |
| track01_velvet | -14.00 | -14.00 | -5.91 | 0.49 | 0.38 | 1226 | 0 | pass |
| track02_club | -14.00 | -14.00 | -5.58 | 0.42 | 0.28 | 1254 | 0 | pass |
| track02_arcade | -14.00 | -14.00 | -5.59 | 0.39 | 0.36 | 1254 | 0 | pass |
| track02_velvet | -14.00 | -14.00 | -5.70 | 0.31 | 0.32 | 1254 | 0 | pass |
- Loudness is equal by construction (the output gain is calibrated on the engine's own render, as the
  prototype normalises its own); the figure that shows the match is the loudness **before** that gain:
  engine −8.96 / −8.92 / −8.25 / −8.52 / −8.53 / −8.41 LUFS against the prototype's −8.94 / −8.90 /
  −8.33 / −8.43 / −8.54 / −8.34 (within 0.09 LU).
- The worst section is always the Break (−0.2 to −0.6 dB): the pad stem is 0.7 dB below the prototype's,
  and the pad is the Break's main voice. Most of it is the oscillators' random start phases (the
  detuned saw pairs beat): the prototype's own pad moves by 0.58 dB between phase draws (six draws:
  −19.14 to −19.73 dB), and numpy's draw cannot be reproduced.
- Effect sheet and lead timbres (`sfxproto.py`'s renders, run the same way): engine −15.23 LUFS / −4.37
  dBFS peak vs −15.2 / −4.4; −14.03 / −2.69 vs −14.0 / −2.6. Gameplay mock: −12.94 LUFS, true peak −1.71
  (the prototype's own mock, a different 16-bar script: −12.9 / −2.4).
- One-shots against the prototype's synthesis, sample by sample (`scripts/audio_samples.py`):
  deterministic ones are identical (shards, spawns, bells, interface sounds, game over, enemy shot);
  noise-based ones agree in their band statistics, within the spread of the noise draw (±1–2 dB in a
  band for short bursts). Stems (`--stems`): bass, stab, pulse, snare, lead, drums within 0.15 dB RMS;
  pad −0.68 dB; reverb return +0.26 dB (FDN, below).

## Where the engine differs from the prototype, and why

1. **Reverb.** The prototype convolves with 1.82 s of decaying noise per channel (FFT). Real time: two
   mono feedback delay networks (8 prime-length lines, Hadamard feedback, per-line losses for the exact
   RT60) after the same input mix (0.6 mono + 0.4 channel), pre-delay and first-order 320 Hz / 7 kHz band
   limits, with four allpass diffusers whose output is also tapped for a dense onset. The output gain
   is measured at load so the impulse response has unit energy, as the prototype normalises its IR.
   Measured: the reverb return is 0.9 dB above the prototype's on this music. The effects' reverb send
   (sfxproto: a second reverb after the music) joins the music reverb, divided by the master's present
   gain: one reverb instead of two (CPU).
2. **Zero-phase FFT filters on running buses** (pad high-pass 220 Hz, Break hats low-pass 5 kHz, master
   high-pass 30 Hz) → causal Butterworth biquads: the same magnitude, a different phase. Baked one-shots
   keep the exact zero-phase filters (Bluestein FFT for any length on deterministic signals; spectral
   synthesis for noise).
3. **Noise** comes from the engine's own generator (numpy's cannot be reproduced exactly), so noise
   one-shots match statistically, not sample by sample. The prototype's kick click is a quiet draw (its
   2.5–6 kHz band three standard deviations below the average draw); the engine's click seed was chosen
   (`raveform_headless --seed-search kick`) to give the same click spectrum (all six kicks now within
   about ±0.7 dB per band, centroids within 6 Hz).
4. **Riser**: the twelve band noises are synthesised frame by frame in the frequency domain with the
   exact band shapes (4096-point frames, sqrt-Hann overlap-add), instead of 24 FFT filters as long as the
   Build; same statistics, centroid 4492 vs 4433 Hz.
5. **Loudness normalisation**: a fixed output gain per Track × variant (`src/audio/loudness_calibration.json`,
   written by `scripts\calibrate-audio.ps1`, embedded; a hash of the music data and the DSP version
   flags it stale at start-up and in `scripts\test.ps1`).
6. **Control rate**: filter coefficients and envelopes are exact every 32 samples of each voice's age
   and interpolated linearly between (coefficients, not cutoff), so nothing is divided per sample and the
   output does not depend on the device's callback sizes (tested bit for bit).
7. **Polyphony**: each player's lead holds at most 12 notes; the oldest then fades out in 5 ms instead of
   being cut (the prototype's notes overlap freely: about three at 1/16, six at 1/32).
8. **Output stage**: after the music's soft clip and output gain, a soft knee (linear to 0.66, ceiling
   0.8) keeps music plus effects below −1 dBFS true peak; the music alone peaks near −5 dBFS and passes
   untouched.
9. Conductor rules where the staged arrangement and conductor.json disagree, conductor mode follows
   conductor.json: Surge moves the lead one grid step (1/16 → 1/16 triplet; the staged "double" lead is
   1/32), the Build keeps the bed as it stood (clap included), Riffs rest in the Break. The Build's last
   bar runs at 85%: the riser is stretched to cover it and the swell starts so it ends on the downbeat.
   Kill aggregation: small kills as the prototype's mock (`1.6 × (1 − e^(−n/2.5))`), medium and large so
   that one kill plays at its LEVEL, more up to +4 dB. A Shard run restarts after a 0.5 s pause.

## Device (Moto One 5G Ace, shared phone, Swappy 60 Hz, `bench_idle`)

| Run | Panel missed | CPU-side P95 / max ms | Main thread CPU P95 | Underruns | Engine time per callback P50 / P95 / max | Load P95 (time / callback length) | Callback cores | Result file |
|---|---|---|---|---|---|---|---|---|
| no band (`--band off`, 15 s) | 0% | 17.78 / 19.51 | 1.69 ms | 0 | – | – | – | `ZY22BXMJG3-bench_idle-20261111-175755.json` |
| full band (`--band full`, 30 s) | 0% | 17.93 / 20.31 | 1.78 ms | 0 | 0.90 / 1.15 / 1.29 ms (CPU time P95 1.13) | **0.237** | big cores 6 and 7 | `ZY22BXMJG3-bench_idle-20261111-180352.json` |

- "Every layer playing" (`--band full`): the Drop held (the whole bed: kick, clap, hat 16ths, open hat,
  drop bass, shaker, pad), every Riff at level 2 (stab, pulse every beat, shaker), four players firing
  P1–P4 on the 1/16-triplet grid with one Split harmony each, kills and Shards every frame. Peak 72
  voices, no steals. Burst 240 frames (5 ms), buffer 480, AAudio shared (not MMAP). Callback wall time
  equals its thread CPU time (no preemption). `--band stress` doubles the harmony (Split ×2): the
  first build of the engine measured P95 0.30 there; not re-measured.
- Main thread per frame (`Band::update`): 25 µs P50, 31 µs P95. Heard-clock latency 32–35 ms, from the
  device's timestamps.
- Heap allocations on the main thread in the window: 60 in 30 s with the band, 30 in 15 s without (the
  same 2 per second): the overlay's `AudioDevice::stats()` (Oboe) allocates; the band caches what it needs
  at stream open and adds none. 0 in the callback.

## Shared files touched (all additive, in `// M2 audio begin/end` blocks unless noted)

| File | Change | Why |
|---|---|---|
| `CMakeLists.txt` | Embed the music JSON and the calibration; `raveform_audio` static library; link it into `raveform_core`; band and scene sources; desktop `raveform_headless` and `raveform_tests` | The engine without SDL for the headless tools and tests |
| `src/core/args.h` | `#include "audio/audio_args.h"`; `AudioArgs audio;` in `AppArgs` | Command-line switches of the band |
| `src/core/args.cpp` | One `else if (isAudioArg(arg))` branch before the final `else` | Parsed in `audio/audio_args.cpp` |
| `src/platform/app.h` | Includes; `Band band_; AudioScene audioScene_; bool openAudio();`; **`Content::Audio` added to the enum (one modified line)** | The band and the audio scenes |
| `src/platform/app.cpp` | `band_.init` + scene init before the device opens; `openAudio()` replaces the two `audio_.open(TestTone::render, &tone_)` calls (**two modified lines**); key and touch hooks; `band_.update()` and the scene in `frame()`; `band_.resetMeasurements()` at the bench's measure start; `band_.writeJson(j)` after the `audio` object; one overlay line; **the unknown-scenario check also accepts `Content::Audio` (one modified line)** | Integration |
| `android/.../RaveformActivity.java` | Imports; `haptic(int)` and `audioDeviceType(int)` | Haptics and Bluetooth detection |
| `src/audio/audio_device*.{h,cpp}` | Timestamps, generations, reopen on any error | (M0 files in `src/audio/`, now owned by M2) |
| New scripts | `audio-common.ps1`, `audio-device.ps1`, `calibrate-audio.ps1`, `render-audio.ps1`, `test.ps1`, `audio_ref.py`, `audio_metrics.py`, `audio_check.py`, `audio_samples.py` | — |

Not touched: `docs/PROGRESS.md`, `CLAUDE.md`, `src/gfx/`, `shaders/`, `src/game/stage.*`, `src/game/lattice.*`, `design/`.

## For the stage after the merge (the M1 stage keeps its scripted clock until then)

The stage reads heard time from `band_.view()` (`BeatView`, `src/audio/band.h`). The lines:

```cpp
// stage.h (Stage): the band's heard clock, when it plays
const BeatView* beat_ = nullptr;
void setBeat(const BeatView* v) { beat_ = v; }
float beatLen() const { return beat_ && beat_->running ? static_cast<float>(60.0 / beat_->bpm) : 60.0f / 132.0f; }

// stage.cpp, Stage::advance(): the song clock is heard time; sixteenths come from heard ticks
if (beat_ && beat_->running) songT_ = static_cast<float>(beat_->songSeconds); else songT_ += dt;
...
if (beat_ && beat_->running) {
    while (static_cast<double>(nextSixteenth_) * 24.0 <= beat_->heardTick) onSixteenth(nextSixteenth_++);
} else {
    while (static_cast<float>(nextSixteenth_) * sixteenth <= songT_) onSixteenth(nextSixteenth_++);
}

// stage.cpp, Stage::envelopes(): the heard events drive the pumps
if (beat_ && beat_->running) {
    kick_ = beat_->envelope(audio::BeatKind::Kick, look_->kickDecayMs * 1e-3f);
    snare_ = std::fmax(beat_->envelope(audio::BeatKind::Clap, look_->snareDecayMs * 1e-3f),
                       beat_->envelope(audio::BeatKind::Snare, look_->snareDecayMs * 1e-3f));
    hat_ = beat_->envelope(audio::BeatKind::Hat, look_->hatDecayMs * 1e-3f);
    bass_ = beat_->envelope(audio::BeatKind::Bass, 0.18f);
    lastKickT_ = songT_ - beat_->since(audio::BeatKind::Kick);
    return;
}

// app.cpp, App::init(), after band_.init(...) and stage_.init(...):
stage_.setBeat(&band_.view());   // and run the stage with --band play (or make play the default)
```

Other heard data the game can use: `view().events` / `eventCount` (this frame's heard events: kind,
note, velocity, bar, section), `bar`, `beatInBar`, `beatPhase`, `section`, `sectionBar`, `sectionBars`,
and `shots` (TECH_SPEC 6.3: the local player's heard lead steps with fire held, each with its age, to
spawn bullets slightly along their path). Fire, intensity, sections, Riffs, mods and effects are set
through `Band` (`setFire`, `setIntensity`, `nextSection`, `toggleRiff`, `playSfx`; the engine's
`Command`s for the rest).

## Commands

| Task | Command |
|---|---|
| Unit tests + calibration check | `scripts\test.ps1` |
| Match table, onset check, listening files | `scripts\render-audio.ps1 [-SkipRef] [-Only track01_club] [-NoListening]` |
| Prototype references | `python -I -B scripts\audio_ref.py captures\m2\ref [track01 club]` (writes nothing under `design/`) |
| Loudness calibration (after music data or DSP changes) | `scripts\calibrate-audio.ps1` then rebuild |
| One-shots vs the prototype's synthesis | `build\desktop-dev\raveform_headless.exe --dump-samples captures\m2\samples`, then `python -I -B scripts\audio_samples.py captures\m2\samples` |
| Engine profile per stage | `build\desktop-dev\raveform_headless.exe --render full --seconds 30 --profile` |
| Desktop scenes | `scripts\run-desktop.ps1 -AppArgs '--scenario audio_test'` / `'--scenario sync'` |
| Device jobs under the phone lock | `scripts\audio-device.ps1 -Run bench [-Seconds 30] [-AppArgs '--band full']`, `-Run screenshot -AppArgs '--scenario sync --no-overlay'`, `-Run soak -Seconds 600` |

App switches: `--scenario audio_test|sync`, `--band off|play|staged|full|stress|metronome`, `--track 1|2`,
`--sound club|arcade|velvet`, `--intensity X`, `--section groove|build|drop|break`, `--hold`, `--fire`,
`--haptics on|off`, `--sync-offset MS`. audio_test keys: Space (hold) fire, V sound, N next section, H
hold, X next Track, Up/Down intensity, 1/2/3 Riffs, M staged/conductor, C sync screen, G haptics; mouse
or finger held anywhere else = fire; on-screen buttons for all of them. Sync screen: Left/Right ±5 ms, A
auto, Enter/Esc/Back done; touch: drag the slider, − / + (hold repeats 4/s), AUTO, DONE.

## Not done, not verified, open

- Needs the user: that it grooves; that flash and click coincide on a speaker (the automatic part is
  the device timestamp plus a display lead of two refresh periods); that Bluetooth can be corrected with
  the slider (the Bluetooth sentence appears when the output device is Bluetooth; not tested, no
  Bluetooth device here); whether haptics on kicks feel right.
- The pad stem is 0.7 dB below the prototype's (mostly the start phases, above): inside the match.
- The callback load at P95 (23.7%) leaves little margin under 25%; the next step would be processing
  lead voices in pairs or four at a time (they are latency-bound per sample).
- Bake time on the Moto not measured separately (0.66 s on the desktop); the app starts the band
  before the device opens.
- 12R and tablet not attached: no numbers.
- Settings screen (sound variant, haptics strength, volumes) comes later; variants switch on the next bar
  line from the key, the tap and `--sound`.
- Reduce-flashing mode for the sync screen (disc at 0.2, no expanding ring) is not wired (M1 owns the
  setting).

## Files for the lead (absolute paths)

`D:\Stuff\DevFolder\Android_Games\Raveform-m2\captures\m2\` — `track01_club.mp3`, `track01_arcade.mp3`,
`track01_velvet.mp3`, `track02_club.mp3`, `sfx_sheet.mp3`, `lead_timbres.mp3`, `gameplay_mock.mp3`
(each also as a 16-bit `.wav`), `match_table.md`, screenshots `desktop_audio_test_clean.png`,
`desktop_sync.png`, `moto_sync.png`, `moto_audio_test.png`.
