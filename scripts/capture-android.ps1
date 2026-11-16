# Screenshots from a device into captures\.
#   scripts\capture-android.ps1 -Serial ZY22BXMJG3 [-Small]
#       adb screencap of the panel; RAVEFORM must be in front (we never capture another app's screen).
#   scripts\capture-android.ps1 -Serial ZY22BXMJG3 -InApp [-Frames 180] [-Small]
#       launches the app with --screenshot (once the launcher or RAVEFORM is in front): it reads back its
#       own swapchain image, undoes the pre-rotation, writes an upright PNG to its external files dir
#       and quits; the PNG is pulled.
# -Small also writes <name>.small.png, at most 1600 px wide (python + PIL), for review.
param(
    [Parameter(Mandatory)][string]$Serial,
    [string]$Out = '',
    [switch]$InApp,
    [int]$Frames = 180,
    [string]$AppArgs = '',
    [switch]$Small
)
. "$PSScriptRoot\env.ps1"
$Serial = Resolve-RfSerial $Serial
Assert-RfDevice $Serial

$dir = Join-Path $RF_ROOT 'captures'
New-Item -ItemType Directory -Force $dir | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $Out) { $Out = Join-Path $dir ("{0}-{1}{2}.png" -f (Get-RfDeviceId $Serial), $stamp, $(if ($InApp) { '-inapp' } else { '' })) }

if ($InApp) {
    $name = 'rf_inapp.png'
    $remote = "/sdcard/Android/data/$RF_APP_ID/files/$name"
    if (-not (Wait-RfDeviceIdle -Serial $Serial -QuietSeconds 5)) { throw "$Serial is in use by another session" }
    & $RF_ADB -s $Serial shell rm -f $remote | Out-Null
    Start-RfApp -Serial $Serial -AppArgs "$AppArgs --screenshot $name --frames $Frames".Trim() | Out-Null
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-RfPid $Serial) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 500 }
    if (Get-RfPid $Serial) { Stop-RfApp $Serial; throw "the app did not finish its screenshot run within 30 s" }
    & $RF_ADB -s $Serial pull $remote $Out | Out-Null
} else {
    $fg = Get-RfForegroundPackage $Serial
    if ($fg -ne $RF_APP_ID) { throw "RAVEFORM is not in front on $Serial ('$fg'); not capturing another app's screen" }
    $remote = '/sdcard/Download/rf_capture.png'
    & $RF_ADB -s $Serial shell screencap -p $remote
    & $RF_ADB -s $Serial pull $remote $Out | Out-Null
    & $RF_ADB -s $Serial shell rm -f $remote | Out-Null
}
if (-not (Test-Path $Out)) { throw "capture failed: $Out was not written" }
Write-Host "[capture] $Out"
if ($Small) { python "$PSScriptRoot\shrink_png.py" $Out }
