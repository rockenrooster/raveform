# Fetches the pinned third-party sources into third_party/. Safe to re-run: existing trees are kept.
# Versions here are the single source of truth; mirror any change in docs/LICENSES.md.

. "$PSScriptRoot\env.ps1"

$tp = Join-Path $RF_ROOT 'third_party'
New-Item -ItemType Directory -Force $tp | Out-Null

function Get-PinnedRepo {
    # -Commit: the full commit id the tag must resolve to (a checksum of the whole tree); checked after the
    # clone and on every run, so a moved tag or a changed tree stops the build.
    param([string]$Name, [string]$Url, [string]$Tag, [string]$Commit = '')
    $dst = Join-Path $tp $Name
    if (Test-Path (Join-Path $dst '.git')) {
        Write-Host "[deps] $Name already present"
    } else {
        Write-Host "[deps] cloning $Name @ $Tag"
        Invoke-RfGit clone --depth 1 --branch $Tag --single-branch $Url $dst
    }
    if ($Commit) {
        $head = (& $RF_GIT -C $dst rev-parse HEAD).Trim()
        if ($head -ne $Commit) { throw "[deps] $Name is at $head, expected $Commit ($Tag): delete third_party\$Name and fetch again" }
        $dirty = (& $RF_GIT -C $dst status --porcelain --untracked-files=no)
        if ($dirty) { throw "[deps] $Name has local changes to tracked files: $dirty" }
        Write-Host "[deps] $Name verified at $Commit"
    }
}

Get-PinnedRepo -Name 'SDL'  -Url 'https://github.com/libsdl-org/SDL.git' -Tag 'release-3.4.20'
Get-PinnedRepo -Name 'volk' -Url 'https://github.com/zeux/volk.git'      -Tag 'vulkan-sdk-1.4.363.0'
# M4: ENet 1.3.18 (LAN co-op transport), the upstream release tag; its commit id is the checksum.
Get-PinnedRepo -Name 'enet' -Url 'https://github.com/lsalzman/enet.git'  -Tag 'v1.3.18' -Commit '2662c0de09e36f2a2030ccc2c528a3e4c9e8138a'

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
