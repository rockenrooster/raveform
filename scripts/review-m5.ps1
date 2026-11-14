# The M5 review sets for the lead (desktop, Signal look, 2400x1080, fixed 1/60 s steps), one folder a part:
# captures\m5\<part>\ with review copies (*.small.png, at most 1600 px wide) and a contact sheet for each strip
# (*_sheet.png: eight frames at half size, two rows of four; a strip's frames are four frames apart, so they cover a beat).
# The moments are scripted in src/game/play_pictures_m5.cpp: each run jumps to its fight (--fight: the fight's home
# Track, fast-forwarded to it, its parts taken down to the phase), the careful bot plays with hits costing nothing, and
# the picture is taken when its moment comes (at most 60 s).
#   Part 1 (-Part part1): Longwave lw_swell, lw_jaw, lw_chop, lw_whitecap (the lunge's line), lw_bare, lw_death, strip
#   lw_strip; the Choir ch_unison, ch_canon, ch_cage, ch_fugue, ch_solo (the held note), ch_death, strip ch_strip; the
#   mini-bosses: alpha_tell (the howl), alpha_attack (the charge), alpha_x2 (winded), alpha_half, alpha_death, strip
#   alpha_strip; hive_tell (x 2), hive_ring, hive_spear, hive_wall, hive_half, hive_death, strip hive_strip; ram_tell
#   (the lock), ram_attack, ram_x2 (stunned), ram_death, strip ram_strip; carom_tell (a bounce), carom_x2, carom_split,
#   carom_death, strip carom_strip.
#   Part 2 (-Part part2): each new family at its moment round a still ship (fam_flanker, fam_skitter, fam_moth, fam_brood,
#   fam_brood_c, fam_blink, fam_hound_c, fam_drifter_b, fam_carom_b; strips fam_moth_strip, fam_blink_strip), the bot's run in
#   Track 2 cycle 3 and Track 3 cycles 2 and 3 (t2_c3, t3_c2, t3_c3), and the glyph sheets of all sixteen families.
#   Part 3 (-Part part3): every new upgrade round a still ship (play_pictures_m5.cpp kUpPics): the weapon Mods (up_weapons,
#   up_wave, up_helix, up_accent, up_standing_wave, up_kills, up_wingmen, up_quartet), defence and rhythm (up_defence, up_barline),
#   the Riffs (riff_lancer, riff_singularity), the Boosts in pairs (boost_*) and the Fusions (fusion_*).
#   Part 4 (-Part part4): Event Horizon hz_accretion (the lanes), hz_starved (the gasp), hz_fed (the eruption), hz_tidal
#   (the wave), hz_collapse (the jets shown), hz_jets (burning), hz_death, strip hz_strip.
#   Part 5 (-Part part5): each elite trait on a body ahead of a still ship (el_*), each structure (st_*), each event (ev_*).
#   scripts\review-m5.ps1 [-Part part1|part2|part3|part4] [-Out captures\m5\<part>] [-Config debug|dev] [-Only lw_chop,ch_cage]
# Debug (the default) runs with validation; the script stops on the first run that is not clean.
param(
    [string]$Part = 'part1',
    [string]$Out = '',
    [ValidateSet('debug', 'dev')][string]$Config = 'debug',
    [string[]]$Only = @(),
    [switch]$Sound,  # let the app be heard (default: --silent, the PC's runs are silent)
    [switch]$Watch  # show the windows (default: --offscreen, the PC's test runs stay off screen)
)
. "$PSScriptRoot\env.ps1"
if (-not $Out) { $Out = "captures\m5\$Part" }
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
    $late = Select-String -Path $log -Pattern 'did not come in 60 s' | Select-Object -First 1
    if ($late) { Write-Warning "$Png : its moment did not come; taken as it was" }
}

$sheet = 'import sys; from PIL import Image; fs=[Image.open(p).convert("RGB") for p in sys.argv[2:]]; w,h=fs[0].width//2,fs[0].height//2; ' +
         's=Image.new("RGB",(4*w,2*h)); [s.paste(f.resize((w,h),Image.LANCZOS),((i%4)*w,(i//4)*h)) for i,f in enumerate(fs)]; s.save(sys.argv[1])'

