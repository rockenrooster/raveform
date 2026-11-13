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
#   scripts\review-m5.ps1 [-Part part1] [-Out captures\m5\part1] [-Config debug|dev] [-Only lw_chop,ch_cage]
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
}
if (-not $sets.ContainsKey($Part)) { throw "no review set '$Part' (part1)" }
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
Write-Host "[review] done: $Out"
