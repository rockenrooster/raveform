# RAVEFORM — release checklist (M7, release preparation)

What the build can show about Play readiness, checked on 2026-11-14 (commit noted in PROGRESS), and what only the owner can
do. The long soak of PLAN M7 is replaced by the owner's short-runs rule. Nothing here needed an SDK download or a store account.

## Open items (first: what blocks a store release)

### Only the owner can do these

1. **The signing key.** The `release` variant is signed with the debug key only (no keystore was created). Create the upload
   key, keep it outside the repository, enrol in Play App Signing, and sign the App Bundle with it. Never upload a
   debug-signed build.
2. **The Play Console**: the developer account, the app (package `dev.raveform.game`), its countries and price (free; no
   in-app purchases, no ads).
3. **The store listing**: the title (`RAVEFORM`), the short and full descriptions, the screenshots (phone and 7"/10" tablet),
   the feature graphic (1024 x 500), the 512 px icon (`captures\m6\icon_512.png`, drawn by `scripts\make_icon.py`), the
   category (Games: Music or Arcade), a contact address. The listing should say the game has flashing lights and offers
   Reduce flashing.
4. **The privacy policy URL** (the Console asks for one) and **the Data safety form**: the draft answers are below.
5. **The content rating questionnaire** (IARC), **target audience and content**, **ads** (none), **app access** (no login):
   the draft answers are below.
6. **The version**: `versionCode 1`, `versionName '0.0.1-m0'` in `android/app/build.gradle`; the release's numbers are the
   owner's (or the lead's) to set.
7. **A test track first**: an internal testing release, and the Console's pre-launch report on its test devices. The device
   catalogue will exclude phones without Vulkan 1.1 (the manifest requires it: intended, PLAN 3.2).

### For the lead (needs a decision or a download)

