# The M4 stage B review set for the lead (desktop, 2400x1080, real time, silent): every co-op screen and state of
# design/screens.md "Co-op (M4)" and the co-op moments of look.md 6, written to captures\m4b\ with review copies
# (*.small.png, at most 1600 px wide). Each picture is one to five processes on this PC talking over UDP (the game's
# real lobby: beacons on 47777, the game on 47778): the process named `shot` takes the picture when its --coop-shot
# moment comes and quits; the others are killed after it. Their logs sit next to the pictures (<name>_<role>.log).
#   permission, permission_refused  the local network notice, and as it reads after a refusal
#   join_empty                      the join screen with no room heard (LOOKING FOR ROOMS)
#   join_list                       a room heard (one row however many networks it is heard on) and three made-up ones
#                                   (--coop-demo-rooms: FULL and PLAYING rows) so the layout shows
#   join_code, join_address         a room code typed on the pad; an address typed (the system keyboard's mode)
#   mismatch, no_room               refusals: another game's data; a code nobody answers
#   lobby_host, lobby_client        the lobby with two seated, from the host (START) and the joiner (WAITING FOR HOST)
#   lobby_seats                     all four seats (the host and three headless joiners)
#   frame_host, frame_client        one moment (bar 40) seen from the host and from a client, both cameras on the host's ship
#   beacon, beacon_off, revive      a downed teammate's beacon on screen, off screen (its chevron), a revive half filled
#   hud                             the co-op HUD 30 s in (the team's score, each player's ranked)
#   pause_menu, pause               the shared pause (a client paused it) and its shared count-in, on the host's screen
#   over, over_client               the co-op game over from the host (RETRY) and from a client (WAITING FOR HOST)
#   host_left                       the host quits in the run: the client's join screen with the message
#   scripts\review-m4b.ps1 [-Out captures\m4b] [-Config debug|dev] [-Only lobby_host,hud]
# Debug (the default) runs with validation: a picture whose logs show validation messages, or a control within 24 px of
# another ("ui:" warnings, screens.md), is reported at the end and the script exits 1.
param(
    [string]$Out = 'captures\m4b',
    [ValidateSet('debug', 'dev')][string]$Config = 'debug',
    [string[]]$Only = @(),
    [int]$TimeoutS = 180,
    [switch]$Watch  # show the windows (default: --offscreen and a hidden console, the PC's test runs stay off screen)
)
. "$PSScriptRoot\env.ps1"
$OutFull = [IO.Path]::GetFullPath((Join-Path (Get-Location) $Out))
New-Item -ItemType Directory -Force $OutFull | Out-Null
$exe = Join-Path $RF_ROOT "build\desktop-$Config\raveform.exe"
$headless = Join-Path $RF_ROOT 'build\desktop-dev\raveform_headless.exe'
if (-not (Test-Path $exe)) { throw "missing $exe - build first" }

$big = '--res 2400x1080 --no-overlay'
$small = '--res 1280x576 --no-overlay'
$bots = '--coop-start 2 --bot'
# Each run: role -> arguments (host first; a client and headless joiners start after it). The `shot` role's arguments
# get --screenshot <name>.png. {code} is replaced by the host's room code (read from its log).
$runs = [ordered]@{
    'permission'         = @{ shot = "--coop join --coop-permission-screen --coop-shot permission $big" }
    'permission_refused' = @{ shot = "--coop join --coop-permission-screen refused --coop-shot permission $big" }
    'join_empty'         = @{ shot = "--coop join --coop-shot join $big" }
    'join_list'          = @{ host = "--coop host --coop-name PC $small"; shot = "--coop join --coop-demo-rooms --coop-shot join $big" }
    'join_code'          = @{ host = "--coop host --coop-name PC $small"; shot = "--coop join:{code} --coop-shot code $big" }
    'join_address'       = @{ shot = "--coop join:192.168.1.23 --coop-shot address $big" }
    'mismatch'           = @{ host = "--coop host --coop-name PC $small"; shot = "--coop join:list --net-content-salt 7 --coop-shot message $big" }
    'no_room'            = @{ shot = "--coop join:QQQQ --coop-shot message $big" }
    'lobby_host'         = @{ host = "--coop host --coop-name PC --coop-shot lobby2 $big"; client = "--coop join:list --coop-name TABLET $small"; shotRole = 'host' }
    'lobby_client'       = @{ host = "--coop host --coop-name PC $small"; shot = "--coop join:list --coop-name TABLET --coop-shot lobby2 $big" }
    'lobby_seats'        = @{ host = "--coop host --coop-name PC --coop-shot lobby4 $big"; shotRole = 'host'; headless = @('MOTO', 'TABLET', 'OP12R') }
    'frame_host'         = @{ host = "--host --players 2 --bind 127.0.0.1 --bot --net-capture-bar 40 --net-view-seat 0 $big"; shotRole = 'both'
                              shot = "--join 127.0.0.1 --bind 127.0.0.1 --bot --net-capture-bar 40 --net-view-seat 0 $big"; clientPng = 'frame_client' }
    'beacon'             = @{ host = "--coop host $bots --coop-down-at 20 $small"; shot = "--coop join:list --bot --coop-shot beacon $big" }
    'beacon_off'         = @{ host = "--coop host $bots --coop-down-at 20 $small"; shot = "--coop join:list --bot --coop-shot beacon_off $big" }
    'revive'             = @{ host = "--coop host $bots --coop-down-at 20 $small"; shot = "--coop join:list --bot --coop-shot revive $big" }
    'hud'                = @{ host = "--coop host $bots --coop-shot hud $big"; client = "--coop join:list --bot $small"; shotRole = 'host' }
    'pause_menu'         = @{ host = "--coop host $bots --coop-shot pause_menu $big"; client = "--coop join:list --bot --coop-pause-at 20 $small"; shotRole = 'host' }
    'pause'              = @{ host = "--coop host $bots --coop-shot pause $big"; client = "--coop join:list --bot --coop-pause-at 20 $small"; shotRole = 'host' }
    'over'               = @{ host = "--coop host $bots --coop-down-at 20 --coop-shot over $big"; client = "--coop join:list --bot --coop-down-at 21 $small"; shotRole = 'host' }
    'over_client'        = @{ host = "--coop host $bots --coop-down-at 20 $small"; shot = "--coop join:list --bot --coop-down-at 21 --coop-shot over_client $big" }
    'host_left'          = @{ host = "--coop host $bots --coop-quit-at 25 $small"; shot = "--coop join:list --bot --coop-shot message $big" }
}

