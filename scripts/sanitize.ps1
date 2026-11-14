# M7 (PLAN, release preparation): the tests and the headless tool under AddressSanitizer and UndefinedBehaviorSanitizer
# (llvm-mingw clang: the dynamic ASan runtime and UBSan, no download). Builds build\desktop-asan (RelWithDebInfo with
# -fsanitize=address,undefined; vptr is off: the code has no RTTI) and runs, each with its own log in captures\asan\:
#   the audio, simulation, network and save unit tests; the determinism test; a 3-minute headless bot run from Track 1; one fight
#   start in Tracks 4, 7 and 10 (the careful bot to the Track, then the novice to its boss: --handover); the 3-minute
#   loopback net soak.
# A run fails on a non-zero exit or on any sanitizer report ("ERROR: AddressSanitizer", "runtime error:") in its log.
#   scripts\sanitize.ps1 [-Only tests,soak,fights,net] [-NoBuild]
param(
    [string[]]$Only = @(),
    [switch]$NoBuild
)
. "$PSScriptRoot\env.ps1"
$dir = Join-Path $RF_ROOT 'build\desktop-asan'
$out = Join-Path $RF_ROOT 'captures\asan'
New-Item -ItemType Directory -Force $out | Out-Null
if (-not $NoBuild) {
    if (-not (Test-Path (Join-Path $dir 'build.ninja'))) {
        $clang = (Get-Command clang -ErrorAction Stop).Source
        $clangxx = (Get-Command clang++ -ErrorAction Stop).Source
        $ninja = (Get-Command ninja -ErrorAction Stop).Source
        $san = '-fsanitize=address,undefined -fno-sanitize=vptr -fno-omit-frame-pointer'
        & $RF_CMAKE -S $RF_ROOT -B $dir -G Ninja '-DCMAKE_BUILD_TYPE=RelWithDebInfo' "-DCMAKE_MAKE_PROGRAM=$ninja" `
            "-DCMAKE_C_COMPILER=$clang" "-DCMAKE_CXX_COMPILER=$clangxx" "-DCMAKE_C_FLAGS=$san" "-DCMAKE_CXX_FLAGS=$san" `
            '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined' '-DRF_SANITIZE=ON' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }
    }
    & $RF_CMAKE --build $dir --parallel $RF_JOBS --target raveform_tests raveform_sim_tests raveform_net_tests raveform_save_tests raveform_headless | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "sanitizer build failed ($LASTEXITCODE)" }
}
# Every report, not only the first; leaks are not checked (LeakSanitizer does not run on Windows).
$env:ASAN_OPTIONS = 'halt_on_error=0:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1'
$env:UBSAN_OPTIONS = 'print_stacktrace=1'
$failed = @()
function Invoke-San([string]$Name, [string]$Exe, [string[]]$Arguments) {
    $log = Join-Path $out "$Name.log"
    $sw = [Diagnostics.Stopwatch]::StartNew()
    & (Join-Path $dir $Exe) @Arguments *> $log
    $code = $LASTEXITCODE
    $reports = @(Select-String -Path $log -Pattern 'ERROR: AddressSanitizer|runtime error:|ERROR: UndefinedBehaviorSanitizer')
    $line = '[san] {0}: exit {1}, {2} sanitizer report(s), {3:N0} s' -f $Name, $code, $reports.Count, $sw.Elapsed.TotalSeconds
    Write-Host $line
    if ($code -ne 0 -or $reports.Count) {
        $script:failed += $Name
        $reports | Select-Object -First 5 | ForEach-Object { Write-Host "    $($_.Line)" }
    }
}
$all = $Only.Count -eq 0
if ($all -or $Only -contains 'tests') {
    Invoke-San 'audio_tests' 'raveform_tests.exe' @()
    Invoke-San 'sim_tests' 'raveform_sim_tests.exe' @()
    Invoke-San 'net_tests' 'raveform_net_tests.exe' @()
    Invoke-San 'save_tests' 'raveform_save_tests.exe' @()
    Invoke-San 'determinism' 'raveform_headless.exe' @('--sim', 'determinism', '--minutes', '2')
}
if ($all -or $Only -contains 'soak') { Invoke-San 'bot_3min' 'raveform_headless.exe' @('--sim', 'soak', '--minutes', '3') }
if ($all -or $Only -contains 'fights') {
    foreach ($t in 4, 7, 10) {
        Invoke-San "fight_track$t" 'raveform_headless.exe' @('--sim', 'crowd', '--seeds', '1', '--minutes', '60', '--policy', 'novice', '--handover', "$t")
    }
}
if ($all -or $Only -contains 'net') { Invoke-San 'net_soak_3min' 'raveform_headless.exe' @('--net', 'soak', '--minutes', '3') }
if ($failed.Count) { Write-Host "[san] failed: $($failed -join ', ')"; exit 1 }
Write-Host '[san] all clean'