$sets = @{
    'part1' = @{
        singles = @('lw_swell', 'lw_jaw', 'lw_chop', 'lw_whitecap', 'lw_bare', 'lw_death', 'ch_unison', 'ch_canon', 'ch_cage', 'ch_fugue', 'ch_solo',
                    'ch_death', 'alpha_tell', 'alpha_attack', 'alpha_x2', 'alpha_half', 'alpha_death', 'hive_tell', 'hive_ring', 'hive_spear',
                    'hive_wall', 'hive_half', 'hive_death', 'ram_tell', 'ram_attack', 'ram_x2', 'ram_death', 'carom_tell', 'carom_x2', 'carom_split',
                    'carom_death')
        strips = @('lw_strip', 'ch_strip', 'alpha_strip', 'hive_strip', 'ram_strip', 'carom_strip')
    }
    'part2' = @{
        singles = @('fam_flanker', 'fam_skitter', 'fam_moth', 'fam_brood', 'fam_brood_c', 'fam_blink', 'fam_hound_c', 'fam_drifter_b', 'fam_carom_b',
                    't2_c3', 't3_c2', 't3_c3')
        strips = @('fam_moth_strip', 'fam_blink_strip')
        sheets = @(1, 2)
    }
    'part3' = @{
        singles = @('up_weapons', 'up_wave', 'up_helix', 'up_accent', 'up_standing_wave', 'up_kills', 'up_wingmen', 'up_quartet', 'up_defence',
                    'up_barline', 'riff_lancer', 'riff_singularity', 'boost_supernova_chain', 'boost_colossus_carousel', 'boost_freeze_fever',
                    'boost_black_hole', 'boost_chorus_beam', 'boost_fractal_stretch', 'fusion_tesla', 'fusion_shard_halo', 'fusion_cannon',
                    'fusion_collapse_murmuration')
        strips = @()
    }
    'part4' = @{
        singles = @('hz_accretion', 'hz_starved', 'hz_fed', 'hz_tidal', 'hz_collapse', 'hz_jets', 'hz_death', 'pr_one', 'pr_rings', 'pr_hemiola',
                    'pr_burn', 'pr_solo', 'pr_death', 'fb_slapback', 'fb_open', 'fb_regeneration', 'fb_howl', 'fb_death', 'mq_court', 'mq_open',
                    'mq_half', 'mq_death', 'bm_nest', 'bm_exposed', 'bm_half', 'bm_death', 'bs_draw', 'bs_fire', 'bs_dazed', 'bs_half', 'bs_death',
                    'sa_clock', 'sa_half', 'sa_death')
        strips = @('hz_strip', 'pr_strip', 'fb_strip', 'mq_strip', 'bs_strip', 'sa_strip')
    }
    'part6' = @{
        singles = @('cd_progression', 'cd_modulation', 'cd_inversion', 'cd_death', 'ap_quarters', 'ap_load', 'ap_eighths', 'ap_move', 'ap_two', 'ap_death',
                    'mg_drift', 'mg_rest', 'mg_mirage', 'mg_blink', 'mg_next', 'mg_leap', 'mg_death',
                    'rp_reprise', 'rp_kaleidos', 'rp_longwave', 'rp_choir', 'rp_horizon', 'rp_poly', 'rp_feedback', 'rp_last', 'rp_death',
                    'mm_answer', 'mm_copy', 'mm_half', 'mm_death', 'tt_rope', 'tt_slack', 'tt_flail', 'tt_death', 'wv_cage', 'wv_knot', 'wv_burst',
                    'wv_half', 'wv_death', 'dw_fill', 'dw_choke', 'dw_ring', 'dw_half', 'dw_death')
        strips = @('cd_strip', 'ap_strip', 'mg_strip', 'rp_strip', 'wv_strip')
    }
    'part5' = @{
        singles = @('el_armored', 'el_swift', 'el_giant', 'el_splitting', 'el_volatile', 'el_regenerating', 'el_shielded', 'el_twin', 'el_magnetic',
                    'el_commander', 'el_cloaked', 'el_phasing', 'st_vault', 'st_pylon', 'st_rift', 'st_bumper', 'st_current', 'st_lens',
                    'ev_vault_drop', 'ev_rift_surge', 'ev_elite_hunt', 'ev_comet', 'ev_eclipse', 'ev_meteor_rain', 'ev_convoy', 'ev_storm_cell')
        strips = @()
    }
}
if (-not $sets.ContainsKey($Part)) { throw "no review set '$Part' (part1, part2, part3, part4, part5, part6)" }
$singles = $sets[$Part].singles
$strips = $sets[$Part].strips
if ($Only.Count) {
    $singles = @($singles | Where-Object { $Only -contains $_ })
    $strips = @($strips | Where-Object { $Only -contains $_ })
}
foreach ($s in $singles) {
    $png = Join-Path $Out "$s.png"
    if (Test-Path $png) { Remove-Item $png }
    Invoke-Shot $png "--scenario play --picture m5_$s --strip 1"
    if (-not (Test-Path $png)) { throw "missing $png (the script's moment never came?)" }
    python "$PSScriptRoot\shrink_png.py" $png | Out-Null
    Write-Host "[review] $s"
}
foreach ($s in $strips) {
    $png = Join-Path $Out "$s.png"
    Get-ChildItem $Out -Filter "${s}_*.png" -ErrorAction SilentlyContinue | Remove-Item
    Invoke-Shot $png "--scenario play --picture m5_$s --strip 8"
    $frames = 0..7 | ForEach-Object { Join-Path $Out "${s}_$_.png" }
    foreach ($f in $frames) { if (-not (Test-Path $f)) { throw "missing $f" } }
    python -I -c $sheet (Join-Path $Out "${s}_sheet.png") @frames
    python "$PSScriptRoot\shrink_png.py" (Join-Path $Out "${s}_sheet.png") | Out-Null
    Write-Host "[review] $s (strip)"
}
# The glyph sheets (every family's five tiers in the engine: the silhouettes; part 2 on).
$sheets = if ($sets[$Part].ContainsKey('sheets')) { $sets[$Part].sheets } else { @() }
foreach ($pg in $sheets) {
    if ($Only.Count -and -not ($Only -contains "glyphs_$pg")) { continue }
    $png = Join-Path $Out "glyphs_$pg.png"
    Invoke-Shot $png "--scenario glyph_sheet --page $pg --frames 4"
    python "$PSScriptRoot\shrink_png.py" $png | Out-Null
    Write-Host "[review] glyphs_$pg"
}
Write-Host "[review] done: $Out"
