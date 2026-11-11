# RAVEFORM — Progress

## M0 Bootstrap — status 2026-11-10: IN PROGRESS (stopped at the usage limit)

All device numbers were taken on **shared phones**: other sessions launched other apps on both phones
during this work (see CLAUDE.md "Shared test devices"). Results in `captures/bench/*.json`.

### Done and verified
- Desktop build + run (`scripts\build-desktop.ps1`, `run-desktop.ps1`): static llvm-mingw exe.
- Desktop 60 s `bench_idle`, Debug, validation + sync validation: **0 validation messages, 0 loader
  messages, 0 heap allocations** in the window. `lifecycle_test` 20 cycles (10 surface release/restore,
  20 swapchain rebuilds): 0 messages.
- Desktop screenshot 2400x1080 written and inspected (`captures/desktop_2400x1080.small.png`).
- Android APK builds (`build-android.ps1`), installs and launches (`deploy-android.ps1`) on Moto and 12R.
- Pre-rotation on the Moto: swapchain 1080x2400 `rotate90`, logical 2400x1080; adb screencap shows the
  overlay upright and readable.
- Swappy active on both phones (VK_GOOGLE_display_timing enabled); Oboe AAudio 48 kHz low latency.
- Two unplanned pauses (another app took the screen) on Moto and 12R: surface released in 39-47 ms via
  the surfaceDestroyed hand-shake, resumed and presented again.

### Benchmarks (bench_idle, 60 s unless noted; interval ms after present returns)
| Device | Surface | Pacing | P50 | P99 | Max | Missed | Underruns | Notes |
|---|---|---|---|---|---|---|---|---|
| Moto, run A | 2400x1080 | Swappy 60 Hz | 16.70 | **19.52** | 41.7 | 22 (0.61%) | 0 | cpu P99 1.69 |
| Moto, run C | 2400x1080 | Swappy 60 Hz | 16.72 | **35.13** | 44.8 | 118 (3.3%) | 0 | SurfaceFlinger cross-check: P50 16.70, P99 33.24, 124 missed; cpu P99 4.58 |
| Moto, 15 s | 2400x1080 | plain FIFO | 16.69 | 29.29 | 47.1 | 10 (1.1%) | 0 | 0 heap allocations |
| 12R (old build, validity unknown) | 2376x1080 | Swappy, panel stayed **60 Hz** | 16.58 | 18.01 | 34.0 | n/a | 0 | Adreno 740, Vulkan 1.3.128, 4 images |

Contract (Moto P99 ≤ 16.9 ms) is **not met** in these shared conditions; the SurfaceFlinger data shows
real missed vsyncs, not only measurement jitter. Cause not isolated (other sessions' load vs. pacing).

### Done but unverified
- 12R 120 Hz: `ANativeWindow_setFrameRate` vote added after the 60 Hz result; not yet run.
- Focus-loss validity flag in the bench JSON (`valid`, `interruptions`) and the shared-device scripts.
- `lifecycle-android.ps1` (20 HOME/am start cycles) and `capture-android.ps1 -InApp` on device: not run.
- 60 s held test tone on the phones (`--tone`): not run (0 underruns measured without the tone).

### Not started / open
- Tablet (`R5GL362BEHX`) not attached. Rotation 180° and lock/unlock need the user. Audibility needs the user.
- Swappy allocates ~5 times per frame inside `SwappyVk_queuePresent` (plus ~1800/min on other threads); our code: 0.

### Deviations
- `c++_shared` (Oboe prefab), no VMA, no ImGui (lead's direction). Network permissions deferred to M4.
- `VK_LAYER_PATH` set per process: the registry lists missing SDK 1.1.92.1 layers.