8. **Three licence files are not among the fetched sources.** Oboe and Android Frame Pacing come as Maven packages without
   their LICENSE file, and the Vulkan headers are copied from the Vulkan SDK's include directory without theirs. The credits
   show for them the Apache License 2.0 as the LLVM project ships it (the NDK's `NOTICE`, llvm-mingw's `LICENSE.TXT`; the
   same licence text) and, for the Vulkan headers, their own copyright and SPDX line from `vulkan_core.h`. Their own files at
   the pinned versions (Oboe 1.11.0's `LICENSE`, the Android Games SDK's `LICENSE` for games-frame-pacing 2.1.3, the
   Vulkan-Headers `LICENSE.md` for the SDK's 1.4.363) are a download: approve it and `fetch-deps.ps1` can fetch them and
   `cmake/Credits.cmake` read them instead.
9. **The C++ library is credited though screens.md's list does not name it**: `LLVM LIBC++ (APACHE 2.0 WITH LLVM
   EXCEPTIONS)` (`libc++_shared.so` in the APK; linked statically into the desktop build). The rule "the list is whatever the
   build really links" put it there; design may want to word it.
10. **The font has no `@` or `;`**: in the credits' licence texts they show as `?` (design/vector_font.txt is design's).
11. **Development-only code stays in the release library**: the pictures, the benchmarks, the M0/M1 stages, the glyph sheet,
    the scene replay and the dev overlay, about 0.14 MB of `libmain.so`'s 1.3 MB of code (`llvm-size`; the audio test scene
    also holds the SYNC screen, which players use). The release build ignores the app arguments that reach them; taking the
    code out needs compile-time switches across a dozen files and was not done (the APK would shrink by about 2%).

## Checked

### API levels

| | Set | Play |
|---|---|---|
| `targetSdk` | 36 (Android 16) | New apps and updates must target Android 15 (API 35) since 31 August 2025; the yearly step to API 36 is expected by 31 August 2026. 36 meets both. |
| `compileSdk` | 36 | The installed SDK platform (no download). |
| `minSdk` | 29 (Android 10) | Play has no floor; the Vulkan 1.1 requirement excludes older phones anyway. |

`ACCESS_LOCAL_NETWORK` (Android 17's local network permission) is declared and inert while `targetSdk` is 36; moving to 37 needs
that SDK platform (a download) and the permission flow RaveformActivity already has.

### 16 KB page alignment

Every native library in the release APK (`libmain.so`, `libSDL3.so`, `libc++_shared.so`, `liboboe.so`, arm64-v8a only) has
its LOAD segments aligned to 0x4000 (`llvm-readelf -l`) and is stored uncompressed at a 16 KB-aligned offset in the zip.
NDK r29 links with 16 KB pages by default; our build flags needed no change.

### Permissions (the merged manifest, `aapt2 dump permissions`)

| Permission | Why |
|---|---|
| `VIBRATE` | Haptics (M2; SETTINGS, SOUND, HAPTICS: off, soft, strong) |
| `INTERNET` | LAN co-op over UDP (M4): Android needs it for any socket, local or not |
| `ACCESS_NETWORK_STATE` | Co-op: whether a network is up before hosting or joining |
| `ACCESS_WIFI_STATE` | Co-op: this device's address on the Wi-Fi for the room code and the beacons |
| `CHANGE_WIFI_MULTICAST_STATE` | Co-op: the multicast lock while the join screen listens for rooms' broadcast beacons |
| `ACCESS_LOCAL_NETWORK` | Android 17's local network permission (inert at `targetSdk` 36) |

Features: Vulkan 1.1 required; touchscreen, gamepad, Bluetooth, USB host, PC and Wi-Fi not required (M7: Wi-Fi was implied
required by the Wi-Fi permissions; co-op is optional). Landscape is implied by `sensorLandscape`.

**Nothing is collected or sent off the local network.** The game has no accounts, analytics, ads, crash reporting or
downloads. The only network traffic is co-op between phones on the same Wi-Fi: ENet over UDP to the address a player chose
(a room from the beacons, a room code, or a typed address), and the room beacons broadcast on the local subnet while a room is
open. Settings, scores and caches stay in the app's private storage (`allowBackup="false"`).

### Photosensitivity

The first launch shows `BEFORE YOU PLAY` with the notice (flashing lights, fast movement and pulsing colour timed to the music)
and a REDUCE FLASHING switch before CONTINUE (GAME_DESIGN 11.4; `settings.txt notice_seen`); the switch is in SETTINGS, LOOK,
and the credits repeat the notice under PLAY SAFE. Reduce flashing covers every M5 and M6 effect (PROGRESS, M6 B5).

### The release build

`scripts\build-android.ps1 -Config release [-Bundle]`: native Release (`-O3`, with `-g` kept only in the unstripped copy),
not debuggable, no app arguments (RaveformActivity ignores the `args` extra in a build that is not debuggable), signed with the
debug key; the unstripped libraries beside the APK as `app-release-symbols.zip`; `-Bundle` also writes `app-release.aab` (Play
takes App Bundles for new apps). Size by part: **7.01 MB** — `libmain.so` 2.90 MB (41.6%), `libSDL3.so` 2.04 MB (29.2%), `libc++_shared.so` 1.31 MB (18.8%), `liboboe.so` 0.32 MB (4.6%), resources 0.31 MB (4.5%; the icon's layers most of it), dex 0.10 MB (1.4%), the manifest and signature under 0.01 MB. The App Bundle: 6.18 MB. The unstripped libraries: 13.8 MB beside it.

### Crash-safe saves

Settings and hints, high scores, the tier cache, the baked audio and the sync calibration are written to a temporary file,
flushed and renamed over the old one (`core/save_file`); a missing, short or damaged file loads as defaults
(`raveform_save_tests`, in `scripts\test.ps1`).

### Sanitizers (desktop)

`scripts\sanitize.ps1` (llvm-mingw clang's AddressSanitizer and UndefinedBehaviorSanitizer; nothing downloaded): the audio, simulation, network and save unit tests, the loudness calibration check, the determinism test, a 3-minute headless bot run from Track 1, a fight start in Tracks 4, 7 and 10 (the careful bot to the Track, the novice to its boss: Event Horizon, Cadence and Kaleidos Reprise, each fought and won) and the 3-minute loopback net soak (0 census mismatches): **all clean** after one fix. Found: a negative float converted to an unsigned integer (the Brood Mother's hatching cue, `world_miniboss2.cpp`: fixed), and in ENet (third party, as fetched) its offsetof through a null pointer and `1 << 31`, undefined in the letter and harmless: UBSan is off for ENet in that build, AddressSanitizer stays on. Not run: LeakSanitizer (not on Windows); the game's own binary (Vulkan and SDL: the lead asked for the tests and the headless tool, which cover the simulation, the band and the network); the 6-minute impaired soak of `test.ps1` (the 3-minute clean one was asked).

### The thermal check (the Moto)

`bench_play --drop 10.1 --drop-hold --nohits --tier low`, 300 s after a 5 s warm-up, Track 10's first Drop held throughout (324 enemies in view on average, 775 at most; 900 alive at most); a shared phone, the panel at 60 Hz. **Whole run: 0 missed of 17,968 frames**, the panel's grade PASS, main thread CPU p99 5.3 ms (max 7.8), GPU P95 14.3 ms, the present call's own interval P95 17.7 ms (Swappy's pacing), worst 23.5 ms. **The last minute alone: 0 missed of 3,593 frames**, 59-60 a second, worst interval 23.5 ms; the main thread's CPU 301 ms a second against 286 in the first minute. Audio 0 underruns, heap 0. **Battery 21.3 °C before, 22.0 °C after** (`dumpsys battery`, read only); thermal status 0 (none) before and after.

## Draft answers

### Data safety

- Does the app collect or share any of the required user data types? **No.**
- Is all user data encrypted in transit? Not applicable (no user data leaves the device; co-op game state goes only to other
  players' phones on the same local network).
- Do you provide a way for users to request that their data be deleted? Not applicable: nothing is collected (uninstalling, or
  clearing the app's storage, removes the local settings and scores).

### Content rating (IARC questionnaire)

- Category: Game.
- Violence: abstract geometric shapes are destroyed with light and particles; no people, animals, blood or gore; the player's
  ship is a shape too. (Fantasy, non-realistic, no humans.)
- Fear, horror: no. Sexuality, nudity: no. Language: no. Controlled substances (drugs, alcohol, tobacco): no.
- Gambling or simulated gambling: no. Loot boxes or paid random items: no (there are no purchases at all).
- User interaction: players can play together on the same local network (co-op); there is no chat, no text or voice
  communication, no user-generated content and no sharing of location or personal information.
- Digital purchases: none. Ads: none.
- Expected outcome: ESRB Everyone, PEGI 3 or 7 (the fantasy violence decides).
- Also worth stating in the listing (not a rating question): flashing lights, with a Reduce flashing setting.

### Target audience and content

- Target age groups: 13 and over (not designed for children; no features for them). The game does not appeal primarily to
  children.
- Ads: the app contains no ads.
- App access: everything is available without a login.
