# M4 stage B device checks with the Moto (or another phone) and this PC on one network, under the shared-phone rules
# (CLAUDE.md "Shared test devices"): the lock captures\locks\<serial>.lock (name m4b) and the time, the foreground checked
# first (another app in front, or RAVEFORM that this script did not start: the phone is busy, nothing is launched), -s on
# every adb call, nothing device-wide, force-stop of dev.raveform.game only, our own APK deployed before each run, the
# lock removed after. PC processes are silent (raveform_headless opens no audio device; the desktop game gets --silent).
# Nothing here changes a system or network setting: a "client drop" is a PC process killed or our app force-stopped.
#   discovery_pc_host    the PC's game hosts a room: the phone's join screen hears it (a picture on the phone), then the
#                        phone joins it from the list, by its room code and by its address (a picture once seated each)
#   discovery_moto_host  the phone hosts a room (its bot plays, the run starts at three seats): the PC's join screen hears
#                        it (a picture), the PC's game joins by the room code and a PC bot by the address; 75 s into the
#                        run the PC bot is killed (a client drop), 30 s later the phone's app is stopped (the host quits):
#                        the PC's game shows the message (a picture)
#   long_pc_host         15 minutes: the PC hosts four seats (a headless host), the phone joins (bench_play, measured), two
#                        PC bots join
#   long_moto_host       15 minutes: the phone hosts four seats (bench_play, measured), three PC bots join
#   -Impaired            every peer sends through the condition simulator: 5% loss, 80 ms jitter
#   scripts\coop-device.ps1 -Run long_moto_host [-Serial moto] [-Minutes 15] [-Impaired] [-Out captures\m4b\device]
param(
    [Parameter(Mandatory)][ValidateSet('discovery_pc_host', 'discovery_moto_host', 'long_pc_host', 'long_moto_host')][string]$Run,
    [string]$Serial = 'moto',
    [double]$Minutes = 15,
    [switch]$Impaired,
    [string]$Out = 'captures\m4b\device',
    [string]$PcIp = '',
    [switch]$NoDeploy
)
. "$PSScriptRoot\env.ps1"
$Serial = Resolve-RfSerial $Serial
Assert-RfDevice $Serial
$OutFull = [IO.Path]::GetFullPath((Join-Path (Get-Location) $Out))
New-Item -ItemType Directory -Force $OutFull | Out-Null
$lock = Join-Path $RF_ROOT "captures\locks\$Serial.lock"
$headless = Join-Path $RF_ROOT 'build\desktop-dev\raveform_headless.exe'
$game = Join-Path $RF_ROOT 'build\desktop-dev\raveform.exe'
$stamp = Get-Date -Format 'HHmmss'
$tag = "${stamp}_$Run$(if ($Impaired) { '_impaired' })"
$netsim = if ($Impaired) { 'loss=0.05,jitter=80' } else { '' }

# The phone's and this PC's addresses on the phone's Wi-Fi network.
$phoneIp = ((& $RF_ADB -s $Serial shell "ip -4 addr show wlan0") | Select-String 'inet (\d+\.\d+\.\d+\.\d+)' | Select-Object -First 1).Matches[0].Groups[1].Value
if (-not $phoneIp) { throw "the phone has no Wi-Fi address" }
if (-not $PcIp) {
    $prefix = ($phoneIp -split '\.')[0..2] -join '.'
    $PcIp = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -like "$prefix.*" } | Select-Object -First 1).IPAddress
}
if (-not $PcIp) { throw "this PC has no address on the phone's network ($phoneIp)" }
"phone $phoneIp, PC $PcIp"

