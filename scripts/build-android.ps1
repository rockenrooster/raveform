# Builds the APK with the Gradle wrapper (AGP 8.13.0, Gradle 8.13, NDK r29, SDK CMake 3.22.1).
#   scripts\build-android.ps1 [-Config dev|debug]
# dev = native RelWithDebInfo, debuggable, debug-signed (the benchmarked build); debug = native Debug.
# Output: android\app\build\outputs\apk\<config>\app-<config>.apk
param([ValidateSet('dev', 'debug')][string]$Config = 'dev')
. "$PSScriptRoot\env.ps1"

$task = ':app:assemble' + $Config.Substring(0, 1).ToUpper() + $Config.Substring(1)
Push-Location (Join-Path $RF_ROOT 'android')
try {
    & .\gradlew.bat $task --console=plain
    if ($LASTEXITCODE -ne 0) { throw "gradle $task failed ($LASTEXITCODE)" }
} finally {
    Pop-Location
}
$apk = Join-Path $RF_ROOT "android\app\build\outputs\apk\$Config\app-$Config.apk"
Write-Host "[build] $apk"
