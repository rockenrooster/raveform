# RAVEFORM — the release pipeline (GitHub)

Set up on 2026-11-15 at the owner's request: "a GitHub repo and a release pipeline that automatically builds a release apk
available for download".

## Where

- The repository: `rockenrooster/raveform` on GitHub, **public** since 2026-11-15 (below), the remote `origin` of this checkout. Sessions push only when
  the owner asks (CLAUDE.md, Conventions).
- The workflow: `.github/workflows/release.yml` ("Release APK").

## What runs, and when

- Every push to `main`, unless everything it changes is documentation (`docs/**`, any `*.md`, `design/previews/**`).
- By hand: `gh workflow run release.yml -R rockenrooster/raveform`, or the repository's Actions tab, "Release APK", "Run
  workflow". Run from another branch, the APK stays with the run (Actions, the run's page) for 7 days and no release is made.
- One build at a time: a newer push cancels a build still running (the newer build has its commits too).

The job (GitHub's `ubuntu-24.04` runner) does what `scripts\build-android.ps1 -Config release` does on Windows:

1. JDK 17 (Temurin); the Android SDK's NDK `29.0.14206865`, CMake `3.22.1` and platform 36 (`sdkmanager`).
2. `scripts/fetch-deps.sh`: the pinned SDL, volk and ENet of `scripts/fetch-deps.ps1`, and the Vulkan headers from Khronos's
   Vulkan-Headers at the SDK's tag `vulkan-sdk-1.4.363.0`, with `vk_enum_string_helper.h` from Vulkan-Utility-Libraries at the
   same tag (both commits checked): the headers the Vulkan SDK 1.4.363.0 ships, which this PC copies from its SDK (checked
   2026-11-15: the same files, but for the SDK's CRLF line endings).
3. `gradlew :app:assembleRelease`: native Release, the libraries stripped, not debuggable, no app arguments.
4. The release `build-<N>`, N the number of commits on `main` (it only grows), titled "RAVEFORM <versionName> build <N>" and
   marked latest. Its files: `raveform.apk`, `raveform.apk.sha256`, and `raveform-symbols.zip` (the unstripped native
   libraries, for reading a crash report with `ndk-stack`). Its notes: the commit, the build time, the signing certificate and
   the commits since the previous build. The APK's size by part (`scripts/apk_size.py`) is in the run's log.

## Downloading

- The newest build is always at `https://github.com/rockenrooster/raveform/releases/latest/download/raveform.apk`; every
  build is on the repository's Releases page. Anyone can download them, without a GitHub account.
- On a phone: open the link (or the release page) in a browser, download, open the file; Android asks once
  to allow installs from that browser.
- On this PC: `gh release download -R rockenrooster/raveform -p raveform.apk`, then `adb -s <serial> install -r raveform.apk`
  (the shared-phone rules of CLAUDE.md apply).

## Signing

The workflow signs with the key in four repository secrets, when they are set (`android/app/build.gradle` reads them as the
environment variables `RF_KEYSTORE`, `RF_KEYSTORE_PASSWORD`, `RF_KEY_ALIAS`, `RF_KEY_PASSWORD`):

| Secret | Holds |
|---|---|
| `RF_KEYSTORE_BASE64` | the keystore file, in base64 |
| `RF_KEYSTORE_PASSWORD` | the keystore's password |
| `RF_KEY_ALIAS` | the key's alias |
| `RF_KEY_PASSWORD` | the key's password |

**The owner chose a new release key (2026-11-15)** and made it that day with `scripts\release-key.ps1` (certificate SHA-256
`dc52de684584b98f11864631b89b7cd84b708e42e08bbe7e0b3f1db79066b605`). The keystore is `keys\raveform-release.keystore` in the
project folder (the owner's choice; PKCS12, RSA 4096, alias `raveform`, valid 10,000 days), ignored by git (`/keys/`,
`*.keystore`, `*.jks`, `*.p12`, `*.pfx`): never commit it, the repository is public. It was first made at
`%USERPROFILE%\.raveform\`, where a copy stays. The script: keytool asks for a new password; the script asks for it once
more, checks that it opens the keystore, sets the four secrets through gh's standard input (never on a command line),
prints the certificate's SHA-256 (the release notes print the same) and starts a release build. `-SecretsOnly` uploads the
existing keystore again; `-NoBuild` starts no build. Back the keystore and its password up off this PC: a build signed with
any other key cannot update an installed copy, and only Play App Signing (once enrolled) can replace a lost upload key.

By hand instead: `gh secret set <NAME> -R rockenrooster/raveform` asks for each value.

**Without them** the APK is signed with a debug key the run makes for itself, a different one every run. Android installs an
update only when it is signed with the same key as the installed app, so such an APK installs only where RAVEFORM is not
installed: uninstall first, and the phone's settings and scores go with it. The release notes and the run's summary say which
key signed each build.

What the choice means (the other was this PC's debug keystore, which Play never takes): this PC's builds
(`deploy-android.ps1`) stay debug-signed, so on a phone a GitHub build and a deployed build replace each other only through an
uninstall, which takes the phone's settings and scores. A local release build signs with the release key when the same four
variables are set (`RF_KEYSTORE` the keystore's path) before `build-android.ps1 -Config release`.

## Version

`versionName` is `android/app/build.gradle`'s (kept equal to `CMakeLists.txt`'s project VERSION); the title screen shows it
with the build's commit and date. `versionCode` stays 1 on every build: Android refuses an install with a lower versionCode
than the installed one, and the shared phones get builds of different commits from this PC's sessions and from GitHub. Raise
it for a store release.

## Cost

GitHub's standard runners cost nothing for a public repository (while it was private: 2,000 Linux minutes a month). A
release build takes about 4 minutes (the first, `build-284`: 3 min 39 s). Release files count toward no storage quota.
Documentation-only pushes do not build.

## Public

The repository has been public since 2026-11-15 (the owner: "Make it public"): the source, its whole history (the commits'
author email included) and every release are open to anyone. Checked before the switch: no key, token, password or
keystore in any commit, by file name and by content over the whole history. The signing secrets stay secret: GitHub never
shows them, and only pushes to `main` and runs by hand start the workflow, both of which need write access.
