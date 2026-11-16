# Co-op across several phones at once (2026-11-15, the owner: "i now have 4 devices connected, please do several co-op runs to find
# any issues"). The first serial hosts a room (--coop host, the run starts once every seat is taken), the others join it by its
# address, its room code or the list of rooms heard; every seat's bot plays. The run lasts -Minutes from its start; then each
# device's RAVEFORM log is saved (captures\coop4\<stamp>_<name>\<device>.log) and scanned (summary.txt).
# Under the shared-phone rules (CLAUDE.md): a lock per device (name coop4), each foreground checked first (the launcher or
# RAVEFORM only), -s on every adb call, nothing device-wide, our APK deployed first (unless -NoDeploy), force-stop of
# dev.raveform.game only, the locks removed after.
#   scripts\coop-phones.ps1 -Serials <host>,<a>,<b>,<c> [-Minutes 5] [-Join address|code|list] [-HostArgs '...']
#       [-JoinArgs '...','...'] [-HomeIndex i -HomeAt s -HomeFor s] [-Name run1] [-Out captures\coop4] [-NoDeploy]
#   -HomeIndex/-HomeAt/-HomeFor: HOME on device i (0 the host) s seconds into the run, back (its launcher icon) HomeFor s later.
param(
    [Parameter(Mandatory)][string[]]$Serials,
    [double]$Minutes = 5,
    [ValidateSet('address', 'code', 'list')][string]$Join = 'address',
    [string]$HostArgs = '',
    [string[]]$JoinArgs = @(),
    [int]$HomeIndex = -1,
    [double]$HomeAt = 0,
    [double]$HomeFor = 15,
    [int]$KillIndex = -1,   # force-stop device i (0 the host) KillAt s into the run: a player's app dying without a word
    [double]$KillAt = 0,
    [string]$Name = 'run',
    [string]$Out = 'captures\coop4',
    [string]$LockName = 'coop4',
    [switch]$NoDeploy
)
. "$PSScriptRoot\env.ps1"
if (-not (Get-Command Get-RfDeviceId -ErrorAction SilentlyContinue)) {
    # (an env.ps1 without it: the hardware serial behind an ip:port transport, for lock and log names)
    function Get-RfDeviceId([string]$Serial) {
        if ($Serial -notmatch ':') { return $Serial }
        $id = "$(& $RF_ADB -s $Serial shell getprop ro.serialno 2>$null)".Trim()
        if ($id) { return $id } else { return ($Serial -replace '[^\w.-]', '_') }
    }
}
$Serials = @($Serials | ForEach-Object { Resolve-RfSerial $_ })
foreach ($s in $Serials) { Assert-RfDevice $s }
$ids = @($Serials | ForEach-Object { Get-RfDeviceId $_ })
$stamp = Get-Date -Format 'HHmmss'
$dir = [IO.Path]::GetFullPath((Join-Path (Get-Location) (Join-Path $Out "${stamp}_$Name")))
New-Item -ItemType Directory -Force $dir | Out-Null
$summary = Join-Path $dir 'summary.txt'
function Say([string]$t) { $line = "[$(Get-Date -Format 'HH:mm:ss')] $t"; Write-Host $line; Add-Content $summary $line }
function Lock-Path([string]$id) { Join-Path $RF_ROOT "captures\locks\$id.lock" }
function Get-Ip([string]$s) {
    $m = (& $RF_ADB -s $s shell "ip -4 addr show wlan0") | Select-String 'inet (\d+\.\d+\.\d+\.\d+)' | Select-Object -First 1
    if ($m) { return $m.Matches[0].Groups[1].Value } else { return '' }
}
function Get-RoomCode([string]$Ip) {
    # TECH_SPEC 8.3 (src/net/lobby.cpp): the low 16 bits of the host's address, four symbols of "34679ACDFHJKMPQR".
    $o = $Ip.Split('.') | ForEach-Object { [int]$_ }
    $low = ($o[2] -shl 8) -bor $o[3]
    $a = '34679ACDFHJKMPQR'
    -join (0..3 | ForEach-Object { $a[($low -shr (12 - 4 * $_)) -band 15] })
}

