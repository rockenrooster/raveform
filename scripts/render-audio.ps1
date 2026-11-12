# Offline renders of the band (TECH_SPEC 6.7) and the M2 match against the prototype.
#   scripts\render-audio.ps1                 every Track x variant: engine render, match table, onset check,
#                                            and the listening files (WAV + MP3) in captures\m2\
#   scripts\render-audio.ps1 -Only track01_club
#   scripts\render-audio.ps1 -SkipRef        reuse captures\m2\ref (prototype renders take ~3 minutes)
# Writes captures\m2\match_table.md. The prototype references come from scripts\audio_ref.py.
param(
    [string]$Only = '',
    [switch]$SkipRef,
    [switch]$NoListening
)
. "$PSScriptRoot\env.ps1"
. "$PSScriptRoot\audio-common.ps1"

$exe = Build-RfHeadless
$out = Join-Path $RF_ROOT 'captures\m2'
$eng = Join-Path $out 'eng'
$ref = Join-Path $out 'ref'
New-Item -ItemType Directory -Force $out, $eng, $ref | Out-Null
$tracks = @('track01', 'track02', 'track03')
$sounds = @('club', 'arcade', 'velvet')
$haveFfmpeg = [bool](Get-Command ffmpeg -ErrorAction SilentlyContinue)

$rows = @()
foreach ($t in $tracks) {
    foreach ($s in $sounds) {
        $name = "${t}_$s"
        if ($Only -and $name -ne $Only) { continue }
        $refJson = Join-Path $ref "$name.ref.json"
        if (-not (Test-Path $refJson) -and -not $SkipRef) {
            & python -I -B (Join-Path $PSScriptRoot 'audio_ref.py') $ref $t $s | Out-Host
        }
        $wav = Join-Path $eng "$name.f32.wav"
        $events = Join-Path $eng "$name.events.csv"
        $meta = Join-Path $eng "$name.meta.json"
        & $exe --render staged --track $t --sound $s --out $wav --events $events --meta $meta | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "render $name failed" }
        $match = & python -I -B (Join-Path $PSScriptRoot 'audio_check.py') match $wav $meta $refJson
        $match | Out-Host
        $ons = & python -I -B (Join-Path $PSScriptRoot 'audio_check.py') onsets $events $meta
        $ons | Out-Host
        $m = Get-Content (Join-Path $eng "$name.f32.match.json") -Raw | ConvertFrom-Json
        $bandDiff = 0.0
        for ($i = 0; $i -lt 6; ++$i) { $bandDiff = [Math]::Max($bandDiff, [Math]::Abs($m.engine.bands_db[$i] - $m.reference.bands_db[$i])) }
        $secDiff = 0.0
        for ($i = 0; $i -lt $m.engine.sections.Count; ++$i) { $secDiff = [Math]::Max($secDiff, [Math]::Abs($m.engine.sections[$i].rms_db - $m.reference.sections[$i].rms_db)) }
        $onsetLine = ($ons | Select-String 'onsets:').Line
        $worst = if ($onsetLine -match 'worst \|frame - grid\| = (\d+)') { [int]$Matches[1] } else { -1 }
        $count = if ($onsetLine -match 'onsets: (\d+) checked') { [int]$Matches[1] } else { 0 }
        $rows += [pscustomobject]@{
            Render = $name; LufsEngine = $m.engine.lufs; LufsRef = $m.reference.lufs; TruePeak = $m.engine.true_peak_db
            BandWorst = $bandDiff; SectionWorst = $secDiff; Onsets = $count; OnsetWorst = $worst; Pass = $m.pass -and $worst -le 1
        }
        if (-not $NoListening -and ($s -eq 'club' -or $t -eq 'track01')) {
            $listen = Join-Path $out "$name.wav"
            & $exe --render staged --track $t --sound $s --out $listen --pcm16 | Out-Null
            if ($haveFfmpeg) { & ffmpeg -y -loglevel error -i $listen -codec:a libmp3lame -q:a 2 (Join-Path $out "$name.mp3") }
        }
    }
}

$md = @('| Render | LUFS engine | LUFS prototype | True peak dBFS | Worst band diff dB | Worst section diff dB | Onsets checked | Worst onset error (samples) | Verdict |',
        '|---|---|---|---|---|---|---|---|---|')
foreach ($r in $rows) {
    $md += '| {0} | {1:N2} | {2:N2} | {3:N2} | {4:N2} | {5:N2} | {6} | {7} | {8} |' -f $r.Render, $r.LufsEngine, $r.LufsRef, $r.TruePeak,
        $r.BandWorst, $r.SectionWorst, $r.Onsets, $r.OnsetWorst, $(if ($r.Pass) { 'pass' } else { 'FAIL' })
}
if (-not $Only) { $md | Set-Content -Path (Join-Path $out 'match_table.md') }
$md | Out-Host

if (-not $NoListening -and -not $Only) {
    foreach ($job in @(@('sfx', 'sfx_sheet'), @('leads', 'lead_timbres'), @('mock', 'gameplay_mock'))) {
        $listen = Join-Path $out "$($job[1]).wav"
        & $exe --render $job[0] --out $listen --pcm16 --events (Join-Path $eng "$($job[1]).events.csv") | Out-Host
        if ($haveFfmpeg) { & ffmpeg -y -loglevel error -i $listen -codec:a libmp3lame -q:a 2 (Join-Path $out "$($job[1]).mp3") }
    }
}
