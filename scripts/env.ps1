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
# The registry still lists explicit layers of the removed SDK 1.1.92.1; the loader reports each missing
# manifest as an error. Searching only the current SDK's layers keeps validation runs clean.
$env:VK_LAYER_PATH = Join-Path $RF_VKSDK 'Bin'

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

# Fails early when the device is not attached (adb waits forever otherwise).
function Assert-RfDevice {
    param([Parameter(Mandatory)][string]$Serial)
    $state = (& $RF_ADB -s $Serial get-state 2>$null)
    if ($state -ne 'device') { throw "device $Serial is not attached (adb state: '$state')" }
}

# Package of the resumed (foreground) activity, '' if none.
function Get-RfForegroundPackage {
    param([Parameter(Mandatory)][string]$Serial)
    $line = (& $RF_ADB -s $Serial shell "dumpsys activity activities | grep -E 'mResumedActivity|topResumedActivity'") |
        Select-Object -First 1
    if ($line -match 'u0 ([\w\.]+)/') { return $Matches[1] }
    return ''
}

function Get-RfHomePackage {
    param([Parameter(Mandatory)][string]$Serial)
    $line = (& $RF_ADB -s $Serial shell 'cmd package resolve-activity --brief -a android.intent.action.MAIN -c android.intent.category.HOME') |
        Select-Object -Last 1
    return ($line -split '/')[0].Trim()
}

# The test phones are shared with other projects' sessions. Before taking the screen, wait until the
# launcher or RAVEFORM has been in front for $QuietSeconds, so we do not interrupt someone else's run.
function Wait-RfDeviceIdle {
    param([Parameter(Mandatory)][string]$Serial, [int]$QuietSeconds = 10, [int]$TimeoutSeconds = 900)
    $homePkg = Get-RfHomePackage $Serial
    $quietSince = $null
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $announced = $false
    while ((Get-Date) -lt $deadline) {
        $fg = Get-RfForegroundPackage $Serial
        if ($fg -eq '' -or $fg -eq $homePkg -or $fg -eq $RF_APP_ID) {
            if (-not $quietSince) { $quietSince = Get-Date }
            if (((Get-Date) - $quietSince).TotalSeconds -ge $QuietSeconds) { return $true }
        } else {
            if (-not $announced) { Write-Host "[device] $Serial is busy with $fg; waiting"; $announced = $true }
            $quietSince = $null
        }
        Start-Sleep -Seconds 2
    }
    return $false
}

# Wakes the screen, stops any running copy and starts the activity. $AppArgs becomes argv through
# the 'args' intent extra (RaveformActivity.getArguments).
function Start-RfApp {
    param([Parameter(Mandatory)][string]$Serial, [string]$AppArgs = '')
    & $RF_ADB -s $Serial shell input keyevent KEYCODE_WAKEUP | Out-Null
    & $RF_ADB -s $Serial shell am force-stop $RF_APP_ID | Out-Null
    $cmd = "am start -W -n $RF_ACTIVITY"
    if ($AppArgs) { $cmd += " --es args '$AppArgs'" }
    & $RF_ADB -s $Serial shell $cmd
    if ($LASTEXITCODE -ne 0) { throw "am start failed on $Serial" }
}
