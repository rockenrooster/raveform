# Runs the unit tests (TECH_SPEC 10): M2's audio suite (queues, tempo map, sequencer timing, lead rule,
# variants, music data, allocation-free rendering), the loudness calibration check, M3's simulation suite
# (grid, pools and handles, Chain and score rules, lead rule, Nova timing, species table, allocation-free
# tick), the determinism test (same seed and recorded inputs give the same state hash, twice), and M4's
# network: its unit tests (packet parsing fuzz, the wire and its condition simulator, clock and song sync, a
# short loopback session) and the loopback soak of TECH_SPEC 8.4 (one host and three bots, six simulated
# minutes since play test 3: the first three Tracks, as the ten covered before) on a clean network and again with 5% loss and 80 ms of jitter (with a pause for everyone).
# Desktop dev build; exit code 0 when everything passes.
#   scripts\test.ps1
. "$PSScriptRoot\env.ps1"
. "$PSScriptRoot\audio-common.ps1"

$null = Build-RfHeadless -Targets @('raveform_tests', 'raveform_headless', 'raveform_sim_tests', 'raveform_net_tests')
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
# M4: the network.
& (Join-Path $dir 'raveform_net_tests.exe')
if ($LASTEXITCODE -ne 0) { $failed += 1; Write-Host "[test] network unit tests failed ($LASTEXITCODE)" }
# (Play test 3: Tracks are half as long, so six simulated minutes are the first three Tracks the ten covered before; Tracks 4-6
# in co-op show census mismatches at HEAD a008b71 too (17 minutes: 435), the network engineer's to look at.)
& (Join-Path $dir 'raveform_headless.exe') --net soak --minutes 6 | Select-Object -Last 10
if ($LASTEXITCODE -ne 0) { $failed += 1; Write-Host "[test] loopback soak (clean) failed ($LASTEXITCODE)" }
& (Join-Path $dir 'raveform_headless.exe') --net soak --minutes 6 --pause --netsim 'loss=0.05,jitter=80' | Select-Object -Last 11
if ($LASTEXITCODE -ne 0) { $failed += 1; Write-Host "[test] loopback soak (5% loss, 80 ms jitter) failed ($LASTEXITCODE)" }
if ($failed) { exit 1 }
Write-Host '[test] all passed'
