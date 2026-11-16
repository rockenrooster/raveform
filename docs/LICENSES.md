# RAVEFORM — Licences and provenance

Every library, tool output and non-procedural file that ships in a build, with version, source and
licence. Update this file with every new dependency or asset (PLAN §6). The in-game credits screen
(M7) is generated from it.

## Third-party code in the builds

| Library | Version (pin) | Source | Licence | Where it ends up |
|---|---|---|---|---|
| SDL | 3.4.20 (tag `release-3.4.20`, commit `f00dc919ca35`) | https://github.com/libsdl-org/SDL (`scripts/fetch-deps.ps1`) | zlib | Static in the desktop exe; `libSDL3.so` in the APK; SDL's Java glue (`android-project/app/src/main/java`, used unmodified as a Gradle source set) |
| volk | tag `vulkan-sdk-1.4.363.0` (commit `54fc0d7a7288`) | https://github.com/zeux/volk (`scripts/fetch-deps.ps1`) | MIT | Compiled into both builds |
| ENet | 1.3.18 (tag `v1.3.18`, commit `2662c0de09e3`) | https://github.com/lsalzman/enet (`scripts/fetch-deps.ps1`) | MIT | Compiled into both builds (M4 co-op; unmodified, its socket and clock calls routed to `src/net/wire.cpp` by a forced include) |
| Vulkan-Headers | 1.4.363 (copied from the local Vulkan SDK 1.4.363.0; the GitHub release workflow, which has no SDK: the repository's tag `vulkan-sdk-1.4.363.0`, commit `6802bb4733b6`, by `scripts/fetch-deps.sh`, the same files as the SDK's but for line endings; `vk_enum_string_helper.h`, which the SDK takes from Vulkan-Utility-Libraries (Apache-2.0), from that repository's tag `vulkan-sdk-1.4.363.0`, commit `57f01541b395`, byte for byte the SDK's) | https://github.com/KhronosGroup/Vulkan-Headers | Apache-2.0 OR MIT | Headers only |
| Oboe | 1.11.0 (`com.google.oboe:oboe:1.11.0`, prefab AAR) | Google Maven; https://github.com/google/oboe | Apache-2.0 | `liboboe.so` in the APK |
| Android Frame Pacing (Swappy) | 2.1.3 (`androidx.games:games-frame-pacing:2.1.3`, prefab AAR, module `swappy_static`) | Google Maven; Android Game Development Kit | Apache-2.0 | Statically linked into `libmain.so` |
| LLVM libc++ (NDK r29 `libc++_shared.so`) | NDK 29.0.14206865 | Android NDK | Apache-2.0 WITH LLVM-exception | `libc++_shared.so` in the APK (required by Oboe's prefab package) |
| llvm-mingw runtime (libc++, libunwind, mingw-w64 CRT, winpthreads) | llvm-mingw 20260602 (clang 22.1.7) | https://github.com/mstorsjo/llvm-mingw | Apache-2.0 WITH LLVM-exception; mingw-w64 runtime: ZPL/public domain and permissive notices | Statically linked into the Windows dev exe (not distributed) |
| MoltenVK (iOS only, docs/IOS.md) | the Vulkan SDK for macOS 1.4.363.0's `iOS/lib/MoltenVK.xcframework` (static); its LICENSE fetched by `scripts/fetch-deps.sh` | https://github.com/KhronosGroup/MoltenVK | Apache-2.0 | Statically linked into the iOS app (not built yet) |
| Gradle wrapper (`gradlew`, `gradlew.bat`, `gradle-wrapper.jar`) | copied from SDL 3.4.20's `android-project`; runs Gradle 8.13 | https://gradle.org | Apache-2.0 | Build tooling only |

## Build tools (not shipped)

| Tool | Version | Licence | Use |
|---|---|---|---|
| Android Gradle Plugin | 8.13.0 | Apache-2.0 | APK build |
| Gradle | 8.13 | Apache-2.0 | APK build |
| Android NDK (clang 21, `glslc` from shader-tools) | r29 (29.0.14206865) | Apache-2.0 and component licences | Android native build, shaders |
| SDK CMake / Ninja | 3.22.1 | BSD-3-Clause / Apache-2.0 | Android native build |
| CMake | 4.4.4 | BSD-3-Clause | Desktop build |
| Vulkan SDK (`glslc`, validation layers) | 1.4.363.0 | Apache-2.0 and component licences | Desktop shaders and validation |
| Python 3.14 + Pillow | local install | PSF / HPND (MIT-CMU) | `scripts/shrink_png.py` (review copies of screenshots) |

## Original assets

| Path | Origin | Author / tool | Licence | Date | Notes |
|---|---|---|---|---|---|
| `design/vector_font.txt` | Original stroke font "RF Cut" designed for RAVEFORM | RAVEFORM lead (planning session) | Project-owned, original | 2026-11-10 | Embedded at build time by `cmake/EmbedText.cmake`; glyphs used as designed |
| `android/app/src/main/res/drawable/ic_launcher_placeholder.xml` | PLACEHOLDER launcher icon (cyan diamond) written as a vector drawable | executor | Project-owned, original | 2026-11-10 | Replace with the real icon (M6) |
| `design/look.json` | The numbers of the look bible (`design/look.md` v2) as data | RAVEFORM lead (look-dev, `design/tools/lookdev.py`) | Project-owned, original | 2026-11-10 | Embedded at build time (M1); every renderer and effect number is read from it |
| `design/look_variants.json` | The three look variants (Signal, Laser, Haze) | RAVEFORM lead (`design/tools/variants.py`) | Project-owned, original | 2026-11-10 | Embedded at build time (M1); switched while running |
| `design/palettes.json` | The six Track palettes and the constant colours, OKLCH exported to linear sRGB | RAVEFORM lead (`design/tools/palettes.py`) | Project-owned, original | 2026-11-10 | Embedded at build time (M1). The working tree's line-ending difference on this file is left as it is |
| `design/glyphs.json` | Every enemy, player, pickup and threat shape as line segments | RAVEFORM lead (`design/tools/glyphs.py`) | Project-owned, original | 2026-11-10 | Embedded at build time (M1); built into static glyph meshes at start-up |
| `design/scenes/*.rfscene`, `design/scenes/look_card.probes.json` | Acceptance scenes for the renderer (target frame, calibration card) | RAVEFORM lead (`design/tools/scenes.py`) | Project-owned, original | 2026-11-11 | Read from disk by `--scene` for the acceptance pictures; not packaged in the APK |

All other code in `src/`, `shaders/`, `scripts/` and `android/app/src/main/java/dev/` is original to
RAVEFORM. The PNG writer, stroke-font renderer and test tone are written from scratch (no stb).

The other files under `design/` (icons, species, music prototypes and their generator scripts in
`design/tools/`) are the lead's procedural work and are not used by the M1 build; rows are added when a
milestone starts shipping them.
