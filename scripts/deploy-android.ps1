# Builds (unless -NoBuild), installs and launches RAVEFORM on one device.
#   scripts\deploy-android.ps1 -Serial ZY22BXMJG3              (or a name from env.ps1: moto, op12r, tablet)
#   scripts\deploy-android.ps1 -Serial a0e622a5 -AppArgs '--tone' -Config debug
# Shared phones (CLAUDE.md): if another app is in front, the APK is installed but not launched.
param(
    [Parameter(Mandatory)][string]$Serial,
    [ValidateSet('dev', 'debug')][string]$Config = 'dev',
    [switch]$NoBuild,
    [switch]$NoLaunch,
    [string]$AppArgs = ''
)
. "$PSScriptRoot\env.ps1"
$Serial = Resolve-RfSerial $Serial
Assert-RfDevice $Serial

if (-not $NoBuild) { & "$PSScriptRoot\build-android.ps1" -Config $Config }
$apk = Join-Path $RF_ROOT "android\app\build\outputs\apk\$Config\app-$Config.apk"
if (-not (Test-Path $apk)) { throw "missing $apk - build first" }

Write-Host "[deploy] installing $apk on $Serial"
& $RF_ADB -s $Serial install -r $apk
if ($LASTEXITCODE -ne 0) { throw "adb install failed on $Serial" }
if ($NoLaunch) { return }
if (-not (Test-RfDeviceFree $Serial)) {
    Write-Host "[deploy] installed but not launched: $(Get-RfForegroundPackage $Serial) is in front (shared phone)"
    return
}
$appPid = Start-RfApp -Serial $Serial -AppArgs $AppArgs
Write-Host "[deploy] RAVEFORM running on $Serial (pid $appPid); close it with: adb -s $Serial shell am force-stop $RF_APP_ID"
