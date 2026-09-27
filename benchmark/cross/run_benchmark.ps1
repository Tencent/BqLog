param(
    [Parameter(Mandatory=$true)][string]$Lib,
    [int]$From = 1,
    [int]$To = 10
)
$ErrorActionPreference = 'Stop'

$runDir = 'E:\BqLog\benchmark\cross\run'
$exeDir = 'E:\BqLog\benchmark\cross\build\Release'
$csv = Join-Path $runDir 'results.csv'
$exe = Join-Path $exeDir "bench_$Lib.exe"

New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $runDir 'output') | Out-Null
if (-not (Test-Path $csv)) { 'lib,test,threads,ms,peak_mb' | Out-File -Encoding ascii $csv }

foreach ($n in $From..$To) {
    # clean previous output files so file-size measurement is per-run
    Get-ChildItem (Join-Path $runDir 'output') -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force

    # fmtlog runs each test in a separate process (setLogFile races with its polling thread)
    $argSets = if ($Lib -eq 'fmtlog') { @("$n mp", "$n np") } else { @("$n") }
    $idx = 0
    foreach ($argString in $argSets) {
        $idx++
        $outFile = Join-Path $runDir "stdout_${Lib}_${n}_$idx.txt"
        if (Test-Path $outFile) { Remove-Item $outFile }
        $p = Start-Process -FilePath $exe -ArgumentList $argString -WorkingDirectory $runDir `
            -Wait -PassThru -NoNewWindow -RedirectStandardOutput $outFile
        foreach ($line in Get-Content $outFile) {
            if ($line -match '^RESULT\|') {
                $parts = $line.Split('|')
                "$($parts[1]),$($parts[2]),$($parts[3]),$($parts[4])," | Out-File -Append -Encoding ascii $csv
                Write-Host "$Lib t=$n $($parts[2]) : $($parts[4]) ms"
            }
        }
    }

    # record output file sizes for the 1-thread run (both tests = 4M entries total)
    if ($n -eq 1) {
        $sizes = @{}
        Get-ChildItem (Join-Path $runDir 'output') -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
            $key = if ($_.Name -like 'bqlog_compress_enc*') { 'bqlog_compress_enc' }
                   elseif ($_.Name -like 'bqlog_compress*') { 'bqlog_compress' }
                   elseif ($_.Name -like 'bqlog_text*') { 'bqlog_text' }
                   else { $Lib }
            $sizes[$key] = ($sizes[$key] + $_.Length)
        }
        foreach ($k in $sizes.Keys) {
            "FILESIZE|$k|$([math]::Round($sizes[$k]/1MB,1)) MB|$([math]::Round($sizes[$k]/4000000,1)) B/entry" |
                Out-File -Append -Encoding ascii (Join-Path $runDir 'filesizes.txt')
        }
    }
}
