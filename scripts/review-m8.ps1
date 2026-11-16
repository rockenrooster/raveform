# M8's review pictures (docs/MODES_PLAN.md 7), desktop, Signal look, 2400x1080, fixed 1/60 s steps, written to captures\m8\<part>\
# with review copies (*.small.png, at most 1600 px wide) and contact sheets for the strips (*_sheet.png, four frames a row at half
# size). The solo pictures are scripted in src/game/play_pictures_m8.cpp; the room's are co-op shots on this PC (review-m4b.ps1's
# m8_ entries: a host and a joiner over the real lobby on 127.0.0.1).
#   p0: m8_title_pill_<mode> (seven), m8_modes_<mode> (seven), m8_modes_room, m8_lobby_host, m8_lobby_join, m8_lobby_versus,
#       m8_join_modes; strips a quarter beat apart: m8_t_lift (12 frames), m8_t_back (16), m8_t_mode (8), m8_t_track (16),
#       m8_t_drop (16). (m8_modes_bonus waits for the bonus Tracks: part 4.)
#   p1: ONE TRACK; p2: HEADLINERS; p3: FULL BLAST (m8_bl_lastcall, m8_bl_cut, m8_bl_result; strips m8_bl_torrent, m8_bl_wave,
#       m8_bl_back, m8_bl_count); p4: TRACERS (m8_tr_line, m8_tr_lock, m8_tr_floor4, m8_tr_result; strips m8_tr_pop, m8_tr_house,
#       m8_tr_count); p5: SONGLINE on Aurora (m8_sl_groove, m8_sl_build, m8_sl_intime, m8_sl_braid, m8_sl_count, m8_sl_result;
#       strips m8_sl_drop (a sixteenth apart), m8_sl_finale (a quarter beat apart from bar 47)).
#   scripts\review-m8.ps1 -Part p0 [-Config debug|dev] [-Only m8_modes_tracers,m8_t_lift] [-Out captures\m8\p0]
# Debug (the default) runs with validation; exit 1 on a validation message or a layout warning (two controls, or a control and a
# line, closer than 24 px).
param(
    [ValidateSet('p0', 'p1', 'p2', 'p3', 'p4', 'p5', 'p6')][string]$Part = 'p0',
    [ValidateSet('debug', 'dev')][string]$Config = 'debug',
    [string[]]$Only = @(),
    [string]$Out = '',
    [string]$Res = '2400x1080',
    [switch]$Sound,  # let the app be heard (default: --silent, the PC's runs are silent)
    [switch]$Watch   # show the windows (default: --offscreen, the PC's test runs stay off screen)
)
. "$PSScriptRoot\env.ps1"
if (-not $Out) { $Out = "captures\m8\$Part" }
New-Item -ItemType Directory -Force $Out | Out-Null
$exe = Join-Path $RF_ROOT "build\desktop-$Config\raveform.exe"
if (-not (Test-Path $exe)) { throw "missing $exe - build first" }
$script:problems = 0

function Invoke-Shot([string]$Png, [string]$AppArgs) {
    $log = [IO.Path]::ChangeExtension($Png, '.log')
    $full = [IO.Path]::GetFullPath($Png)
    $argv = @($AppArgs -split '\s+' | Where-Object { $_ }) + @('--res', $Res, '--no-overlay', '--screenshot', $full) +
            @(if ($Sound) { @() } else { @('--silent') }) + @(if ($Watch) { @() } else { @('--offscreen') })
    & $exe @argv *> $log
    if ($LASTEXITCODE -ne 0) { throw "run failed ($LASTEXITCODE) for $Png; see $log" }
    $v = Select-String -Path $log -Pattern 'validation messages: (\d+)' | Select-Object -Last 1
    if ($v -and $v.Matches[0].Groups[1].Value -ne '0') { Write-Host "  [validation] $($v.Line.Trim())"; $script:problems++ }
    Select-String -Path $log -Pattern 'ui: ' | ForEach-Object { Write-Host "  [layout] $($_.Line.Trim())"; $script:problems++ }
}

# A contact sheet: the frames at half size, four a row.
$sheet = 'import sys; from PIL import Image; fs=[Image.open(p).convert("RGB") for p in sys.argv[2:]]; w,h=fs[0].width//2,fs[0].height//2; ' +
         'rows=(len(fs)+3)//4; s=Image.new("RGB",(4*w,rows*h)); [s.paste(f.resize((w,h),Image.LANCZOS),((i%4)*w,(i//4)*h)) for i,f in enumerate(fs)]; s.save(sys.argv[1])'

