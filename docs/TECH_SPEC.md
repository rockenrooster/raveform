# RAVEFORM — Technical Spec

Companion to `PLAN.md` (decisions, environment, performance contract) and `GAME_DESIGN.md` (terms such
as Track, phrase cycle, Riff, Shard). This describes how the game is built. It fixes architecture and
the constraints that are expensive to get wrong; inside those, implementation choices are yours.
Numbers are initial values. API names are from memory — check them against the pinned headers.

---

## 1. Stack and dependencies

| Library | Use | Licence | Source | Notes |
|---|---|---|---|---|
| SDL 3.4.x | Window, events, touch, gamepad, paths, threads | zlib | github.com/libsdl-org/SDL | Build from source for both targets; Java glue comes from its `android-project` folder |
| Vulkan-Headers | API headers | Apache-2.0 / MIT | github.com/KhronosGroup/Vulkan-Headers | One pinned tag for both platforms (do not mix SDK and NDK headers) |
| volk | Vulkan loader | MIT | github.com/zeux/volk | No link-time dependency on `vulkan-1` |
| Vulkan Memory Allocator | GPU memory | MIT | github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator | |
| Oboe | Android audio device | Apache-2.0 | github.com/google/oboe | Android only |
| Android Frame Pacing (Swappy) | Frame pacing | Apache-2.0 | Maven `androidx.games:games-frame-pacing` (prefab) | Android only |
| ENet | Reliable/unreliable UDP | MIT | github.com/lsalzman/enet | |
| Dear ImGui | Dev overlay | MIT | github.com/ocornut/imgui | Dev builds only |
| stb | PNG writing, Ogg Vorbis decoding | MIT / public domain | github.com/nothings/stb | |
| doctest | Unit tests | MIT | github.com/doctest/doctest | Desktop only |
| Tracy (optional) | Profiler | BSD-3 | github.com/wolfpld/tracy | Dev builds only |

Pin every dependency to an exact tag or commit (CMake `FetchContent` with `GIT_SHALLOW`, or submodules
if the user wants git). The first Gradle build also downloads the Android Gradle Plugin and Maven
artifacts. All of this is downloading: show the user the list with versions and sizes and get a yes
first. Record every library in `docs/LICENSES.md`.

No engine, physics library, scene graph, asset pipeline, font library or scripting language. The game
does not need them.

---

## 2. Build system

- **One CMake tree.** Targets: `raveform_core` (static: everything platform-neutral), `raveform_desktop`
  (exe), `main` (Android shared library loaded by `SDLActivity`), `raveform_headless` (sim, net and audio
  tools without a window), `raveform_tests`.
- **Desktop:** native CMake (`C:\Program Files\CMake\bin\cmake.exe`), Ninja generator, llvm-mingw
  `clang`/`clang++` from PATH. Link statically or copy the runtime DLLs beside the exe. Fallback if a
  dependency misbehaves: the Visual Studio 2026 generator with MSVC.
- **Android:** `android/` Gradle project derived from SDL's template; `gradlew.bat` (wrapper committed);
  `ndkVersion "29.0.14206865"`, CMake `3.22.1`, `abiFilters "arm64-v8a"`, minSdk 29, targetSdk 36.
  SDL3 is a C shared library; all C++ (game, Oboe, ENet, Swappy static) links into the single `libmain.so`,
  so `c++_static` is safe. If a second C++ shared object ever appears, switch to `c++_shared`.
- **Shaders:** GLSL in `shaders/`, compiled at build time with `glslc --target-env=vulkan1.1 -O` and
  embedded in the binary as arrays. Desktop uses the Vulkan SDK's `glslc`; the Android build uses the
  NDK's `shader-tools` copy. Fragment shaders default to `mediump`; positions and UV maths stay `highp`.
- **Flags:** `-std=c++20 -fno-exceptions -fno-rtti -Wall -Wextra`; release `-O2 -fno-math-errno`. No
  global `-ffast-math`. Baseline ARMv8.0 + NEON. Native libraries stay 16 KB page compatible (NDK r29 default).
- **Configurations:** Debug (asserts, validation layers on desktop), Dev (optimized, overlay, light
  asserts — the everyday build and the one benchmarked), Release (no overlay, no logging).
