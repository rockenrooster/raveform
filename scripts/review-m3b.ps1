# The M3 stage B review set for the lead (desktop, Signal look, 2400x1080, fixed 1/60 s steps), written to
# captures\m3b\ with review copies (*.small.png, at most 1600 px wide) and contact sheets for the strips
# (*_sheet.png: eight frames at half size, two rows of four). The scripted moments are in
# src/game/play_pictures.cpp:
#   screens: screen_notice, screen_title, screen_settings_sound / _controls / _look, screen_pause (mid
#   count-in), screen_game_over; the Trio: trio, trio_answer, trio_take (strip), and since play test 1 trio_locked,
#   trio_hold (half filled) and split5 (Split at five lanes, play test 3); the HUD: hud_groove,
#   hud_build, hud_drop (Surge), hud_break; riff_orbitals, riff_pulse, riff_sentinel; boost_prism,
#   boost_hyper, boost_halo, boost_magnet_storm; edge; Kaleidos: boss_bloom, boss_refraction, boss_spiral,
#   boss_naked, boss_beam_tell, boss_beam, boss_iris, boss_pair_death (strip), boss_death (strip);
#   track_change (Track 2's name card after the Break); bot_3, bot_6, bot_10, bot_15 (the careful bot's real run
#   at minutes 3, 6, 10, 15) and bot_3_nohits ... bot_15_nohits (one run, hits costing nothing).
#   scripts\review-m3b.ps1 [-Out captures\m3b] [-Config debug|dev] [-Only trio,boss_beam]
# The stage C set: -Out captures\m3c -Only bot_3,bot_6,bot_10,bot_15,hud_drop,edge,boost_prism,track_change,boss_death
# The stage C2 set: -Out captures\m3c2 -Config dev -Only bot_3,bot_6,bot_10,bot_15,bot_3_nohits,bot_6_nohits,bot_10_nohits,
#   bot_15_nohits,hud_drop,boost_prism
# Play test 2's set: -Out captures\playtest2 -Only pt2_level_trio,pt2_cell_crowd,pt2_halo8,pt2_overcharge_0,pt2_overcharge_1,
#   pt2_overcharge_2,pt2_overcharge_3,pt2_overcharge_4,pt2_overcharge_5,boost_prism and -Config dev -Only pt2_hud_level (a
#   level-up Trio in a fight with the ship sheltered in an orb, a Cell in a crowd, the halo at eight cells with one
#   growing back, Overcharge 0-5, the HUD's Shard level); it also writes overcharge_sheet.png (the six side by side) and
#   pt2_halo8_zoom.png (the halo three times larger). The loop: loop_encore (the loop's first Track card, ENCORE ...
#   NO MORE REFILLS, HALO 6) and loop_burn (two arcs shattering as the halo burns down; also loop_burn_zoom.png).
# Play test 3's set: -Out captures\playtest3 -Only pt3_ring_broken,pt3_ring_relight,pt3_life_orb,pt3_life_use,pt3_vault,boss_hit,split5,
#   hud_groove (the Orbitals' guard ring broken and relighting, the Extra Life's orb and its use, a Vault's name, a hit on
#   Kaleidos's weak point as a strip, Split at five lanes, Track 1's phrase bar).
# Debug (the default) runs with validation; the script stops on the first run that is not clean.
param(
    [string]$Out = 'captures\m3b',
    [ValidateSet('debug', 'dev')][string]$Config = 'debug',
    [string[]]$Only = @(),
    [switch]$Sound,  # let the app be heard (default: --silent, the PC's runs are silent)
    [switch]$Watch  # show the windows (default: --offscreen, the PC's test runs stay off screen)
)
. "$PSScriptRoot\env.ps1"
New-Item -ItemType Directory -Force $Out | Out-Null
$exe = Join-Path $RF_ROOT "build\desktop-$Config\raveform.exe"
if (-not (Test-Path $exe)) { throw "missing $exe - build first" }

function Invoke-Shot([string]$Png, [string]$AppArgs) {
    $log = [IO.Path]::ChangeExtension($Png, '.log')
    $full = [IO.Path]::GetFullPath($Png)
    $argv = @($AppArgs -split '\s+' | Where-Object { $_ }) + @('--res', '2400x1080', '--no-overlay', '--screenshot', $full) + @(if ($Sound) { @() } else { @('--silent') }) + @(if ($Watch) { @() } else { @('--offscreen') })
    & $exe @argv *> $log
    if ($LASTEXITCODE -ne 0) { throw "run failed ($LASTEXITCODE) for $Png; see $log" }
    $v = Select-String -Path $log -Pattern 'validation messages: (\d+)' | Select-Object -Last 1
    if ($v -and $v.Matches[0].Groups[1].Value -ne '0') { throw "validation messages in $log" }
}

