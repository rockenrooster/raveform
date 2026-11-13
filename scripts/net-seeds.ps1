# The loopback soak over many seeds (M4 net follow-up, docs/PROGRESS_m4_net.md): one soak's error swings with what its run
# happened to do, so variants of the network code are compared over seeds. Runs at most 4 soaks at once (the PC rule).
#   scripts\net-seeds.ps1 [-Seeds 1..10] [-Minutes 10] [-Impaired] [-Extra '--remote-poses','predict'] [-Out captures\m4net\seeds\x]
# Prints each seed's RFNET figures and the mean, median and worst of the on-screen error; writes each soak's output to -Out.
param(
    [int[]]$Seeds = (1..10),
    [double]$Minutes = 10,
    [switch]$Impaired,
    [string[]]$Extra = @(),
    [string]$Out = 'captures\m4net\seeds\run',
    # -Low: the soaks run at idle priority (a device run's real-time PC bots on the same cores go first).
    [switch]$Low,
    [string]$Exe = 'build\desktop-dev\raveform_headless.exe'
)
. "$PSScriptRoot\env.ps1"
$exe = Join-Path $RF_ROOT $Exe
$OutFull = [IO.Path]::GetFullPath((Join-Path $RF_ROOT $Out))
New-Item -ItemType Directory -Force $OutFull | Out-Null
$running = @()
foreach ($s in $Seeds) {
    $a = @('--net', 'soak', '--minutes', "$Minutes", '--seed', "$s") + $Extra
    if ($Impaired) { $a += @('--pause', '--netsim', 'loss=0.05,jitter=80') }
    while (@($running | Where-Object { -not $_.HasExited }).Count -ge $RF_JOBS) { Start-Sleep -Milliseconds 200 }
    $p = Start-Process -FilePath $exe -ArgumentList $a -RedirectStandardOutput "$OutFull\seed$s.txt" -RedirectStandardError "$OutFull\seed$s.err" -PassThru -WindowStyle Hidden
    if ($Low) { try { $p.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::Idle } catch {} }
    $running += $p
}
foreach ($p in $running) { $p.WaitForExit() }
$rows = foreach ($s in $Seeds) {
    $line = Get-Content "$OutFull\seed$s.txt" | Where-Object { $_ -match '^RFNET ' } | Select-Object -Last 1
    if (-not $line) { [pscustomobject]@{ seed = $s; err = [double]::NaN; pass = $false }; continue }
    $j = $line.Substring(6) | ConvertFrom-Json
    [pscustomobject]@{ seed = $s; err = $j.err_rms_wu; p95 = $j.err_p95_wu; corr = $j.err_at_corr_rms_wu; down = $j.down_kbps_max; up = $j.up_kbps_max; pass = $j.pass }
}
$rows | Format-Table -AutoSize | Out-String -Width 160
$e = @($rows | Where-Object { -not [double]::IsNaN($_.err) } | ForEach-Object { $_.err } | Sort-Object)
if ($e.Count) {
    $mean = ($e | Measure-Object -Average).Average
    $med = if ($e.Count % 2) { $e[($e.Count - 1) / 2] } else { ($e[$e.Count / 2 - 1] + $e[$e.Count / 2]) / 2 }
    $rms = [Math]::Sqrt((($e | ForEach-Object { $_ * $_ }) | Measure-Object -Average).Average)
    "on-screen error over {0} seeds: mean {1:N2}, median {2:N2}, RMS of the runs {3:N2}, best {4:N2}, worst {5:N2} wu; passed {6} of {0}; down max {7}, up max {8}" -f `
        $e.Count, $mean, $med, $rms, $e[0], $e[-1], @($rows | Where-Object { $_.pass }).Count, ($rows | Measure-Object -Property down -Maximum).Maximum,
        ($rows | Measure-Object -Property up -Maximum).Maximum
}
