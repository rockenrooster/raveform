# RAVEFORM — notes for Claude sessions

Read `docs/PLAN.md` first (locked decisions, performance contract, milestones, working rules), then
`docs/PROGRESS.md` (milestone status, benchmark numbers, deviations, open questions). Read
`docs/TECH_SPEC.md` and `docs/GAME_DESIGN.md` sections as the milestone names them. Creative content
(font, palettes, shapes, music data) arrives from the lead under `design/`; code never redesigns it.

## Commands (PowerShell 7, repo root; every script dot-sources `scripts\env.ps1`)

| Task | Command |
|---|---|
| Fetch pinned SDL / volk / Vulkan headers (no-op when present) | `scripts\fetch-deps.ps1` |
| Build desktop (`debug` = Debug + validation, `dev` = RelWithDebInfo) | `scripts\build-desktop.ps1 [-Config debug\|dev] [-Clean]` |
| Build and run desktop | `scripts\run-desktop.ps1 [-Config dev] [-NoBuild] [-AppArgs '<app args>']` |
| Desktop screenshot (+ review copy ≤ 1600 px) | `scripts\run-desktop.ps1 -Screenshot captures\x.png -Res 2400x1080 -Small` |
| Desktop 60 s validation + timing run | `scripts\run-desktop.ps1 -AppArgs '--scenario bench_idle --seconds 60'` |
| Desktop lifecycle test (surface release/restore + resizes) | `scripts\run-desktop.ps1 -AppArgs '--scenario lifecycle_test --cycles 20'` |
| Build APK (`dev` = native RelWithDebInfo, debuggable, debug-signed; `debug` = native Debug; `-Offline` = no network) | `scripts\build-android.ps1 [-Config dev\|debug] [-Offline] [-Clean]` |
| Build, install, launch on one device | `scripts\deploy-android.ps1 -Serial <serial\|moto\|op12r\|tablet> [-NoBuild] [-NoLaunch] [-AppArgs '--tone']` |
| Benchmark on a device (saves `captures\bench\*.json` + `*.load.txt`; 5 s warm-up; samples the phone's CPU load) | `scripts\bench-android.ps1 -Serial <serial> -Scenario bench_idle [-Seconds 60] [-Warmup 5] [-AppArgs '--tone'] [-Deploy] [-NoLoad] [-SfLatency]` |
| Screenshot of the panel (adb screencap; RAVEFORM must be in front) | `scripts\capture-android.ps1 -Serial <serial> [-Small]` |
| Screenshot read back by the app (pre-rotation undone) | `scripts\capture-android.ps1 -Serial <serial> -InApp [-Small]` |
| 20 background/foreground cycles | `scripts\lifecycle-android.ps1 -Serial <serial> [-Cycles 20]` |
| Review copy of any PNG | `python scripts\shrink_png.py <png> [--max-width 1600]` |
| Acceptance still (desktop) + compare | `scripts\run-desktop.ps1 -NoBuild -Res 2400x1080 -Frames 4 -Screenshot captures\m1\frame_reef.png -AppArgs '--scene design/scenes/frame_reef.rfscene --still --bloom on --no-overlay'`, then `python -I design\tools\scenes.py compare captures\m1\frame_reef.png frame_reef` (`look_card` also with `--bloom off` / `--nobloom`) |
| Scene picture or strip (desktop, fixed 1/60 s steps) | `scripts\run-desktop.ps1 -NoBuild -Res 2400x1080 -Frames 600 -Screenshot captures\m1\x.png -AppArgs '--scenario bench_endgame --seconds 600 --warmup 0 --no-overlay [--look haze] [--palette furnace] [--strip 8]'` |
| M1 contract run (Moto) | `scripts\bench-android.ps1 -Serial moto -Scenario bench_endgame -Seconds 120 -Warmup 600 -AppArgs '--tier low' -Deploy` (10-minute soak, then 120 s) |
| GPU cost at the top clock | `scripts\bench-android.ps1 -Serial moto -Scenario bench_endgame -Seconds 15 -Warmup 4 -AppArgs '--tier low --gpu-burn 40' -Deploy` |
| The M1 review set (stills + compare, glyph sheets, frame-600 pictures, both strips, `scenes.py light` lines) | `scripts\review-m1.ps1 [-Out captures\m1]` (writes `captures\m1\` unless `-Out`) |
| Where a frame's light comes from (one class off at a time) | `scripts\light-breakdown.ps1 [-Frame 600] [-Out captures\light]` |
| Unit tests (audio suite: queues, tempo map, sequencer timing, lead rule, variants, music data, no allocation; loudness calibration check; M3 sim suite: grid, pools and handles, Chain and score, lead rule, Nova, species, no allocation; the determinism test) | `scripts\test.ps1` (exit 0 = all passed) |
| Audio match against the prototype: every Track × variant, match table, onset check, listening files | `scripts\render-audio.ps1 [-SkipRef] [-Only track03_club] [-NoListening]` (writes `captures\m2\`, `match_table.md`) |
| Prototype references (musicproto.py as a library; writes nothing under `design/`) | `python -I -B scripts\audio_ref.py captures\m2\ref [track03 [club]]` |
| Loudness calibration (after any music data or DSP change; `test.ps1` fails while it is stale) | `scripts\calibrate-audio.ps1 [-Check]`, then rebuild |
| Offline renders of the band (headless, the same engine) | `build\desktop-dev\raveform_headless.exe --render staged\|mock\|sfx\|leads\|full\|surge [--track 1\|2\|3] [--sound club\|arcade\|velvet] --out x.wav [--pcm16] [--events x.csv] [--meta x.json] [--stems dir] [--random-bursts] [--profile] [--seconds S]` |
| One-shots against the prototype's synthesis | `raveform_headless.exe --dump-samples captures\m2\samples`, then `python -I -B scripts\audio_samples.py captures\m2\samples` |
| Audio scenes (desktop) | `scripts\run-desktop.ps1 -AppArgs '--scenario audio_test'` / `'--scenario sync'` |
| Audio device jobs under the phone lock (deploys the last built APK) | `scripts\audio-device.ps1 -Run bench\|screenshot\|soak [-Seconds 60] [-Scenario bench_idle] [-AppArgs '--band full']` |
| Play the game (M3; the default with no scenario) | `scripts\run-desktop.ps1 -Config dev` (`-AppArgs '--bot'`: the bot plays); on a phone `scripts\deploy-android.ps1 -Serial moto` |
| Simulation, headless (determinism, 10-minute bot soak, stress) | `build\desktop-dev\raveform_headless.exe --sim determinism\|soak\|stress [--minutes M] [--seconds S] [--seed N] [--policy play\|reckless]` (exit 0 = pass; one `RFSIM` JSON line) |
| M3 stress contract run (Moto) | `scripts\bench-android.ps1 -Serial moto -Scenario bench_stress -Seconds 60 -Warmup 60 -AppArgs '--tier low' -Deploy` (`bench_play`: the bot plays the normal game, measured) |
| The M3 review set (scripted strips, bot frames at 30 s / 2 / 5 min, stress) | `scripts\review-m3.ps1 [-Out captures\m3] [-Config debug\|dev] [-SkipStrips] [-SkipFrames]` |

Desktop exit codes: 0 ok, 1 init failed, 2 bad arguments, 3 validation messages, 4 benchmark aborted,
5 screenshot failed. Device serials: Moto `ZY22BXMJG3`, OnePlus 12R `a0e622a5`, Galaxy Tab `R5GL362BEHX`.

## App arguments (desktop argv; Android: the `args` string intent extra)

`--scenario play|bench_play|bench_stress|touch_test|bench_idle|bench_mid|bench_endgame|glyph_sheet|lifecycle_test` (the
first three: the game, below; `bench_mid` / `bench_endgame`:
the scripted M1 stages of `design/scenes.md`; `glyph_sheet --page 1|2|3`: a still), `--tier
low|medium|high|ultra|auto` (effect tier, TECH_SPEC 5.4; without it pictures and tests use high, everything else
the tier the first-launch probe chose for this GPU, cached in `SDL_GetPrefPath()/tier.txt`; `auto` measures
again), `--cuts N` (the first N cuts of look.md §10 on top of the tier), `--reduce-flashing` (look.md §7; key R),
`--governor on|off` (governor v1; default on for interactive stages, off in benchmarks), `--look
signal|laser|haze` (Signal is the game's look; the others are developer switches), `--palette <id>` (default
reef), `--mute <names>` (analysis pictures: `background, grid, heat, enemy, elite, boss, aura, bullet, priority,
sparks, flash, killpool, fx, warp, shard, player, afterglow, distortion, aberration, vignette, pump, sparkles,
pools`; `core/args.h`), `--scene <file.rfscene> --still --bloom on|off` (acceptance stills), `--strip N` (also
capture the N-1 frames before the `--screenshot` frame: `<name>_<k>.png`), `--fixed-step` (1/60 s per frame;
implied by `--screenshot`), `--gpu-burn N` (profiling only: a saturating GPU load before the timed passes,
so they show their cost at the GPU's top clock),
`--seconds N` (measured, after `--warmup S`, default 2),
`--frames N`, `--screenshot <png>` (relative paths go to the app's external files dir on Android),
`--res WxH` (desktop window), `--validation` / `--no-validation`, `--tone` (hold the test tone),
`--no-swappy` (plain FIFO), `--fps HZ` (Android target refresh; default = panel maximum), `--no-overlay`,
`--affinity big|all` (Android: pin the main thread and the threads it starts to the non-little cores, or
leave placement to the scheduler), `--cycles N`. Keys: F1 overlay, F12 screenshot, T tone hold, L next look, P next
palette, K next tier, R reduce flashing, Esc / Back quit. Taps (decided when the last finger lifts): two fingers next look, three
the overlay, four next palette. Raw launch: `adb -s <serial> shell "am start -n dev.raveform.game/.RaveformActivity --es args '--scenario bench_idle --seconds 60'"`.

The game (M3 stage A, `game/play.h`, `src/sim/`): no `--scenario` (or `--scenario play`) starts the fight;
`--scenario touch_test` is M0's touch scene (formerly the default). `bench_play` (the bot plays the normal
game) and `bench_stress` (about 700 enemies and 1,500 enemy bullets around a bot-driven, invulnerable ship)
are measured like the other benchmarks and add a `sim` object to the result (tick time per 60 Hz tick
[p50, p95, max], entity counts, kills, deaths, pool overflows). `--bot` (the bot plays), `--seed N`,
`--fast-forward S` (S seconds of play before the first frame, for pictures), `--picture <name>` with `--strip 8
--screenshot <png>` (scripted review scenes: `fire, kill_small, kill_medium, kill_large, shards, hit, nova,
nova_front, dash, perfect_dash, warp, warp_land, sentry`; the strip starts at the scripted moment). The game
runs on the band's heard clock (Groove held, intensity from the threat on screen) and uses its Track's
palette (Track 1: Ultraviolet) unless `--palette`; pictures run on the simulated clock without the band.
Controls: WASD or arrows move, mouse aims with the left button held to fire, E or the right button Nova,
Space Dash, Enter / E / Space / click retry after death; gamepad left stick, right stick (fires beyond 30%),
RB Nova, LB Dash, A or Start retry; touch: floating sticks (left half moves, right half aims and fires),
NOVA and DASH buttons, tap to retry. With the aim idle the auto-aim assist fires at the nearest enemy on the
half-rate grid. The game starts with the overlay off (F1) and has no tap gestures.

Audio (M2, `src/audio/audio_args.h`): `--scenario audio_test|sync` (the band on heard time with its
controls; the sync calibration screen of `design/screens.md`), `--band off|play|staged|full|stress|metronome`
(default: `play` in audio_test, `metronome` in sync, `full` in every `bench_*` unless pictures are taken, else
off; `full` = the load test, every layer playing: Drop held, four players firing on the 1/16-triplet grid with
one Split harmony, every Riff at level 2, kills every frame; `stress` = Split ×2), `--no-audio` (no band in
any scenario: M0's silent stream, as M1 measured; for comparison), `--track 1|2|3` (default 1; `bench_*`:
the palette's Track, Reef = 3 at the stage's 132 BPM), `--sound club|arcade|velvet`, `--intensity X`,
`--section groove|build|drop|break`, `--hold`, `--fire`, `--haptics on|off` (default on in the audio scenes),
`--sync-offset MS` (this run only). In real time the stages run on the band's heard clock and heard beat
events (`Stage::setBeat`); `--screenshot` / `--fixed-step` runs keep the scripted 132 BPM clock and play no
band unless `--band` asks (the stage still keeps the script), so pictures stay repeatable. Keys in
audio_test: Space (hold) fire, V sound, N next section, H hold, X next Track, Up/Down intensity, 1/2/3 Riffs,
M staged/conductor, C sync screen, G haptics; mouse or finger held anywhere else = fire, on-screen buttons
for all of them. Sync screen: Left/Right ±5 ms, A auto, Enter/Esc/Back done; touch: drag the slider, − / +
(hold repeats), AUTO, DONE. Benchmarks with the band add `audio_engine` to the result (callback wall and
CPU time, load = engine time / callback period, cores, callback heap allocations, voices, latency).

Log tags: `RAVEFORM` (everything), `RFBENCH` (one JSON line per benchmark or test, also on desktop
stdout), `RFLIFE` (background / foreground / first present after resume). On Android the `RFBENCH`
line is a summary (logcat entries stop at 4068 bytes); the full result, with the worst-frame table and
the per-second timeline, is `<scenario>.json` in the app's external files dir, which `bench-android.ps1`
pulls. Reading a result: `interval_ms` is present call to present call on the CPU; `display` is the
panel side (plain FIFO: VK_GOOGLE_display_timing present times; Swappy: Swappy's own statistics, where
`missed` = frames shown one refresh period or more later than asked; GPU-bound frames that take two periods
are not "late" to Swappy, so `bench-android.ps1` also counts present intervals above 1.5 periods);
`phases_ms`, `sched`, `late` and `worst` attribute every interval (`src/tools/frame_probe.h`); `device_load`
is the phone's CPU load. M1 adds `tier`, `look`, `palette`, `gpu_ms` (per pass [p50, p95, max] from
timestamp queries; `total` is work only, `wait` is the gap before the swapchain pass) and `scene_counts`.
GPU times follow the GPU's own frequency governor (it keeps the GPU about 85% busy, and the shell user
cannot read its clock): compare passes at the top clock with `--gpu-burn 40`.

## Conventions

- C++20, no exceptions, no RTTI, `-Wall -Wextra -Werror` on our code. Layout per PLAN §4 (`src/core`,
  `src/gfx`, `src/audio`, `src/platform`, `src/game`, `src/tools`, and since M3 `src/sim`: the simulation,
  a static library without SDL, Vulkan or audio that the game, `raveform_headless` and the tests link).
- The simulation is deterministic (TECH_SPEC 7.3): no wall clock, no unordered iteration, its own random
  streams, no allocation after `World::init`; `World::hash()` digests the state and `scripts\test.ps1` runs
  the determinism test. Gameplay numbers come from design data or the docs; numbers no document gives are
  in `src/sim/tuning.h` (and the view's constants block in `src/game/play_view.cpp`), listed in PROGRESS.md.
- No heap allocation in the steady-state frame: global `operator new` is counted and benchmarks report
  `heap_allocs` for the measured window (must stay 0). Fixed-size arrays, persistently mapped buffers.
- Vulkan 1.1 core only (the Moto is 1.1.128 without debug_utils, sync2, dynamic rendering, timeline
  semaphores). SPIR-V via `glslc --target-env=vulkan1.1`, embedded at build time. No optional features.
- Desktop Debug runs validation + synchronization validation; it must report 0 messages. Loader
  messages are counted separately (`VK_LAYER_PATH` in `env.ps1` hides the stale SDK 1.1.92.1 registry entries).
- Android lifecycle lives in the SDL event watch (`App::eventWatch`): the pump blocks while paused.
  Background destroys the swapchain + surface and closes audio; `RaveformActivity.surfaceDestroyed`
  waits for that. The next `beginFrame` rebuilds them.
- Pre-rotation: `Swapchain::rotationDeg` drives both the clip-space matrix (`renderer.cpp`) and the
  PNG un-rotation (`png.cpp`); change them together.
- Native resolution always: the surface must match the display mode (2400x1080 on the Moto). The log
  says "surface ... matches the display mode"; a "differs" warning is a bug (the M0 Moto runs were at
  2174x1020 until `RaveformActivity` called `setDecorFitsSystemWindows(false)`).
- Design data (`design/look.json`, `look_variants.json`, `palettes.json`, `glyphs.json`, `vector_font.txt`)
  is embedded at build time and read at start-up; numbers are never retyped into code.
- The audio callback never allocates, locks, logs or calls JNI. The music (`design/music/*.json`) is
  embedded like the other design data; the loudness calibration (`src/audio/loudness_calibration.json`)
  carries a hash of it and of `kDspVersion` (bump it when the DSP changes a render's loudness).
- Python prototypes run as libraries with `python -I -B` and never write under `design/`.
- Only phone numbers count for performance; desktop timing is informational.
- Git: `Invoke-RfGit` (Git for Windows + schannel). Commit only at working checkpoints, never push, never
  commit `design/` files you did not write. Build output, `captures/` and fetched `third_party/` are ignored.

## PC CPU limit (user rule)

Anything this project runs on the PC uses at most 4 CPU cores: builds above all, also shader compiles,
offline tools and test runs.

- `scripts\env.ps1` confines its PowerShell process to one logical CPU on each of 4 physical cores (read
  from the processor topology, all in one L3 domain; on the 5950X: logical CPUs 24, 26, 28, 30, mask
  `0x55000000`). Every process it starts inherits that. Run builds, tools and tests through the scripts,
  or dot-source `env.ps1` first. Per process only: no system settings, no other process touched.
- Job counts too: `cmake --build --parallel 4`, Gradle `--max-workers=4`, and a Ninja job pool in
  `CMakeLists.txt` (`RF_MAX_JOBS`), which also holds for the Ninja that the Android Gradle plugin runs.
- Gradle: `build-android.ps1` gives this project's daemon its own immutable JVM options
  (`-XX:ActiveProcessorCount=4`, a project-local `-XX:ErrorFile`), so it never reuses a daemon that another
  session or an older build started without the limit (`-D` options do not separate daemons). Never run
  `gradlew --stop`: other sessions share the Gradle user home.

## Shared test devices

Both phones are used by other Claude sessions at the same time. The device scripts follow these rules;
do the same for any manual adb command.

1. Nothing device-wide: no `adb kill-server` / `start-server`, no reboot, no `adb logcat -c` (read with
   `-d` and filter by tag, time or our PID: `logcat -d --pid=<pid>`), no settings changes (`settings put`,
   `wm size`, rotation locks), and no clear, uninstall or force-stop of any package but `dev.raveform.game`.
2. Before launching, benchmarking, sending key events or taking a screenshot, check the foreground
   (`adb -s <serial> shell dumpsys activity activities | grep -E "mResumedActivity|topResumedActivity"`).
   If another app (neither the launcher nor ours) is in front, someone else is using the phone: wait and
   re-check, try the other phone meanwhile. Never send HOME or start our activity over someone else's app.
3. Keep device runs short and leave the phone tidy: benchmarks exit by themselves; after any other run,
   `adb -s <serial> shell am force-stop dev.raveform.game`.
4. A benchmark is valid only if RAVEFORM kept focus for the whole run. The app logs focus loss and
   background events (`RFLIFE`), aborts on background and reports `"valid": false` on focus loss;
   `bench-android.ps1` retries such runs later. Numbers are taken on shared phones: say so when reporting.
5. Background/foreground cycles send HOME: run them in one short burst only while the launcher or our
   app is in front; if the phone stays busy, skip the check and report it as not verified.
6. Every adb command needs `-s <serial>`.
7. Between this project's coders (lead's rule, 2026-11-11): before using a phone write
   `captures\locks\<serial>.lock` holding your name (e.g. `m1`) and the time, and delete it when the run ends;
   a lock under 30 minutes old with another name means the phone is busy (refresh yours on runs past 30
   minutes). Another coder installs its own `dev.raveform.game`: deploy your APK right before every device run
   (`-Deploy`) and check which build ran before debugging a strange result.
