#!/usr/bin/env bash
# Builds RAVEFORM for iPhone and iPad on a Mac (docs/IOS.md): CMake's Xcode generator (CMAKE_SYSTEM_NAME=iOS, arm64, iOS 15+),
# then xcodebuild through CMake. MoltenVK comes from the Vulkan SDK for macOS (VULKAN_SDK, with the SDK's iOS component).
#
#   scripts/build-ios.sh --team ABCDE12345                 build build/ios/<config>-iphoneos/RAVEFORM.app (RelWithDebInfo)
#   scripts/build-ios.sh --team ABCDE12345 --config Release
#   scripts/build-ios.sh --team ABCDE12345 --install       build, then install on the one connected device (xcrun devicectl)
#   scripts/build-ios.sh --team ABCDE12345 --install --run "--scenario bench_idle --seconds 60"
#   scripts/build-ios.sh --open                            generate the Xcode project and open it (sign and run from Xcode)
#   scripts/build-ios.sh --clean                           start from an empty build/ios
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/ios"
CONFIG=RelWithDebInfo
TEAM="${RF_IOS_TEAM:-}"
OPEN=0
CLEAN=0
INSTALL=0
RUN=""
RUN_SET=0

while [ $# -gt 0 ]; do
    case "$1" in
        --team) TEAM="$2"; shift 2 ;;
        --config) CONFIG="$2"; shift 2 ;;
        --open) OPEN=1; shift ;;
        --clean) CLEAN=1; shift ;;
        --install) INSTALL=1; shift ;;
        --run) RUN="$2"; RUN_SET=1; shift 2 ;;
        -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
        *) echo "build-ios: unknown argument $1" >&2; exit 2 ;;
    esac
done

[ "$(uname)" = "Darwin" ] || { echo "build-ios: iOS builds need a Mac with Xcode (docs/IOS.md)" >&2; exit 1; }
command -v cmake >/dev/null || { echo "build-ios: cmake not found (brew install cmake)" >&2; exit 1; }
command -v xcodebuild >/dev/null || { echo "build-ios: Xcode's command line tools not found (xcode-select --install)" >&2; exit 1; }
if [ -z "${VULKAN_SDK:-}" ]; then
    echo "build-ios: VULKAN_SDK is not set: source ~/VulkanSDK/<version>/setup-env.sh (docs/IOS.md)" >&2
    exit 1
fi
for dep in SDL/CMakeLists.txt volk/volk.c vulkan-headers/include/vulkan/vulkan.h enet/host.c moltenvk/LICENSE; do
    [ -e "$ROOT/third_party/$dep" ] || { echo "build-ios: third_party/$dep is missing: run scripts/fetch-deps.sh" >&2; exit 1; }
done

if [ "$CLEAN" = 1 ]; then rm -rf "$BUILD"; fi

cmake -S "$ROOT" -B "$BUILD" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
    -DRF_IOS_TEAM="$TEAM"

if [ "$OPEN" = 1 ]; then
    open "$BUILD/raveform.xcodeproj"
    exit 0
fi

if [ -z "$TEAM" ]; then
    echo "build-ios: no --team (or RF_IOS_TEAM): Xcode cannot sign the app; pass your team id (Xcode: Settings, Accounts)" >&2
    exit 1
fi

cmake --build "$BUILD" --config "$CONFIG" --target raveform_ios -- -allowProvisioningUpdates
APP="$BUILD/$CONFIG-iphoneos/RAVEFORM.app"
[ -d "$APP" ] || { echo "build-ios: the build did not produce $APP" >&2; exit 1; }
echo "[build] $APP"

if [ "$INSTALL" = 1 ]; then
    # The one connected device (xcrun devicectl, Xcode 15+); its identifier from the first line of the device list.
    DEVICE="$(xcrun devicectl list devices --hide-default-columns --columns Identifier --hide-headers 2>/dev/null | head -n 1 | tr -d '[:space:]')"
    [ -n "$DEVICE" ] || { echo "build-ios: no device connected (or not paired)" >&2; exit 1; }
    xcrun devicectl device install app --device "$DEVICE" "$APP"
    echo "[deploy] installed on $DEVICE"
    if [ "$RUN_SET" = 1 ]; then
        # shellcheck disable=SC2086
        xcrun devicectl device process launch --terminate-existing --device "$DEVICE" dev.raveform.game $RUN
        echo "[deploy] launched with: $RUN"
    fi
fi
