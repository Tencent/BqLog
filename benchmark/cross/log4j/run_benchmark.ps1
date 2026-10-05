# Log4j2 throughput runner, Windows. Needs a JDK 17+ (java and javac on PATH, or JAVA_HOME).
# Usage: powershell -File run_benchmark.ps1 [-Threads 1,2,4,6,8,10]
# Results are appended to $env:RUN_DIR\results.csv (default: ..\run).
param(
    [int[]]$Threads = @(1, 2, 4, 6, 8, 10)
)
$ErrorActionPreference = 'Stop'

$dir = Split-Path $MyInvocation.MyCommand.Path
$runDir = if ($env:RUN_DIR) { $env:RUN_DIR } else { Join-Path $dir '..\run' }
$java = if ($env:JAVA_HOME) { Join-Path $env:JAVA_HOME 'bin\java.exe' } else { 'java' }
$javac = if ($env:JAVA_HOME) { Join-Path $env:JAVA_HOME 'bin\javac.exe' } else { 'javac' }
$csv = Join-Path $runDir 'results.csv'

New-Item -ItemType Directory -Force -Path $runDir, (Join-Path $dir 'classes'), (Join-Path $dir 'output') | Out-Null
if (-not (Test-Path $csv)) { 'lib,test,threads,ms,cpu_ms,peak_mb' | Out-File -Encoding ascii $csv }

Push-Location $dir
& $javac -cp "lib\*" -d classes src\bq\benchmark\log4j\main.java
foreach ($n in $Threads) {
    Get-ChildItem (Join-Path $dir 'output') -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force
    $stdout = Join-Path $runDir "stdout_log4j2_$n.txt"
    & $java -cp "classes;lib\*;." bq.benchmark.log4j.main $n mp | Out-File -Encoding ascii $stdout
    foreach ($line in Get-Content $stdout) {
        if ($line -match '^RESULT\|') {
            # RESULT|lib|test|threads|ms|cpu_ms|peak_mb (peak memory is not measured for the JVM)
            $p = $line.Split('|')
            "$($p[1]),$($p[2]),$($p[3]),$($p[4]),$($p[5]),$($p[6])" | Out-File -Append -Encoding ascii $csv
            Write-Host "log4j2 t=$n $($p[2]) : $($p[4]) ms, cpu $($p[5]) ms"
        }
    }
}
Pop-Location
