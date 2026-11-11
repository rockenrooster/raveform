# Runs a benchmark scenario on one device, collects its result, saves it to captures\bench\ (with
# battery temperature, thermal status and the device's CPU load during the run) and prints a summary.
#   scripts\bench-android.ps1 -Serial ZY22BXMJG3 -Scenario bench_idle [-Seconds 60] [-Warmup 5] [-AppArgs '--tone'] [-Deploy]
# bench_idle discards a warm-up, then measures -Seconds of present-to-present intervals. The app writes
# the full result (worst-frame table, per-second timeline) to its files dir; the RFBENCH logcat line is
# a summary that fits one log entry.
# Device load: a light sampler on the phone reads /proc/stat and our process's CPU time once a second,
# and every process's CPU time at the start and the end, so a run can be judged quiet or disturbed and
# late frames can be lined up with foreign CPU use (other sessions share these phones).
# Shared phones (CLAUDE.md): each attempt starts only when the launcher or RAVEFORM is in front; a run
# during which RAVEFORM lost focus or went to the background is invalid and is retried after a pause.
param(
    [Parameter(Mandatory)][string]$Serial,
    [string]$Scenario = 'bench_idle',
    [int]$Seconds = 60,
    [double]$Warmup = 5,
    [string]$AppArgs = '',
    [switch]$Deploy,
    [int]$Attempts = 3,
    [int]$IdleSeconds = 10,
    [int]$BusyTimeoutSeconds = 300,
    # Skip the device-load sampler (it costs the phone about one fork per second).
    [switch]$NoLoad,
    # Cross-check: poll `dumpsys SurfaceFlinger --latency` during the run and report the intervals
    # between the frames' actual present times on the panel. The polling costs device CPU and
    # SurfaceFlinger time, so leave it off for reference numbers.
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

# --- Device load sampler ---------------------------------------------------------------------------
# Runs on the phone (sh, toybox): "S0"/"S1" every process's /proc/<pid>/stat at the start and the end,
# "Q" our threads every 5 s, then once a second "T <uptime>", the /proc/stat cpu, ctxt and processes
# lines and "P <our /proc/<pid>/stat>". Inside the loop only grep and sleep start processes. It stops
# when our process exits. Uptime is CLOCK_BOOTTIME, like the app's boottime_start_s, so the samples
# line up with the app's per-second timeline.
$RF_SAMPLER = @'
pid=$1; max=$2; n=0
read up idle < /proc/uptime; echo "S0 $up"; cat /proc/[0-9]*/stat 2>/dev/null
while [ $n -lt $max ] && [ -d /proc/$pid ]; do
  read up idle < /proc/uptime; echo "T $up"
  grep -E '^(cpu|ctxt|processes)' /proc/stat
  read st < /proc/$pid/stat 2>/dev/null && echo "P $st"
  read g < /sys/class/kgsl/kgsl-3d0/gpuclk 2>/dev/null && echo "G $g"
  read b < /sys/class/kgsl/kgsl-3d0/gpu_busy_percentage 2>/dev/null && echo "B $b"
  if [ $((n % 5)) -eq 0 ]; then echo "Q $up"; for f in /proc/$pid/task/*/stat; do read l < $f && echo "$l"; done 2>/dev/null; fi
  sleep 1; n=$((n+1))
done
read up idle < /proc/uptime; echo "S1 $up"; cat /proc/[0-9]*/stat 2>/dev/null
'@

function Start-RfSampler([string]$AppPid, [int]$MaxSeconds, [string]$OutFile) {
    $script = $RF_SAMPLER -replace "`r", ''
    $b64 = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes($script))
    $cmd = "echo $b64 | base64 -d | sh -s $AppPid $MaxSeconds"
    Start-Process -FilePath $RF_ADB -ArgumentList @('-s', $Serial, 'shell', "`"$cmd`"") -NoNewWindow -PassThru `
        -RedirectStandardOutput $OutFile -RedirectStandardError "$OutFile.err"
}

# /proc/<pid>/stat line -> pid, comm, utime+stime (clock ticks); the comm may contain spaces.
function ConvertFrom-ProcStat([string]$line) {
    $open = $line.IndexOf('(')
    $close = $line.LastIndexOf(')')
    if ($open -lt 1 -or $close -lt $open) { return $null }
    $rest = $line.Substring($close + 2) -split ' '
    if ($rest.Count -lt 13) { return $null }
    [pscustomobject]@{
        pid   = $line.Substring(0, $open).Trim()
        comm  = $line.Substring($open + 1, $close - $open - 1)
        ticks = [long]$rest[11] + [long]$rest[12]
    }
}

# Summarises the sampler output for the measured window [start, start + seconds] (boottime seconds).
function Get-RfDeviceLoad([string]$File, [string]$AppPid, $Result) {
    if (-not (Test-Path $File)) { return $null }
    $start = [double]$Result.boottime_start_s
    $window = [double]$Result.seconds
    $hz = 100.0  # USER_HZ
    $samples = [System.Collections.Generic.List[object]]::new()
    $snap = @{ S0 = @{}; S1 = @{} }
    $threads = [ordered]@{}
    $section = ''
    $cur = $null
    foreach ($line in Get-Content $File) {
        if ($line -match '^S([01]) ') { $section = "S$($Matches[1])"; continue }
        if ($line -match '^Q ') { $section = 'Q'; continue }
        if ($line -match '^T ([\d\.]+)') {
            $cur = [pscustomobject]@{ t = [double]$Matches[1]; cpus = [ordered]@{}; ours = $null; forks = $null; ctxt = $null; gpuHz = $null; gpuBusy = $null }
            $samples.Add($cur); $section = 'T'; continue
        }
        if ($section -eq 'T' -and $line -match '^G (\d+)') { $cur.gpuHz = [double]$Matches[1]; continue }
        if ($section -eq 'T' -and $line -match '^B (\d+)') { $cur.gpuBusy = [double]$Matches[1]; continue }
        if ($section -eq 'T' -and $line -match '^processes (\d+)') { $cur.forks = [long]$Matches[1]; continue }
        if ($section -eq 'T' -and $line -match '^ctxt (\d+)') { $cur.ctxt = [long]$Matches[1]; continue }
        if ($section -eq 'T' -and $line -match '^(cpu\d*)\s+(.*)$') {
            $v = [long[]]($Matches[2].Trim() -split '\s+')
            $busy = $v[0] + $v[1] + $v[2] + $v[5] + $v[6] + $v[7]
            $cur.cpus[$Matches[1]] = @($busy, ($busy + $v[3] + $v[4]))
            continue
        }
        if ($section -eq 'T' -and $line -match '^P (.*)$') {
            $p = ConvertFrom-ProcStat $Matches[1]; if ($p) { $cur.ours = $p.ticks }; continue
        }
        if ($section -like 'S?' -and $line -match '^\d+ \(') {
            $p = ConvertFrom-ProcStat $line; if ($p) { $snap[$section][$p.pid] = $p }; continue
        }
        if ($section -eq 'Q' -and $line -match '^\d+ \(') {
            $p = ConvertFrom-ProcStat $line
            if ($p) {
                if (-not $threads.Contains($p.pid)) { $threads[$p.pid] = [pscustomobject]@{ comm = $p.comm; first = $p.ticks; last = $p.ticks } }
                else { $threads[$p.pid].last = $p.ticks }
            }
        }
    }
    # Per-second deltas inside the window.
    $perSecond = @{}
    $forksPerSecond = @{}
    $forksAll = 0; $ctxtAll = 0
    $busyAll = 0.0; $totalAll = 0.0; $oursAll = 0.0; $span = 0.0; $maxForeign = 0.0
    $coreBusy = [ordered]@{}; $coreTotal = [ordered]@{}
    for ($i = 1; $i -lt $samples.Count; ++$i) {
        $a = $samples[$i - 1]; $b = $samples[$i]
        $mid = ($a.t + $b.t) / 2.0
        if ($mid -lt $start -or $mid -gt $start + $window) { continue }
        if (-not $a.cpus.Contains('cpu') -or -not $b.cpus.Contains('cpu')) { continue }
        $dt = $b.t - $a.t
        $busy = $b.cpus['cpu'][0] - $a.cpus['cpu'][0]
        $total = $b.cpus['cpu'][1] - $a.cpus['cpu'][1]
        $ours = if ($null -ne $a.ours -and $null -ne $b.ours) { $b.ours - $a.ours } else { 0 }
        $foreignCores = [Math]::Max(0.0, ($busy - $ours) / $hz / $dt)
        $busyAll += $busy; $totalAll += $total; $oursAll += $ours; $span += $dt
        if ($foreignCores -gt $maxForeign) { $maxForeign = $foreignCores }
        $sec = [int][Math]::Floor($mid - $start)
        $perSecond[$sec] = $foreignCores
        if ($null -ne $a.forks -and $null -ne $b.forks) {
            # The sampler itself starts grep and sleep every second (everything else is a builtin).
            $f = [Math]::Max(0, ($b.forks - $a.forks) - 2)
            $forksPerSecond[$sec] = $f; $forksAll += $f
        }
        if ($null -ne $a.ctxt -and $null -ne $b.ctxt) { $ctxtAll += $b.ctxt - $a.ctxt }
        foreach ($k in $b.cpus.Keys) {
            if ($k -eq 'cpu' -or -not $a.cpus.Contains($k)) { continue }
            $coreBusy[$k] = [double]$coreBusy[$k] + ($b.cpus[$k][0] - $a.cpus[$k][0])
            $coreTotal[$k] = [double]$coreTotal[$k] + ($b.cpus[$k][1] - $a.cpus[$k][1])
        }
    }
    if ($span -le 0) { return $null }
    $foreignAvg = [Math]::Max(0.0, ($busyAll - $oursAll) / $hz / $span)
    # GPU clock (kgsl, Hz) and busy % sampled once a second inside the window (context for GPU timings:
    # the GPU's own governor picks the clock).
    $inWindow = @($samples | Where-Object { $_.t -ge $start -and $_.t -le $start + $window })
    $clocks = @($inWindow | Where-Object { $null -ne $_.gpuHz } | ForEach-Object { $_.gpuHz / 1e6 })
    $busyPct = @($inWindow | Where-Object { $null -ne $_.gpuBusy } | ForEach-Object { $_.gpuBusy })
    $gpuClock = if ($clocks.Count) {
        $m = $clocks | Measure-Object -Average -Minimum -Maximum
        [ordered]@{ avg = [Math]::Round($m.Average, 0); min = [Math]::Round($m.Minimum, 0); max = [Math]::Round($m.Maximum, 0) }
    } else { $null }
    $gpuBusyAvg = if ($busyPct.Count) { [Math]::Round(($busyPct | Measure-Object -Average).Average, 1) } else { $null }
    # Foreign load in the seconds that had late frames versus the clean seconds (app timeline:
    # [second, frames, missed, missed on the panel, ...]).
    $lateLoad = @(); $cleanLoad = @(); $lateForks = @(); $cleanForks = @()
    $lateDetail = @()
    $lateWithStarts = 0
    foreach ($row in @($Result.timeline)) {
        $s = [int]$row[0]
        if (-not $perSecond.ContainsKey($s)) { continue }
        $late = [int]$row[2] -gt 0 -or [int]$row[3] -gt 0
        if ($late) { $lateLoad += $perSecond[$s] } else { $cleanLoad += $perSecond[$s] }
        if ($forksPerSecond.ContainsKey($s)) { if ($late) { $lateForks += $forksPerSecond[$s] } else { $cleanForks += $forksPerSecond[$s] } }
        if ($late) {
            # The sampler second can straddle the app second: look at both neighbours too.
            $starts = 0
            foreach ($k in @(($s - 1), $s, ($s + 1))) { if ($forksPerSecond.ContainsKey($k)) { $starts += $forksPerSecond[$k] } }
            if ($starts -gt 0) { ++$lateWithStarts }
            # [second, missed on the CPU side, missed on the panel, foreign process starts in s-1..s+1, foreign cores in s]
            $lateDetail += , @($s, [int]$row[2], [int]$row[3], $starts, [Math]::Round($perSecond[$s], 2))
        }
    }
    $avg = { param($x) if (@($x).Count) { [Math]::Round((@($x) | Measure-Object -Average).Average, 2) } else { $null } }
    # Top processes between the two snapshots (whole run, our process excluded).
    $spanAll = 0.0
    $s0t = ($samples | Select-Object -First 1).t; $s1t = ($samples | Select-Object -Last 1).t
    if ($s0t -and $s1t) { $spanAll = [Math]::Max(1.0, $s1t - $s0t) }
    $top = foreach ($p in $snap.S1.Values) {
        if ($p.pid -eq $AppPid -or -not $snap.S0.ContainsKey($p.pid)) { continue }
        $d = $p.ticks - $snap.S0[$p.pid].ticks
        if ($d -gt 0) { [pscustomobject]@{ name = $p.comm; pid = [int]$p.pid; cpu_pct = [Math]::Round(100.0 * $d / $hz / $spanAll, 1) } }
    }
    $top = @($top | Sort-Object cpu_pct -Descending | Select-Object -First 10)
    $ourThreads = @($threads.Values | ForEach-Object {
            [pscustomobject]@{ name = $_.comm; cpu_ticks = $_.last - $_.first } } |
        Where-Object { $_.cpu_ticks -gt 0 } | Sort-Object cpu_ticks -Descending | Select-Object -First 8)
    [ordered]@{
        samples                   = $perSecond.Count
        cpu_busy_pct              = [Math]::Round(100.0 * $busyAll / [Math]::Max(1, $totalAll), 1)
        ours_cores                = [Math]::Round($oursAll / $hz / $span, 2)
        foreign_cores_avg         = [Math]::Round($foreignAvg, 2)
        foreign_cores_max_1s      = [Math]::Round($maxForeign, 2)
        foreign_cores_late_s      = & $avg $lateLoad
        foreign_cores_clean_s     = & $avg $cleanLoad
        late_seconds              = $lateLoad.Count
        # New processes started on the phone during the window (the sampler's own excluded): app
        # launches, adb shell commands, other sessions' tools. A quiet phone starts almost none.
        foreign_forks             = $forksAll
        forks_per_s_late          = & $avg $lateForks
        forks_per_s_clean         = & $avg $cleanForks
        context_switches_per_s    = [Math]::Round($ctxtAll / $span, 0)
        late_seconds_with_starts  = $lateWithStarts
        late_detail               = $lateDetail
        gpu_clock_mhz             = $gpuClock
        gpu_busy_pct_avg          = $gpuBusyAvg
        core_busy_pct             = [ordered]@{} + ($coreBusy.Keys | ForEach-Object -Begin { $h = [ordered]@{} } -Process {
                $h[$_] = [Math]::Round(100.0 * $coreBusy[$_] / [Math]::Max(1, $coreTotal[$_]), 1) } -End { $h })
        top_processes_core_pct    = $top
        our_threads_ticks         = $ourThreads
    }
}

# --- Contract grading (docs/PLAN.md section 5, since 2026-11-10) ------------------------------------
# Pass = P95 present-to-present interval within the panel rate's limit, at most 5% of frames missing
# vsync, no frame over 50 ms. Graded twice:
# - panel: what reached the display. Plain FIFO: VK_GOOGLE_display_timing present times; the panel only
#   changes image at a vsync, so intervals are counted in refresh periods (the raw present-fence
#   timestamps scatter about 0.25 ms around the period, which is signalling jitter, not display
#   jitter; the raw P95 is printed too). Swappy: its statistics (frames shown a period or more late).
# - cpu: when the present call returned (jitters a millisecond or two around the period).
function Get-RfContractLimitMs([double]$refreshMs) {
    foreach ($line in @(@(16.667, 16.9), @(11.111, 11.3), @(8.333, 8.6))) {
        if ([Math]::Abs($refreshMs - $line[0]) -lt 0.4) { return $line[1] }
    }
    return [Math]::Round($refreshMs + 0.25, 2)
}

function Get-RfContract($r) {
    $refresh = [double]$r.target_ms
    $limit = Get-RfContractLimitMs $refresh
    $cpuMax = [double]$r.interval_ms.max
    $cpu = [ordered]@{
        p95_ms = [double]$r.interval_ms.p95; missed_pct = [double]$r.missed_pct; max_ms = $cpuMax
    }
    $cpu.pass = $cpu.p95_ms -le $limit -and $cpu.missed_pct -le 5.0 -and $cpu.max_ms -le 50.0
    $panel = $null
    $d = $r.display
    if ($d -and $d.source -eq 'display_timing' -and $d.frames -gt 0) {
        # refresh_periods: intervals of <0.5, 1, 2, 3, 4+ periods.
        $hist = @($d.refresh_periods)
        $total = ($hist | Measure-Object -Sum).Sum
        $acc = 0; $p95Periods = 4
        for ($k = 0; $k -lt $hist.Count; ++$k) { $acc += $hist[$k]; if ($acc -ge 0.95 * $total) { $p95Periods = [Math]::Max(1, $k); break } }
        $period = [double]$d.p50  # the panel's measured period (59.9 Hz on the Moto: 16.695 ms)
        $panel = [ordered]@{
            source = 'display_timing'; p95_periods = $p95Periods; p95_ms = [Math]::Round($p95Periods * $period, 3)
            p95_raw_ms = [double]$d.p95; p99_raw_ms = [double]$d.p99; missed_pct = [double]$d.missed_pct; max_ms = [double]$d.max
        }
        $panel.pass = $panel.p95_ms -le $limit -and $panel.missed_pct -le 5.0 -and $panel.max_ms -le 50.0
    } elseif ($d -and $d.source -eq 'swappy_stats' -and $d.frames -gt 0) {
        $late = @($d.late_periods)
        $missed = 0; for ($k = 1; $k -lt $late.Count; ++$k) { $missed += $late[$k] }
        $pct = [Math]::Round(100.0 * $missed / $d.frames, 2)
        # Swappy counts a frame late only against the time it asked for. When the GPU cannot finish a
        # frame in one period, every frame takes two and none is "late": the frame rate shows it. So a
        # present-call interval above 1.5 periods (the app's "missed", far outside the 1-2 ms jitter)
        # counts as a miss too, and the CPU-side median in whole periods is a floor for the panel.
        $pct = [Math]::Max($pct, [double]$r.missed_pct)
        $base = [Math]::Max(1, [int][Math]::Round([double]$r.interval_ms.p50 / $refresh))
        if ($base -gt 1) { $pct = 100.0 }
        $panel = [ordered]@{
            source = 'swappy_stats'; p95_periods = $(if ($base -gt 1) { $base } elseif ($pct -le 5.0) { 1 } else { 2 }); missed_pct = $pct
            max_ms = $cpuMax  # not measured on the panel side in this mode: the CPU-side maximum stands in
        }
        $panel.p95_ms = [Math]::Round($panel.p95_periods * $refresh, 3)
        $panel.pass = $panel.p95_ms -le $limit -and $pct -le 5.0 -and $cpuMax -le 50.0
    }
    [ordered]@{ refresh_ms = $refresh; p95_limit_ms = $limit; missed_limit_pct = 5.0; max_limit_ms = 50.0
                panel = $panel; cpu = $cpu; pass = $(if ($panel) { $panel.pass } else { $cpu.pass }) }
}

$runArgs = "--scenario $Scenario --seconds $Seconds --warmup $Warmup $AppArgs".Trim()
$resultFile = "/sdcard/Android/data/$RF_APP_ID/files/$Scenario.json"  # written by the app on completion
$result = $null
$stamp = $null
for ($attempt = 1; $attempt -le $Attempts -and -not $result; ++$attempt) {
    if (-not (Wait-RfDeviceIdle -Serial $Serial -QuietSeconds $IdleSeconds -TimeoutSeconds $BusyTimeoutSeconds)) {
        throw "$Serial stayed in use by another session for $BusyTimeoutSeconds s; benchmark not run"
    }
    $before = Get-DeviceHeat
    & $RF_ADB -s $Serial shell rm -f $resultFile | Out-Null
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    Write-Host "[bench] $Serial attempt ${attempt}: $runArgs"
    $launched = Get-Date
    $appPid = Start-RfApp -Serial $Serial -AppArgs $runArgs
    if (-not $appPid) { throw "RAVEFORM did not start on $Serial" }
    $loadFile = Join-Path $env:TEMP ("rf-load-{0}-{1}.txt" -f $Serial, $stamp)
    $sampler = if ($NoLoad) { $null } else { Start-RfSampler -AppPid $appPid -MaxSeconds ([int]($Warmup + $Seconds + 60)) -OutFile $loadFile }

    # What the panel scans out at, read during the warm-up (dumpsys display is not free).
    Start-Sleep -Seconds ([Math]::Max(1, [Math]::Min(2, $Warmup - 1)))
    $display = ((& $RF_ADB -s $Serial shell dumpsys display) -match 'mActiveModeId|renderFrameRate [0-9]' |
                Select-Object -First 2 | ForEach-Object { $t = $_.Trim(); $t.Substring(0, [Math]::Min(160, $t.Length)) }) -join ' | '

    # No adb traffic until the measured window is surely over (the app starts a second or two after
    # $launched), except the optional SurfaceFlinger polling: every adb shell command runs processes on
    # the phone, and the old 2 s logcat polling measurably cost frames.
    $sfSet = [System.Collections.Generic.HashSet[long]]::new()
    $sfRefresh = 0
    $layer = if ($SfLatency) { Get-SfLayer } else { $null }
    $expectedEnd = $launched.AddSeconds($Warmup + $Seconds + 6)
    while ((Get-Date) -lt $expectedEnd) {
        if ($layer) {
            Start-Sleep -Milliseconds 700
            $r = Add-SfPresents $layer $sfSet
            if ($r -gt 0) { $sfRefresh = $r }
        } else {
            Start-Sleep -Milliseconds ([Math]::Min(2000, [Math]::Max(100, ($expectedEnd - (Get-Date)).TotalMilliseconds)))
        }
    }
    # Then wait for the result file (or the process to vanish), with shell builtins only.
    $deadline = (Get-Date).AddSeconds(40)
    while ((Get-Date) -lt $deadline) {
        $state = "$(& $RF_ADB -s $Serial shell "test -f $resultFile && echo done || { test -d /proc/$appPid || echo gone; }")".Trim()
        if ($state) { break }
        Start-Sleep -Seconds 1
    }
    if ($sampler) { [void]$sampler.WaitForExit(30000); if (-not $sampler.HasExited) { $sampler.Kill() } }

    $text = (& $RF_ADB -s $Serial shell cat $resultFile 2>$null) -join ''
    if ($text -notmatch '^\{') {
        # No file (aborted run, or the app is still running): the RFBENCH summary line, if any.
        $line = (& $RF_ADB -s $Serial logcat -d --pid=$appPid -s RFBENCH:I) -match 'RFBENCH' | Select-Object -Last 1
        $text = if ($line) { $line.Substring($line.IndexOf('{')) } else { '' }
    }
    if (-not $text) {
        Write-Host "[bench] attempt ${attempt}: no result (foreground: $(Get-RfForegroundPackage $Serial))"
        Stop-RfApp $Serial
        continue
    }
    $parsed = $text | ConvertFrom-Json
    if ($parsed.aborted -or -not $parsed.valid) {
        $why = if ($parsed.aborted) { $parsed.aborted } else { "focus lost $($parsed.interruptions)x" }
        Write-Host "[bench] attempt $attempt invalid ($why); foreground now: $(Get-RfForegroundPackage $Serial)"
        Stop-RfApp $Serial
        Start-Sleep -Seconds 20
        continue
    }
    $parsed | Add-Member -NotePropertyName serial -NotePropertyValue $Serial
    $parsed | Add-Member -NotePropertyName attempt -NotePropertyValue $attempt
    $parsed | Add-Member -NotePropertyName app_args -NotePropertyValue $runArgs
    $parsed | Add-Member -NotePropertyName heat_before -NotePropertyValue $before
    $parsed | Add-Member -NotePropertyName heat_after -NotePropertyValue (Get-DeviceHeat)
    $parsed | Add-Member -NotePropertyName display_dumpsys -NotePropertyValue $display
    if (-not $NoLoad) {
        $parsed | Add-Member -NotePropertyName device_load -NotePropertyValue (Get-RfDeviceLoad $loadFile $appPid $parsed)
    }
    if ($SfLatency) {
        $parsed | Add-Member -NotePropertyName surfaceflinger -NotePropertyValue (Get-SfSummary $sfSet $sfRefresh)
    }
    $parsed | Add-Member -NotePropertyName contract -NotePropertyValue (Get-RfContract $parsed)
    $result = $parsed
}
if (Get-RfPid $Serial) { Stop-RfApp $Serial }  # leave the phone tidy
if (-not $result) { throw "no valid benchmark on $Serial after $Attempts attempts" }

$dir = Join-Path $RF_ROOT 'captures\bench'
New-Item -ItemType Directory -Force $dir | Out-Null
$file = Join-Path $dir ("{0}-{1}-{2}.json" -f $Serial, $Scenario, $stamp)
$result | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 $file
if (-not $NoLoad -and (Test-Path $loadFile)) {
    Move-Item -Force $loadFile ([IO.Path]::ChangeExtension($file, '.load.txt'))
    Remove-Item -ErrorAction SilentlyContinue "$loadFile.err"
}

$iv = $result.interval_ms
Write-Host ("[bench] {0} {1}: surface {2}x{3}, {4} @ {5} Hz (voted {6}, target {7} ms), {8} frames in {9} s, overlay {10}" -f `
        $result.device, $Scenario, $result.surface[0], $result.surface[1], $result.present, $result.display_hz,
        $result.voted_hz, $result.target_ms, $result.frames, $result.seconds, $result.overlay)
Write-Host ("[bench] interval ms (present call): p50 {0}  p95 {1}  p99 {2}  p99.9 {3}  max {4}; missed {5} ({6}%); over 50 ms {7}" -f `
        $iv.p50, $iv.p95, $iv.p99, $iv.p999, $iv.max, $result.missed, $result.missed_pct, $result.over_50ms)
if ($result.display) {
    $d = $result.display
    if ($d.source -eq 'display_timing') {
        Write-Host ("[bench] panel (display timing): {0} frames, p50 {1}  p95 {2}  p99 {3}  max {4}; missed {5} ({6}%), periods {7}" -f `
                $d.frames, $d.p50, $d.p95, $d.p99, $d.max, $d.missed, $d.missed_pct, ($d.refresh_periods -join '/'))
    } else {
        Write-Host ("[bench] panel (swappy stats): {0} frames, shown late by n refresh periods {1} (missed {2} = {3}%); latency periods {4}" -f `
                $d.frames, ($d.late_periods -join '/'), $d.missed, $d.missed_pct, ($d.latency_periods -join '/'))
    }
}
$l = $result.late
Write-Host ("[bench] late frames: {0} (own work {1}, waits {2}; run-queue wait >= 2 ms {3}; major faults {4}); dominant {5}" -f `
        $l.missed, $l.own_work, $l.waits, $l.run_wait_2ms, $l.major_faults, (($l.dominant_phase.PSObject.Properties | ForEach-Object { "$($_.Name) $($_.Value)" }) -join ', '))
$s = $result.sched
Write-Host ("[bench] main thread: cpu p50 {0} p95 {9} p99 {1} max {2} ms; work p99 {3} max {4} ms; run-queue wait {5} ms total, max {6} ms; involuntary switches {7}; frames by CPU {8}" -f `
        $result.cpu_ms.p50, $result.cpu_ms.p99, $result.cpu_ms.max, $result.work_ms.p99, $result.work_ms.max, $s.run_wait_ms,
        $s.run_wait_max_ms, $s.nivcsw, ($s.frames_by_cpu -join '/'), $result.cpu_ms.p95)
if ($result.gpu_ms) {
    $g = $result.gpu_ms
    Write-Host ("[bench] GPU ms [p50/p95/max] ({0} frames): total {1}; scene {2}; pools {3}; background {9}; bloom {4}; afterglow {5}; distortion {6}; composite {7}; wait for the image {8}" -f `
            $g.frames, ($g.total -join '/'), ($g.scene -join '/'), ($g.pools -join '/'), ($g.bloom -join '/'), ($g.afterglow -join '/'),
            ($g.distortion -join '/'), ($g.composite -join '/'), ($g.wait -join '/'), ($g.background -join '/'))
}
if ($result.scene_counts) {
    $c = $result.scene_counts
    Write-Host ("[bench] tier {0}, look {1}, palette {2}; per frame [mean/max]: enemies in view {3}, bullets in view {4}, glyphs {5}, sparks {6}, capsules {7}; kills {8}, Drops {9}, boss breaks {10}, enemy slots {11}" -f `
            $result.tier, $result.look, $result.palette, ($c.enemies_in_view -join '/'), ($c.bullets_in_view -join '/'),
            ($c.glyph_instances -join '/'), ($c.sparks_live -join '/'), ($c.capsules -join '/'), $c.kills, $c.drops, $c.boss_breaks,
            $c.enemy_slots)
    if ($c.crowd_gain) {
        Write-Host ("[bench] light budget [mean/min]: crowd gain {0}, swarm gain {1}; load [mean/max] {2}" -f `
                ($c.crowd_gain -join '/'), ($c.swarm_gain -join '/'), ($c.load -join '/'))
    }
}
Write-Host ("[bench] audio {0} {1} Hz burst {2} buffer {3}: underruns {4}, worst callback {5} ms; heap allocs ours {6}, in Swappy {7}, other threads {8}; battery {9} -> {10} C, thermal {11} -> {12}" -f `
        $result.audio.api, $result.audio.rate, $result.audio.burst, $result.audio.buffer, $result.audio.underruns,
        $result.audio.callback_worst_ms, $result.heap_allocs, $result.heap_allocs_in_pacer, $result.heap_allocs_other_threads,
        $result.heat_before.battery_c, $result.heat_after.battery_c, $result.heat_before.thermal_status, $result.heat_after.thermal_status)
if ($result.device_load) {
    $dl = $result.device_load
    Write-Host ("[bench] device load: busy {0}% of all cores; ours {1} cores; foreign {2} cores avg, {3} max (1 s); foreign in late seconds {4} vs clean {5} ({6} late s)" -f `
            $dl.cpu_busy_pct, $dl.ours_cores, $dl.foreign_cores_avg, $dl.foreign_cores_max_1s, $dl.foreign_cores_late_s,
            $dl.foreign_cores_clean_s, $dl.late_seconds)
    if ($dl.gpu_clock_mhz) {
        Write-Host ("[bench] GPU clock {0} MHz avg ({1}-{2}), busy {3}% avg" -f $dl.gpu_clock_mhz.avg, $dl.gpu_clock_mhz.min,
            $dl.gpu_clock_mhz.max, $dl.gpu_busy_pct_avg)
    }
    Write-Host ("[bench] new processes on the phone in the window: {0} (per s: late {1}, clean {2}); context switches {3}/s; late seconds with foreign process starts within 1 s: {4} of {5}" -f `
            $dl.foreign_forks, $dl.forks_per_s_late, $dl.forks_per_s_clean, $dl.context_switches_per_s,
            $dl.late_seconds_with_starts, $dl.late_seconds)
    Write-Host ("[bench] top processes (% of one core): {0}" -f (($dl.top_processes_core_pct | Select-Object -First 6 | ForEach-Object { "$($_.name) $($_.cpu_pct)" }) -join ', '))
}
Write-Host "[bench] display: $($result.display_dumpsys)"
$c = $result.contract
$verdict = { param($ok) if ($ok) { 'PASS' } else { 'FAIL' } }
Write-Host ("[bench] contract at {0:N2} Hz: P95 <= {1} ms, missed <= 5%, max <= 50 ms" -f (1000.0 / $c.refresh_ms), $c.p95_limit_ms)
if ($c.panel -and $c.panel.source -eq 'display_timing') {
    Write-Host ("[bench]   panel (graded): P95 {0} period(s) = {1} ms, missed {2}%, max {3} ms -> {4}   [raw present-fence P95 {5} ms, P99 {6} ms]" -f `
            $c.panel.p95_periods, $c.panel.p95_ms, $c.panel.missed_pct, $c.panel.max_ms, (& $verdict $c.panel.pass),
            $c.panel.p95_raw_ms, $c.panel.p99_raw_ms)
} elseif ($c.panel) {
    Write-Host ("[bench]   panel (graded, Swappy statistics): missed {0}% -> P95 {1} period(s) = {2} ms; max (CPU side) {3} ms -> {4}" -f `
            $c.panel.missed_pct, $c.panel.p95_periods, $c.panel.p95_ms, $c.panel.max_ms, (& $verdict $c.panel.pass))
}
Write-Host ("[bench]   cpu (present call returns): P95 {0} ms, missed {1}%, max {2} ms -> {3}" -f `
        $c.cpu.p95_ms, $c.cpu.missed_pct, $c.cpu.max_ms, (& $verdict $c.cpu.pass))
if ([Math]::Abs($result.display_hz - $result.voted_hz) -gt 1 -and $result.voted_hz -gt 0) {
    Write-Host ("[bench]   note: the app voted {0} Hz but the panel ran at {1} Hz (graded at the panel's rate)" -f $result.voted_hz, $result.display_hz)
}
if ($result.surfaceflinger) {
    $sf = $result.surfaceflinger
    Write-Host ("[bench] SurfaceFlinger present-to-present ({0} intervals, refresh {1} ms): p50 {2}  p95 {3}  p99 {4}  max {5}; missed {6}" -f `
            $sf.intervals, $sf.refresh_ms, $sf.p50, $sf.p95, $sf.p99, $sf.max, $sf.missed)
}
Write-Host "[bench] saved $file"
exit 0