$sheet = 'import sys; from PIL import Image; fs=[Image.open(p).convert("RGB") for p in sys.argv[2:]]; w,h=fs[0].width//2,fs[0].height//2; ' +
         's=Image.new("RGB",(4*w,2*h)); [s.paste(f.resize((w,h),Image.LANCZOS),((i%4)*w,(i//4)*h)) for i,f in enumerate(fs)]; s.save(sys.argv[1])'

$singles = @('screen_notice', 'screen_title', 'screen_settings_sound', 'screen_settings_controls', 'screen_settings_look', 'screen_pause',
             'screen_game_over', 'trio', 'trio_answer', 'trio_locked', 'trio_hold', 'split5', 'hud_groove', 'hud_build', 'hud_drop', 'hud_break', 'riff_orbitals', 'riff_pulse',
             'riff_sentinel', 'boost_prism', 'boost_hyper', 'boost_halo', 'boost_magnet_storm', 'edge', 'boss_bloom', 'boss_refraction',
             'boss_spiral', 'boss_naked', 'boss_beam_tell', 'boss_beam', 'boss_iris', 'track_change', 'bot_3', 'bot_6', 'bot_10', 'bot_15', 'bot_3_nohits', 'bot_6_nohits', 'bot_10_nohits', 'bot_15_nohits',
             'pt2_level_trio', 'pt2_cell_crowd', 'pt2_halo8', 'pt2_hud_level', 'pt2_overcharge_0', 'pt2_overcharge_1', 'pt2_overcharge_2',
             'pt2_overcharge_3', 'pt2_overcharge_4', 'pt2_overcharge_5', 'loop_encore', 'loop_burn',
             'pt3_ring_broken', 'pt3_ring_relight', 'pt3_life_orb', 'pt3_life_use', 'pt3_vault')
$strips = @('trio_take', 'boss_pair_death', 'boss_death', 'boss_hit')
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
# Play test 2: Overcharge 0-5 in one sheet (the stretch in front of the ship), the halo three times larger.
$oc = 0..5 | ForEach-Object { Join-Path $Out "pt2_overcharge_$_.png" }
if (($oc | Where-Object { Test-Path $_ }).Count -eq 6) {
    $ocSheet = 'import sys; from PIL import Image, ImageDraw; cs=[]; ' +
               '[cs.append(Image.open(p).convert("RGB").crop((1120,240,1960,640))) for p in sys.argv[2:]]; ' +
               '[ImageDraw.Draw(c).rectangle((0,0,230,34),fill=(0,0,0)) or ImageDraw.Draw(c).text((8,8),"OVERCHARGE %d" % i,fill=(255,255,255)) for i,c in enumerate(cs)]; ' +
               'w,h=cs[0].size; s=Image.new("RGB",(2*w,3*h)); [s.paste(c,((i%2)*w,(i//2)*h)) for i,c in enumerate(cs)]; s.save(sys.argv[1])'
    python -I -c $ocSheet (Join-Path $Out 'overcharge_sheet.png') @oc
    python "$PSScriptRoot\shrink_png.py" (Join-Path $Out 'overcharge_sheet.png') | Out-Null
    Write-Host "[review] overcharge_sheet"
}
$halo = Join-Path $Out 'pt2_halo8.png'
if (Test-Path $halo) {
    python -I -c 'import sys; from PIL import Image; Image.open(sys.argv[1]).convert("RGB").crop((1090,430,1310,650)).resize((660,660),Image.LANCZOS).save(sys.argv[2])' $halo (Join-Path $Out 'pt2_halo8_zoom.png')
    Write-Host "[review] pt2_halo8_zoom"
}
# The loop: the halo burning down (two arcs shattering from eight), three times larger.
$burn = Join-Path $Out 'loop_burn.png'
if (Test-Path $burn) {
    python -I -c 'import sys; from PIL import Image; Image.open(sys.argv[1]).convert("RGB").crop((1050,390,1350,690)).resize((900,900),Image.LANCZOS).save(sys.argv[2])' $burn (Join-Path $Out 'loop_burn_zoom.png')
    Write-Host "[review] loop_burn_zoom"
}
# Play test 3: the guard ring, three times larger (broken, relighting).
foreach ($r in 'pt3_ring_broken', 'pt3_ring_relight') {
    $png = Join-Path $Out "$r.png"
    if (Test-Path $png) {
        python -I -c 'import sys; from PIL import Image; Image.open(sys.argv[1]).convert("RGB").crop((1050,390,1350,690)).resize((900,900),Image.LANCZOS).save(sys.argv[2])' $png (Join-Path $Out "${r}_zoom.png")
        Write-Host "[review] ${r}_zoom"
    }
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
