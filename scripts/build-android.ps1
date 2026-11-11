# Builds the APK with the Gradle wrapper (AGP 8.13.0, Gradle 8.13, NDK r29, SDK CMake 3.22.1).
#   scripts\build-android.ps1 [-Config dev|debug] [-Offline] [-Clean]
# dev = native RelWithDebInfo, debuggable, debug-signed (the benchmarked build); debug = native Debug.
# -Offline: no network access at all (every dependency must already be in the Gradle cache).
# Output: android\app\build\outputs\apk\<config>\app-<config>.apk
# CPU limit (env.ps1): Gradle runs with --max-workers=4 and in a daemon of its own, started from this
# 4-core-confined process so it inherits the affinity. Gradle reuses any idle daemon whose immutable
# JVM options match (-D properties do not count), so this project's daemon gets two of its own:
# -XX:ActiveProcessorCount=4 (also sizes the JVM's thread pools) and a project-local -XX:ErrorFile.
# Daemons that other sessions or older builds started are never reused (and never stopped: no
# `gradlew --stop`). The native build's Ninja runs at most 4 jobs through the job pools in CMakeLists.txt.
param(
    [ValidateSet('dev', 'debug')][string]$Config = 'dev',
    [switch]$Offline,
    [switch]$Clean
)
. "$PSScriptRoot\env.ps1"

$task = ':app:assemble' + $Config.Substring(0, 1).ToUpper() + $Config.Substring(1)
$tasks = @()
if ($Clean) { $tasks += ':app:clean' }
$tasks += $task
$errorFile = Join-Path $RF_ROOT 'android\.gradle\hs_err_daemon.log'  # gitignored; marks this project's daemon
$gradleArgs = @('--console=plain', "--max-workers=$RF_JOBS",
                "-Dorg.gradle.jvmargs=-Xmx2048m -Dfile.encoding=UTF-8 -XX:ActiveProcessorCount=$RF_JOBS -XX:ErrorFile=$errorFile")
if ($Offline) { $gradleArgs += '--offline' }
Push-Location (Join-Path $RF_ROOT 'android')
try {
    & .\gradlew.bat @tasks @gradleArgs
    if ($LASTEXITCODE -ne 0) { throw "gradle $($tasks -join ' ') failed ($LASTEXITCODE)" }
} finally {
    Pop-Location
}
$apk = Join-Path $RF_ROOT "android\app\build\outputs\apk\$Config\app-$Config.apk"
Write-Host "[build] $apk"
