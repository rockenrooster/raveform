# The M3 stage B review set for the lead (desktop, Signal look, 2400x1080, fixed 1/60 s steps), written to
# captures\m3b\ with review copies (*.small.png, at most 1600 px wide) and contact sheets for the strips
# (*_sheet.png: eight frames at half size, two rows of four). The scripted moments are in
# src/game/play_pictures.cpp:
#   screens: screen_notice, screen_title, screen_settings_sound / _controls / _look, screen_pause (mid
#   count-in), screen_game_over; the Trio: trio, trio_answer, trio_take (strip); the HUD: hud_groove,
#   hud_build, hud_drop (Surge), hud_break; riff_orbitals, riff_pulse, riff_sentinel; boost_prism,
#   boost_hyper, boost_halo, boost_magnet_storm; edge; Kaleidos: boss_bloom, boss_refraction, boss_spiral,
#   boss_naked, boss_beam_tell, boss_beam, boss_iris, boss_pair_death (strip), boss_death (strip);
#   track_change (Track 1 -> 2 mid cross-fade); bot_3, bot_6, bot_10 (the bot's run at minutes 3, 6, 10).
#   scripts\review-m3b.ps1 [-Out captures\m3b] [-Config debug|dev] [-Only trio,boss_beam]
# Debug (the default) runs with validation; the script stops on the first run that is not clean.
param(
    [string]$Out = 'captures\m3b',
    [ValidateSet('debug', 'dev')][string]$Config = 'debug',
    [string[]]$Only = @()
)
. "$PSScriptRoot\env.ps1"
New-Item -ItemType Directory -Force $Out | Out-Null
$exe = Join-Path $RF_ROOT "build\desktop-$Config\raveform.exe"
if (-not (Test-Path $exe)) { throw "missing $exe - build first" }

function Invoke-Shot([string]$Png, [string]$AppArgs) {
    $log = [IO.Path]::ChangeExtension($Png, '.log')
    $full = [IO.Path]::GetFullPath($Png)
    $argv = @($AppArgs -split '\s+' | Where-Object { $_ }) + @('--res', '2400x1080', '--no-overlay', '--screenshot', $full)
    & $exe @argv *> $log
    if ($LASTEXITCODE -ne 0) { throw "run failed ($LASTEXITCODE) for $Png; see $log" }
    $v = Select-String -Path $log -Pattern 'validation messages: (\d+)' | Select-Object -Last 1
    if ($v -and $v.Matches[0].Groups[1].Value -ne '0') { throw "validation messages in $log" }
}

$sheet = 'import sys; from PIL import Image; fs=[Image.open(p).convert("RGB") for p in sys.argv[2:]]; w,h=fs[0].width//2,fs[0].height//2; ' +
         's=Image.new("RGB",(4*w,2*h)); [s.paste(f.resize((w,h),Image.LANCZOS),((i%4)*w,(i//4)*h)) for i,f in enumerate(fs)]; s.save(sys.argv[1])'

$singles = @('screen_notice', 'screen_title', 'screen_settings_sound', 'screen_settings_controls', 'screen_settings_look', 'screen_pause',
             'screen_game_over', 'trio', 'trio_answer', 'hud_groove', 'hud_build', 'hud_drop', 'hud_break', 'riff_orbitals', 'riff_pulse',
             'riff_sentinel', 'boost_prism', 'boost_hyper', 'boost_halo', 'boost_magnet_storm', 'edge', 'boss_bloom', 'boss_refraction',
             'boss_spiral', 'boss_naked', 'boss_beam_tell', 'boss_beam', 'boss_iris', 'track_change', 'bot_3', 'bot_6', 'bot_10')
$strips = @('trio_take', 'boss_pair_death', 'boss_death')
if ($Only.Count) {
    $singles = @($singles | Where-Object { $Only -contains $_ })
    $strips = @($strips | Where-Object { $Only -contains $_ })
}
foreach ($s in $singles) {
    $png = Join-Path $Out "$s.png"
    if (Test-Path $png) { Remove-Item $png }
    Invoke-Shot $png "--scenario play --picture $s --strip 1"
    if (-not (Test-Path $png)) { throw "missing $png (the script's moment never came?)" }
    python "$PSScriptRoot\shrink_png.py" $png | Out-Null
    Write-Host "[review] $s"
}
foreach ($s in $strips) {
    $png = Join-Path $Out "strip_$s.png"
    Invoke-Shot $png "--scenario play --picture $s --strip 8"
    $frames = 0..7 | ForEach-Object { Join-Path $Out "strip_${s}_$_.png" }
    foreach ($f in $frames) { if (-not (Test-Path $f)) { throw "missing $f (the script's moment never came?)" } }
    python -I -c $sheet (Join-Path $Out "strip_${s}_sheet.png") @frames
    python "$PSScriptRoot\shrink_png.py" (Join-Path $Out "strip_${s}_sheet.png") | Out-Null
    Write-Host "[review] strip $s"
}
