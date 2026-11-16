# RAVEFORM — the release pipeline (GitHub)

Set up on 2026-11-15 at the owner's request: "a GitHub repo and a release pipeline that automatically builds a release apk
available for download".

## Where

- The repository: `rockenrooster/raveform` on GitHub, **private**, the remote `origin` of this checkout. Sessions push only when
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
   Vulkan-Headers at the SDK's tag `vulkan-sdk-1.4.363.0` (commit checked): the headers the Vulkan SDK 1.4.363.0 ships, which
   this PC copies from its SDK.
3. `gradlew :app:assembleRelease`: native Release, the libraries stripped, not debuggable, no app arguments.
4. The release `build-<N>`, N the number of commits on `main` (it only grows), titled "RAVEFORM <versionName> build <N>" and
   marked latest. Its files: `raveform.apk`, `raveform.apk.sha256`, and `raveform-symbols.zip` (the unstripped native
   libraries, for reading a crash report with `ndk-stack`). Its notes: the commit, the build time, the signing certificate and
   the commits since the previous build. The APK's size by part (`scripts/apk_size.py`) is in the run's log.

## Downloading

- The newest build is always at `https://github.com/rockenrooster/raveform/releases/latest/download/raveform.apk`; every
  build is on the repository's Releases page. While the repository is private, the browser must be signed in to GitHub.
- On a phone: open the link (or the release page) in a browser signed in to GitHub, download, open the file; Android asks once
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

Set them with `gh secret set <NAME> -R rockenrooster/raveform` (it asks for the value), the keystore from its base64 text:
`[Convert]::ToBase64String([IO.File]::ReadAllBytes('<keystore>')) | gh secret set RF_KEYSTORE_BASE64 -R rockenrooster/raveform`.

**Without them** the APK is signed with a debug key the run makes for itself, a different one every run. Android installs an
update only when it is signed with the same key as the installed app, so such an APK installs only where RAVEFORM is not
installed: uninstall first, and the phone's settings and scores go with it. The release notes and the run's summary say which
key signed each build.

The owner's choice (PLAN: keystores are the owner's):

- **This PC's debug keystore** (`%USERPROFILE%\.android\debug.keystore`; password `android`, alias `androiddebugkey`, key
  password `android`): GitHub's builds and this PC's builds (`deploy-android.ps1`) then install over each other, and the phones
  keep their data. Play never takes a debug-signed build.
- **A new upload key** (RELEASE_CHECKLIST item 1): the key Play will take. This PC's builds are still debug-signed, so on a
  phone a GitHub build and a deployed build replace each other only through an uninstall, unless the same four variables are
  set before `build-android.ps1 -Config release`.

## Version

`versionName` is `android/app/build.gradle`'s (kept equal to `CMakeLists.txt`'s project VERSION); the title screen shows it
with the build's commit and date. `versionCode` stays 1 on every build: Android refuses an install with a lower versionCode
than the installed one, and the shared phones get builds of different commits from this PC's sessions and from GitHub. Raise
it for a store release.

## Cost

GitHub's free plan gives a private repository's Actions 2,000 Linux minutes a month (a release build: see PROGRESS.md,
"Release pipeline"); release files do not count toward the storage quota. Documentation-only pushes do not build.

## Making the download public

The repository is private, so only accounts with access can download. To let anyone download: make the repository public
(`gh repo edit rockenrooster/raveform --visibility public --accept-visibility-change-consequences`: the source and its whole
history become public), or publish the APKs to a second, public repository (the workflow would need a token for it as a
secret).
