# Each upgrade alone over whole runs (2026-11-15, the owner: "measure each upgrade in isolation ... see how it affects runs ...
# see if any upgrades don't really help"). Runs `raveform_headless --sim isolate` in four parts at once (the PC's four cores),
# then writes the table: every Mod and Riff that can be dealt, held from the first tick at its cap (or -Rank N), against the bare
# ship on the same seeds. -Policy: novice (default: a young player's first week), assist, play (careful), reckless.
# -Base split,double_time: those first, one rank each, under every row (the first Trio's build): what a pick adds to a build.
#   scripts\isolate-upgrades.ps1 [-Seeds 40] [-Minutes 40] [-Policy novice] [-Rank cap|N] [-Base split,double_time] [-Only split,pulse_ring]
#                                [-Out captures\isolate]
param(
    [int]$Seeds = 40,
    [int]$Minutes = 40,
    [string]$Policy = 'novice',
    [string]$Rank = 'cap',
    [string[]]$Base = @(),  # (a list: -Base split,double_time is an array to PowerShell)
    [string[]]$Only = @(),
    [string]$Out = 'captures\isolate',
    [string]$Rebuild = '',  # a run's tag (its files' name before .partN.txt): its table again, nothing run
    [int]$Parts = 16  # (the owner, 2026-11-15: "i have 16 cores, cmon...": one process a physical core, the PC's whole CPU)
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'env.ps1')
[string]$baseList = ($Base | ForEach-Object { $_ -split '[,\s]+' } | Where-Object { $_ }) -join ','
[string]$onlyList = ($Only | ForEach-Object { $_ -split '[,\s]+' } | Where-Object { $_ }) -join ','
$root = Split-Path $PSScriptRoot
$exe = Join-Path $root 'build\desktop-dev\raveform_headless.exe'
if (-not (Test-Path $exe)) { throw "build the dev configuration first: scripts\build-desktop.ps1 -Config dev" }
$outDir = if ([IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $root $Out }
New-Item -ItemType Directory -Force $outDir | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$tag = "$Policy" + "_rank$Rank" + $(if ($baseList) { '_base-' + ($baseList -replace ',', '+') } else { '' }) + "_$stamp"
if ($Rebuild) { $tag = $Rebuild; $stamp = ($Rebuild -split '_')[-2..-1] -join '_' }  # (-Rebuild <tag>: the table again from that run's parts)
$t0 = Get-Date
$procs = @()
for ($k = 0; $k -lt $Parts -and -not $Rebuild; ++$k) {
    $a = @('--sim', 'isolate', '--seeds', "$Seeds", '--minutes', "$Minutes", '--policy', $Policy, '--rank', $Rank, '--part', "$k", '--parts', "$Parts")
    if ($onlyList) { $a += @('--only', $onlyList) }
    if ($baseList) { $a += @('--base', $baseList) }
    $p = Start-Process -FilePath $exe -ArgumentList $a -RedirectStandardOutput (Join-Path $outDir "$tag.part$k.txt") `
        -RedirectStandardError (Join-Path $outDir "$tag.part$k.err") -WindowStyle Hidden -PassThru
    # env.ps1 keeps this script on 4 logical CPUs (the project's rule for builds); the measurement's processes get every CPU
    # (the owner's word for these runs), so the scheduler gives each its own core.
    try { $p.ProcessorAffinity = [IntPtr]::new(([Int64]1 -shl [Environment]::ProcessorCount) - 1) } catch { }
    $procs += $p
}
$procs | ForEach-Object { $_.WaitForExit() }
$rows = @()
foreach ($part in Get-ChildItem (Join-Path $outDir "$tag.part*.txt")) {
    foreach ($line in Get-Content $part.FullName) {
        if ($line -match '^RFSIM (\{"test":"isolate".*\})$') { $rows += ($Matches[1] | ConvertFrom-Json) }
    }
}
# (PowerShell's names ignore case: the reference row is $ref, never $base, which is the -Base parameter)
$ref = $rows | Where-Object { $_.id -eq 'none' } | Select-Object -First 1
if (-not $ref) { throw "no bare-ship row (see $outDir\$tag.part0.txt)" }
if ($Rebuild) { $baseList = $ref.base; $Policy = $ref.policy; $Seeds = $ref.seeds; $Minutes = $ref.minutes }
$f1 = { param($v) '{0:0.0}' -f $v }
$sd = { param($v, $fmt) if ($v -ge 0) { '+' + ($fmt -f $v) } else { $fmt -f $v } }
$boss = { param($s, $won) if ($won -gt 0) { '{0:0} s ({1})' -f $s, $won } else { '-' } }
$lines = @()
$what = if ($baseList) { "on the base build $baseList (one rank each)" } else { 'alone' }
$lines += "# Each upgrade $what ($Policy bot, rank $Rank, $Seeds seeds, at most $Minutes minutes, $stamp)"
$lines += ''
$lines += 'Held from the run''s first tick, the loadout frozen (no Trio, no Boost), the first run from Track 1 to its first death; the'
$lines += 'same seeds for every row; each change is against the first row (the bare ship, or the base build alone). Sorted by the'
$lines += 'bosses won. Read the Track reached and the bosses won first: a pick that kills faster reaches the harder Tracks sooner, so'
$lines += 'its runs can end sooner in minutes while getting further (and one that cannot die but cannot kill stalls in a boss fight).'
$lines += 'Noise (the bare ship on four seed sets of 40): Track +-0.1, bosses won +-4, minutes +-0.5. Kaleidos, Longwave, the Choir:'
$lines += 'seconds to kill at the first meeting, where won (how many runs won it).'
$lines += ''
$lines += '| Upgrade | Rank | Track reached | Change | Bosses won | Change | Minutes (mean) | Change | Alive at end | Hits a min | Kills a min | Kaleidos | Longwave | The Choir |'
$lines += '|---|---|---|---|---|---|---|---|---|---|---|---|---|---|'
$sortedRows = @($ref) + @($rows | Where-Object { $_.id -ne 'none' } |
    Sort-Object -Property @{ Expression = { [double]$_.bosses }; Descending = $true }, @{ Expression = { [double]$_.mean }; Descending = $true })
foreach ($r in $sortedRows) {
    $isRef = $r.id -eq 'none'
    $name = if ($isRef) { if ($baseList) { "(the base build: $baseList)" } else { '(bare ship)' } } else { $r.pick }
    $dTrack = if ($isRef) { '' } else { & $sd ([double]$r.track - [double]$ref.track) '{0:0.0}' }
    $dBoss = if ($isRef) { '' } else { & $sd ([double]$r.bosses - [double]$ref.bosses) '{0:0}' }
    $dMin = if ($isRef) { '' } else { & $sd ([double]$r.mean - [double]$ref.mean) '{0:0.0}' }
    $lines += ('| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} | {8}/{9} | {10:0.00} | {11:0} | {12} | {13} | {14} |' -f $name, $r.rank, (& $f1 $r.track), $dTrack,
        $r.bosses, $dBoss, (& $f1 $r.mean), $dMin, $r.alive, $r.seeds, [double]$r.hits_min, [double]$r.kills_min,
        (& $boss $r.kaleidos_s $r.kaleidos_won), (& $boss $r.longwave_s $r.longwave_won), (& $boss $r.choir_s $r.choir_won))
}
$table = Join-Path $outDir "$tag.md"
$lines | Set-Content -Path $table -Encoding utf8
$lines | ForEach-Object { $_ }
"[isolate] $($rows.Count) rows in $([int]((Get-Date) - $t0).TotalSeconds) s: $table"