- **Scripts** (`scripts/*.ps1`): `env.ps1` sets the per-process environment from PLAN §3.1 and is
  dot-sourced by the rest; `build-desktop`, `run-desktop`, `build-android`, `deploy-android -Serial`,
  `bench-android -Serial -Scenario`, `capture-android -Serial`, `render-audio`, `test`.

---

## 3. Platform layer

### 3.1 Android

Activity: `RaveformActivity extends SDLActivity`, with small static helpers called through JNI:
haptics, immersive mode, multicast lock, local-network permission request, intent extras →
command-line arguments (override `getArguments()` so `adb shell am start … --es scenario bench_endgame`
reaches `main`).

Manifest and resources that matter:

- `android:appCategory="game"` and `android:isGame="true"`. On Android 16 (the tablet) apps targeting
  API 36 lose orientation and resizability locks on large screens **unless** they are games.
  Re-check this rule before raising targetSdk to 37.
- `screenOrientation="sensorLandscape"`, `configChanges` covering orientation, screen size, keyboard,
  density and UI mode so the activity is never recreated; `layoutInDisplayCutoutMode="shortEdges"`;
  keep-screen-on; sticky immersive full screen.
- `android:enableOnBackInvokedCallback="false"` so the Back gesture still arrives as a key event (→ pause).
- `<uses-feature android:name="android.hardware.vulkan.version" android:version="0x401000" android:required="true"/>`.
- Game-mode config (`game_mode_config`): `allowGameDownscaling="false"`, `allowGameFpsOverride="false"`,
  and declare support for performance and battery modes. At start-up, compare the surface size with the
  display mode and log loudly if they differ.
- Permissions: `INTERNET`, `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE`, `CHANGE_WIFI_MULTICAST_STATE`,
  `VIBRATE`. Before raising targetSdk to 37, add the runtime `ACCESS_LOCAL_NETWORK` request when the
  player opens CO-OP (PLAN §11 links the documentation).

Lifecycle: on background, stop the audio stream, destroy the swapchain and surface, auto-pause a solo
run. On foreground, recreate them. Never render without a surface. Rendering targets survive; only the
swapchain is rebuilt.

Frame rate: request the governor's target with `ANativeWindow_setFrameRate` (API 30+) so the panel
switches mode (the tablet must drop to 60 Hz rather than show 60 fps on a 90 Hz scan-out), and set
Swappy's swap interval to match.

Thermal and scheduler hints are optional and runtime-checked because minSdk is 29: thermal status
(API 30), thermal headroom (API 31), performance hint sessions (API 33).

### 3.2 Desktop

A window that emulates a device: `--res 2400x1080`, `--tier low|med|high|ultra`, `--scenario <name>`,
`--seed`, `--frames N`, `--screenshot <png>`, `--strip <dir> --every K`, `--host`, `--join <ip>`,
`--bot`. Keyboard and mouse map to the controls in GAME_DESIGN §2. Validation layers are on in Debug
and must stay silent.

### 3.3 Input

SDL touch events (normalized coordinates) are converted to pixels and to dp. Events are pumped
immediately before the local player update so input is at most one frame old. Gamepads use SDL's
gamepad API, hot-plug included. Control sizes are in dp.

---

## 4. Time model

Three clocks:

| Clock | Unit | Owner | Used for |
|---|---|---|---|
| Wall | Monotonic nanoseconds | OS | Frame pacing, network time sync |
| Sim | Ticks at **60 Hz**, fixed | Main thread | Enemies, bullets, collisions, director |
| Music | PPQ ticks (96 per quarter: 1/16 = 24, 1/32 = 12, 1/16 triplet = 16) | Audio thread | Everything rhythmic |

- **Fixed step with interpolation.** The sim advances in 1/60 s ticks from an accumulator (at most 3
  ticks per frame; beyond that, drop time rather than spiral). Rendering interpolates entity transforms
  between the previous and current tick. This is mandatory: the test devices run at 60, 90 and 120 Hz.
- **The local player is an input to the sim, not part of it.** The local ship is integrated every
  rendered frame with the real frame time and the freshest input, and the tick reads its pose. Remote
  players are inputs in exactly the same way (§8), which is why co-op needs no rollback.
- **Stateless things render at exact time.** Particles and straight-flying bullets are evaluated
  analytically at render time and need no interpolation.
