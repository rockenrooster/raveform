# Runs the unit tests (TECH_SPEC 10): M2's audio suite (queues, tempo map, sequencer timing, lead rule,
# variants, music data, allocation-free rendering), the loudness calibration check, M3's simulation suite
# (grid, pools and handles, Chain and score rules, lead rule, Nova timing, species table, allocation-free
# tick) and the determinism test (same seed and recorded inputs give the same state hash, twice).
# Desktop dev build; exit code 0 when everything passes.
#   scripts\test.ps1
. "$PSScriptRoot\env.ps1"
. "$PSScriptRoot\audio-common.ps1"

$null = Build-RfHeadless -Targets @('raveform_tests', 'raveform_headless', 'raveform_sim_tests')
$dir = Join-Path $RF_ROOT 'build\desktop-dev'
$failed = 0
& (Join-Path $dir 'raveform_tests.exe')
if ($LASTEXITCODE -ne 0) { $failed += 1; Write-Host "[test] audio unit tests failed ($LASTEXITCODE)" }
& (Join-Path $dir 'raveform_headless.exe') --calibrate --check | Select-Object -Last 1
if ($LASTEXITCODE -ne 0) { $failed += 1; Write-Host '[test] loudness calibration is out of date (scripts\calibrate-audio.ps1)' }
& (Join-Path $dir 'raveform_sim_tests.exe')
if ($LASTEXITCODE -ne 0) { $failed += 1; Write-Host "[test] simulation unit tests failed ($LASTEXITCODE)" }
& (Join-Path $dir 'raveform_headless.exe') --sim determinism --minutes 2
if ($LASTEXITCODE -ne 0) { $failed += 1; Write-Host "[test] determinism test failed ($LASTEXITCODE)" }
if ($failed) { exit 1 }
Write-Host '[test] all passed'
