# Builds (unless -NoBuild) and runs the desktop app; returns the app's exit code.
#   scripts\run-desktop.ps1                                   interactive, debug build (validation on)
#   scripts\run-desktop.ps1 -AppArgs '--scenario bench_idle --seconds 58'
#   scripts\run-desktop.ps1 -Screenshot captures\desktop.png -Res 2400x1080 -Small
# Exit codes: 0 ok, 1 init failed, 2 bad arguments, 3 validation messages, 4 bench aborted, 5 screenshot failed.
param(
    [ValidateSet('debug', 'dev')][string]$Config = 'debug',
    [switch]$NoBuild,
    [string]$AppArgs = '',
    [string]$Screenshot = '',  # PNG path (relative to the current directory)
    [int]$Frames = 0,          # with -Screenshot: frame to capture (default 120)
    [string]$Res = '',         # window size, e.g. 2400x1080
    [switch]$Small             # also write <name>.small.png, at most 1600 px wide
)
. "$PSScriptRoot\env.ps1"

if (-not $NoBuild) { & "$PSScriptRoot\build-desktop.ps1" -Config $Config }
$exe = Join-Path $RF_ROOT "build\desktop-$Config\raveform.exe"
if (-not (Test-Path $exe)) { throw "missing $exe - build first" }

$argv = @()
if ($AppArgs) { $argv += @($AppArgs -split '\s+' | Where-Object { $_ }) }
if ($Res) { $argv += @('--res', $Res) }
$shot = $null
if ($Screenshot) {
    $shot = [IO.Path]::GetFullPath((Join-Path (Get-Location) $Screenshot))
    New-Item -ItemType Directory -Force (Split-Path -Parent $shot) | Out-Null
    $argv += @('--screenshot', $shot)
    if ($Frames -le 0) { $Frames = 120 }
}
if ($Frames -gt 0) { $argv += @('--frames', "$Frames") }

Write-Host "[run] $exe $($argv -join ' ')"
& $exe @argv
$code = $LASTEXITCODE
if ($shot -and $Small -and (Test-Path $shot)) {
    python "$PSScriptRoot\shrink_png.py" $shot
}
exit $code
