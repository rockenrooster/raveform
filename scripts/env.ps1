# Dot-source this from every script. It sets the per-process environment for RAVEFORM builds.
# It never changes system or user settings (see docs/PLAN.md section 3.1 for why each override exists).

$ErrorActionPreference = 'Stop'

$RF_ROOT   = Split-Path -Parent $PSScriptRoot
$RF_JDK    = 'C:\Program Files\Eclipse Adoptium\jdk-17.0.21.9-hotspot'   # JAVA_HOME on this PC is stale
$RF_VKSDK  = 'C:\VulkanSDK\1.4.363.0'                                    # VULKAN_SDK on this PC is stale
$RF_CMAKE  = 'C:\Program Files\CMake\bin\cmake.exe'                      # PATH cmake is an MSYS2 build
$RF_GIT    = 'C:\Program Files\Git\cmd\git.exe'                          # PATH git is an MSYS2 build
$RF_SDK    = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { "$env:LOCALAPPDATA\Android\Sdk" }
$RF_NDK    = Join-Path $RF_SDK 'ndk\29.0.14206865'
$RF_ADB    = Join-Path $RF_SDK 'platform-tools\adb.exe'
$RF_APP_ID = 'dev.raveform.game'
$RF_ACTIVITY = 'dev.raveform.game/.RaveformActivity'

foreach ($p in @($RF_JDK, $RF_VKSDK, $RF_CMAKE, $RF_GIT, $RF_NDK, $RF_ADB)) {
    if (-not (Test-Path $p)) { throw "Missing tool: $p (see docs/PLAN.md section 3.1)" }
}

$env:JAVA_HOME    = $RF_JDK
$env:VULKAN_SDK   = $RF_VKSDK
$env:ANDROID_HOME = $RF_SDK

# The user's ~/.gitconfig points http.sslcainfo at a file that no longer exists, so HTTPS needs the
# Windows certificate store. This is passed per invocation; the config file is left alone.
function Invoke-RfGit {
    & $RF_GIT -c http.sslBackend=schannel -c advice.detachedHead=false @args
    if ($LASTEXITCODE -ne 0) { throw "git $($args -join ' ') failed ($LASTEXITCODE)" }
}

function Invoke-RfAdb {
    param([Parameter(Mandatory)][string]$Serial, [Parameter(ValueFromRemainingArguments)][string[]]$Rest)
    & $RF_ADB -s $Serial @Rest
}

# Known test devices (docs/PLAN.md section 3.2).
$RF_DEVICES = @{
    moto   = 'ZY22BXMJG3'     # Motorola One 5G Ace, 60 Hz, the performance floor
    tablet = 'R5GL362BEHX'    # Galaxy Tab A11+, 90 Hz
    op12r  = 'a0e622a5'       # OnePlus 12R, 120 Hz
}

function Resolve-RfSerial {
    param([string]$NameOrSerial)
    if ($RF_DEVICES.ContainsKey($NameOrSerial)) { return $RF_DEVICES[$NameOrSerial] }
    return $NameOrSerial
}
