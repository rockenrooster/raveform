# Shared by the M2 audio scripts (dot-source after env.ps1): builds the headless tools of the desktop dev
# configuration (configuring it first when needed) and returns the renderer's path.

function Build-RfHeadless {
    param([string[]]$Targets = @('raveform_headless'))
    $dir = Join-Path $RF_ROOT 'build\desktop-dev'
    if (-not (Test-Path (Join-Path $dir 'build.ninja'))) {
        $clang = (Get-Command clang -ErrorAction Stop).Source
        $clangxx = (Get-Command clang++ -ErrorAction Stop).Source
        $ninja = (Get-Command ninja -ErrorAction Stop).Source
        & $RF_CMAKE -S $RF_ROOT -B $dir -G Ninja '-DCMAKE_BUILD_TYPE=RelWithDebInfo' "-DCMAKE_MAKE_PROGRAM=$ninja" `
            "-DCMAKE_C_COMPILER=$clang" "-DCMAKE_CXX_COMPILER=$clangxx" | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }
    }
    & $RF_CMAKE --build $dir --parallel $RF_JOBS --target @Targets | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "build of $($Targets -join ', ') failed ($LASTEXITCODE)" }
    return (Join-Path $dir 'raveform_headless.exe')
}
