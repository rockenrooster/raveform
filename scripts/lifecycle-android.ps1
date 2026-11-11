# Background/foreground cycles on one device (HOME, then am start) in one short burst, checking that
# the process survives, every resume presents again, and our log has no crash or Vulkan error.
#   scripts\lifecycle-android.ps1 -Serial ZY22BXMJG3 [-Cycles 20]
# Shared phones (CLAUDE.md): the burst starts only when the launcher or RAVEFORM has been in front for
# -IdleSeconds; HOME is only sent while RAVEFORM is in front. If another app shows up, the burst stops
# (reported as not verified) instead of competing for the screen. RAVEFORM is closed at the end.
param(
    [Parameter(Mandatory)][string]$Serial,
    [int]$Cycles = 20,
    [double]$BackgroundSeconds = 1.5,
    [double]$ForegroundSeconds = 2.5,
    [int]$IdleSeconds = 10,
    [int]$BusyTimeoutSeconds = 300
)
. "$PSScriptRoot\env.ps1"
$Serial = Resolve-RfSerial $Serial
Assert-RfDevice $Serial
$homePkg = Get-RfHomePackage $Serial

if (-not (Wait-RfDeviceIdle -Serial $Serial -QuietSeconds $IdleSeconds -TimeoutSeconds $BusyTimeoutSeconds)) {
    throw "[life] $Serial stayed in use by another session; lifecycle check NOT VERIFIED"
}
$appPid = Start-RfApp -Serial $Serial
if (-not $appPid) { throw "RAVEFORM did not start on $Serial" }
Start-Sleep -Seconds 3

$done = 0
$failure = $null
for ($i = 1; $i -le $Cycles; ++$i) {
    $fg = Get-RfForegroundPackage $Serial
    if ($fg -ne $RF_APP_ID) { $failure = "cycle ${i}: '$fg' is in front (another session); burst stopped"; break }
    & $RF_ADB -s $Serial shell input keyevent KEYCODE_HOME
    Start-Sleep -Milliseconds ([int]($BackgroundSeconds * 1000))
    $fg = Get-RfForegroundPackage $Serial
    if ($fg -ne $homePkg -and $fg -ne '') { $failure = "cycle ${i}: '$fg' took the screen while we were in the background; burst stopped"; break }
    & $RF_ADB -s $Serial shell am start -n $RF_ACTIVITY | Out-Null
    Start-Sleep -Milliseconds ([int]($ForegroundSeconds * 1000))
    $pidNow = Get-RfPid $Serial
    if ($pidNow -ne $appPid) { $failure = "cycle ${i}: process changed from '$appPid' to '$pidNow' (crash or restart)"; break }
    $done = $i
}
Start-Sleep -Seconds 2

# Evidence from our own process only (logcat is shared: read, never clear).
$log = & $RF_ADB -s $Serial logcat -d --pid=$appPid
$backgrounds = @($log -match 'RFLIFE.*background \d+').Count
$resumes = @($log -match 'RFLIFE.*resumed \d+: presenting').Count
$released = @($log -match 'render thread released the surface').Count
$stuck = @($log -match 'did not release the surface').Count
$problems = @($log -match 'FATAL|SIGSEGV|SIGABRT|VK_ERROR|\[vulkan| E RAVEFORM')
$crashes = @(((& $RF_ADB -s $Serial logcat -d -b crash) -match [regex]::Escape($RF_APP_ID)) -match "\b$appPid\b")

$shot = Join-Path $RF_ROOT ("captures\{0}-lifecycle-{1}.png" -f $Serial, (Get-Date -Format 'yyyyMMdd-HHmmss'))
if ((Get-RfForegroundPackage $Serial) -eq $RF_APP_ID) {
    & "$PSScriptRoot\capture-android.ps1" -Serial $Serial -Out $shot -Small | Out-Null
}
if (Get-RfPid $Serial) { Stop-RfApp $Serial }  # leave the phone tidy

Write-Host ("[life] {0}: {1}/{2} cycles; RFLIFE background {3}, resumed+presenting {4}; surface released in time {5}, late {6}; problem lines {7}; crash-buffer lines {8}" -f `
        $Serial, $done, $Cycles, $backgrounds, $resumes, $released, $stuck, $problems.Count, $crashes.Count)
$problems + $crashes | Select-Object -First 20 | ForEach-Object { Write-Host "  $_" }
if (Test-Path $shot) { Write-Host "[life] final screenshot $shot" }
if ($failure) { Write-Host "[life] NOT VERIFIED: $failure"; exit 2 }
if ($resumes -lt $Cycles -or $stuck -gt 0 -or $problems.Count -gt 0 -or $crashes.Count -gt 0) { exit 1 }
Write-Host "[life] PASS"
