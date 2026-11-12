# M2 device runs under the phone lock of the shared-phone rule (CLAUDE.md, lead's rule of 2026-11-11):
# writes captures\locks\<serial>.lock in the main checkout ("m2" and the time) when the phone is free,
# deploys this checkout's last built APK right before the run, and deletes the lock when the run ends.
#   scripts\audio-device.ps1 -Run bench [-Seconds 60] [-AppArgs '--band full']      bench_idle with the band
#   scripts\audio-device.ps1 -Run screenshot -AppArgs '--scenario sync --no-overlay'  in-app capture
#   scripts\audio-device.ps1 -Run soak -Seconds 600                                  the 10-minute underrun run
# Exit code 10: the phone is locked by someone else (try again later).
param(
    [string]$Serial = 'moto',
    [ValidateSet('bench', 'screenshot', 'soak')][string]$Run = 'bench',
    [int]$Seconds = 60,
    [string]$Scenario = 'bench_idle',
    [string]$AppArgs = '--band full',
    [int]$Frames = 240,
    [string]$Out = '',
    [switch]$NoDeploy
)
. "$PSScriptRoot\env.ps1"
$Serial = Resolve-RfSerial $Serial
Assert-RfDevice $Serial

$lockDir = 'D:\Stuff\DevFolder\Android_Games\Raveform\captures\locks'
$lock = Join-Path $lockDir "$Serial.lock"
if (Test-Path $lock) {
    $age = ((Get-Date) - (Get-Item $lock).LastWriteTime).TotalMinutes
    $owner = (Get-Content $lock -Raw).Trim()
    if ($age -lt 30 -and $owner -notmatch '^m2\b') {
        Write-Host "[lock] $Serial is busy: '$owner' ($([int]$age) min old)"
        exit 10
    }
}
New-Item -ItemType Directory -Force $lockDir | Out-Null
Set-Content -Path $lock -Value "m2 $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Host "[lock] took $lock"
try {
    if (-not $NoDeploy) { & "$PSScriptRoot\deploy-android.ps1" -Serial $Serial -NoBuild -NoLaunch }
    switch ($Run) {
        'bench' {
            & "$PSScriptRoot\bench-android.ps1" -Serial $Serial -Scenario $Scenario -Seconds $Seconds -AppArgs $AppArgs
        }
        'soak' {
            # Ten minutes measured: the callback load and the underrun count come with the result.
            & "$PSScriptRoot\bench-android.ps1" -Serial $Serial -Scenario $Scenario -Seconds $Seconds -AppArgs $AppArgs -NoLoad
        }
        'screenshot' {
            $args2 = @{ Serial = $Serial; InApp = $true; Frames = $Frames; AppArgs = $AppArgs; Small = $true }
            if ($Out) { $args2.Out = $Out }
            & "$PSScriptRoot\capture-android.ps1" @args2
            Stop-RfApp $Serial
        }
    }
} finally {
    Remove-Item -Path $lock -ErrorAction SilentlyContinue
    Write-Host "[lock] released $lock"
}
