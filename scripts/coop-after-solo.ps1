# The owner's "missing bullets on a client" (2026-11-23; "it happened when i played single player for a bit and then joined as a
# client afterwards"; earlier, 2026-11-14: "bullets on p2 and p3 are not rendering ... killed and relaunched ... and it's fixed"):
# a host and a client on this PC, the client first playing a solo run for -SoloSeconds (--coop-after), then joining by address;
# after -CoopSeconds of the session both are closed and their logs' `net: world:` lines (every 20 s: enemy bullets alive, drawn
# on screen, fired) are printed side by side. -SoloSeconds 0: the client joins at once (the control). (No pictures: --screenshot
# runs skip the game's screens, and with them --coop.)
param([double]$SoloSeconds = 40, [double]$CoopSeconds = 90, [int]$ShotBar = 45, [string]$Out = 'captures\coop_after', [ValidateSet('debug', 'dev')][string]$Config = 'dev')
. "$PSScriptRoot\env.ps1"
$exe = Join-Path $RF_ROOT "build\desktop-$Config\raveform.exe"
$Out = [IO.Path]::GetFullPath((Join-Path $RF_ROOT $Out))
New-Item -ItemType Directory -Force $Out | Out-Null
$tag = if ($SoloSeconds -gt 0) { "after$([int]$SoloSeconds)" } else { 'fresh' }
# (--no-audio: no band, so the host's song is the simulated one; a PC without an audio device has a band that never plays, and
# the host publishes no song until it does: the run never starts)
$small = @('--offscreen', '--no-audio', '--res', '1600x720')
$hostArgs = $small + @('--coop', 'host', '--coop-name', 'HOST', '--coop-start', '2', '--bot')
$joinArgs = $small + @('--coop', 'join:list', '--coop-name', 'JOINER', '--bot')  # (the room heard on the network, as flow-m8.ps1)
if ($SoloSeconds -gt 0) { $joinArgs += @('--coop-after', "$SoloSeconds") }
# Each peer's picture as the room's run reaches bar -ShotBar (Kaleidos's fight in Track 1, its bullets), then it quits. (A
# --coop-shot name keeps the game in real time with its screens; "runbar" is none of its moments, so only the bar takes it.)
if ($ShotBar -gt 0) {
    $hostArgs += @('--coop-shot', 'runbar', '--net-capture-bar', "$ShotBar", '--net-capture-run', '--screenshot', "$Out\${tag}_host.png")
    $joinArgs += @('--coop-shot', 'runbar', '--net-capture-bar', "$ShotBar", '--net-capture-run', '--screenshot', "$Out\${tag}_client.png")
}
$hl = "$Out\${tag}_host.log"; $jl = "$Out\${tag}_client.log"
$h = Start-Process -FilePath $exe -ArgumentList $hostArgs -RedirectStandardOutput $hl -RedirectStandardError "$hl.err" -WindowStyle Hidden -PassThru
Start-Sleep -Seconds 2
$j = Start-Process -FilePath $exe -ArgumentList $joinArgs -RedirectStandardOutput $jl -RedirectStandardError "$jl.err" -WindowStyle Hidden -PassThru
# (the title's intro, the solo run and its way home, the lobby and the run's drop-in, then the co-op time; with a picture both
# quit once it is taken)
$deadline = (Get-Date).AddSeconds(30 + $SoloSeconds + $CoopSeconds)
while ((Get-Date) -lt $deadline -and -not ($h.HasExited -and $j.HasExited)) { Start-Sleep -Seconds 2 }
foreach ($p in $j, $h) { if (-not $p.HasExited) { $p.Kill() } }
Start-Sleep -Seconds 1
foreach ($l in $hl, $jl) {
    Write-Host "== $(Split-Path $l -Leaf)"
    Select-String -Path $l -Pattern '--coop-after|the run starts on bar|session ended' | ForEach-Object { "  $($_.Line.Trim())" }
}
# Each peer's last `net: world:` line in each bar: enemy bullets alive / drawn on screen / fired in all.
function Bars($log) {
    $m = @{}
    foreach ($x in (Select-String -Path $log -Pattern 'net: world: Track (\d+) bar (\d+), (\d+) enemies, (\d+) enemy bullets alive \((\d+) drawn on screen\), (\d+) fired')) {
        $g = $x.Matches[0].Groups
        $m["$($g[1].Value).$($g[2].Value)"] = "$($g[4].Value)/$($g[5].Value)/$($g[6].Value)"
    }
    return $m
}
$hb = Bars $hl; $jb = Bars $jl
Write-Host "Track.bar  host alive/drawn/fired   client alive/drawn/fired"
foreach ($k in ($hb.Keys + $jb.Keys | Sort-Object -Unique { [double]$_ })) { if ($hb[$k] -or $jb[$k]) { Write-Host ('{0,-10} {1,-24} {2}' -f $k, $hb[$k], $jb[$k]) } }
