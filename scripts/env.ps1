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

# --- CPU limit (user rule: anything this project runs on the PC uses at most 4 CPU cores) ----------
# This PowerShell process is confined to one logical CPU on each of 4 different physical cores (read
# from the processor topology, all in one L3 cache domain) and every process it starts inherits that
# affinity: cmake, ninja, clang, glslc, Gradle and its daemon, python, the desktop app. Builds also
# limit themselves to $RF_JOBS parallel jobs (cmake --parallel, Gradle --max-workers, Ninja job pools
# in CMakeLists.txt). Per process only: no system setting and no other process is touched.
$RF_JOBS = 4
if (-not ('RfCpuTopology' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class RfCpuTopology {
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool GetLogicalProcessorInformation(IntPtr buffer, ref uint length);
    // One mask per physical core (its logical processors) and one per L3 cache domain.
    public static void Read(List<ulong> cores, List<ulong> l3) {
        uint len = 0;
        GetLogicalProcessorInformation(IntPtr.Zero, ref len);
        if (len == 0) return;
        IntPtr buf = Marshal.AllocHGlobal((int)len);
        try {
            if (!GetLogicalProcessorInformation(buf, ref len)) return;
            int size = IntPtr.Size == 8 ? 32 : 24;          // SYSTEM_LOGICAL_PROCESSOR_INFORMATION
            int unionOffset = IntPtr.Size == 8 ? 16 : 8;
            for (int off = 0; off + size <= (int)len; off += size) {
                ulong mask = (ulong)Marshal.ReadIntPtr(buf, off).ToInt64();
                int relation = Marshal.ReadInt32(buf, off + IntPtr.Size);
                if (relation == 0) cores.Add(mask);                                               // RelationProcessorCore
                else if (relation == 2 && Marshal.ReadByte(buf, off + unionOffset) == 3) l3.Add(mask); // RelationCache, L3
            }
        } finally { Marshal.FreeHGlobal(buf); }
    }
}
'@
}

# Affinity mask: the first logical CPU of the last $Cores physical cores of the last L3 domain that has
# enough of them (core 0, where much of the system's work lands, stays free).
function Get-RfCpuMask([int]$Cores = $RF_JOBS) {
    $coreMasks = [System.Collections.Generic.List[uint64]]::new()
    $l3Masks = [System.Collections.Generic.List[uint64]]::new()
    [RfCpuTopology]::Read($coreMasks, $l3Masks)
    if ($coreMasks.Count -eq 0) { return [uint64]0 }
    $domains = if ($l3Masks.Count) { @($l3Masks) } else { @([uint64]::MaxValue) }
    for ($d = $domains.Count - 1; $d -ge 0; --$d) {
        $inDomain = @($coreMasks | Where-Object { ($_ -band $domains[$d]) -eq $_ })
        if ($inDomain.Count -lt $Cores) { continue }
        [uint64]$mask = 0
        foreach ($core in ($inDomain | Select-Object -Last $Cores)) {
            for ($bit = 0; $bit -lt 64; ++$bit) {
                if (($core -shr $bit) -band 1) { $mask = $mask -bor ([uint64]1 -shl $bit); break }
            }
        }
        return $mask
    }
    return [uint64]0
}

$RF_CPU_MASK = Get-RfCpuMask
$rfSelf = [System.Diagnostics.Process]::GetCurrentProcess()
[uint64]$rfCurrent = $rfSelf.ProcessorAffinity.ToInt64()
$rfBits = 0; for ($b = 0; $b -lt 64; ++$b) { if (($rfCurrent -shr $b) -band 1) { ++$rfBits } }
if ($RF_CPU_MASK -ne 0 -and $rfBits -gt $RF_JOBS) {
    $rfSelf.ProcessorAffinity = [IntPtr]::new([int64]$RF_CPU_MASK)  # never widened if already narrower
}
$RF_CPU_MASK = [uint64]$rfSelf.ProcessorAffinity.ToInt64()

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

# --- Shared test devices (see CLAUDE.md) ---------------------------------------------------------
# Other sessions use the same phones. Never clear logcat, never touch other packages, and only take
# the screen when the launcher or RAVEFORM is in front.

# True when nothing but the launcher or RAVEFORM is in front right now.
function Test-RfDeviceFree {
    param([Parameter(Mandatory)][string]$Serial)
    $fg = Get-RfForegroundPackage $Serial
    return ($fg -eq '' -or $fg -eq (Get-RfHomePackage $Serial) -or $fg -eq $RF_APP_ID)
}

# Waits until the launcher or RAVEFORM has been in front for $QuietSeconds.
function Wait-RfDeviceIdle {
    param([Parameter(Mandatory)][string]$Serial, [int]$QuietSeconds = 10, [int]$TimeoutSeconds = 300)
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
            if (-not $announced) { Write-Host "[device] $Serial is in use ($fg in front); waiting"; $announced = $true }
            $quietSince = $null
        }
        Start-Sleep -Seconds 2
    }
    return $false
}

# PID of our process ('' when not running). Logs are read with `logcat -d --pid`, never cleared.
function Get-RfPid {
    param([Parameter(Mandatory)][string]$Serial)
    return "$(& $RF_ADB -s $Serial shell pidof $RF_APP_ID)".Trim()
}

# Closes RAVEFORM (only our package) so it does not sit in the foreground after a test.
function Stop-RfApp {
    param([Parameter(Mandatory)][string]$Serial)
    & $RF_ADB -s $Serial shell am force-stop $RF_APP_ID | Out-Null
}

# Starts the activity fresh and returns its PID. Refuses when another app is in front. $AppArgs
# becomes argv through the 'args' intent extra (RaveformActivity.getArguments).
function Start-RfApp {
    param([Parameter(Mandatory)][string]$Serial, [string]$AppArgs = '')
    if (-not (Test-RfDeviceFree $Serial)) {
        throw "$Serial is in use by another app ($(Get-RfForegroundPackage $Serial)); not launching over it"
    }
    & $RF_ADB -s $Serial shell input keyevent KEYCODE_WAKEUP | Out-Null
    Stop-RfApp $Serial
    $cmd = "am start -W -n $RF_ACTIVITY"
    if ($AppArgs) { $cmd += " --es args '$AppArgs'" }
    & $RF_ADB -s $Serial shell $cmd | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "am start failed on $Serial" }
    return Get-RfPid $Serial
}