- **Heard time vs write time.** `writeFrame` is how many audio frames the engine has rendered.
  `heardFrame(t)` is the frame leaving the speaker at wall time `t`, taken from the device timestamp
  (`frame F was presented at time T` → `F + (t − T) × rate`) plus the user's calibration offset.
  `outputLatency = (writeFrame − heardFrame(now)) / rate`. The tempo map (segments of
  `{startFrame, startTick, ticksPerFrame}`, published by the audio thread) converts frames to musical ticks.
  **Gameplay and visuals run on heard time.** The audio thread works on write time.
- **Sub-tick events.** Grid steps rarely coincide with sim ticks. Anything spawned on a step carries
  its exact time and is advanced by the remainder (a bullet starts slightly along its path).
- **Clock source is an interface:** device clock (normal), network-steered clock (co-op clients),
  simulated clock derived from sim time (headless runs, tests, benchmarks). The sim never reads the
  wall clock.

---

## 5. Renderer

### 5.1 Principles

1. **The scene is additive light.** Every scene draw uses additive blending into an HDR target with no
   depth buffer. Order never matters, so there is no sorting and batching is maximal. Darkening effects
   happen in the composite, not in the scene.
2. **Sharp HDR cores, bloom makes the glow.** Lines and sparks are thin and hot. Wide halos come from the
   bloom chain at fractional resolution, never from large blended sprites — fill-rate is the scarce
   resource on phones.
3. **One instance format per primitive, one streamed buffer.** All dynamic instance data goes into a
   persistently mapped per-frame bump buffer. No staging copies, no per-frame allocation.
4. **Everything is created up front.** Pipelines, render passes, targets and descriptor sets are built
   at load; the pipeline cache is saved to disk.
5. **Vulkan 1.1 core.** Classic render passes and framebuffers, fences and binary semaphores. Clear or
   don't-care on load, never load-and-continue. Push constants ≤ 128 bytes, at most 4 descriptor sets.

### 5.2 Frame

Logical size is landscape `W × H` (the Moto: 2400×1080). Targets use `B10G11R11_UFLOAT_PACK32`
(fallback `R16G16B16A16_SFLOAT`) unless noted.

| # | Pass | Size | Content |
|---|---|---|---|
| 0 | Background | W/4 | Cheap procedural field or tinted nebula mask, modulated by the bass envelope. Low tier: none (gradient in the composite) |
| 1 | Distortion | W/8, `R16G16_SFLOAT` | Shockwave rings and gravity lenses drawn as offset sprites. Medium tier and up |
| 2 | **Scene** | **W × H** | Grid, enemy glyphs, bullets, trails, beams, ships, particles, Shards. Additive |
| 3 | Prefilter + afterglow | W/2 | Downsample the scene; `afterglow = max(scene, previous afterglow × decay)` with the previous buffer reprojected by camera motion and zoom |
| 4 | Bloom down | W/4 … | 4–6 levels |
| 5 | Bloom up | … W/2 | Tent upsample, accumulate |
| 6 | **Composite** | **swapchain** | `scene(uv + distortion)` + bloom + afterglow + background → exposure → tone map → vignette → dither → output. Then, in the same pass: threat layer, then HUD |

Only passes 2 and 6 are full resolution.

- **Tone map:** hue-preserving. Compress on the brightest channel, scale RGB by the same factor, then
  bleed toward white only for very hot values, so saturated neon stays saturated and cores go white.
  Exposure and bloom strength take a small beat pump from the kick envelope.
- **Dither** before the 8-bit write (blue-noise tile or interleaved gradient noise). Dark glows band
  badly without it.
- **Chromatic aberration** (High and up): three scene taps offset along the distortion vector plus a
  small global term.
- **Threat layer and HUD** are drawn after tone mapping with ordinary alpha blending: enemy bullets,
  active lasers, lethal trails and telegraphs with a thin dark rim, then HUD and touch controls. They
  are unaffected by bloom, distortion and screen shake (the HUD ignores the world camera entirely).
- **Pre-rotation (Android):** create the swapchain with `preTransform = currentTransform` and, for 90°
  and 270°, the extent swapped to the panel's native orientation. Passes 0–5 stay in logical landscape.
  Only pass 6 knows about rotation: rotate the full-screen UVs and multiply the threat and HUD
  projections by the rotation. Recreate the swapchain on `VK_SUBOPTIMAL_KHR` / `OUT_OF_DATE`.
