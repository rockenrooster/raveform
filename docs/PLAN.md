# RAVEFORM — Master Plan

Status: decisions approved by the user on 2026-11-10. Written by the planning session for a different
executor (Claude Opus 5.5). Nothing is built yet — only the three documents in this folder exist.

| Doc | Purpose | When to read |
|---|---|---|
| `PLAN.md` (this) | Decisions, verified environment, performance contract, milestones, working rules | Start of every session |
| `GAME_DESIGN.md` | What the game is: loop, rhythm rules, upgrades, enemies, arena, co-op, UX | Sections named by the milestone |
| `TECH_SPEC.md` | How it is built: stack, renderer, audio, sim, netcode, perf, tooling | Sections named by the milestone |
| `PROGRESS.md` | Create in M0. Milestone status, benchmark numbers per device, deviations, open questions | Start of every session; update at the end |

---

## 0. Executor briefing

1. **You cannot hear audio or see motion.** Verify with numbers, screenshots and frame strips. The user is
   the eyes, ears and thumbs. Never report that something "sounds", "looks" or "feels" good unless the user
   said so; report what you measured and what still needs their judgment.
2. **Specs are starting points, the contract is not.** Numbers in the two spec docs are initial values —
   measurement and playtest feedback override them. Log each deviation in `PROGRESS.md` with the reason.
   The locked decisions in §2 and the performance contract in §5 change only with the user's explicit OK.
3. **De-risk first.** The two things that can sink this project are fill-rate on the floor phone and the
   quality of generated music. M1 and M2 exist to prove both early. Do not build content on top of an
   unproven renderer or audio engine.
4. **Originality.** RAVEFORM is an original neon twin-stick arena shooter. Geometry Wars, PewPew Live and
   Pulsebound are genre and feel references only. Do not reuse their names, enemy designs, level layouts,
   UI, sounds or music, and do not rebuild any one of them under a new name.
5. **Permissions.** Downloading anything (dependencies, assets, tools) needs the user's explicit yes in
   the current session: list name, source, version, license and size first. Do not change system
   environment variables or system settings; set environment per process in scripts. Passwords,
   keystores and store accounts are the user's to handle.
