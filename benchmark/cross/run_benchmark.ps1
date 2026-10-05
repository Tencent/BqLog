# Throughput runner, Windows.
# Usage: powershell -File run_benchmark.ps1 -Lib <bqlog|spdlog|glog|fmtlog|quill> [-Threads 1,2,4,6,8,10]
# Results are appended to $env:RUN_DIR\results.csv (default: run\ next to this script); log files go to output\.
# Run it several times to get several rounds; make_tables.py takes the median.
param(
    [Parameter(Mandatory=$true)][string]$Lib,
    [int[]]$Threads = @(1, 2, 4, 6, 8, 10)
)
$ErrorActionPreference = 'Stop'

$dir = Split-Path $MyInvocation.MyCommand.Path
$runDir = if ($env:RUN_DIR) { $env:RUN_DIR } else { Join-Path $dir 'run' }
$exeDir = if ($env:BIN_DIR) { $env:BIN_DIR } else { Join-Path $dir 'build\Release' }
$csv = Join-Path $runDir 'results.csv'
$exe = Join-Path $exeDir "bench_$Lib.exe"

New-Item -ItemType Directory -Force -Path (Join-Path $runDir 'output') | Out-Null
if (-not (Test-Path $csv)) { 'lib,test,threads,ms,cpu_ms,peak_mb' | Out-File -Encoding ascii $csv }

$bqConfigs = @('bqlog_fast_compress', 'bqlog_fast_compress_enc', 'bqlog_fast_text', 'bqlog_compress', 'bqlog_compress_enc', 'bqlog_text')

foreach ($n in $Threads) {
    Get-ChildItem (Join-Path $runDir 'output') -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force

    # fmtlog: one test per process (setLogFile races with its polling thread)
    # BqLog: one logger per process, so the memory column belongs to that logger alone
    # BqLog and quill: fixed size blocking buffers, then buffers that grow when full
    $argSets = switch ($Lib) {
        'fmtlog' { @("$n mp", "$n np") }
        'bqlog' { foreach ($mode in 'block', 'expand') { foreach ($cfg in $bqConfigs) { "$n $mode $cfg" } } }
        'quill' { @("$n block", "$n expand") }
        default { @("$n") }
    }
    $idx = 0
    foreach ($argString in $argSets) {
        $idx++
        $outFile = Join-Path $runDir "stdout_${Lib}_${n}_$idx.txt"
        Start-Process -FilePath $exe -ArgumentList $argString -WorkingDirectory $runDir `
            -Wait -NoNewWindow -RedirectStandardOutput $outFile | Out-Null
        foreach ($line in Get-Content $outFile) {
            if ($line -match '^RESULT\|') {
                # RESULT|lib|test|threads|ms|cpu_ms|peak_mb
                $p = $line.Split('|')
                "$($p[1]),$($p[2]),$($p[3]),$($p[4]),$($p[5]),$($p[6])" | Out-File -Append -Encoding ascii $csv
                Write-Host "$Lib t=$n $($p[1]) $($p[2]) : $($p[4]) ms, cpu $($p[5]) ms"
            }
        }
    }

    # output file sizes of the 1-thread run
    if ($n -eq $Threads[0]) {
        Get-ChildItem (Join-Path $runDir 'output') -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
            "FILE|$Lib|$($_.Name)|$($_.Length) bytes" | Out-File -Append -Encoding ascii (Join-Path $runDir 'filesizes.txt')
        }
    }
}