function Get-RoomCode([string]$Ip) {
    # TECH_SPEC 8.3 (src/net/lobby.cpp): the low 16 bits of the host's address, four symbols of "34679ACDFHJKMPQR".
    $o = $Ip.Split('.') | ForEach-Object { [int]$_ }
    $low = ($o[2] -shl 8) -bor $o[3]
    $a = '34679ACDFHJKMPQR'
    -join (0..3 | ForEach-Object { $a[($low -shr (12 - 4 * $_)) -band 15] })
}
function Test-Phone {
    if (Test-Path $lock) {
        $t = (Get-Content $lock -Raw).Trim()
        $age = ((Get-Date) - (Get-Item $lock).LastWriteTime).TotalMinutes
        if ($t -notmatch '^m4b\b' -and $age -lt 30) { Write-Host "BUSY: lock '$t' ($([int]$age) min old)"; return $false }
    }
    $fg = Get-RfForegroundPackage $Serial
    $homePkg = Get-RfHomePackage $Serial
    Write-Host "foreground: '$fg' (home: $homePkg)"
    if ($fg -eq $RF_APP_ID) { Write-Host 'BUSY: RAVEFORM is in front and this script did not start it'; return $false }
    if ($fg -ne '' -and $fg -ne $homePkg) { Write-Host "BUSY: $fg is in front"; return $false }
    return $true
}
function Lock-Phone { New-Item -ItemType Directory -Force (Split-Path $lock) | Out-Null; Set-Content -Encoding ascii $lock ('m4b ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')) }
function Start-Pc([string]$Name, [string]$Exe, [string[]]$ArgList) {
    Start-Process -FilePath $Exe -ArgumentList $ArgList -WorkingDirectory $RF_ROOT -RedirectStandardOutput "$OutFull\${tag}_$Name.txt" `
        -RedirectStandardError "$OutFull\${tag}_$Name.err" -PassThru -WindowStyle Hidden
}
function Deploy-Once {
    if ($script:deployed -or $NoDeploy) { return }
    & "$PSScriptRoot\deploy-android.ps1" -Serial $Serial -NoBuild -NoLaunch | Out-Null
    $script:deployed = $true
}
function Launch-Phone([string]$AppArgs) {
    Deploy-Once
    return (Start-RfApp $Serial $AppArgs)
}
function Save-PhoneLog([string]$PhonePid, [string]$Name) {
    if ($PhonePid) { & $RF_ADB -s $Serial shell "logcat -d --pid=$PhonePid" | Set-Content "$OutFull\${tag}_$Name.logcat.txt" }
    Get-Content "$OutFull\${tag}_$Name.logcat.txt" -ErrorAction SilentlyContinue | Where-Object { $_ -match 'net: |multicast|RFLIFE|validation' } |
        ForEach-Object { if ($_.Length -gt 240) { $_.Substring(0, 240) } else { $_ } } | Select-Object -First 40
}
function Pull-PhonePicture([string]$Name) {
    $files = "/sdcard/Android/data/$RF_APP_ID/files"
    & $RF_ADB -s $Serial pull "$files/$Name" "$OutFull\${tag}_$Name" 2>&1 | Out-Null
    & $RF_ADB -s $Serial shell "rm -f $files/$Name" | Out-Null
    if (Test-Path "$OutFull\${tag}_$Name") { python "$PSScriptRoot\shrink_png.py" "$OutFull\${tag}_$Name" | Out-Null; "picture: ${tag}_$Name" }
    else { "NO PICTURE: $Name" }
}
function Net-Args([string[]]$a) { if ($netsim) { return $a + @('--netsim', $netsim) } else { return $a } }

if (-not (Test-Phone)) { exit 3 }
Lock-Phone
$script:deployed = $false
$pcs = @()
try {
    switch ($Run) {
        'discovery_pc_host' {
            $pcs += Start-Pc 'pc_host' $game @('--coop', 'host', '--coop-name', 'PC', '--res', '1280x576', '--silent')
            Start-Sleep -Seconds 5
            $code = Get-RoomCode $PcIp
            "the PC's room code: $code"
            $tries = @(
                @{ name = 'phone_hears_pc.png'; args = '--coop join --coop-shot join --screenshot phone_hears_pc.png'; wait = 15 },
                @{ name = 'phone_joined_list.png'; args = '--coop join:list --coop-shot lobby2 --screenshot phone_joined_list.png'; wait = 30 },
                @{ name = 'phone_joined_code.png'; args = "--coop join:$code --coop-shot lobby2 --screenshot phone_joined_code.png"; wait = 30 },
                @{ name = 'phone_joined_address.png'; args = "--coop join:$PcIp --coop-shot lobby2 --screenshot phone_joined_address.png"; wait = 30 }
            )
            foreach ($t in $tries) {
                "-- $($t.name)"
                $phonePid = Launch-Phone $t.args
                Start-Sleep -Seconds $t.wait
                Save-PhoneLog $phonePid ([IO.Path]::GetFileNameWithoutExtension($t.name))
                Stop-RfApp $Serial
                Pull-PhonePicture $t.name
                Start-Sleep -Seconds 2
            }
            '-- the PC host'
            Get-Content "$OutFull\${tag}_pc_host.txt" | Where-Object { $_ -match 'net: |validation' } | Select-Object -First 30
        }
        'discovery_moto_host' {
            $phonePid = Launch-Phone '--coop host --coop-name MOTO --coop-start 3 --bot'
            Start-Sleep -Seconds 4
            $list = Start-Pc 'pc_list' $game @('--coop', 'join', '--coop-shot', 'join', '--res', '2400x1080', '--no-overlay', '--screenshot', "$OutFull\${tag}_pc_hears_phone.png", '--silent')
            $list.WaitForExit(60000) | Out-Null
            if (Test-Path "$OutFull\${tag}_pc_hears_phone.png") { python "$PSScriptRoot\shrink_png.py" "$OutFull\${tag}_pc_hears_phone.png" | Out-Null; "picture: ${tag}_pc_hears_phone.png" }
            $code = Get-RoomCode $phoneIp
            "the phone's room code: $code"
            # The PC's game joins by the code (its bot plays) and keeps the message the host's leaving brings (a picture).
            $pcs += Start-Pc 'pc_game' $game @('--coop', "join:$code", '--bot', '--coop-shot', 'message', '--res', '2400x1080', '--no-overlay',
                                               '--screenshot', "$OutFull\${tag}_pc_host_left.png", '--silent')
            $pcs += Start-Pc 'pc_bot' $headless @('--net', 'join', $phoneIp, '--wait', '120', '--minutes', '10', '--name', 'PCBOT')
            # The run starts at three seats; 75 s into it the PC bot drops, half a minute later the host quits.
            $until = (Get-Date).AddSeconds(120)
            while ((Get-Date) -lt $until -and -not (Select-String -Path "$OutFull\${tag}_pc_game.txt" -Pattern 'the run starts' -Quiet)) { Start-Sleep -Seconds 1 }
            "the run started at $(Get-Date -Format 'HH:mm:ss')"
            Start-Sleep -Seconds 75
            "the PC bot drops (killed) at $(Get-Date -Format 'HH:mm:ss')"
            Stop-Process -Id $pcs[1].Id -Force -ErrorAction SilentlyContinue
            Start-Sleep -Seconds 30
            Save-PhoneLog $phonePid 'phone_host'
            "the host quits (the phone's app stopped) at $(Get-Date -Format 'HH:mm:ss')"
            Stop-RfApp $Serial
            $pcs[0].WaitForExit(60000) | Out-Null
            if (Test-Path "$OutFull\${tag}_pc_host_left.png") { python "$PSScriptRoot\shrink_png.py" "$OutFull\${tag}_pc_host_left.png" | Out-Null; "picture: ${tag}_pc_host_left.png" }
            '-- the PC game'
            Get-Content "$OutFull\${tag}_pc_game.txt" | Where-Object { $_ -match 'net: |validation' } | ForEach-Object { if ($_.Length -gt 300) { $_.Substring(0, 300) } else { $_ } }
            '-- the PC bot'
            Get-Content "$OutFull\${tag}_pc_bot.txt" | Where-Object { $_ -match '^\[net\]' } | Select-Object -Last 4
        }
        'long_pc_host' {
            $pcs += Start-Pc 'pc_host' $headless (Net-Args @('--net', 'host', '--players', '4', '--bind', $PcIp, '--minutes', "$($Minutes + 1.5)", '--wait', '400'))
            Start-Sleep -Seconds 2
            foreach ($k in 1..2) { $pcs += Start-Pc "pc_bot$k" $headless (Net-Args @('--net', 'join', $PcIp, '--wait', '400', '--minutes', "$($Minutes + 1)", '--name', "PC$k", '--seed', "$k")) }
            $appArgs = "--join $PcIp" + $(if ($netsim) { " --netsim $netsim" } else { '' })
            & "$PSScriptRoot\bench-android.ps1" -Serial $Serial -Scenario bench_play -Warmup 30 -Seconds ([int]($Minutes * 60)) -AppArgs $appArgs -Deploy
            foreach ($p in $pcs) { $p.WaitForExit(150000) | Out-Null }
            foreach ($n in @('pc_host', 'pc_bot1', 'pc_bot2')) { "-- $n"; Get-Content "$OutFull\${tag}_$n.txt" | Where-Object { $_ -match 'run;|down |on-screen|PASS|FAIL|RFNET' } }
        }
        'long_moto_host' {
            foreach ($k in 1..3) { $pcs += Start-Pc "pc_bot$k" $headless (Net-Args @('--net', 'join', $phoneIp, '--wait', '400', '--minutes', "$($Minutes + 1)", '--name', "PC$k", '--seed', "$k")) }
            $appArgs = '--host --players 4' + $(if ($netsim) { " --netsim $netsim" } else { '' })
            & "$PSScriptRoot\bench-android.ps1" -Serial $Serial -Scenario bench_play -Warmup 30 -Seconds ([int]($Minutes * 60)) -AppArgs $appArgs -Deploy
            foreach ($p in $pcs) { $p.WaitForExit(150000) | Out-Null }
            foreach ($n in @('pc_bot1', 'pc_bot2', 'pc_bot3')) { "-- $n"; Get-Content "$OutFull\${tag}_$n.txt" | Where-Object { $_ -match 'run;|down |on-screen|PASS|FAIL|RFNET' } }
        }
    }
} finally {
    foreach ($p in $pcs) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } }
    Stop-RfApp $Serial
    Remove-Item -Force $lock -ErrorAction SilentlyContinue
    'phone unlocked'
}
