# M8 part 7 (docs/MODES_PLAN.md 8: "every mode from every way in (the title, MODES, RETRY, REMATCH, NEXT TRACK, a room, the pause's
# RESTART) and out"): the ways in and out, driven through the game's own screens on this PC (off screen, silent).
#   -Part solo: one process, --flow-test (game/play_flow_m8.cpp): each of the seven modes in by the title's PLAY, MODES's PLAY, the
#               pause's RESTART, the result's RETRY and (ONE TRACK) NEXT TRACK, out by the pause's QUIT RUN (JAM's LEAVE) and the
#               result's HOME, the runs played by the bot and fast-forwarded to their ends.
#   -Part room: two processes a room (a host and a joiner on this PC's network: they broadcast a beacon), each room's runs starting at
#               a late Drop (--coop-mode <mode>:<track>:dropN, so they end soon): in by the lobby's START and the podium's REMATCH,
#               out by the podium's LOBBY (the room standing: the lobby again) in VERSUS; in by START and the host's RETRY in TEAM.
#               Each room passes when both peers log what the way should do.
# Exit 0 when everything passed. Logs and a summary in captures\m8\flow\.
param(
    [ValidateSet('solo', 'room', 'all')][string]$Part = 'all',
    [ValidateSet('debug', 'dev')][string]$Config = 'dev',
    [int]$TimeoutS = 420,
    [string[]]$Only = @()
)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)
. .\scripts\env.ps1 | Out-Null
$exe = (Resolve-Path "build\desktop-$Config\raveform.exe").Path
$out = 'captures\m8\flow'
New-Item -ItemType Directory -Force $out | Out-Null
$failed = 0
$summary = New-Object System.Collections.Generic.List[string]

if ($Part -eq 'solo' -or $Part -eq 'all') {
    $log = Join-Path $out 'solo.log'
    $p = Start-Process -FilePath $exe -ArgumentList '--offscreen', '--silent', '--flow-test', '--res', '1600x720', '--seed', '3' `
        -RedirectStandardOutput $log -RedirectStandardError (Join-Path $out 'solo.err') -WindowStyle Hidden -PassThru
    if (-not $p.WaitForExit(1800 * 1000)) { $p.Kill(); $summary.Add('solo: TIMED OUT'); $failed++ }
    else {
        $last = Select-String -Path $log -Pattern 'FLOW: ' | Select-Object -Last 1
        $fails = Select-String -Path $log -Pattern '^FLOW FAIL'
        $summary.Add("solo: exit $($p.ExitCode); $($last.Line)")
        foreach ($f in $fails) { $summary.Add("  $($f.Line)") }
        if ($p.ExitCode -ne 0) { $failed++ }
    }
}

if ($Part -eq 'room' -or $Part -eq 'all') {
    # Each room: the host's --coop-mode, and what both logs must show (counted lines).
    $rooms = [ordered]@{
        'tracers_vs'  = @{ mode = 'tracers:3:drop2:vs';  want = @{ 'net: the run starts on bar line' = 1; 'versus match ends' = 2; 'REMATCH' = 1; 'back in its lobby' = 1 } }
        'songline_vs' = @{ mode = 'songline:4:drop2:vs'; want = @{ 'net: the run starts on bar line' = 1; 'versus match ends' = 2; 'REMATCH' = 1; 'back in its lobby' = 1 } }
        'blast_vs'    = @{ mode = 'blast:3:wave4:vs';    want = @{ 'net: the run starts on bar line' = 1; 'versus match ends' = 2; 'REMATCH' = 1; 'back in its lobby' = 1 } }
        'popoff_vs'   = @{ mode = 'jam:12:drop1:vs';     want = @{ 'net: the run starts on bar line' = 1; 'versus match ends' = 2; 'REMATCH' = 1; 'back in its lobby' = 1 } }
        'tracers_team' = @{ mode = 'tracers:3:drop2';    want = @{ 'net: the run starts on bar line' = 1; 'the run ends' = 2; "the host's RETRY" = 1 } }
        'blast_team'  = @{ mode = 'blast:3:wave4';       want = @{ 'net: the run starts on bar line' = 1; 'the run ends' = 2; "the host's RETRY" = 1 } }
    }
    $small = @('--offscreen', '--silent', '--res', '1280x576', '--no-overlay')
    foreach ($name in $rooms.Keys) {
        if ($Only.Count -and -not ($Only -contains $name)) { continue }
        $r = $rooms[$name]
        $hostLog = Join-Path $out "room_$($name)_host.log"
        $joinLog = Join-Path $out "room_$($name)_join.log"
        $hostArgs = $small + @('--coop', 'host', '--coop-name', 'HOST', '--coop-mode', $r.mode, '--coop-start', '2', '--bot', '--coop-back-lobby')
        $joinArgs = $small + @('--coop', 'join:list', '--coop-name', 'JOINER', '--bot')
        $h = Start-Process -FilePath $exe -ArgumentList $hostArgs -RedirectStandardOutput $hostLog -RedirectStandardError "$hostLog.err" -WindowStyle Hidden -PassThru
        Start-Sleep -Seconds 3
        $j = Start-Process -FilePath $exe -ArgumentList $joinArgs -RedirectStandardOutput $joinLog -RedirectStandardError "$joinLog.err" -WindowStyle Hidden -PassThru
        $deadline = (Get-Date).AddSeconds($TimeoutS)
        $ok = $false
        while ((Get-Date) -lt $deadline -and -not $h.HasExited -and -not $j.HasExited) {
            Start-Sleep -Seconds 5
            $all = $true
            foreach ($k in $r.want.Keys) {
                foreach ($lg in @($hostLog, $joinLog)) {
                    $n = @(Select-String -Path $lg -Pattern ([regex]::Escape($k)) -ErrorAction SilentlyContinue).Count
                    if ($n -lt $r.want[$k]) { $all = $false }
                }
            }
            if ($all) { $ok = $true; break }
        }
        foreach ($pr in @($h, $j)) { if (-not $pr.HasExited) { $pr.Kill() } }
        $counts = foreach ($k in $r.want.Keys) {
            $nh = @(Select-String -Path $hostLog -Pattern ([regex]::Escape($k)) -ErrorAction SilentlyContinue).Count
            $nj = @(Select-String -Path $joinLog -Pattern ([regex]::Escape($k)) -ErrorAction SilentlyContinue).Count
            "'$k' host $nh joiner $nj (want $($r.want[$k]))"
        }
        $summary.Add("room $name ($($r.mode)): $(if ($ok) { 'PASS' } else { 'FAIL' }); $($counts -join '; ')")
        if (-not $ok) { $failed++ }
    }
}

$summary | Set-Content (Join-Path $out 'summary.txt')
$summary | ForEach-Object { Write-Host $_ }
if ($failed) { Write-Host "[flow] $failed part(s) failed"; exit 1 }
Write-Host '[flow] every way in and out passed'
exit 0
