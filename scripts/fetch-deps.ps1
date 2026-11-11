# Fetches the pinned third-party sources into third_party/. Safe to re-run: existing trees are kept.
# Versions here are the single source of truth; mirror any change in docs/LICENSES.md.

. "$PSScriptRoot\env.ps1"

$tp = Join-Path $RF_ROOT 'third_party'
New-Item -ItemType Directory -Force $tp | Out-Null

function Get-PinnedRepo {
    param([string]$Name, [string]$Url, [string]$Tag)
    $dst = Join-Path $tp $Name
    if (Test-Path (Join-Path $dst '.git')) { Write-Host "[deps] $Name already present"; return }
    Write-Host "[deps] cloning $Name @ $Tag"
    Invoke-RfGit clone --depth 1 --branch $Tag --single-branch $Url $dst
}

Get-PinnedRepo -Name 'SDL'  -Url 'https://github.com/libsdl-org/SDL.git' -Tag 'release-3.4.20'
Get-PinnedRepo -Name 'volk' -Url 'https://github.com/zeux/volk.git'      -Tag 'vulkan-sdk-1.4.363.0'

# Vulkan headers come from the local Vulkan SDK so both platforms compile against the same copy.
$vh = Join-Path $tp 'vulkan-headers\include'
if (-not (Test-Path (Join-Path $vh 'vulkan\vulkan.h'))) {
    Write-Host "[deps] copying Vulkan headers from $RF_VKSDK"
    New-Item -ItemType Directory -Force $vh | Out-Null
    Copy-Item (Join-Path $RF_VKSDK 'Include\vulkan')   $vh -Recurse
    Copy-Item (Join-Path $RF_VKSDK 'Include\vk_video') $vh -Recurse
} else {
    Write-Host "[deps] Vulkan headers already present"
}

Write-Host "[deps] done"
