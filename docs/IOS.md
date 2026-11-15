# RAVEFORM on iOS

The owner, 2026-11-14: "your last task will be porting to IOS". This page is how the iPhone and iPad build is made, what is
different on iOS, and what has not been verified yet.

**Status: written on the Windows PC, never built.** Apple's compilers, SDK and signing only run on macOS, so nothing under
`ios/`, `src/platform/ios/` or `scripts/*.sh` has been compiled or run. The desktop and Android builds were checked after
every change to shared code. The first build on a Mac is the test; the checklist at the end says what to look at.

## What you need

- A Mac with **Xcode 15 or newer**, its command line tools (`xcode-select --install`), **CMake 3.22 or newer**
  (`brew install cmake`; 3.28+ handles Xcode projects best) and git.
- The **Vulkan SDK for macOS, 1.4.363.0** (the version the Windows build's headers and volk are pinned to), installed **with
  its iOS component**: <https://vulkan.lunarg.com/sdk/home>. Its `setup-env.sh` sets `VULKAN_SDK` (glslc and the headers come
  from there; MoltenVK from `$VULKAN_SDK/../iOS/lib/MoltenVK.xcframework`).
- An **Apple ID** added to Xcode (Settings, Accounts) and its **team id** (a free account signs for 7 days on your own
  devices; the App Store needs the Apple Developer Program).
- An **iPhone or iPad on iOS 15 or newer** (arm64, A9 or later), with Developer Mode on (Settings, Privacy & Security).
  The iOS Simulator is not a target (MoltenVK's simulator slice is not linked).

## Build, install, run

```bash
source ~/VulkanSDK/1.4.363.0/setup-env.sh
scripts/fetch-deps.sh                                   # SDL, volk, ENet, the Vulkan headers, MoltenVK's licence
scripts/build-ios.sh --team ABCDE12345                  # build/ios/RelWithDebInfo-iphoneos/RAVEFORM.app
scripts/build-ios.sh --team ABCDE12345 --install        # and install it on the connected device
scripts/build-ios.sh --team ABCDE12345 --install --run "--scenario bench_idle --seconds 60"
scripts/build-ios.sh --open                             # or open the Xcode project, pick the device, press Run
```

App arguments (the Android build's `args` extra, the desktop's command line) are the launch arguments: `--run "..."` above,
or Xcode's scheme (Product, Scheme, Edit Scheme, Run, Arguments Passed On Launch). Screenshots, strips and benchmark results
go to the app's **Documents** folder, which Finder (the device's Files tab) and the Files app show. The log is the
Xcode console (or Console.app, filter `RAVEFORM`).

## What is different on iOS

