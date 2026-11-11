# Where the light of a scene frame comes from: renders the frame once as it is and once with each class of
# light off (--mute, src/core/args.h), then measures every picture with design/tools/scenes.py light.
#   scripts\light-breakdown.ps1 [-Scenario bench_endgame] [-Frame 600] [-Out captures\light] [-AppArgs '--tier high']
# Desktop, fixed 1/60 s steps, 2400x1080, no overlay. Prints a table (dark < 48 and mean luminance).
param(
    [string]$Scenario = 'bench_endgame',
    [int]$Frame = 600,
    [string]$Out = 'captures\light',
    [string]$AppArgs = '',
    [ValidateSet('debug', 'dev')][string]$Config = 'debug'
)
. "$PSScriptRoot\env.ps1"

# name, extra app arguments (one class of light off per row)
$rows = @(
    @('all', ''),
    @('lattice', '--mute grid'),
    @('hulls', '--mute enemy,elite,boss'),
    @('bullets', '--mute bullet'),
    @('sparks', '--mute sparks'),
    @('pools', '--mute pools'),
    @('sparkles', '--mute sparkles'),
    @('shards', '--mute shard'),
    @('player', '--mute player'),
    @('afterglow', '--mute afterglow'),
    @('background', '--mute background'),
    @('bloom', '--bloom off'),
    @('priority', '--mute priority'),
    @('pump', '--mute pump')
)
New-Item -ItemType Directory -Force $Out | Out-Null
$pngs = @()
foreach ($r in $rows) {
    $png = Join-Path $Out ("{0}_{1}_off_{2}.png" -f $Scenario, $Frame, $r[0])
    $a = "--scenario $Scenario --seconds 600 --warmup 0 --no-overlay $AppArgs $($r[1])".Trim()
    & "$PSScriptRoot\run-desktop.ps1" -Config $Config -NoBuild -Res 2400x1080 -Frames $Frame -Screenshot $png -AppArgs $a | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "run failed for '$($r[0])' ($LASTEXITCODE)" }
    $pngs += $png
}
python -I (Join-Path $RF_ROOT 'design\tools\scenes.py') light @pngs