# Free and lockable: no other coder's lock under 30 minutes, nothing but the launcher or RAVEFORM in front.
for ($k = 0; $k -lt $Serials.Count; ++$k) {
    $lock = Lock-Path $ids[$k]
    if (Test-Path $lock) {
        $t = (Get-Content $lock -Raw).Trim()
        if ($t -notmatch "^$([regex]::Escape($LockName))\b" -and ((Get-Date) - (Get-Item $lock).LastWriteTime).TotalMinutes -lt 30) { throw "$($ids[$k]) is busy: lock '$t'" }
    }
    if (-not (Test-RfDeviceFree $Serials[$k])) { throw "$($ids[$k]) is in use: $(Get-RfForegroundPackage $Serials[$k]) in front" }
}
foreach ($id in $ids) { New-Item -ItemType Directory -Force (Split-Path (Lock-Path $id)) | Out-Null; Set-Content -Encoding ascii (Lock-Path $id) ("$LockName " + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')) }
$since = @{}
$pids = @{}
$lastTs = @{}
# The logs are read every half minute and appended (a phone's log buffer can roll over within a run: the OnePlus's did).
function Save-Logs {
    for ($k = 0; $k -lt $Serials.Count; ++$k) {
        if (-not $since.ContainsKey($k)) { continue }
        $from = if ($lastTs.ContainsKey($k)) { $lastTs[$k] } else { $since[$k] }
        $got = & $RF_ADB -s $Serials[$k] shell "logcat -d -v threadtime -T '$from' -s 'RAVEFORM:*' 'RFLIFE:*' 'AndroidRuntime:*' 'DEBUG:*' 'libc:*' 'SDL:*'"
        $new = @($got | Where-Object { $_ -match '^\d\d-\d\d \d\d:\d\d:\d\d\.\d\d\d' -and (-not $lastTs.ContainsKey($k) -or $_.Substring(0, 18) -gt $lastTs[$k]) })
        if ($new.Count) {
            Add-Content (Join-Path $dir "$($ids[$k]).log") $new
            $lastTs[$k] = $new[-1].Substring(0, 18)
        }
    }
}
try {
    if (-not $NoDeploy) {
        foreach ($s in $Serials) { & "$PSScriptRoot\deploy-android.ps1" -Serial $s -NoBuild -NoLaunch | Out-Null }
        foreach ($s in $Serials) { if (-not (Wait-RfDeviceIdle $s -QuietSeconds 3 -TimeoutSeconds 120)) { throw "$s is busy after the install" } }
    }
    $hostIp = Get-Ip $Serials[0]
    if (-not $hostIp) { throw "the host has no Wi-Fi address" }
    $target = switch ($Join) { 'address' { $hostIp } 'code' { Get-RoomCode $hostIp } 'list' { 'list' } }
    Say "run '$Name': host $($ids[0]) at $hostIp; $($Serials.Count - 1) join by $Join ($target); $Minutes min"
    for ($k = 0; $k -lt $Serials.Count; ++$k) {
        $s = $Serials[$k]
        $since[$k] = "$(& $RF_ADB -s $s shell "date '+%m-%d %H:%M:%S.000'")".Trim()
        $appArgs = if ($k -eq 0) { "--coop host --coop-name HOST --coop-start $($Serials.Count) --bot $HostArgs" }
                   else { "--coop join:$target --coop-name P$($k + 1) --bot $(if ($JoinArgs.Count -ge $k) { $JoinArgs[$k - 1] })" }
        $pids[$k] = Start-RfApp $s $appArgs.Trim()
        Say "$($ids[$k]): pid $($pids[$k]), args: $($appArgs.Trim())"
        Start-Sleep -Seconds $(if ($k -eq 0) { 5 } else { 2 })
    }
    # The run's start, as the host logs it.
    $deadline = (Get-Date).AddSeconds(150)
    $started = $false
    while ((Get-Date) -lt $deadline -and -not $started) {
        Start-Sleep -Seconds 3
        $started = [bool]((& $RF_ADB -s $Serials[0] shell "logcat -d -T '$($since[0])' -s RAVEFORM:*") | Select-String 'the run starts' -Quiet)
    }
    if (-not $started) { Say 'NO RUN START within 150 s (the logs are saved)' }
    else { Say 'the run started' }
    $t0 = Get-Date
    $homeDone = $false; $backDone = $false
    while ($started -and ((Get-Date) - $t0).TotalSeconds -lt $Minutes * 60) {
        Start-Sleep -Seconds 5
        $el = ((Get-Date) - $t0).TotalSeconds
        if ($HomeIndex -ge 0 -and -not $homeDone -and $el -ge $HomeAt) {
            & $RF_ADB -s $Serials[$HomeIndex] shell input keyevent KEYCODE_HOME | Out-Null
            Say "HOME on $($ids[$HomeIndex]) at $([int]$el) s"
            $homeDone = $true
        }
        if ($homeDone -and -not $backDone -and $el -ge $HomeAt + $HomeFor) {
            & $RF_ADB -s $Serials[$HomeIndex] shell "monkey -p $RF_APP_ID -c android.intent.category.LAUNCHER 1" | Out-Null
            Say "back to RAVEFORM on $($ids[$HomeIndex]) at $([int]$el) s"
            $backDone = $true
        }
        if ($KillIndex -ge 0 -and $pids[$KillIndex] -and $el -ge $KillAt) {
            Stop-RfApp $Serials[$KillIndex]
            Say "force-stopped RAVEFORM on $($ids[$KillIndex]) at $([int]$el) s"
            $pids[$KillIndex] = ''
        }
        for ($k = 0; $k -lt $Serials.Count; ++$k) {
            if ($pids[$k] -and -not (Get-RfPid $Serials[$k])) { Say "$($ids[$k]): THE APP IS GONE at $([int]$el) s"; $pids[$k] = '' }
        }
        if ([int]$el % 30 -lt 5) { Save-Logs }
    }
} finally {
    Save-Logs
    for ($k = 0; $k -lt $Serials.Count; ++$k) {
        $s = $Serials[$k]
        if ($since.ContainsKey($k)) {
            & $RF_ADB -s $s shell "logcat -d -b crash -v threadtime -T '$($since[$k])'" | Add-Content (Join-Path $dir "$($ids[$k]).crash.log")
        }
        Stop-RfApp $s
        Remove-Item -Force (Lock-Path $ids[$k]) -ErrorAction SilentlyContinue
    }
    Say 'apps stopped, devices unlocked'
}

# The scan: per device, the session's moments, the warnings and errors, the last network and music lines, any crash.
for ($k = 0; $k -lt $Serials.Count; ++$k) {
    $log = Join-Path $dir "$($ids[$k]).log"
    if (-not (Test-Path $log)) { continue }
    $lines = Get-Content $log
    Add-Content $summary "`n== $($ids[$k]) ($(if ($k -eq 0) { 'host' } else { "seat P$($k + 1)" })): $($lines.Count) lines"
    $cut = { param($l) if ($l.Length -gt 260) { $l.Substring(0, 260) } else { $l } }
    $moments = $lines | Select-String 'net: (hosting|joining|the run starts|away|back|seat \d+ left|the session|--coop)|SET COMPLETE|falls \(|game over|RFLIFE' | Select-Object -First 30
    foreach ($m in $moments) { Add-Content $summary ('  ' + (& $cut $m.Line)) }
    $warn = $lines | Where-Object { $_ -match '^\S+ \S+\s+\d+\s+\d+ [WEF] ' } | ForEach-Object { ($_ -replace '^\S+ \S+\s+\d+\s+\d+ ', '') } | Group-Object | Sort-Object Count -Descending | Select-Object -First 15
    foreach ($w in $warn) { Add-Content $summary ("  [x$($w.Count)] " + (& $cut $w.Name)) }
    foreach ($pat in 'net: frame', 'net: music', 'net: world') {
        $last = $lines | Select-String $pat | Select-Object -Last 1
        if ($last) { Add-Content $summary ('  last ' + (& $cut ($last.Line -replace '^.*?RAVEFORM\s*:\s*', ''))) }
    }
    $crash = Join-Path $dir "$($ids[$k]).crash.log"
    if ((Test-Path $crash) -and (Get-Content $crash | Where-Object { $_ -match 'dev\.raveform|FATAL|signal' })) { Add-Content $summary '  CRASH: see the crash log' }
}
Get-Content $summary
"(logs: $dir)"