| | Android | iOS |
|---|---|---|
| Vulkan | the device's driver | **MoltenVK** (Vulkan on Metal), linked statically; the instance asks for portability devices and the device enables `VK_KHR_portability_subset` (`gfx/gpu_context.cpp`) |
| Surface, rotation | ANativeWindow, pre-rotation | SDL's Metal view; iOS rotates the layer (the transform stays identity); landscape only (Info.plist) |
| Frame pacing | Swappy | FIFO; Info.plist's `CADisableMinimumFrameDurationOnPhone` lets ProMotion run at 120 Hz |
| Audio | Oboe | SDL's CoreAudio device (`audio/audio_device_sdl.cpp`), session category **playback** (the music is the game: other apps' audio stops; the silent switch does not mute it) |
| Haptics | the Vibrator through JNI | UIKit's impact taps (`platform/ios/ios_glue.mm`) |
| Files | the app's files dir; `adb pull` | settings and scores in Library (SDL_GetPrefPath); captures in Documents |
| Window | full screen, cutout insets | full screen, the safe area from SDL; the home indicator auto-hides (`SDL_HINT_IOS_HIDE_HOME_INDICATOR` 2) |
| COLOUR VIVID | Display-P3 where offered (the Moto) | Display-P3 is expected on every recent iPhone and iPad |
| Co-op | discovery by broadcast, join by code or address | **join by code or address works** (the local network prompt appears the first time); the room list needs Apple's multicast entitlement (`com.apple.developer.networking.multicast`, granted on request) before iOS lets the game send broadcasts |
| Icon | adaptive layers | one 1024 px square (`scripts/make_icon.py` writes `ios/Assets.xcassets/AppIcon.appiconset/icon_1024.png`) |
| Credits | SDL, Oboe, Frame Pacing, ENet, volk, Vulkan headers, libc++ | SDL, **MoltenVK**, ENet, volk, Vulkan headers (libc++ is iOS's own) |

## The files

- `CMakeLists.txt`: `RF_IOS` when `CMAKE_SYSTEM_NAME` is iOS: the Objective-C++ glue, the app bundle `raveform_ios`
  (bundle id `dev.raveform.game`, iOS 15, iPhone and iPad, automatic signing with `RF_IOS_TEAM`), MoltenVK and the
  frameworks, no headless tools or tests (they stay desktop-only).
- `ios/Info.plist.in` (its `UILaunchScreen`, black: without a launch screen iOS letterboxes an app at a legacy size),
  `ios/Assets.xcassets` (the icon and the launch colour), `ios/PrivacyInfo.xcprivacy` (no tracking, no data collected, the
  reasons for the time-since-boot, file-timestamp and user-defaults APIs).
- `src/core/platform.h`: `RF_IOS`, `RF_MOBILE` (Android or iOS).
- `src/platform/ios/ios_glue.h/.mm`: haptics, the device's model, the Documents folder, and the reference that keeps
  MoltenVK's entry point in the binary for SDL's Vulkan loader (the refresh rate is SDL's display mode).
- `scripts/fetch-deps.sh`, `scripts/build-ios.sh`: the Mac's side of `fetch-deps.ps1` and `build-desktop.ps1`.

## Not verified (the first build on a Mac)

1. **It compiles and links.** Most likely to need a touch: the Objective-C++ file under `-Werror`, MoltenVK's static
   library and its frameworks, SDL's static library's own framework list, the generator expression for the build's name.
2. **It starts**: the log's `vulkan:` lines name MoltenVK and the Apple GPU; `swapchain:` gives the panel's native size
   (2532x1170 on an iPhone 14, say) and `Display-P3 offered`; `surface ... display ...` agree.
3. **The renderer's formats**: the HDR targets in `B10G11R11_UFLOAT` (the fallback is `R16G16B16A16_SFLOAT`), the distortion
   buffer, the bloom chain. Look at the title and a Drop: colours, glow and lattice as on the PC.
4. **GPU timestamps** may be missing on iOS (the renderer turns them off and says so); `--gpu-burn` and the pass times are
   then unavailable.
5. **Touch**: the sticks and buttons at their physical sizes (a point is taken as 1/163 inch: an iPad's controls come out a
   little smaller), the safe area round the notch or Dynamic Island, the home indicator's edge.
6. **Audio**: the band starts on the title, its latency is reasonable (SETTINGS, SOUND, SYNC calibrates it), it stops in the
   background and comes back; headphones and AirPods (Bluetooth adds latency: SYNC again).
7. **Background and back**: lock and unlock, the app switcher, a phone call: no crash, the run paused, the music back.
8. **120 Hz** on a ProMotion iPhone or iPad: `--run "--scenario bench_idle --seconds 30"`, then the result in Documents
   (`bench_idle.json`: the present interval near 8.3 ms).
9. **Co-op** between an iPhone and an Android phone or the PC: join by code and by address; the local network prompt.
10. **Haptics** on a ring breaking, an Extra Life, a boss's death.

## The App Store (later)

The Apple Developer Program, an App Store Connect record for `dev.raveform.game`, a Release build archived from Xcode
(Product, Archive), the privacy answers matching `PrivacyInfo.xcprivacy` (no data collected), screenshots at the store's
sizes, the content rating (the photosensitivity notice of GAME_DESIGN 11.4 is in the game), and `RF_IOS_BUILD_NUMBER` raised
for every upload. The multicast entitlement, if the room list is wanted on iOS, is requested from Apple first.
