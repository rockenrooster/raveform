#!/usr/bin/env bash
# Fetches the pinned third-party sources into third_party/ on a Mac (docs/IOS.md) or on Linux (the GitHub release workflow,
# docs/RELEASE_PIPELINE.md): the same pins as scripts/fetch-deps.ps1 (the single source of truth on Windows; mirror any change
# there and in docs/LICENSES.md), the Vulkan headers from the Vulkan SDK (without one on Linux: Khronos's Vulkan-Headers at the
# SDK's tag, the headers the SDK ships), and on a Mac MoltenVK's licence for the credits screen. Safe to re-run: existing trees
# are kept and checked.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TP="$ROOT/third_party"
mkdir -p "$TP"

pinned() {
    # name url tag [commit]: the commit, when given, is what the tag must resolve to (a checksum of the whole tree).
    local name="$1" url="$2" tag="$3" commit="${4:-}"
    local dst="$TP/$name"
    if [ -d "$dst/.git" ]; then
        echo "[deps] $name already present"
    else
        echo "[deps] cloning $name @ $tag"
        git clone --depth 1 --branch "$tag" --single-branch "$url" "$dst"
    fi
    if [ -n "$commit" ]; then
        local head
        head="$(git -C "$dst" rev-parse HEAD)"
        if [ "$head" != "$commit" ]; then
            echo "[deps] $name is at $head, expected $commit ($tag): delete third_party/$name and fetch again" >&2
            exit 1
        fi
        if [ -n "$(git -C "$dst" status --porcelain --untracked-files=no)" ]; then
            echo "[deps] $name has local changes to tracked files" >&2
            exit 1
        fi
        echo "[deps] $name verified at $commit"
    fi
}

pinned SDL  https://github.com/libsdl-org/SDL.git  release-3.4.20
pinned volk https://github.com/zeux/volk.git       vulkan-sdk-1.4.363.0
pinned enet https://github.com/lsalzman/enet.git   v1.3.18 2662c0de09e36f2a2030ccc2c528a3e4c9e8138a

# The Vulkan headers from the Vulkan SDK (VULKAN_SDK; on a Mac: source ~/VulkanSDK/<version>/setup-env.sh), as on Windows.
# Linux without an SDK (the release workflow): Khronos's repository at the SDK's tag, its commit checked.
VH="$TP/vulkan-headers/include"
if [ ! -f "$VH/vulkan/vulkan.h" ]; then
    if [ -n "${VULKAN_SDK:-}" ] && [ -d "$VULKAN_SDK/include/vulkan" ]; then
        echo "[deps] copying Vulkan headers from $VULKAN_SDK"
        mkdir -p "$VH"
        cp -R "$VULKAN_SDK/include/vulkan" "$VH/"
        cp -R "$VULKAN_SDK/include/vk_video" "$VH/"
    elif [ "$(uname -s)" != Darwin ]; then
        pinned vulkan-headers/source https://github.com/KhronosGroup/Vulkan-Headers.git vulkan-sdk-1.4.363.0 \
            6802bb4733b63ed5efd3adb308a6c885ef180ea1
        echo "[deps] copying Vulkan headers from Vulkan-Headers"
        mkdir -p "$VH"
        cp -R "$TP/vulkan-headers/source/include/vulkan" "$VH/"
        cp -R "$TP/vulkan-headers/source/include/vk_video" "$VH/"
    else
        echo "[deps] VULKAN_SDK is not set (or has no include/vulkan): install the Vulkan SDK for macOS 1.4.363.0 with its iOS" >&2
        echo "       component and source its setup-env.sh (docs/IOS.md)" >&2
        exit 1
    fi
else
    echo "[deps] Vulkan headers already present"
fi

# MoltenVK's licence (the Apache License 2.0 as MoltenVK ships it), for the iOS credits screen (cmake/Credits.cmake).
if [ "$(uname -s)" = Darwin ]; then
    if [ ! -f "$TP/moltenvk/LICENSE" ]; then
        echo "[deps] fetching MoltenVK's LICENSE"
        mkdir -p "$TP/moltenvk"
        curl -fsSL https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/LICENSE -o "$TP/moltenvk/LICENSE"
    else
        echo "[deps] MoltenVK's LICENSE already present"
    fi
fi

echo "[deps] done"