function Start-Game([string]$Name, [string]$Role, [string]$AppArgs) {
    $argv = @($AppArgs -split '\s+' | Where-Object { $_ }) + @('--silent') + @(if ($Watch) { @() } else { @('--offscreen') })
    Start-Process -FilePath $exe -ArgumentList $argv -WorkingDirectory $RF_ROOT -RedirectStandardOutput "$OutFull\${Name}_$Role.log" `
        -RedirectStandardError "$OutFull\${Name}_$Role.err" -PassThru -WindowStyle $(if ($Watch) { 'Normal' } else { 'Hidden' })
}

$problems = @()
foreach ($name in $runs.Keys) {
    if ($Only.Count -and $Only -notcontains $name) { continue }
    $r = $runs[$name]
    $png = Join-Path $OutFull "$name.png"
    Remove-Item -Force $png, ([IO.Path]::ChangeExtension($png, '.small.png')) -ErrorAction SilentlyContinue
    Remove-Item -Force "$OutFull\${name}_*.log", "$OutFull\${name}_*.err" -ErrorAction SilentlyContinue
    "---- $name"
    $procs = @()
    $shotProcs = @()
    $code = ''
    if ($r.host) {
        $hargs = $r.host
        if ($r.shotRole -eq 'host' -or $r.shotRole -eq 'both') { $hargs += " --screenshot $png" }
        $h = Start-Game $name 'host' $hargs
        $procs += $h
        if ($r.shotRole -eq 'host' -or $r.shotRole -eq 'both') { $shotProcs += $h }
        # The room code from the host's log (it hosts a room at once).
        $until = (Get-Date).AddSeconds(30)
        while ((Get-Date) -lt $until) {
            $m = Select-String -Path "$OutFull\${name}_host.log" -Pattern 'hosting a room: code ([0-9A-Z]{4})' -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($m) { $code = $m.Matches[0].Groups[1].Value; break }
            if ($r.host -notmatch '--coop host') { Start-Sleep -Milliseconds 1500; break }
            Start-Sleep -Milliseconds 250
        }
    }
    $k = 0
    foreach ($seatName in @($r.headless | Where-Object { $_ })) {
        $hp = Start-Process -FilePath $headless -ArgumentList @('--net', 'join', '127.0.0.1', '--bind', '127.0.0.1', '--wait', '60', '--minutes', '1', '--name', $seatName) `
            -WorkingDirectory $RF_ROOT -RedirectStandardOutput "$OutFull\${name}_headless$k.log" -RedirectStandardError "$OutFull\${name}_headless$k.err" -PassThru -WindowStyle Hidden
        $procs += $hp
        ++$k
        Start-Sleep -Milliseconds 1200
    }
    if ($r.client) { $procs += Start-Game $name 'client' $r.client }
    if ($r.shot) {
        $sargs = $r.shot -replace '\{code\}', $code
        $spng = if ($r.clientPng) { Join-Path $OutFull "$($r.clientPng).png" } else { $png }
        $sp = Start-Game $name 'shot' "$sargs --screenshot $spng"
        $procs += $sp
        $shotProcs += $sp
    }
    $deadline = (Get-Date).AddSeconds($TimeoutS)
    while ((Get-Date) -lt $deadline -and ($shotProcs | Where-Object { -not $_.HasExited })) { Start-Sleep -Milliseconds 500 }
    Start-Sleep -Milliseconds 500
    foreach ($p in $procs) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } }
    $pngs = @($png) + @(if ($r.clientPng) { Join-Path $OutFull "$($r.clientPng).png" })
    foreach ($p in $pngs) {
        if (Test-Path $p) {
            python "$PSScriptRoot\shrink_png.py" $p | Out-Null
            "  [pic] $(Split-Path $p -Leaf)"
        } else {
            "  MISSING $(Split-Path $p -Leaf)"
            $problems += "$name : no picture"
        }
    }
    foreach ($log in Get-ChildItem "$OutFull\${name}_*.log") {
        $v = Select-String -Path $log.FullName -Pattern 'validation messages: (\d+)' | Select-Object -Last 1
        if ($v -and $v.Matches[0].Groups[1].Value -ne '0') { $problems += "$($log.Name): $($v.Line.Trim())" }
        Select-String -Path $log.FullName -Pattern 'ui: ' | ForEach-Object { $problems += "$($log.Name): $($_.Line.Trim())" }
        $n = Select-String -Path $log.FullName -Pattern 'net: (hosting|joining|the session|the run starts|--coop|seat \d left)|screenshot' | Select-Object -First 6
        $n | ForEach-Object { "  $($log.BaseName): $($_.Line.Trim())" }
    }
}
if ($problems.Count) {
    '==== problems'
    $problems | Sort-Object -Unique
    exit 1
}
'==== all pictures taken; no validation messages, no controls closer than 24 px'
