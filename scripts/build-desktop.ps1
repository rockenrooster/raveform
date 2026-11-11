# Builds the Windows dev build: native CMake + Ninja + llvm-mingw clang from PATH.
#   scripts\build-desktop.ps1 [-Config debug|dev] [-Clean]
# Output: build\desktop-<config>\raveform.exe (debug = validation layers on by default).
param(
    [ValidateSet('debug', 'dev')][string]$Config = 'debug',
    [switch]$Clean
)
. "$PSScriptRoot\env.ps1"

$buildType = @{ debug = 'Debug'; dev = 'RelWithDebInfo' }[$Config]
$dir = Join-Path $RF_ROOT "build\desktop-$Config"
if ($Clean -and (Test-Path $dir)) { Remove-Item -Recurse -Force $dir }

$clang = (Get-Command clang -ErrorAction Stop).Source
$clangxx = (Get-Command clang++ -ErrorAction Stop).Source
$ninja = (Get-Command ninja -ErrorAction Stop).Source

if (-not (Test-Path (Join-Path $dir 'build.ninja'))) {
    & $RF_CMAKE -S $RF_ROOT -B $dir -G Ninja "-DCMAKE_BUILD_TYPE=$buildType" "-DCMAKE_MAKE_PROGRAM=$ninja" `
        "-DCMAKE_C_COMPILER=$clang" "-DCMAKE_CXX_COMPILER=$clangxx"
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }
}
& $RF_CMAKE --build $dir
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
Write-Host "[build] $dir\raveform.exe"
