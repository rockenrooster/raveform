# Runs a benchmark scenario on one device, collects its RFBENCH result, saves it to captures\bench\
# (with battery temperature and thermal status) and prints a summary.
#   scripts\bench-android.ps1 -Serial ZY22BXMJG3 -Scenario bench_idle [-Seconds 60] [-AppArgs '--tone'] [-Deploy]
# bench_idle discards a 2 s warm-up, then measures -Seconds of present-to-present intervals.
# Shared phones (CLAUDE.md): each attempt starts only when the launcher or RAVEFORM is in front; a run
# during which RAVEFORM lost focus or went to the background is invalid and is retried after a pause.
param(
    [Parameter(Mandatory)][string]$Serial,
    [string]$Scenario = 'bench_idle',
    [int]$Seconds = 60,
    [string]$AppArgs = '',
    [switch]$Deploy,
    [int]$Attempts = 3,
    [int]$IdleSeconds = 10,
    [int]$BusyTimeoutSeconds = 300,
    # Cross-check: poll `dumpsys SurfaceFlinger --latency` during the run and report the intervals
    # between the frames' actual present times on the panel. The polling costs a little device CPU,
    # so leave it off for the reference numbers.
    [switch]$SfLatency
)
. "$PSScriptRoot\env.ps1"
$Serial = Resolve-RfSerial $Serial
Assert-RfDevice $Serial
if ($Deploy) { & "$PSScriptRoot\deploy-android.ps1" -Serial $Serial -NoLaunch }

function Get-DeviceHeat {
    $temp = (& $RF_ADB -s $Serial shell dumpsys battery) -match '^\s*temperature:' | Select-Object -First 1
    $thermal = (& $RF_ADB -s $Serial shell dumpsys thermalservice) -match 'Thermal Status:' | Select-Object -First 1
    [ordered]@{
        battery_c      = if ($temp) { [double]($temp -replace '\D', '') / 10.0 } else { $null }
        thermal_status = if ($thermal) { [int]($thermal -replace '\D', '') } else { $null }
    }
}

function Get-SfLayer {
    $list = & $RF_ADB -s $Serial shell dumpsys SurfaceFlinger --list
    $hit = $list | Where-Object { $_ -match 'SurfaceView' -and $_ -match [regex]::Escape($RF_APP_ID) -and $_ -notmatch 'Background|Bounds' } |
        Select-Object -First 1
    if ($hit) { return $hit.Trim() } else { return $null }
}

# Adds the actual present times (ns) of the layer's recent frames to $set; returns the refresh period.
function Add-SfPresents([string]$layer, [System.Collections.Generic.HashSet[long]]$set) {
    $rows = & $RF_ADB -s $Serial shell "dumpsys SurfaceFlinger --latency '$layer'"
    if (-not $rows) { return 0 }
    foreach ($row in $rows | Select-Object -Skip 1) {
        $cols = $row -split '\s+' | Where-Object { $_ }
        if ($cols.Count -ge 2) {
            $actual = [long]$cols[1]
            if ($actual -gt 0 -and $actual -lt [long]9e18) { [void]$set.Add($actual) }
        }
    }
    return [long]$rows[0]
}

function Get-SfSummary([System.Collections.Generic.HashSet[long]]$set, [long]$refreshNs) {
    $t = [long[]]@($set) ; [Array]::Sort($t)
    $d = for ($k = 1; $k -lt $t.Count; ++$k) { ($t[$k] - $t[$k - 1]) / 1e6 }
    $d = [double[]]@($d | Where-Object { $_ -lt 200 })  # larger gaps are polling holes, not frames
    if ($d.Count -eq 0) { return $null }
    [Array]::Sort($d)
    $rank = { param($p) $d[[Math]::Max(0, [Math]::Ceiling($p * $d.Count) - 1)] }
    $refreshMs = $refreshNs / 1e6
    [ordered]@{
        intervals  = $d.Count
        refresh_ms = [Math]::Round($refreshMs, 3)
        p50        = [Math]::Round((& $rank 0.50), 3)
        p95        = [Math]::Round((& $rank 0.95), 3)
        p99        = [Math]::Round((& $rank 0.99), 3)
        max        = [Math]::Round($d[$d.Count - 1], 3)
        missed     = @($d | Where-Object { $_ -gt 1.5 * $refreshMs }).Count
    }
}

