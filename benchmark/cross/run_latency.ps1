# Logging thread cost runner, Windows.
# Usage: powershell -File run_latency.ps1 [-Rounds 5] [-Threads 1,4]
# Runs every library, rounds interleaved so all libraries see the same machine state.
# Results are appended to $env:RUN_DIR\latency.csv (default: run\ next to this script); take the median of the rounds.
param(
    [int]$Rounds = 5,
    [int[]]$Threads = @(1, 4),
    [string[]]$Libs = @('bqlog_fast', 'bqlog_normal', 'quill', 'fmtlog', 'spdlog')
)
$ErrorActionPreference = 'Stop'

$dir = Split-Path $MyInvocation.MyCommand.Path
$runDir = if ($env:RUN_DIR) { $env:RUN_DIR } else { Join-Path $dir 'run' }
$exeDir = if ($env:BIN_DIR) { $env:BIN_DIR } else { Join-Path $dir 'build\Release' }
$csv = Join-Path $runDir 'latency.csv'

New-Item -ItemType Directory -Force -Path $runDir | Out-Null
if (-not (Test-Path $csv)) {
    'round,lib,threads,mean_ns,p50_ns,p75_ns,p90_ns,p95_ns,p99_ns,p999_ns,worst_ns,consumer_cpu_pct,peak_mb' | Out-File -Encoding ascii $csv
}

foreach ($r in 1..$Rounds) {
    foreach ($n in $Threads) {
        foreach ($lib in $Libs) {
            $out = Join-Path $runDir 'output'
            Remove-Item $out -Recurse -Force -ErrorAction SilentlyContinue
            New-Item -ItemType Directory -Force -Path $out | Out-Null
            $stdout = Join-Path $runDir "lat_stdout.txt"
            Start-Process -FilePath (Join-Path $exeDir "bench_latency_$lib.exe") -ArgumentList "$n" -WorkingDirectory $runDir `
                -Wait -NoNewWindow -RedirectStandardOutput $stdout | Out-Null
            foreach ($line in Get-Content $stdout) {
                if ($line -match '^RESULT_LAT\|') {
                    $p = $line.Split('|')
                    "$r," + (($p[1..12]) -join ',') | Out-File -Append -Encoding ascii $csv
                    Write-Host "round ${r}: $line"
                }
            }
        }
    }
}