$modes = @('set', 'one', 'headliners', 'blast', 'tracers', 'songline', 'jam')
if ($Part -eq 'p0') {
    $singles = @($modes | ForEach-Object { "m8_title_pill_$_" }) + @($modes | ForEach-Object { "m8_modes_$_" }) + @('m8_modes_bonus')
    $strips = [ordered]@{ 'm8_t_lift' = 20; 'm8_t_back' = 24; 'm8_t_mode' = 8; 'm8_t_track' = 16; 'm8_t_drop' = 16; 'm8_t_mode_one' = 8;
                          'm8_t_mode_headliners' = 8; 'm8_t_mode_blast' = 8; 'm8_t_mode_tracers' = 8; 'm8_t_mode_songline' = 8; 'm8_t_mode_jam' = 8 }
    $rooms = @('m8_modes_room', 'm8_lobby_host', 'm8_lobby_join', 'm8_lobby_versus', 'm8_join_modes')
} elseif ($Part -eq 'p1') {
    # p1: ONE TRACK (the load-in a strip two sixteenths apart, the name card, the outro's card, the result)
    $singles = @('m8_card_one', 'm8_one_outro', 'm8_result_one')
    $strips = [ordered]@{ 'm8_loadin' = 16 }
    $rooms = @()
} elseif ($Part -eq 'p2') {
    # p2: HEADLINERS (act 1's card, the result after the ten acts)
    $singles = @('m8_card_act', 'm8_card_act3', 'm8_result_headliners')
    $strips = [ordered]@{}
    $rooms = @()
} elseif ($Part -eq 'p3') {
    # p3: FULL BLAST on Aurora (full_blast.md "Look"): LAST CALL under the run bar, the power cut, the result; strips: the torrent four
    # sixteenths apart (10), wave 2's name a quarter beat apart (8), the power back an eighth of a beat apart (8), the load-out (8)
    $singles = @('m8_bl_lastcall', 'm8_bl_cut', 'm8_bl_result')
    $strips = [ordered]@{ 'm8_bl_torrent' = 10; 'm8_bl_wave' = 8; 'm8_bl_back' = 8; 'm8_bl_count' = 8 }
    $rooms = @()
} elseif ($Part -eq 'p6') {
    # p6: JAM on Toybox (jam.md "What to check"): the room with one layer, four, all eight; a shelf open; DROP ready and resting; the
    # room's Drop; POP-OFF's draft and its Drop
    $singles = @('m8_jam_room1', 'm8_jam_room4', 'm8_jam_room8', 'm8_jam_pad_states', 'm8_jam_shelf', 'm8_jam_drop_ready', 'm8_jam_drop_resting',
                 'm8_jam_room_drop', 'm8_popoff_draft', 'm8_popoff_drop')
    $strips = [ordered]@{ 'm8_jam_empty' = 8 }
    $rooms = @()
} elseif ($Part -eq 'p5') {
    # p5: SONGLINE on Aurora (songline.md "What to check"): a Groove's string with the guide, a Build's runway, a gate threaded in
    # time, four players' braid, the count, the result; strips: a Drop's tube a sixteenth apart (8), the finale a quarter beat apart (8)
    $singles = @('m8_sl_groove', 'm8_sl_build', 'm8_sl_intime', 'm8_sl_braid', 'm8_sl_count', 'm8_sl_result', 'm8_sl_gold_gate', 'm8_sl_streak_1',
                 'm8_sl_streak_2', 'm8_sl_streak_3', 'm8_sl_streak_4', 'm8_sl_bend_back', 'm8_sl_race_4')
    $strips = [ordered]@{ 'm8_sl_drop' = 8; 'm8_sl_finale' = 8 }
    $rooms = @()
} else {
    # p4: TRACERS on Reef (tracers.md "What to check"): a line half drawn with its hint, a lock, the floor at 60% in four colours,
    # the result; strips: the pop a thirty-second apart from the lock (8), FULL HOUSE a quarter beat apart (16), the count (8)
    $singles = @('m8_tr_line', 'm8_tr_lock', 'm8_tr_floor4', 'm8_tr_result', 'm8_tr_turf_4', 'm8_tr_cut', 'm8_tr_spin', 'm8_tr_duet', 'm8_tr_ring_big',
                 'm8_tr_boost_time_stretch', 'm8_tr_boost_freeze_frame', 'm8_tr_boost_halo', 'm8_tr_boost_magnet_storm', 'm8_tr_boost_black_hole',
                 'm8_tr_boost_colossus', 'm8_tr_boost_confetti', 'm8_tr_boost_long', 'm8_tr_boost_echo')
    $strips = [ordered]@{ 'm8_tr_pop' = 8; 'm8_tr_house' = 16; 'm8_tr_count' = 8; 'm8_tr_lights_out' = 8 }
    $rooms = @()
}
if ($Only.Count) {
    $singles = @($singles | Where-Object { $Only -contains $_ })
    $keep = [ordered]@{}
    foreach ($k in $strips.Keys) { if ($Only -contains $k) { $keep[$k] = $strips[$k] } }
    $strips = $keep
    $rooms = @($rooms | Where-Object { $Only -contains $_ })
}
foreach ($s in $singles) {
    $png = Join-Path $Out "$s.png"
    Invoke-Shot $png "--scenario play --picture $s --strip 1"
    python "$PSScriptRoot\shrink_png.py" $png | Out-Null
    Write-Host "[review] $s"
}
foreach ($s in $strips.Keys) {
    $n = $strips[$s]
    $png = Join-Path $Out "$s.png"
    Invoke-Shot $png "--scenario play --picture $s --strip $n"
    $frames = 0..($n - 1) | ForEach-Object { Join-Path $Out "${s}_$_.png" }
    foreach ($f in $frames) { if (-not (Test-Path $f)) { throw "missing $f (the script's moment never came?)" } }
    python -I -c $sheet (Join-Path $Out "${s}_sheet.png") @frames
    python "$PSScriptRoot\shrink_png.py" (Join-Path $Out "${s}_sheet.png") | Out-Null
    Write-Host "[review] strip $s ($n frames)"
}
if ($rooms.Count) {
    $m4b = @{ Out = $Out; Config = $Config; Only = $rooms; Res = $Res }
    if ($Watch) { $m4b.Watch = $true }
    & "$PSScriptRoot\review-m4b.ps1" @m4b
    if ($LASTEXITCODE -ne 0) { $script:problems++ }
}
if ($script:problems) { Write-Host "[review] $script:problems problem(s): see above"; exit 1 }
Write-Host "[review] $Out"
