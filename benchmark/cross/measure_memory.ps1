param(
    [Parameter(Mandatory=$true)][string]$Lib,
    [int[]]$Threads = @(1, 4, 10)
)
$ErrorActionPreference = 'Stop'

$runDir = 'E:\BqLog\benchmark\cross\run'
$exeDir = 'E:\BqLog\benchmark\cross\build\Release'
$csv = Join-Path $runDir 'memory.csv'
$exe = Join-Path $exeDir "bench_$Lib.exe"

New-Item -ItemType Directory -Force -Path (Join-Path $runDir 'output') | Out-Null
if (-not (Test-Path $csv)) { 'lib,threads,peak_mb' | Out-File -Encoding ascii $csv }

foreach ($n in $Threads) {
    Get-ChildItem (Join-Path $runDir 'output') -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force

    # fmtlog runs each test in a separate process (setLogFile races with its polling thread)
    $argSets = if ($Lib -eq 'fmtlog') { @("$n mp", "$n np") } else { @("$n") }
    $peak = 0
    $idx = 0
    foreach ($argString in $argSets) {
        $idx++
        $outFile = Join-Path $runDir "mem_stdout_${Lib}_${n}_$idx.txt"
        if (Test-Path $outFile) { Remove-Item $outFile }
        $p = Start-Process -FilePath $exe -ArgumentList $argString -WorkingDirectory $runDir `
            -PassThru -NoNewWindow -RedirectStandardOutput $outFile
        while (-not $p.HasExited) {
            try { $p.Refresh(); $v = $p.PeakWorkingSet64; if ($v -gt $peak) { $peak = $v } } catch {}
            Start-Sleep -Milliseconds 5
        }
        try { $p.Refresh(); $v = $p.PeakWorkingSet64; if ($v -gt $peak) { $peak = $v } } catch {}
    }
    $peakMB = [math]::Round($peak / 1MB, 1)
    "$Lib,$n,$peakMB" | Out-File -Append -Encoding ascii $csv
    Write-Host "$Lib t=$n peak $peakMB MB"
}