- **Swapchain:** FIFO; `R8G8B8A8`/`B8G8R8A8`; sRGB by default, Display-P3 as an optional "vivid" mode
  where `VK_EXT_swapchain_colorspace` offers it. Two frames in flight. Present through Swappy on Android.

### 5.3 Primitives

| Primitive | Instance (≈ bytes) | Notes |
|---|---|---|
| **Capsule** | endpoints `p0, p1` (f32×4), radius, intensity (f16×2), colour RGBA8, softness/flags → 32 | The universal dynamic primitive: grid segments, bullets, trails, beams, tethers, HUD strokes, font strokes. Vertex shader expands a quad from `gl_VertexIndex`; fragment shader computes distance to the segment in pixels for the core, anti-aliasing and a small skirt |
| **Glyph** | position (f32×2), rotation as cos/sin, scale, pulse, inner rotation, intensity (f16), colour RGBA8 → 24 | Enemy and ship shapes. Each glyph is a static mesh of line quads in local space, built at load from parametric descriptors (polygon, star, nested, spirograph, Lissajous, chain link…). Every segment carries a layer index so inner and outer layers rotate and pulse independently from two instance values, with no CPU geometry work. One instanced draw per glyph in use. LOD variants with fewer segments |
| **Particle** | spawn position (f32×2), velocity (f16×2), spawn time (f32), life, drag, size, stretch (f16×4), colour RGBA8, intensity, seed → 32 | **Stateless.** Written once into a persistently mapped ring buffer. The vertex shader evaluates `p = p0 + v0·(1 − e^(−drag·t))/drag`, stretches the quad along the current velocity, fades by age, and culls when `t < 0` or `t > life`. Draw the live range (one or two instanced draws). Zero CPU per frame |

Details:

- Line thickness is defined in reference pixels (1 = `H/1080` device pixels) so every device looks the
  same physically. Colours are sRGB bytes times an HDR intensity.
- **Space-warp fields:** up to 8 gravity wells and vortices are passed as uniforms; the particle and
  background shaders displace positions through them, so sparks bend around wells without any state.
- Particles are only spawned for events inside the local view plus a 30% margin. Sizes and streak
  lengths are capped, and a per-frame spawn budget scaled by the governor bounds the worst-case fill.
- Keep particle time origins recent (rebase when the ring is empty); f32 seconds lose precision over hours.
- Shader positions are camera-relative, computed in `highp`, before any `mediump` maths.
- **Grid:** a spring-mass lattice simulated on the CPU only for a window around the local camera
  (toroidal arrays; rows entering the window start at rest). Forces come from an impulse list
  (explosions push, wells pull, kicks ripple). Drawn as capsules, brightness and hue by stretch.
- **Vector font:** an original stroke font (capitals, digits, punctuation) defined as polylines on a
  small grid and drawn with capsules. All in-world and HUD text uses it. No font files.
- **Screenshots:** read back the composite image to PNG on request (flag, overlay button, or intent extra).

### 5.4 Effect tiers and governor

| | Low | Medium | High | Ultra |
|---|---|---|---|---|
| Particle ring / density | 16k / 0.35 | 32k / 0.6 | 64k / 1.0 | 128k / 1.5 |
| Bloom levels / filters | 4 / 4-tap | 5 / 4-tap | 6 / 13-tap prefilter, tent up | 6 / same |
| Afterglow | quarter res or off | half | half | half |
| Distortion / aberration | off / off | 1/8 / off | 1/8 / on | 1/4 / on |
| Background | gradient | quarter-res, simple | quarter-res | half-res |
| Grid spacing | 32 wu | 24 wu | 20 wu | 16 wu |
| Glyph LOD | simplified | full | full | full + inner detail |
| Frame-rate target | 60 | 60 or 90 | panel maximum | panel maximum |

The starting tier comes from a two-second off-screen stress run during the first-launch bake screen
(cached), with the GPU name as a hint.

**Governor** (run time): inputs are GPU pass time (timestamp queries), the 95th percentile frame interval
over the last two seconds, and thermal status. It adjusts, in this order: particle density within the
tier → tier → frame-rate target (120 → 90 → 60). Step down after 0.5 s over budget; step up only after
10 s under 70% of budget. It never touches resolution or gameplay caps. In benchmark runs the tier is
fixed and the governor is off, except in the soak scenario.

---

## 6. Audio engine

### 6.1 Shape

