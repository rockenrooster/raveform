# The M3 stage A review set for the lead (desktop, Signal look, 2400x1080, fixed 1/60 s steps), written to
# captures\m3\ with review copies (*.small.png, at most 1600 px wide) and, for each strip, a contact sheet
# (*_sheet.png: the eight frames at half size, two rows of four):
#   strips of eight consecutive frames (scripted scenes, game/play.cpp): fire, kill_small, kill_medium,
#   kill_large, shards, hit, nova, nova_front, dash, perfect_dash, warp, warp_land, sentry;
#   single frames of bot play at 30 s, 2 min and 5 min, and of bench_stress.
#   scripts\review-m3.ps1 [-Out captures\m3] [-Config debug|dev] [-SkipStrips] [-SkipFrames]
# Debug (the default) runs with validation; the script stops on the first run that is not clean.
param(
    [string]$Out = 'captures\m3',
    [ValidateSet('debug', 'dev')][string]$Config = 'debug',
    [switch]$SkipStrips,
    [switch]$SkipFrames
)
. "$PSScriptRoot\env.ps1"
New-Item -ItemType Directory -Force $Out | Out-Null
$exe = Join-Path $RF_ROOT "build\desktop-$Config\raveform.exe"
if (-not (Test-Path $exe)) { throw "missing $exe - build first" }

function Invoke-Shot([string]$Png, [string]$AppArgs, [int]$Frames = 0) {
    $log = [IO.Path]::ChangeExtension($Png, '.log')
    $full = [IO.Path]::GetFullPath($Png)
    $argv = @($AppArgs -split '\s+' | Where-Object { $_ }) + @('--res', '2400x1080', '--no-overlay', '--screenshot', $full)
    if ($Frames -gt 0) { $argv += @('--frames', "$Frames") }
    & $exe @argv *> $log
    if ($LASTEXITCODE -ne 0) { throw "run failed ($LASTEXITCODE) for $Png; see $log" }
    $v = Select-String -Path $log -Pattern 'validation messages: (\d+)' | Select-Object -Last 1
    if ($v -and $v.Matches[0].Groups[1].Value -ne '0') { throw "validation messages in $log" }
}

$sheet = 'import sys; from PIL import Image; fs=[Image.open(p).convert("RGB") for p in sys.argv[2:]]; w,h=fs[0].width//2,fs[0].height//2; ' +
         's=Image.new("RGB",(4*w,2*h)); [s.paste(f.resize((w,h),Image.LANCZOS),((i%4)*w,(i//4)*h)) for i,f in enumerate(fs)]; s.save(sys.argv[1])'

if (-not $SkipStrips) {
    $strips = @('fire', 'kill_small', 'kill_medium', 'kill_large', 'shards', 'hit', 'nova', 'nova_front', 'dash', 'perfect_dash',
                'warp', 'warp_land', 'sentry')
    foreach ($s in $strips) {
        $png = Join-Path $Out "strip_$s.png"
        Invoke-Shot $png "--scenario play --picture $s --strip 8"
        $frames = 0..7 | ForEach-Object { Join-Path $Out "strip_${s}_$_.png" }
        foreach ($f in $frames) { if (-not (Test-Path $f)) { throw "missing $f (the script's moment never came?)" } }
        python -I -c $sheet (Join-Path $Out "strip_${s}_sheet.png") @frames
        python "$PSScriptRoot\shrink_png.py" (Join-Path $Out "strip_${s}_sheet.png") | Out-Null
        Write-Host "[review] strip $s"
    }
}

if (-not $SkipFrames) {
    # Bot play: the game fast-forwarded to five seconds before the moment, then drawn for 300 frames.
    foreach ($m in @(@('bot_0030s', 25), @('bot_0120s', 115), @('bot_0300s', 295))) {
        $png = Join-Path $Out "$($m[0]).png"
        Invoke-Shot $png "--scenario play --bot --fast-forward $($m[1])" 300
        python "$PSScriptRoot\shrink_png.py" $png | Out-Null
        Write-Host "[review] $($m[0])"
    }
    $png = Join-Path $Out 'stress_0020s.png'
    Invoke-Shot $png '--scenario bench_stress --seconds 600 --warmup 0' 1200
    python "$PSScriptRoot\shrink_png.py" $png | Out-Null
    Write-Host '[review] stress'
}
