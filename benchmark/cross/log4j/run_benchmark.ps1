param(
    [int]$From = 1,
    [int]$To = 10,
    [switch]$MemoryOnly
)
$ErrorActionPreference = 'Stop'

$JAVA = 'E:\Android\Android Studio\jbr\bin\java.exe'
$dir = Split-Path $MyInvocation.MyCommand.Path
$runDir = 'E:\BqLog\benchmark\cross\run'
New-Item -ItemType Directory -Force -Path (Join-Path $runDir 'output') | Out-Null

$cp = "classes;lib\*;."
foreach ($n in $From..$To) {
    Get-ChildItem (Join-Path $runDir 'output') -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force

    $p = Start-Process -FilePath $JAVA `
        -ArgumentList "-cp `"$cp`" bq.benchmark.log4j.main $n mp" `
        -WorkingDirectory $dir -PassThru -NoNewWindow `
        -RedirectStandardOutput (Join-Path $runDir "stdout_log4j2_$n.txt")
    $peak = 0
    while (-not $p.HasExited) {
        try { $p.Refresh(); $v = $p.PeakWorkingSet64; if ($v -gt $peak) { $peak = $v } } catch {}
        Start-Sleep -Milliseconds 5
    }
    try { $p.Refresh(); $v = $p.PeakWorkingSet64; if ($v -gt $peak) { $peak = $v } } catch {}
    $peakMB = [math]::Round($peak / 1MB, 1)

    foreach ($line in Get-Content (Join-Path $runDir "stdout_log4j2_$n.txt")) {
        if ($line -match '^RESULT\|') {
            $parts = $line.Split('|')
            if (-not $MemoryOnly) {
                "$($parts[1]),$($parts[2]),$($parts[3]),$($parts[4])," | Out-File -Append -Encoding ascii (Join-Path $runDir 'results.csv')
            }
            Write-Host "log4j2 t=$n $($parts[2]) : $($parts[4]) ms, peak $peakMB MB"
        }
    }
    "log4j2,$n,$peakMB" | Out-File -Append -Encoding ascii (Join-Path $runDir 'memory.csv')

    if ($n -eq 1) {
        $size = (Get-ChildItem (Join-Path $dir 'output') -File -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum
        if ($size) {
            "FILESIZE|log4j2|$([math]::Round($size/1MB,1)) MB (2M entries, multi_param only)|$([math]::Round($size/2000000,1)) B/entry" |
                Out-File -Append -Encoding ascii (Join-Path $runDir 'filesizes.txt')
        }
    }
}
