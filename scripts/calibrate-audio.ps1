# Measures the engine's own render of every Track's staged arrangement in each sound variant and writes the
# output gain that puts it at target_lufs (patches.json) into src/audio/loudness_calibration.json, the way
# musicproto.py normalises its renders. Run it after the music data or the DSP changes (the engine warns at
# start-up when the calibration was made from other data), then rebuild.
#   scripts\calibrate-audio.ps1 [-Check]      (-Check: exit 3 when the stored calibration is out of date)
param([switch]$Check)
. "$PSScriptRoot\env.ps1"
. "$PSScriptRoot\audio-common.ps1"

$exe = Build-RfHeadless
if ($Check) {
    & $exe --calibrate --check
    exit $LASTEXITCODE
}
& $exe --calibrate --write (Join-Path $RF_ROOT 'src\audio\loudness_calibration.json')
if ($LASTEXITCODE -ne 0) { throw "calibration failed ($LASTEXITCODE)" }
Write-Host "[calibrate] wrote src\audio\loudness_calibration.json; rebuild to embed it"