6. **The user's standing rules** (from their CLAUDE.md files): keep chat replies short; prefer `rg`; small
   focused changes; no destructive git or shell commands unless asked; do not revert unrelated changes;
   make a manual backup before major or breaking changes (use `D:\Stuff\DevFolder\_backups\Raveform\`);
   run builds and tests when needed to verify; measure before optimizing; UI must be readable,
   space-efficient and have clear high-contrast pressed states. Commit only when the user asks.

---

## 1. What we are building

RAVEFORM is a neon vector twin-stick shooter for Android in which **sound becomes light**. One Endless
mode, solo or LAN co-op for 2–4 players. The music is generated live by the game and by each player's
loadout; every shot, spawn, explosion and screen pulse lands on the beat. Runs start sparse and readable
and escalate into a screen-filling light storm with thousands of enemies — at 60+ FPS P95 at the
device's native resolution.

Pillars, in priority order:

1. **Sound becomes light.** One musical clock drives audio, gameplay cadence and every visual pulse.
2. **Instant fun.** Playing within five seconds of launch. Two thumbs (or one, with auto-aim). No menus
   mid-run. No grind: every run starts identical.
3. **Escalating spectacle that never drops a frame.** Performance is a feature, measured every milestone.
4. **Readable chaos.** Threats are always legible, however loud the screen gets.
5. **Hundreds of enemies, dozens of toys.** Variety comes from composable systems, not hand-built one-offs.

---

## 2. Locked decisions

| Topic | Decision | Reason |
|---|---|---|
| Engine | Custom C++20 engine: Vulkan 1.1 core renderer, SDL3 platform layer, Oboe audio on Android, ENet networking | Best frame pacing and audio control, small APK, fully code-driven so the executor can build, run, profile and screenshot it |
| Targets | Android arm64 (minSdk 29) plus a Windows dev build that is also a full LAN peer | Fast desktop iteration; PC can host or join for multiplayer tests |
| Performance | 60+ FPS P95 at native resolution on the floor device (Motorola One 5G Ace); stronger devices get more effects and higher refresh. Effects scale, resolution never does | User requirement (P95 since 2026-11-10; originally P99) |
| Music | Generative and build-driven: in-engine sequencer, synth and sampler; weapons and upgrades add layers | User choice; matches the Pulsebound-style feel they asked for |
| Permanence | Run-only. Upgrades last until the run ends. No cross-run power and no unlock gating. Only settings, high scores and stats persist | User choice |
| Name | RAVEFORM | User choice. A quick web search found no game or app with this name (not a trademark clearance) |
| Multiplayer | LAN co-op, 2–4 players, inside the single Endless mode | User choice |
| Assets | Procedural first; AI-generated assets allowed; CC0/permissively licensed free assets allowed. Everything non-procedural is logged with provenance | User direction |

---

## 3. Verified environment (checked 2026-11-10)

### 3.1 Development PC (Windows 11 Pro, PowerShell primary)

| Item | Fact |
|---|---|
| CPU / GPU | Ryzen 9 5950X (16c/32t) / Radeon RX 9070 XT, Vulkan 1.4.359 driver; monitor 2560×1440 @ 60 Hz |
| Android SDK | `C:\Users\Tyler\AppData\Local\Android\Sdk` (`ANDROID_HOME` is set correctly) |
| NDK | r27 (27.0.12077973, 27.2.12479018), r28 (28.2.13676358), **r29 (29.0.14206865) — use this** |
| SDK platforms / build-tools | android-29, 30, 33, 35, 36, 37.0 / up to 36.0.0 |
| SDK CMake | 3.22.1 (with its own `ninja.exe`) — used by Gradle's externalNativeBuild |
| Native CMake | `C:\Program Files\CMake\bin\cmake.exe` 4.4.4 — **use this for desktop builds** |
| Compilers | llvm-mingw clang 22.1.7 on PATH (target `x86_64-w64-windows-gnu`); Visual Studio Community 2026 (18.11) at `C:\Program Files\Microsoft Visual Studio\18\Community` |
| Ninja / Git / Python | ninja 1.13.2; git 2.54.0; Python 3.14.10 with `numpy` and `PIL` (no scipy, matplotlib or soundfile) |
| JDK | `C:\Program Files\Eclipse Adoptium\jdk-17.0.21.9-hotspot` (has `javac`) |
| Gradle | Not on PATH. Wrapper distributions already cached in `~\.gradle\wrapper\dists`: 8.11.1, 8.13, 9.2.1, 9.4.1, 9.8.0, 9.8.2 |
| Vulkan SDK | `C:\VulkanSDK\1.4.363.0` (`glslc`, validation layers, `vulkaninfoSDK`) |
| Shader compiler for Android builds | `…\ndk\29.0.14206865\shader-tools\windows-x86_64\glslc.exe` |
| ffmpeg | 8.0.1 (full build) on PATH — use for spectrograms, loudness, frame strips |
| Not installed | Android Studio, Unreal, Unity, Godot |

Environment traps — handle these in the build scripts, per process, without touching system settings:

- `JAVA_HOME` is **stale** (points at `jdk-17.0.20.8-hotspot`, which no longer exists). Scripts must set
  `JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-17.0.21.9-hotspot`. `java` on PATH is a JRE 21, not a JDK.
- `VULKAN_SDK` is **stale** (points at `C:\VulkanSDK\1.1.92.1`). Scripts must set it to `C:\VulkanSDK\1.4.363.0`.
- `cmake` on PATH resolves to an MSYS2 build (`c:\devkitPro\msys2\usr\bin\cmake.exe`). Never use it; call
  the native CMake by full path.
- `git` on PATH is also the MSYS2 build. It works, but pass Windows paths carefully.
- The working folder `D:\Stuff\DevFolder\Android_Games` is not a git repository. Ask the user in M0 whether
  to `git init` inside `Raveform\`.

### 3.2 Test devices

| | Floor phone | Tablet | High-tier phone | PC |
|---|---|---|---|---|
| Device | Motorola One 5G Ace | Samsung Galaxy Tab A11+ (SM-X230) | OnePlus 12R | Windows dev build |
| adb serial | `ZY22BXMJG3` | `R5GL362BEHX` | not connected over USB | — |
| SoC / GPU | Snapdragon 750G / Adreno 619 | MediaTek MT8775 / Mali-G615 MC2 | Snapdragon 8 Gen 2 / Adreno 740 * | RX 9070 XT |
| CPU max freq | 2×2.2 GHz + 6×1.8 GHz | 4×2.5 GHz + 4×2.0 GHz | — | — |
| RAM | 6 GB | 6 GB | 8 or 16 GB * | — |
| Display | 1080×2400, **60 Hz only** | 1200×1920 (16:10), **90 / 60 Hz** | 1264×2780, up to **120 Hz** * | windowed |
| Android | 11 (API 30) | 16 (API 36) | 14 or newer * | — |
| Vulkan | **1.1.128**, driver from July 2021 | 1.3.247 | 1.3 * | 1.4 |
| `VK_EXT_debug_utils` | **absent** (only `VK_EXT_debug_report`) | present | — | present |
| Wide colour swapchain | P3 available | sRGB only | expected * | — |
| Audio output | 48 kHz, 240-frame bursts (5 ms), fast mixer ≈ 23 ms; no `audio.low_latency` feature flag | 48 kHz, 1024-frame HAL buffer; `audio.low_latency` declared | — | — |
| Page size | 4 KB | 4 KB | — | — |

\* From public specifications, not measured. Confirm when the device is available. The 12R is the 120 Hz
reference; benchmark it through the in-app benchmark screen (results shown on screen) or wireless adb if
the user enables it.

Facts that shape the code (measured on both USB devices unless noted):

- `wideLines` is unsupported and `lineWidthRange` is `[1, 1]` → all lines are quad-expanded capsules.
- `B10G11R11_UFLOAT_PACK32` is colour-attachment + blend + linear-filter capable → use it for HDR targets
  (fallback `R16G16B16A16_SFLOAT`, also supported).
- `R16G16_SFLOAT` and `R8G8_SNORM` are renderable → distortion buffer.
- GPU timestamp queries work (`timestampValidBits` 48 and 64).
- `multiDrawIndirect`, `drawIndirectFirstInstance`, `VK_GOOGLE_display_timing`, `VK_KHR_incremental_present`
  and `VK_EXT_swapchain_colorspace` exist on both.
- The tablet's Mali driver lacks `fillModeNonSolid`, `shaderClipDistance`, `dualSrcBlend` and
  vertex-stage storage writes. Enable no optional feature the renderer does not strictly need.
- The Moto has **no** dynamic rendering, synchronization2, timeline semaphores, descriptor indexing or
  imageless framebuffers. The renderer is Vulkan **1.1 core**: classic render passes and framebuffers,
  fences and binary semaphores, SPIR-V 1.3 (`glslc --target-env=vulkan1.1`).
- The three devices run at 60, 90 and 120 Hz → the sim tick is decoupled from display rate and render
  interpolation is mandatory from the start (TECH_SPEC §4).
- With two or more devices attached, every adb command needs `-s <serial>`.

---

## 4. Architecture at a glance

```
            ┌────────────── main thread ───────────────┐      ┌── audio thread (real-time) ──┐
 input ───▶ │ events → net recv → sim ticks (60 Hz)    │ cmds │ sequencer → synth/sampler →  │
            │   → extract (interpolated) → Vulkan      │ ───▶ │ buses → sidechain → limiter  │──▶ device
            │   record → present (Swappy)              │ ◀─── │ clock snapshots, beat events │
            └──────────────┬───────────────────────────┘      └──────────────────────────────┘
                           │ parallel_for                      lock-free SPSC queues both ways
                    ┌──────▼──────┐
                    │ worker pool │  grid springs, steering chunks, extraction culling
                    └─────────────┘
```

| Layer | Choice |
|---|---|
| Language | C++20, clang on both platforms, no exceptions, no RTTI |
| Platform | SDL 3.4.x: window, events, touch, gamepad, file paths. Android via `SDLActivity` |
| Renderer | Vulkan 1.1 core, `volk` loader, VMA allocator; GLSL → SPIR-V at build time, embedded in the binary |
| Frame pacing | Android Frame Pacing library (Swappy) on Android; FIFO on desktop |
| Audio | Own sequencer, synth, sampler and mixer (float32, 48 kHz). Device: Oboe on Android, SDL3 audio on Windows |
| Networking | ENet over UDP, custom replication; UDP broadcast discovery plus room code / manual IP |
| Dev tooling | Dear ImGui overlay, doctest, optional Tracy, headless runner, benchmark scenarios, screenshot capture |
| Build | One CMake tree; Gradle project (from SDL's Android template) for the APK; PowerShell scripts wrap both |

Repository layout (create in M0):

```
Raveform/
  CLAUDE.md            build/run/test commands and conventions (write in M0, once the commands are real)
  docs/                PLAN.md, GAME_DESIGN.md, TECH_SPEC.md, PROGRESS.md, LICENSES.md
  CMakeLists.txt  cmake/
  src/core/            math, rng, arenas, containers, jobs, log, profile, tweakables
  src/platform/        app loop, window, input, files, haptics, thermal (android/ and desktop/)
  src/gfx/             device, swapchain, passes, capsule/glyph/particle renderers, post, vector font
  src/audio/           device, clock, sequencer, synth, sampler, fx, sfx bake, beat events
  src/sim/             world state, pools, spatial grid, behaviours, weapons, collisions, director
  src/net/             transport, discovery, protocol, replication, lobby, netsim
  src/game/            states (title/lobby/run/gameover), HUD, camera, juice, settings, save
  src/content/         species, glyphs, upgrades, themes, patterns, palettes (data tables)
  src/tools/           headless runner, bench, audio render, bot player
  shaders/  assets/  android/  third_party/  tests/  scripts/
```

---

## 5. Performance contract

**Definition.** "60+ FPS P95" means: over a 120-second `bench_endgame` run started after a 10-minute
warm soak, the 95th percentile present-to-present interval is ≤ 16.9 ms, at most 5% of frames miss
vsync, and no frame exceeds 50 ms. (The user changed the percentile from the 99th to the 95th on
2026-11-10 and confirmed that the missed-vsync allowance moves from 1% to 5% with it.) The interval is
measured at the panel: when each frame actually reached the display, counted in whole refresh periods
because the panel only changes at a vsync. The time at which the present call returns jitters by more
than the margin even when no frame is late, so it is reported but not graded (confirmed by the user on
2026-11-10). P99 and the maximum are still measured and reported. Resolution is always the panel's native resolution: the scene target,
composite and UI are rendered at native size. Soft, low-frequency effects (bloom, afterglow, distortion,
background) may use fractional-resolution buffers because that is visually equivalent.

| Device | Refresh target | Required | Effects tier expected |
|---|---|---|---|
| Moto One 5G Ace | 60 Hz | P95 ≤ 16.9 ms | Low–Medium |
| Galaxy Tab A11+ | 90 Hz when the governor can hold it, else the panel is switched to 60 Hz | P95 ≤ 11.3 ms at 90, or ≤ 16.9 ms at 60 | Medium |
| OnePlus 12R | 120 Hz | P95 ≤ 8.6 ms | High–Ultra |

Budgets on the floor phone at endgame (initial; refine in M1):

| Thread | Budget | Split |
|---|---|---|
| Main (CPU) | ≤ 8 ms P95 | input+net 0.3, sim tick 2.5, extraction 1.5, command recording 1.0, rest slack |
| GPU | ≤ 12 ms P95 | background 0.4, distortion 0.2, scene 4.0, prefilter+afterglow 1.0, bloom 1.6, composite+UI 2.5 |
| Audio callback | ≤ 25% of the burst period P99, zero underruns | |
| Memory / APK | < 300 MB RSS / < 40 MB | |
| Startup | cold start to title < 3 s; tap to playing < 1 s | |

Rules that make the contract achievable (details in TECH_SPEC §5 and §9):

- Gameplay entity caps are global and sized for the floor phone's CPU. Past the caps, difficulty grows
  through tougher enemies, not more of them. Cosmetic density (particles, bloom quality, grid resolution)
  is per-device and governed at run time.
- The scene is 100% additive light: no sorting, a handful of draw calls, one streamed instance buffer.
- Particles are stateless (analytic motion in the vertex shader): zero CPU per frame.
- No heap allocation, file I/O, JNI chatter or shader compilation during gameplay.
- Every milestone ends with the benchmark suite on both USB devices; numbers go in `PROGRESS.md`.
  A P95 regression above 10% blocks the milestone.

---

## 6. Asset strategy

The art direction (glowing vector lines, particles, procedural backgrounds) and the generative audio
mean the game needs almost no traditional assets. Sources, in order of preference:

1. **Procedural (default).** Shapes, particles, grid, backgrounds, UI and the vector font are generated in
   code. Drums, bass, leads, pads, risers and all sound effects are synthesized (baked at load, cached).
2. **AI-generated.** Use where synthesis falls short or art is needed:
   - Audio: drum and synth one-shots, multi-sampled stabs, risers/impacts/sweeps, textures, optional
     announcer callouts ("SURGE", "NEW TRACK", boss names) and optional full tracks for a later playlist mode.
   - Images: app icon, logo, store art; optional greyscale nebula masks for the background layer
     (tinted by palette at run time — one texture fetch is cheaper than procedural noise).
   - Workflow: the executor writes a request sheet per batch (count, duration, BPM, key, format, loudness,
     suggested prompts) → the user generates with their tools and drops files in `assets/inbox/` → the
     executor validates (ffprobe, peak/LUFS, tempo and key check, loop points), converts, integrates and
     logs it. The executor may also write generators (code that renders assets) — that counts as procedural.
   - Rules: only generators whose terms allow commercial use of outputs on the user's plan; never prompt
     with artist names, song titles or existing game IP; keep tool, plan, date and prompt in the log.
3. **Free assets.** CC0 preferred; CC-BY allowed with attribution; nothing with unclear, non-commercial or
   share-alike terms. Candidates to check at intake: Kenney audio packs, freesound (CC0 filter),
   OpenGameArt (CC0 filter), HoliznaCC0 on Free Music Archive. Fonts, if ever needed, must be SIL OFL.

Every non-procedural file gets a row in `docs/LICENSES.md`: path, origin (URL or generator), author or
tool, licence, date, notes. Third-party libraries are listed there too. An in-game credits screen is
generated from the same data in M7.

---

## 7. Milestones

Sizes are relative (S/M/L/XL). Each milestone ends with: benchmark suite on both USB devices, an update
to `PROGRESS.md`, a short note to the user saying what to try, and the user checkpoint. While waiting for
feedback, continue with work that does not depend on it.

### M0 — Bootstrap (M)

Read: PLAN §3–4, TECH_SPEC §1–4, §10, §12.

Build:
- Ask the user to approve the dependency list (TECH_SPEC §1) and whether to `git init`.
- Repo skeleton, CMake tree, PowerShell scripts that set the per-process environment from §3.1.
- Desktop: SDL3 window, Vulkan 1.1 device with validation, clear + one capsule test draw, frame loop,
  ImGui overlay, `--screenshot` and `--frames` flags.
- Android: Gradle project from SDL's template (arm64 only, minSdk 29, targetSdk 36, NDK r29), Vulkan
  surface with pre-rotation, Swappy pacing, Oboe stream playing a test tone on tap, touch points drawn,
  immersive landscape, lifecycle handling.
- Frame-time histogram, `bench_idle`, log output tagged `RFBENCH`.
- Write `CLAUDE.md` (real commands only) and create `PROGRESS.md` and `LICENSES.md`.

Exit:
- [ ] One command builds and runs the desktop app; one command builds, installs and launches on a given serial.
- [ ] Desktop: 60 s run with zero validation messages; screenshot PNG written and inspected.
- [ ] Moto and tablet: `bench_idle` holds the panel rate (60 / 90) with P95 inside the contract.
- [ ] Background → foreground 20 times, rotate 180°, lock/unlock: no crash, no leak, swapchain recovers.
- [ ] Test tone plays on both devices with zero underruns over 60 s (the user confirms it is audible).

### M1 — Renderer and performance proof (L) — go/no-go gate

Read: TECH_SPEC §5, §9; GAME_DESIGN §11.

Build: HDR scene target, capsule pipeline, instanced glyphs with per-layer animation, stateless
particle ring, warping grid, bloom chain, afterglow, distortion buffer, composite (tone map, dither,
vignette, chromatic aberration), background layer, threat layer, vector font, camera, effect tiers and
governor v1, GPU pass timers, and the synthetic scenes `bench_mid` and `bench_endgame`.

Exit:
- [ ] `bench_endgame` meets the contract on the Moto (60 Hz) and on the tablet (90 or 60 Hz).
- [ ] If it does not at the Low tier: apply the fallbacks in §9 in order and re-measure. If it still
      fails, stop and bring the measured options to the user. Do not quietly lower resolution.
- [ ] Desktop validation clean; screenshots of both scenes at phone resolution inspected for banding,
      aliasing, clipping and legibility of threats.
- [ ] 12R result recorded when the user can run the in-app benchmark.

User checkpoint: does it look spectacular on the phones? Pick between two or three palette and glow
variants.

### M2 — Audio engine and beat clock (L) — second gate

Read: TECH_SPEC §4, §6; GAME_DESIGN §3, §4, §11.

Build: device layer (Oboe / SDL), audio clock from device timestamps, sequencer with tempo map, drum
synth, bass, lead, pad, sampler and baked sound effects, buses with sidechain, limiter, reverb and
delay, phrase-cycle state machine (driven from the debug overlay for now), beat-event stream driving
visual pulses, offline renderer, latency calibration screen, haptic pulses, and music themes for
Tracks 1 and 2.

Exit:
- [ ] Offline render of three minutes of Track 1: true peak ≤ −1 dBFS, integrated loudness −14 ± 2 LUFS,
      every scheduled onset within one sample of its grid position, energy present in every band of the
      spectrogram (checked numerically and by inspecting the PNG).
- [ ] On both devices: callback CPU inside budget, zero underruns over 10 minutes at full polyphony.
- [ ] Visual metronome and click are in sync on speaker after automatic compensation (user confirms);
      the manual offset slider fixes Bluetooth.

User checkpoint: listen to rendered WAVs of each section and to the live build. Does Track 1 groove?
Choose between two or three patch variants. If two rounds of iteration do not satisfy, switch timbres to
AI-generated or CC0 samples (§6); if that still fails, propose stem-based music and ask.

### M3 — Vertical slice (L)

Read: GAME_DESIGN §2–9, §12; TECH_SPEC §7.

Build: sim core and pools, player controller (touch sticks, gamepad, keyboard+mouse on desktop),
auto-aim assist, primary weapon on the beat grid, 12 species from 6 families, collisions, shards and
chain, shield cells, Nova, Dash, director v1 with the phrase cycle, upgrade Trio at each Break
(8 Mods, 3 Riffs, 4 Boosts), HUD, title → run → game over → retry, first-pass juice, arena walls and
camera, edge indicators, Tracks 1–2 with one boss, and the bot player (headless and attract mode).

Exit:
- [ ] A full solo loop is playable on both devices; the real game at the 10-minute mark meets the contract.
- [ ] Cold start to playing ≤ 5 s on the Moto.
- [ ] Determinism test passes (same seed and inputs → same state hash twice).
- [ ] Bot player runs 30 simulated minutes headless without crash, leak or pool overflow.

User checkpoint: is it fun in the first 30 seconds and at minute 10? Controls, difficulty ramp, upgrade
pacing.

### M4 — LAN co-op (L)

Read: TECH_SPEC §8; GAME_DESIGN §10.

Build: transport, discovery and lobby UI, room code and manual IP join, clock and song sync,
replication as specified, co-op rules (instanced loot, revive, scaling), net overlay, network
condition simulator, loopback soak tests, the local-network permission flow, and the PC peer.

Exit:
- [ ] PC host + Moto + tablet (+ bot or 12R) complete a 15-minute run: no disconnects, bandwidth inside
      budget, on-screen enemy position error versus host < 12 world units RMS, song clock offset
      between peers < 10 ms, every device inside the contract.
- [ ] With 5% loss and 80 ms jitter injected: playable, no stalls, corrections stay smooth.
- [ ] Join works by discovery and by room code; host quit and client drop are handled cleanly.

User checkpoint: a real co-op session in one room. Impression of the phones' music playing together.

### M5 — Content scale-up (XL, one batch)

**One batch, everything** (the user, 2026-11-12: "all of the rest of the content should go in the
next batch. All of it"). The next build the user plays has the whole of GAME_DESIGN §13's last row:
ten Tracks, ten bosses, twelve mini-bosses, sixteen families and their 240 species, the elite traits,
36 Mods, 12 Riffs, 16 Boosts, 8 Fusions, the structures and the events. The batches below are now
the order of work inside that one delivery, each still closed with its own checks, pictures and
commits; nothing is handed to the user to play until all of it is in.

Read: GAME_DESIGN §6–9, §13.

Build in batches of roughly 40 species each: the full genome system, 240+ species, 12 elite traits,
12 mini-bosses, 6 bosses; Mods to 36, Riffs to 12, Boosts to 16, Fusions to 8; Tracks to 10 (theme,
palette, hazards) plus the endless remix rule; arena structures and events; director tuning with
offline simulation charts.

Order inside M5 (the user after playing the co-op build of 2026-11-12: it is fun, and it needs more
variety in Tracks, bosses and mini-bosses). What a player notices first comes first: every batch
opens with its bosses, its mini-bosses and its new Tracks, and is handed over to be played as soon
as those work; species, upgrades and structures fill in behind them. Batch 1 gives each of the three
Tracks its own boss (Kaleidos, Longwave, The Choir) and the first four mini-bosses. Batch 2 adds
Tracks 4 and 5 with Event Horizon, Polyrhythm and four more mini-bosses. Batch 3 adds Track 6,
Feedback and the last four mini-bosses. Batch 4 (added when the user asked for ten Tracks) adds
Tracks 7 to 10 with four new bosses. Counts: GAME_DESIGN §13.

Exit (for each part as it closes, and for the whole):
- [ ] Contact-sheet screenshot shows every new species has a distinct, readable silhouette.
- [ ] Bot harness spawns and kills every species; no perf regression on either device.
- [ ] Director intensity chart for a 30-minute simulated run reviewed.

- [ ] The careful bot plays the ten Tracks through; GAME_DESIGN §8's targets for the ten-Track run are
      reported (met or not, with the numbers).
- [ ] The Moto meets the contract in every Track's densest part and in every boss and mini-boss fight.

User checkpoint: one play test of the whole thing.

### M6 — Polish and experience (L)

Onboarding hints, full settings, accessibility (reduce flashing, colour-blind palettes, UI scale),
haptics pass, game-over stats and local high scores, second juice pass, audio mix pass with the user,
90/120 Hz polish, wide-colour output where available, optional announcer callouts, app icon and name
screen. Exit: user sign-off on feel, plus the contract on all three devices.

### M7 — Hardening and release prep (M)

60-minute soak and thermal tuning, sanitizer runs on desktop, crash-safe saves, APK size and stripping,
credits/licences screen, release build variant, and the Play-readiness checklist (target API, 16 KB
pages, no data collection, content rating, photosensitivity notice). Exit: final benchmark matrix on
every device recorded in `PROGRESS.md`.

---

## 8. Working agreements

- **Verification loops.** Visuals: run a scenario on desktop with `--screenshot`, then open the PNG; on
  device use `adb exec-out screencap`. Motion: capture every Nth frame and tile them into a strip with
  ffmpeg. Audio: render offline to WAV, analyse with numpy and ffmpeg, hand the WAV to the user.
  Performance: only numbers from the phones count.
- **Desktop first, device always.** Develop against the Windows build with validation layers on; run on
  both USB devices before calling anything done. The Moto's 2021 Adreno driver and the tablet's Mali
  driver disagree with desktop drivers more often than you expect.
- **Dev tooling is part of the product.** The overlay, benchmark scenarios, bot player, offline audio
  renderer and network simulator are what make an executor that cannot see or hear effective. Build them
  early and keep them working.
- **Content is data.** Species, upgrades, patterns, themes and palettes live in data tables, separate
  from logic. Tunable numbers are registered as tweakables editable in the overlay.
- **User-facing steps.** Tell the user plainly when you need them: approve downloads, allow the Windows
  firewall prompt for the desktop build, accept install or debugging prompts on a device, listen, play.
- **Checkpoints are real.** Do not mark a milestone complete without the user checkpoint. Report failures
  with the numbers.

---

## 9. Risks and fallbacks

| # | Risk | Level | Mitigation / fallback order |
|---|---|---|---|
| 1 | Fill-rate on Adreno 619 at 1080×2400 | High | M1 gate. Fallbacks: (a) fewer bloom levels and 4-tap filters; (b) afterglow at quarter res or off; (c) coarser grid, glyph LOD; (d) composite without distortion or aberration; (e) LDR direct-to-swapchain scene with a low-res re-draw for bloom. Resolution stays native |
| 2 | Generated music not good enough; executor cannot hear | High | M2 gate with user listening; conservative harmony and proven patterns; sample-based timbres (AI-generated or CC0); last resort stem-based music |
| 3 | Audio latency varies by device and route (Bluetooth) | Medium | Clock from device timestamps plus a per-device calibration slider; rules in TECH_SPEC §6.3 |
| 4 | Several phones in one room playing slightly out of sync | Medium | Network song clock + per-device latency compensation; target < 15 ms; "music from host only" option; headphones unaffected |
| 5 | Netcode for thousands of entities | Medium | Soft-sync design (local simulation + corrections), loopback soak tests, PC peer, network simulator |
| 6 | Old Adreno driver (Vulkan 1.1.128, 2021) bugs | Medium | Vulkan 1.1 core only, validation-clean, test on device every milestone, no exotic features |
| 7 | Thermal throttling in long runs | Medium | Governor uses thermal status; soak benchmark; frame-rate target steps down before effects collapse |
| 8 | Scope: hundreds of enemies, dozens of upgrades | High | Compositional systems, batches with contact sheets, bot harness, quality bar per batch |
| 9 | Chaos hides threats | Medium | Threat layer drawn after tone mapping, reserved danger colour and shape, dimmed particles near the player |
| 10 | Photosensitivity | Safety | First-launch notice, "reduce flashing" mode that clamps full-screen luminance changes to ≤ 3 per second |
| 11 | Swappy and SDL3 do not cooperate | Low–Med | Fall back to FIFO presentation plus `VK_GOOGLE_display_timing` |
| 12 | Wi-Fi blocks broadcast or isolates clients | Low | Room code and manual IP; phone hotspot as the network |
| 13 | OEM game modes downscale or cap frame rate | Low–Med | Game-mode config opting out of downscaling and FPS override; verify actual surface size at start-up |

---

## 10. Assumed defaults (the user can change any of these)

- Distribution: sideloaded APK for now, kept ready for the Play Store. No ads, purchases, analytics,
  accounts or internet use beyond the LAN.
- `applicationId` `dev.raveform.game` (pick a final one before any store upload).
- Landscape only (both landscape orientations). English only, with strings in a table.
- Auto-aim assist on by default, so the game is playable with one thumb. Dash is in the first slice and
  is cut if playtests show it adds confusion.
- Joining a co-op run happens in the lobby only; the host leaving ends the run (no host migration in v1).
- Stretch goals, not scheduled: true HDR display output, a playlist mode for pre-made or user tracks with
  beat detection, host migration, up to 8 players, iOS.

---

## 11. Sources used while planning

- Pulsebound (rhythm reference): https://playpulsebound.com/ and its `/guide` and `/faq` pages
- Android local network permission (enforced for apps targeting API 37+ on Android 17):
  https://developer.android.com/privacy-and-security/local-network-permission
- Android Frame Pacing library: https://developer.android.com/games/sdk/frame-pacing
- Android 16 large-screen orientation rules and the exemption for games:
  https://developer.android.com/about/versions/16/behavior-changes-16
- SDL releases (search results showed 3.4.14 as the newest in August 2026 — check for the current one):
  https://github.com/libsdl-org/SDL/releases

Library versions and API signatures in the specs come from the planner's knowledge. Pin the versions
that are current when you start, and check signatures against the pinned headers rather than this text.
