# Installs the newest built APK (dev or debug, whichever was built last) on every connected
# ADB device. No arguments: .\install-all.ps1
. "$PSScriptRoot\scripts\env.ps1"

$apkDir = Join-Path $RF_ROOT 'android\app\build\outputs\apk'
$apk = Get-ChildItem -Path $apkDir -Recurse -Filter '*.apk' |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $apk) { throw "no APK under $apkDir - build first: scripts\build-android.ps1" }

$serials = @(& $RF_ADB devices | ForEach-Object { $_.Trim() } |
    Where-Object { $_ -match '^(\S+)\s+device$' } | ForEach-Object { $Matches[1] })
if (-not $serials.Count) { throw 'no connected ADB devices' }

Write-Host "[install-all] $($apk.FullName) ($($apk.LastWriteTime.ToString('yyyy-MM-dd HH:mm'))) to $($serials.Count) device(s)"
$failed = @()
foreach ($serial in $serials) {
    Write-Host "[install-all] $serial"
    & $RF_ADB -s $serial install -r $apk.FullName
    if ($LASTEXITCODE -ne 0) { $failed += $serial }
}
if ($failed) { throw "install failed on: $($failed -join ', ')" }
