# The M1 review set of design/scenes.md section 5 (desktop, Signal look, 2400x1080, fixed 1/60 s steps),
# written to captures\m1\ (old pictures of the same names are replaced), with the compare output of the
# acceptance stills and a scenes.py light line for every moving-scene picture.
#   scripts\review-m1.ps1 [-Out captures\m1] [-SkipStills] [-SkipStrips]
# Every run is a Debug build with validation; the script stops on the first run that is not clean.
param(
    [string]$Out = 'captures\m1',
    [switch]$SkipStills,
    [switch]$SkipStrips
)
. "$PSScriptRoot\env.ps1"
New-Item -ItemType Directory -Force $Out | Out-Null
$tools = Join-Path $RF_ROOT 'design\tools\scenes.py'

function Invoke-Shot([string]$Png, [int]$Frames, [string]$AppArgs) {
    $log = [IO.Path]::ChangeExtension($Png, '.log')
    & "$PSScriptRoot\run-desktop.ps1" -NoBuild -Res 2400x1080 -Frames $Frames -Screenshot $Png -AppArgs $AppArgs *> $log
    if ($LASTEXITCODE -ne 0) { throw "run failed ($LASTEXITCODE) for $Png; see $log" }
    $v = Select-String -Path $log -Pattern 'validation messages: (\d+)' | Select-Object -Last 1
    if ($v -and $v.Matches[0].Groups[1].Value -ne '0') { throw "validation messages in $log" }
}

if (-not $SkipStills) {
    # Acceptance stills (look.md section 9) and their comparisons.
    $stills = @(
        @('look_card_nobloom', 'look_card', '--bloom off', '--nobloom'),
        @('look_card', 'look_card', '--bloom on', ''),
        @('frame_reef', 'frame_reef', '--bloom on', '')
    )
    foreach ($s in $stills) {
        $png = Join-Path $Out "$($s[0]).png"
        Invoke-Shot $png 4 "--scene design/scenes/$($s[1]).rfscene --still $($s[2]) --no-overlay"
        $cmpArgs = @('compare', $png, $s[1]); if ($s[3]) { $cmpArgs += $s[3] }
        python -I $tools @cmpArgs | Tee-Object -FilePath (Join-Path $Out "$($s[0]).compare.txt")
    }
    foreach ($page in 1..3) { Invoke-Shot (Join-Path $Out "glyph_sheet_$page.png") 4 "--scenario glyph_sheet --page $page --no-overlay" }
}

# Moving scenes at frame 600 (Signal; the endgame also in two more palettes).
$moving = @(
    @('bench_mid_600_signal_reef', '--scenario bench_mid'),
    @('bench_endgame_600_signal_reef', '--scenario bench_endgame'),
    @('bench_endgame_600_signal_ultraviolet', '--scenario bench_endgame --palette ultraviolet'),
    @('bench_endgame_600_signal_furnace', '--scenario bench_endgame --palette furnace')
)
$pngs = @()
foreach ($m in $moving) {
    $png = Join-Path $Out "$($m[0]).png"
    Invoke-Shot $png 600 "$($m[1]) --seconds 600 --warmup 0 --no-overlay"
    $pngs += $png
}

if (-not $SkipStrips) {
    # Eight consecutive frames across the first Drop (bar 8, frame 873) and across a quiet bar.
    foreach ($end in 879, 407) {
        $png = Join-Path $Out "strip_bench_endgame_$end.png"
        Invoke-Shot $png $end "--scenario bench_endgame --seconds 600 --warmup 0 --no-overlay --strip 8"
        $frames = 0..7 | ForEach-Object { Join-Path $Out "strip_bench_endgame_${end}_$_.png" }
        $pngs += $frames
        # Contact sheet: the eight frames at half size, two rows of four.
        $code = 'import sys; from PIL import Image; fs=[Image.open(p).convert("RGB") for p in sys.argv[2:]]; w,h=fs[0].width//2,fs[0].height//2; ' +
                's=Image.new("RGB",(4*w,2*h)); [s.paste(f.resize((w,h),Image.LANCZOS),((i%4)*w,(i//4)*h)) for i,f in enumerate(fs)]; s.save(sys.argv[1])'
        python -I -c $code (Join-Path $Out "strip_bench_endgame_${end}_sheet.png") @frames
    }
}

python -I $tools light @pngs | Tee-Object -FilePath (Join-Path $Out 'light.txt')