$runArgs = "--scenario $Scenario --seconds $Seconds $AppArgs".Trim()
$resultFile = "/sdcard/Android/data/$RF_APP_ID/files/$Scenario.json"  # written by the app on completion
$result = $null
for ($attempt = 1; $attempt -le $Attempts -and -not $result; ++$attempt) {
    if (-not (Wait-RfDeviceIdle -Serial $Serial -QuietSeconds $IdleSeconds -TimeoutSeconds $BusyTimeoutSeconds)) {
        throw "$Serial stayed in use by another session for $BusyTimeoutSeconds s; benchmark not run"
    }
    $before = Get-DeviceHeat
    & $RF_ADB -s $Serial shell rm -f $resultFile | Out-Null
    Write-Host "[bench] $Serial attempt ${attempt}: $runArgs"
    $appPid = Start-RfApp -Serial $Serial -AppArgs $runArgs
    if (-not $appPid) { throw "RAVEFORM did not start on $Serial" }

    # What the panel really scans out at while the run is in progress.
    Start-Sleep -Seconds 6
    $display = ((& $RF_ADB -s $Serial shell dumpsys display) -match 'mActiveModeId|renderFrameRate [0-9]' |
                Select-Object -First 2 | ForEach-Object { $t = $_.Trim(); $t.Substring(0, [Math]::Min(160, $t.Length)) }) -join ' | '

    $deadline = (Get-Date).AddSeconds($Seconds + 40)
    $line = $null
    $sfSet = [System.Collections.Generic.HashSet[long]]::new()
    $sfRefresh = 0
    $layer = if ($SfLatency) { Get-SfLayer } else { $null }
    while (-not $line -and (Get-Date) -lt $deadline) {
        if ($layer) {
            Start-Sleep -Milliseconds 700
            $r = Add-SfPresents $layer $sfSet
            if ($r -gt 0) { $sfRefresh = $r }
        } else {
            Start-Sleep -Seconds 2
        }
        $line = (& $RF_ADB -s $Serial logcat -d --pid=$appPid -s RFBENCH:I) -match 'RFBENCH' | Select-Object -Last 1
        if (-not $line -and -not (Get-RfPid $Serial)) {
            # Process gone: the line may have scrolled out of a busy log buffer; use the app's own file.
            $text = (& $RF_ADB -s $Serial shell cat $resultFile 2>$null) -join ''
            if ($text -match '^\{') { $line = "RFBENCH: $text" } else { break }
        }
    }
    if (-not $line) {
        Write-Host "[bench] attempt ${attempt}: no result (foreground: $(Get-RfForegroundPackage $Serial))"
        Stop-RfApp $Serial
        continue
    }
    $parsed = $line.Substring($line.IndexOf('{')) | ConvertFrom-Json
    if ($parsed.aborted -or -not $parsed.valid) {
        $why = if ($parsed.aborted) { $parsed.aborted } else { "focus lost $($parsed.interruptions)x" }
        Write-Host "[bench] attempt $attempt invalid ($why); foreground now: $(Get-RfForegroundPackage $Serial)"
        Stop-RfApp $Serial
        Start-Sleep -Seconds 20
        continue
    }
    $parsed | Add-Member -NotePropertyName serial -NotePropertyValue $Serial
    $parsed | Add-Member -NotePropertyName attempt -NotePropertyValue $attempt
    $parsed | Add-Member -NotePropertyName heat_before -NotePropertyValue $before
    $parsed | Add-Member -NotePropertyName heat_after -NotePropertyValue (Get-DeviceHeat)
    $parsed | Add-Member -NotePropertyName display_dumpsys -NotePropertyValue $display
    if ($SfLatency) {
        $parsed | Add-Member -NotePropertyName surfaceflinger -NotePropertyValue (Get-SfSummary $sfSet $sfRefresh)
    }
    $result = $parsed
}
if (Get-RfPid $Serial) { Stop-RfApp $Serial }  # leave the phone tidy
if (-not $result) { throw "no valid benchmark on $Serial after $Attempts attempts" }

$dir = Join-Path $RF_ROOT 'captures\bench'
New-Item -ItemType Directory -Force $dir | Out-Null
$file = Join-Path $dir ("{0}-{1}-{2}.json" -f $Serial, $Scenario, (Get-Date -Format 'yyyyMMdd-HHmmss'))
$result | ConvertTo-Json -Depth 6 | Set-Content -Encoding utf8 $file

$iv = $result.interval_ms
Write-Host ("[bench] {0} {1}: surface {2}x{3}, {4} @ {5} Hz (target {6} ms), {7} frames in {8} s" -f `
        $result.device, $Scenario, $result.surface[0], $result.surface[1], $result.present, $result.display_hz,
        $result.target_ms, $result.frames, $result.seconds)
Write-Host ("[bench] interval ms: p50 {0}  p95 {1}  p99 {2}  p99.9 {3}  max {4}; missed {5} ({6}%); cpu p50 {7} p99 {8}" -f `
        $iv.p50, $iv.p95, $iv.p99, $iv.p999, $iv.max, $result.missed, $result.missed_pct, $result.cpu_ms.p50, $result.cpu_ms.p99)
Write-Host ("[bench] audio {0} {1} Hz burst {2} buffer {3}: underruns {4}, worst callback {5} ms; heap allocs {6}; battery {7} -> {8} C, thermal {9} -> {10}" -f `
        $result.audio.api, $result.audio.rate, $result.audio.burst, $result.audio.buffer, $result.audio.underruns,
        $result.audio.callback_worst_ms, $result.heap_allocs, $result.heat_before.battery_c, $result.heat_after.battery_c,
        $result.heat_before.thermal_status, $result.heat_after.thermal_status)
Write-Host "[bench] display: $($result.display_dumpsys)"
if ($result.surfaceflinger) {
    $sf = $result.surfaceflinger
    Write-Host ("[bench] SurfaceFlinger present-to-present ({0} intervals, refresh {1} ms): p50 {2}  p95 {3}  p99 {4}  max {5}; missed {6}" -f `
            $sf.intervals, $sf.refresh_ms, $sf.p50, $sf.p95, $sf.p99, $sf.max, $sf.missed)
}
Write-Host "[bench] saved $file"