The audio thread is the band. It owns the musical clock and plays everything patterned — the bed, each
player's lead, every Riff — sample-accurately from pattern tables and a small mirror of game state. The
game never sends individual notes for patterned parts; it tells the band what should be playing. Because
gameplay reads the same tables on heard time, sound and picture agree by construction, and a dropped
video frame can never make the music stumble.

```
main thread ── MixState deltas, one-shot SFX requests ──▶ SPSC queue ──▶ audio thread
main thread ◀── clock snapshots, tempo map, beat events ── SPSC queue ◀── audio thread
```

`MixState`: Track theme, section and bars remaining, intensity, per player {fire held, lead pattern
parameters from Mods, active Riffs and levels, Surge}, boss layer state, bus gains, filter and
time-stretch effects.

### 6.2 Device

- 48 kHz stereo float (run at the device's native rate if it differs). Callback-driven.
- Android: Oboe output stream — low-latency performance mode, exclusive sharing with shared fallback,
  usage `Game`. Buffer size starts at two bursts and grows on underruns. Reopen the stream on error
  or route change (headphones, Bluetooth). Stop it in the background. Honour audio focus.
- Windows: an SDL3 audio stream with the same callback contract.
- In the callback: no allocation, locks, logging, file access or JNI. Flush denormals to zero.
  Process in 32-frame inner blocks.

### 6.3 Clock, latency and the scheduling rules

- Each callback publishes `{writeFrame, wall time}`; the device timestamp is polled for
  `{presented frame, time}`. When timestamps are missing (the first moments after start, some routes)
  estimate from buffer size plus a default. Bluetooth timestamps are often wrong — hence the
  calibration slider (one number per device fixes both picture/sound offset and device-to-device offset).
- Invariants:
  1. Every sound starts on a write-grid step, so it is always in time with the music.
  2. Every beat-driven visual triggers on a heard-grid step, so it is always in time with what the
     player hears.
  3. Movement, Dash and Nova are never delayed by audio latency.
- **Patterned parts** (bed, Riffs, enemy attack cues): the audio thread plays them as its write head
  crosses each step; the main thread applies their gameplay effect as heard time crosses the same step.
- **The lead (primary fire):** the audio thread emits a lead note at write step `S` if the player's
  `fire held` flag is set at that instant. The main thread spawns the bullets for heard step `S` if
  fire was held at `heardTime(S) − min(outputLatency, 80 ms)`. On speaker and wired routes these are
  the same instant, so shot and note belong together. On high-latency routes the note trails the
  bullet by a whole number of steps and is still on the grid.
- **One-shots** (pickups, hits, UI): sent as requests, played immediately or quantized to the next
  1/32 as flagged. **Kills are aggregated:** per 1/32 step, one cluster sound per size class whose
  gain and brightness follow the kill count, pitched to a chord tone. Thousands of kills per second
  cost a bounded number of voices.
- **Nova:** an immediate layer on press, the blast scheduled on the next beat (GAME_DESIGN R6).

### 6.4 Sequencer and musical content

- Tempo map with bar-aligned changes; tempo ramps only inside Breaks.
- A **theme** per Track: key, scale, chord progression, drum patterns per section, bass pattern (scale
  degrees), lead arpeggio shape, pad voicing, patch parameters. Themes are data.
- **Bed layers** switch on with director intensity: kick → closed hats → bass → clap/snare → open
  hat/ride → percussion (Euclidean patterns) → pad chords → stab or counter-line. Fills at phrase ends;
  seeded variation so every co-op device generates the same bed.
- **Sections:** Build = accelerating snare roll (1/4 → 1/32), noise riser, high-pass sweep on the bed,
  last bar silent with a reverse swell. Drop = everything plus a sub impact. Break = pads and filtered
  hats under a low-pass.
- **Build-driven layers:** the lead reflects Mods (Split adds harmony notes, Double-Time changes the
  step, Echo feeds the delay, Overcharge adds drive, and so on); each Riff adds its instrument on its
  pattern; a boss adds or loses layers with its health.
- Harmony stays safe by rule: everything melodic is quantized to the theme's scale; leads and Riffs
  favour chord tones and the minor pentatonic; bass plays roots and fifths.

### 6.5 Synthesis and mix

- Voices: drum synth (kick = pitched sine sweep + click; snare and clap = noise + tone; hats = filtered
  noise or metallic oscillators), mono bass (band-limited saw/square + sub, resonant low-pass with
  envelope), pluck/bell/stab leads (band-limited oscillators or simple FM), pads (two or three detuned
  oscillators + chorus), noise risers and impacts, and a sampler for baked effects and any sample-based
  timbres (AI-generated or CC0 one-shots drop in here without code changes).
- Polyphony budget: drums 8, bass 1, pad 4 notes, leads 2 per player, Riff layers 8, sampler 24.
- Sound effects are synthesized once at load into buffers (cached on disk), then played by the sampler
  with pitch, pan and gain. Several variants per effect.
- Buses: drums, bass, music, players, effects → sidechain duck from the kick envelope on bass and
  music (the same envelope drives the visual pump) → tempo-synced delay and a small reverb on sends →
  soft-knee limiter → output.
- Mix targets: integrated loudness −14 ± 2 LUFS, true peak ≤ −1 dBFS. Frequency slots: kick body
  50–100 Hz with a 2–4 kHz click; bass 60–250 Hz, ducked by the kick; pads high-passed near 200 Hz;
  leads 500 Hz–5 kHz; hats above 6 kHz.

### 6.6 Beat events and haptics

The audio thread emits time-stamped events (kick, snare, hat, bass note, lead note, bar, section change)
and envelopes (kick, bass, master). The main thread applies each when heard time reaches its stamp.
Haptics use the same events, rate-limited.

### 6.7 Offline rendering and analysis

`raveform_headless --render-audio` runs the same engine against a simulated device and a scripted
`MixState` timeline and writes a WAV plus an event log. A Python script (numpy + `wave`) checks peak,
RMS, crest factor and that every logged onset sits on its grid position; ffmpeg provides LUFS
(`ebur128`) and a spectrogram PNG (`showspectrumpic`). This is how the executor reviews audio; the WAV
goes to the user for listening.

---

## 7. Simulation

### 7.1 Data

Structure-of-arrays pools with fixed capacity, generation-checked handles, swap-remove, no heap use
after load.

| Pool | Cap | Notes |
|---|---|---|
| Players | 4 | Pose is an input (§4) |
| Enemies | 3072 | Network id assigned by the host |
| Enemy bullets | 2048 | Simulated locally on every device |
| Player bullets | 1024 per player | Owner's copies collide; other players' are cosmetic |
| Shards | 2048 | Local to each device |
| Pickups / structures | 64 / 64 | |

Caps are gameplay rules, identical on every device, and sized so the Moto's tick stays inside 2.5 ms.
When a pool is full the director stops spawning into it; upgrade effects carry their own limits.

### 7.2 Tick (60 Hz)

1. Read player poses and triggers (local and remote).
2. Director (host only): spend the threat budget, emit spawn batches.
3. Enemy thinking, staggered: target choice and neighbour queries every few ticks per entity; steering
   every tick. Enemies more than two screens from every player use a cheap far mode and update every
   fourth tick.
4. Integrate; walls and structures.
5. Rebuild the spatial grid: uniform 64 wu cells over the arena, counting sort (150 × 85 cells).
6. Collisions, all circles and capsules: player bullets × enemies, enemies × players, enemy bullets ×
   players, lethal lines × players, Shards and pickups × players.
7. Deaths and effects → event buffer (consumed by particles, audio requests, score, network).
8. Compact pools.

Behaviour is code per family and per attack (GAME_DESIGN §7), driven by the species table. A plain
`switch` per entity is fine at this scale; restructure only when a profile says so. Musical behaviour
(hop on the beat, telegraph for one beat, fire on beat 1) reads the heard musical time through the
clock interface.

### 7.3 Determinism and the bot

- Same binary + same seed + same inputs → same state hash. No wall clock, no uninitialized memory,
  stable iteration order, seeded PCG streams per system and per entity. Cross-device bit-exactness is
  **not** required (co-op does not depend on it).
- The **bot player** (potential-field dodge, aim at the nearest threat, Nova when surrounded, picks
  upgrades at random or by a simple policy) drives headless soak tests, benchmarks, attract mode,
  director tuning and network tests.

---

## 8. Netcode

### 8.1 Model

Listen server on the LAN, 2–4 peers, phone or PC as host. **Soft sync:** every device simulates the
whole world locally and the host nudges it with events and corrections. Players are authoritative for
themselves. Peers are trusted (friends on one network), which removes the need for lockstep, rollback
or server-side validation — and means the local player always has zero input latency.

| Thing | Authority | Replication |
|---|---|---|
| Player pose, aim, fire flag | Owning client | Unreliable, 30 Hz, last two states for redundancy; shown with ≈ 66 ms interpolation |
| Dash, Nova, upgrades taken, hits taken, cells, down/revive | Owning client ("what you see is what hits you") | Reliable events |
| Player bullets and Riff effects | Owner simulates and resolves hits; others spawn cosmetic replicas from the same tables | Hit claims: reliable, batched per tick (`enemy id, damage`) |
| Enemy spawns | Host director | Reliable batch `{activation tick, id, species, position, seed}`, sent at least two beats early — the warp-in hides the latency |
| Enemy motion | Simulated by everyone; host is the reference | Unreliable corrections chosen by a priority accumulator (near that client, fast, long unsent), ≤ 1200 bytes per packet, 15 Hz per client |
| Enemy health and death | Host | Claims in, death events out (reliable). The claimant predicts the death; a death not confirmed within 300 ms is undone |
| Enemy bullets and attacks | Deterministic from enemy state and song time | None |
| Shards | Local | None |
| Boost orbs, Trios, structures, events | Host spawns; loot is instanced per player | Reliable events |
| Section, Track, theme, seed, song clock | Host | Reliable on change + 4 Hz heartbeat |

Corrections are applied as a per-enemy error offset that decays over about 150 ms; snap when the error
exceeds 96 wu. Enemies target each device's own view of player positions; the differences are small and
are what corrections exist for.

### 8.2 Time and song sync

- Time sync: NTP-style exchange at 4 Hz; keep the minimum-RTT sample of the last 16; clients derive
  host time and steer their sim tick with ±1% dilation, never a jump.
- **Song sync:** the host publishes the tempo map anchored to host time ("tick T0 is *heard* at host
  time H0"). Every device steers its audio so that its own heard tick matches the song tick for the
  current host time, compensating for its own output latency, with a gentle rate correction (±0.3%).
  Re-seek only at start or after a stall, on a bar line. Target: under 10 ms between devices, so phones
  on speakers in one room sound like one system. Offer "music from host only" for rooms where it does not.

### 8.3 Lobby and discovery

- Host broadcasts a beacon on UDP **47777** twice a second (magic, protocol version, host name, player
  count, state); the game runs on UDP **47778** (ENet: channel 0 reliable ordered, 1 unreliable
  sequenced, 2 lobby/control).
- Join by list (beacons seen in the last 3 s), by **room code** (four characters encoding the low 16
  bits of the host's IPv4 address — works within one /16) or by typing an IP address.
- Android: hold a `WifiManager.MulticastLock` while browsing. Works through a router or with one phone
  as a hotspot. On Windows the first host triggers a firewall prompt the user must allow.
- Join handshake checks protocol version and a content hash; a mismatch gives a clear message.
- Parse defensively: validate every length, count and id; drop malformed packets; never allocate from a
  size field.

### 8.4 Budgets and tests

- Per client at endgame: ≤ 250 kbit/s down, ≤ 60 kbit/s up. No IP fragmentation.
- A loopback transport runs host and clients in one process; a network simulator injects delay,
  jitter, loss, duplication and reordering. Soak test: one host and three bots for ten simulated
  minutes, asserting position error, entity-set consistency, bandwidth and song-clock offset.

---

## 9. Performance engineering

Budgets and the definition of the frame-rate contract (P95) are in PLAN §5. How to hit and prove them:

- **Measure where it counts.** CPU phase timers, GPU pass timers (timestamp queries), and
  present-to-present intervals feed a histogram (0.5 ms buckets) reporting P50, P95, P99, P99.9,
  maximum and missed-vsync count. A benchmark run prints one JSON line with the tag `RFBENCH` and saves
  it in app storage. Cross-check now and then against `dumpsys SurfaceFlinger --latency` or a Perfetto trace.
- **Scenarios** (deterministic, scripted, bot-driven, fixed tier): `bench_idle`; `bench_mid` (500
  enemies, moderate effects); `bench_endgame` (pools near their caps, about 700 enemies and 1500
  bullets in view, particle ring saturated, boss and Drop effects, continuous explosions);
  `bench_soak` (endgame loop for 30 minutes with the governor on, for thermals). Until the sim exists
  (M1–M2) these are synthetic load generators with the same entity and effect counts; from M3 they run
  the real game with the bot.
- **Always benchmark warm** (ten minutes in) and on battery-typical conditions; note the thermal
  status and battery temperature with the result.
- **Rules:** additive scene and no sorting; stateless particles; instanced glyphs; one streamed
  buffer; only two full-resolution passes; no allocation, I/O, JNI or logging in the frame; pipelines
  prebuilt; `mediump` fragment maths; work spread over big cores first (do not create more workers
  than big cores minus one before measuring); parallelize only what a profile shows.
- **If a budget is missed**, change the cheapest thing that the profile blames. The fallback order for
  fill-rate is in PLAN §9.

---

## 10. Tooling and verification

- **Overlay** (three-finger tap or F1): frame histogram, CPU and GPU phases, pool counts, tier and
  governor state, audio clock, latency and underruns, network stats, tweakables, screenshot button.
- **Tweakables:** named numbers registered in code, editable in the overlay, dumpable as source.
- **Seeing:** `run-desktop --scenario X --frames N --screenshot out.png`, then open the PNG. For
  motion, `--strip` plus ffmpeg's `tile` filter makes a contact sheet. On device:
  `adb -s <serial> shell screencap -p /sdcard/Download/rf.png` then `adb pull` (avoid redirecting
  binary output in PowerShell).
- **Hearing:** §6.7.
- **Devices:** `deploy-android -Serial <id>`;
  `adb -s <id> shell am start -n dev.raveform.game/.RaveformActivity --es scenario bench_endgame --ei seconds 120`;
  `adb -s <id> logcat -d -s RFBENCH`. Wake the device first (`input keyevent KEYCODE_WAKEUP`).
- **Tests:** unit tests for maths, pools, spatial grid, tempo map, sequencer timing and packet
  parsing; determinism test; headless soak; network soak. `scripts/test.ps1` runs them all.

---

## 11. Persistence

App-private storage (`SDL_GetPrefPath`): settings (plain key=value text), local high scores, lifetime
stats, cached baked audio, pipeline cache, first-launch benchmark result. Write to a temporary file and
rename. Nothing else is stored; nothing leaves the device.

---

## 12. Pitfalls checklist

1. `JAVA_HOME` and `VULKAN_SDK` are stale and `cmake` on PATH is an MSYS2 build (PLAN §3.1).
2. The Moto is Vulkan 1.1.128: no dynamic rendering, sync2, timeline semaphores or descriptor indexing;
   no `VK_EXT_debug_utils`. Compile SPIR-V for `vulkan1.1`.
3. `wideLines` is unsupported everywhere that matters: never rely on `lineWidth`.
4. Pre-rotation: swapped extent, rotation only in the composite pass, recreate on suboptimal.
5. Android 16 large screens ignore orientation locks unless `appCategory="game"`; Back needs the
   opt-out flag; test both on the tablet.
6. Never render without a surface; stop audio in the background; survive 20 pause/resume cycles.
7. Audio timestamps are missing at start and wrong on Bluetooth; keep the fallback and the slider.
8. The audio callback must never block, allocate or log. Flush denormals.
9. Do not tie anything to the refresh rate: the devices are 60, 90 and 120 Hz.
10. The scene pass is additive only — nothing in it may depend on draw order.
11. Keep world positions `highp` and camera-relative; Adreno and Mali differ in `mediump` behaviour.
12. f32 time in shaders loses precision in long runs; rebase time origins.
13. With two devices attached every adb command needs `-s`.
14. Benchmarks from a cold phone lie. Soak first.
15. OEM game services can downscale or cap frame rate; verify the surface size and the achieved rate.
16. Wi-Fi power saving adds jitter; hold the multicast lock while browsing; target API 37 needs the
    local-network permission.
17. One C++ shared library only, or switch the STL to shared.
18. The first Gradle build and every dependency fetch download files: ask first.
19. You cannot watch video or hear audio: frame strips, spectrograms, numbers, and the user.
20. The tablet's Mali driver lacks `fillModeNonSolid`, `shaderClipDistance`, `dualSrcBlend` and
    vertex-stage storage writes; the Moto lacks most post-1.1 extensions. Check support before using
    any optional feature, and test every renderer change on both.
