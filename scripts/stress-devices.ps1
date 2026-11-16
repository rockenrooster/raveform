# A long stress test on every connected device (2026-11-15, the owner: "stress test the game (2 hours) with connected devices").
# Rounds of four-seat co-op (scripts\coop-phones.ps1, a different host and way of joining each round, a HOME and back in one, a
# player's app killed in another) while the fifth device plays alone (--autoplay: the bot plays every run and retries), and every
# device's memory (PSS) and battery temperature sampled every few minutes. Under the shared-phone rules (CLAUDE.md): locks, the
# foreground checked, -s on every adb call, nothing device-wide, our APK deployed first, only dev.raveform.game stopped.
#   scripts\stress-devices.ps1 [-RoundMinutes 38] [-Rounds 3] [-Devices moto,tablet,tablet2,moto2,tablet3] [-Out captures\stress]
param(
    [string[]]$Devices = @('moto', 'tablet', 'tablet2', 'moto2', 'tablet3'),
    [double]$RoundMinutes = 38,
    [int]$Rounds = 3,
    [double]$SampleMinutes = 5,
    [string]$Out = 'captures\stress',
    [switch]$NoDeploy  # the APK already on every device (an install can bring up Play Protect's prompt on top: see Wake-Device)
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\env.ps1"
if ($Devices.Count -ne 5) { throw 'five devices: four seats and one alone' }
$serials = @($Devices | ForEach-Object { Resolve-RfSerial $_ })
foreach ($s in $serials) { Assert-RfDevice $s }
$ids = @($serials | ForEach-Object { Get-RfDeviceId $_ })
# A device asleep behind its lock screen gives the game no window ("renderer: no window surface": the first try, 2026-11-15, lost
# its round to four sleeping tablets and phones). Before every round each one is woken and its lock screen (no PIN on these)
# dismissed; one that stays locked stops the test with its name.
function Wake-Device([string]$s, [string]$id) {
    & $RF_ADB -s $s shell input keyevent KEYCODE_WAKEUP | Out-Null
    Start-Sleep -Milliseconds 600
    & $RF_ADB -s $s shell wm dismiss-keyguard | Out-Null
    Start-Sleep -Milliseconds 1200
    # Play Protect's "Send app for a security check?" about a sideloaded debug build (it came up over the Moto after an install;
    # Back does not close it): its "Don't send" button, found by its text, sends nothing and changes no setting.
    if ((& $RF_ADB -s $s shell 'dumpsys window') | Select-String 'mCurrentFocus=.*PlayProtectDialogsActivity' -Quiet) {
        $xml = "$(& $RF_ADB -s $s shell 'uiautomator dump /dev/tty' 2>$null)"
        $m = [regex]::Match($xml, 'text="Don.t send"[^>]*?bounds="\[(\d+),(\d+)\]\[(\d+),(\d+)\]"')
        if ($m.Success) {
            $x = ([int]$m.Groups[1].Value + [int]$m.Groups[3].Value) / 2; $y = ([int]$m.Groups[2].Value + [int]$m.Groups[4].Value) / 2
            & $RF_ADB -s $s shell input tap ([int]$x) ([int]$y) | Out-Null
            Start-Sleep -Milliseconds 1200
            Say "${id}: Play Protect's prompt answered Don't send"
        } else {
            throw "${id}: Play Protect's prompt is up and its Don't send button was not found: answer it by hand"
        }
    }
    $awake = [bool]((& $RF_ADB -s $s shell 'dumpsys power') | Select-String 'mWakefulness=Awake' -Quiet)
    $locked = [bool]((& $RF_ADB -s $s shell 'dumpsys window') | Select-String 'isKeyguardShowing=true' -Quiet)
    if (-not $awake -or $locked) { throw "$id stays asleep or locked (a PIN?): unlock it by hand" }
}
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$dir = [IO.Path]::GetFullPath((Join-Path $RF_ROOT (Join-Path $Out $stamp)))
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir 'stress.txt'
function Say([string]$t) { $line = "[$(Get-Date -Format 'HH:mm:ss')] $t"; Write-Host $line; Add-Content $log $line }
Say "devices: $(($Devices | ForEach-Object -Begin { $i = 0 } -Process { "$_=$($ids[$i++])" }) -join ', ')"

# The memory and temperature of every device, sampled in the background for the whole test.
$samples = Join-Path $dir 'samples.csv'
Set-Content $samples 'time,device,pss_kb,battery_temp_c,app'
$sampler = Start-Job -ArgumentList $serials, $ids, $samples, $SampleMinutes, $RF_ADB, $RF_APP_ID, ($Rounds * ($RoundMinutes + 4) + 10) -ScriptBlock {
    param($serials, $ids, $samples, $every, $adb, $app, $totalMinutes)
    $end = (Get-Date).AddMinutes($totalMinutes)
    while ((Get-Date) -lt $end) {
        for ($k = 0; $k -lt $serials.Count; ++$k) {
            $s = $serials[$k]
            $pss = ''
            $m = (& $adb -s $s shell "dumpsys meminfo $app" 2>$null) | Select-String 'TOTAL PSS:\s+(\d+)|^\s+TOTAL\s+(\d+)' | Select-Object -First 1
            if ($m) { $pss = if ($m.Matches[0].Groups[1].Value) { $m.Matches[0].Groups[1].Value } else { $m.Matches[0].Groups[2].Value } }
            $temp = ''
            $b = (& $adb -s $s shell 'dumpsys battery' 2>$null) | Select-String 'temperature:\s*(\d+)' | Select-Object -First 1
            if ($b) { $temp = '{0:0.0}' -f ([double]$b.Matches[0].Groups[1].Value / 10.0) }
            $running = [bool]("$(& $adb -s $s shell "pidof $app" 2>$null)".Trim())
            Add-Content $samples "$(Get-Date -Format 'HH:mm:ss'),$($ids[$k]),$pss,$temp,$(if ($running) { 'running' } else { 'stopped' })"
        }
        Start-Sleep -Seconds ([int]($every * 60))
    }
}

# Rounds: the seats (the first hosts) and the one alone; how the others join; what is done to the room.
$plan = @(
    @{ Seats = @(0, 1, 2, 3); Alone = 4; Join = 'address'; Extra = @{} },
    @{ Seats = @(1, 4, 0, 3); Alone = 2; Join = 'code'; Extra = @{ HomeIndex = 2; HomeAt = 600; HomeFor = 20 } },
    @{ Seats = @(3, 2, 4, 1); Alone = 0; Join = 'list'; Extra = @{ KillIndex = 3; KillAt = [int]($RoundMinutes * 60 * 0.6) } }
)
try {
    if (-not $NoDeploy) {
        foreach ($s in $serials) { & "$PSScriptRoot\deploy-android.ps1" -Serial $s -NoBuild -NoLaunch | Out-Null }
        Say 'the APK deployed on every device'
    }
    for ($r = 0; $r -lt $Rounds; ++$r) {
        $p = $plan[$r % $plan.Count]
        $seatSerials = @($p.Seats | ForEach-Object { $serials[$_] })
        $alone = $serials[$p.Alone]
        $aloneId = $ids[$p.Alone]
        Say "round $($r + 1): co-op host $($ids[$p.Seats[0]]), joining by $($p.Join): $(($p.Seats[1..3] | ForEach-Object { $ids[$_] }) -join ', '); alone: $aloneId"
        for ($k = 0; $k -lt $serials.Count; ++$k) { Wake-Device $serials[$k] $ids[$k] }
        # The one alone: its lock, its foreground, --autoplay (the bot plays each run and retries), its log read every half minute.
        $lock = Join-Path $RF_ROOT "captures\locks\$aloneId.lock"
        Set-Content -Encoding ascii $lock ("stress " + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
        if (-not (Test-RfDeviceFree $alone)) { throw "$aloneId is in use: $(Get-RfForegroundPackage $alone) in front" }
        $soloLog = Join-Path $dir "round$($r + 1)_alone_$aloneId.log"
        $since = "$(& $RF_ADB -s $alone shell "date '+%m-%d %H:%M:%S.000'")".Trim()
        $soloPid = Start-RfApp $alone '--autoplay'
        Say "$aloneId alone: pid $soloPid (--autoplay)"
        $soloJob = Start-Job -ArgumentList $alone, $soloLog, $since, ($RoundMinutes + 3), $RF_ADB, $RF_APP_ID -ScriptBlock {
            param($s, $out, $since, $minutes, $adb, $app)
            $end = (Get-Date).AddMinutes($minutes)
            $last = $since
            $gone = 0
            while ((Get-Date) -lt $end) {
                Start-Sleep -Seconds 30
                $got = & $adb -s $s shell "logcat -d -v threadtime -T '$last' -s 'RAVEFORM:*' 'RFLIFE:*' 'AndroidRuntime:*' 'DEBUG:*' 'libc:*' 'SDL:*'"
                $new = @($got | Where-Object { $_ -match '^\d\d-\d\d \d\d:\d\d:\d\d\.\d\d\d' -and $_.Substring(0, 18) -gt $last })
                if ($new.Count) { Add-Content $out $new; $last = $new[-1].Substring(0, 18) }
                if (-not "$(& $adb -s $s shell "pidof $app")".Trim()) { $gone++; Add-Content $out "STRESS: THE APP IS GONE ($(Get-Date -Format 'HH:mm:ss'))"; break }
            }
            $gone
        }
        # The four seats (coop-phones.ps1 saves their logs and summary under $dir\coop).
        $extra = $p.Extra
        & "$PSScriptRoot\coop-phones.ps1" -Serials $seatSerials -Minutes $RoundMinutes -Join $p.Join -Name "round$($r + 1)" -Out (Join-Path $Out "$stamp\coop") -LockName 'stress' -NoDeploy @extra |
            Select-String 'run started|NO RUN START|THE APP IS GONE|force-stopped|HOME on|back to|CRASH|apps stopped' | ForEach-Object { Say "  co-op: $($_.Line)" }
        $goneAlone = Receive-Job -Wait $soloJob
        Remove-Job $soloJob
        & $RF_ADB -s $alone shell "logcat -d -b crash -v threadtime -T '$since'" | Add-Content (Join-Path $dir "round$($r + 1)_alone_$aloneId.crash.log")
        Stop-RfApp $alone
        Remove-Item -Force $lock -ErrorAction SilentlyContinue
        $runs = @(Get-Content $soloLog -ErrorAction SilentlyContinue | Select-String 'the run starts|game over|RETRY|run ends').Count
        Say "round $($r + 1) done; $aloneId alone: $(if ($goneAlone) { 'THE APP WENT AWAY' } else { 'ran through' }), $runs run lines in its log"
    }
} finally {
    Stop-Job $sampler -ErrorAction SilentlyContinue
    Remove-Job $sampler -Force -ErrorAction SilentlyContinue
    foreach ($s in $serials) { Stop-RfApp $s }
    Get-ChildItem (Join-Path $RF_ROOT 'captures\locks') -Filter *.lock -ErrorAction SilentlyContinue |
        Where-Object { (Get-Content $_.FullName -Raw) -match '^stress\b' } | Remove-Item -Force -ErrorAction SilentlyContinue
    Say 'all apps stopped, the locks removed'
}
# Memory and temperature: first and last sample of each device, and its highest.
$rows = Import-Csv $samples
foreach ($id in $ids) {
    $mine = @($rows | Where-Object { $_.device -eq $id -and $_.pss_kb })
    if (-not $mine.Count) { continue }
    $hot = ($mine | Where-Object { $_.battery_temp_c } | Measure-Object -Property battery_temp_c -Maximum).Maximum
    $big = ($mine | Measure-Object -Property pss_kb -Maximum).Maximum
    Say ("{0}: PSS {1} -> {2} MB (most {3}), battery up to {4} C, {5} samples" -f $id, [int]([double]$mine[0].pss_kb / 1024), [int]([double]$mine[-1].pss_kb / 1024),
        [int]([double]$big / 1024), $hot, $mine.Count)
}
"(everything under $dir)"
