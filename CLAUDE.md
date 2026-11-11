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
| Build APK (`dev` = native RelWithDebInfo, debuggable, debug-signed; `debug` = native Debug) | `scripts\build-android.ps1 [-Config dev\|debug]` |
| Build, install, launch on one device | `scripts\deploy-android.ps1 -Serial <serial\|moto\|op12r\|tablet> [-NoBuild] [-NoLaunch] [-AppArgs '--tone']` |
| Benchmark on a device (saves `captures\bench\*.json`) | `scripts\bench-android.ps1 -Serial <serial> -Scenario bench_idle [-Seconds 60] [-AppArgs '--tone'] [-Deploy]` |
| Screenshot of the panel (adb screencap; RAVEFORM must be in front) | `scripts\capture-android.ps1 -Serial <serial> [-Small]` |
| Screenshot read back by the app (pre-rotation undone) | `scripts\capture-android.ps1 -Serial <serial> -InApp [-Small]` |
| 20 background/foreground cycles | `scripts\lifecycle-android.ps1 -Serial <serial> [-Cycles 20]` |
| Review copy of any PNG | `python scripts\shrink_png.py <png> [--max-width 1600]` |

Desktop exit codes: 0 ok, 1 init failed, 2 bad arguments, 3 validation messages, 4 benchmark aborted,
5 screenshot failed. Device serials: Moto `ZY22BXMJG3`, OnePlus 12R `a0e622a5`, Galaxy Tab `R5GL362BEHX`.

## App arguments (desktop argv; Android: the `args` string intent extra)

`--scenario bench_idle|lifecycle_test`, `--seconds N` (measured, after `--warmup S`, default 2),
`--frames N`, `--screenshot <png>` (relative paths go to the app's external files dir on Android),
`--res WxH` (desktop window), `--validation` / `--no-validation`, `--tone` (hold the test tone),
`--no-swappy` (plain FIFO), `--fps HZ` (Android target refresh; default = panel maximum), `--no-overlay`,
`--cycles N`. Keys: F1 overlay, F12 screenshot, T tone hold, Esc / Back quit. Three-finger tap toggles
the overlay. Raw launch: `adb -s <serial> shell "am start -n dev.raveform.game/.RaveformActivity --es args '--scenario bench_idle --seconds 60'"`.

Log tags: `RAVEFORM` (everything), `RFBENCH` (one JSON line per benchmark or test, also on desktop
stdout), `RFLIFE` (background / foreground / first present after resume).

## Conventions

- C++20, no exceptions, no RTTI, `-Wall -Wextra -Werror` on our code. Layout per PLAN §4 (`src/core`,
  `src/gfx`, `src/audio`, `src/platform`, `src/game`, `src/tools`).
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
- The audio callback never allocates, locks, logs or calls JNI.
- Only phone numbers count for performance; desktop timing is informational.
- Git: `Invoke-RfGit` (Git for Windows + schannel). Commit only at working checkpoints, never push, never
  commit `design/` files you did not write. Build output, `captures/` and fetched `third_party/` are ignored.

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
