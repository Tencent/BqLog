# Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
param(
    [Parameter(Mandatory=$true)][string]$CppExecutable,
    [Parameter(Mandatory=$true)][string]$GoExecutable,
    [Parameter(Mandatory=$true)][string]$LibraryDirectory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$BaselineGoExecutable = '',
    [int[]]$ThreadCounts = @(1, 8),
    [int]$Repeats = 3
)
$ErrorActionPreference = 'Stop'
$cppPath = (Resolve-Path -LiteralPath $CppExecutable).Path
$goPath = (Resolve-Path -LiteralPath $GoExecutable).Path
$baselineGoPath = if ($BaselineGoExecutable) { (Resolve-Path -LiteralPath $BaselineGoExecutable).Path } else { '' }
$libraryPath = (Resolve-Path -LiteralPath $LibraryDirectory).Path
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $outputPath) {
    throw "Choose a new output directory so old benchmark files cannot influence this run: $outputPath"
}
New-Item -ItemType Directory -Path $outputPath | Out-Null
$env:PATH = $libraryPath + ';' + $env:PATH
$env:BENCH_POOL_SIZE = '50000'
$env:GOMAXPROCS = [string][Environment]::ProcessorCount
$env:GOGC = '100'
$env:GOMEMLIMIT = 'off'
$env:GODEBUG = ''
$results = New-Object 'System.Collections.Generic.List[object]'
foreach ($workers in $ThreadCounts) {
    for ($repeat = 1; $repeat -le $Repeats; $repeat++) {
        # Alternate the order to reduce systematic ordering/thermal bias.
        $languages = if ($repeat % 2 -eq 1) { @('cpp', 'go') } else { @('go', 'cpp') }
        if ($baselineGoPath) {
            $languages = if ($repeat % 2 -eq 1) { @('cpp', 'go_before', 'go') } else { @('go', 'go_before', 'cpp') }
        }
        foreach ($language in $languages) {
            $runDirectory = Join-Path $outputPath ("{0}_threads{1}_run{2}" -f $language, $workers, $repeat)
            New-Item -ItemType Directory -Path $runDirectory | Out-Null
            $logPath = Join-Path $runDirectory 'stdout.txt'
            Write-Output ("START {0} workers={1} repeat={2}" -f $language,$workers,$repeat)
            Push-Location $runDirectory
            try {
                if ($language -eq 'cpp') {
                    & $cppPath $workers 2>&1 | Tee-Object -FilePath $logPath | ForEach-Object {
                        if ($_ -match '^CASE |^Time Cost:|^BqLog=') { Write-Output $_ }
                    }
                } else {
                    $executable = if ($language -eq 'go_before') { $baselineGoPath } else { $goPath }
                    & $executable -threads $workers 2>&1 | Tee-Object -FilePath $logPath | ForEach-Object {
                        if ($_ -match '^CASE |^Time Cost:|^RESULT |^Go=|^SKIP:') { Write-Output $_ }
                    }
                }
                if ($LASTEXITCODE -ne 0) { throw "Benchmark failed: $language workers=$workers repeat=$repeat exit=$LASTEXITCODE" }
            } finally {
                Pop-Location
            }
            $currentCase = ''
            foreach ($line in Get-Content -LiteralPath $logPath) {
                if ($line -match '^CASE (\S+) ') { $currentCase = $Matches[1] }
                if ($line -match '^Time Cost:(\d+)') {
                    $results.Add([PSCustomObject]@{
                        Language=$language; Workers=$workers; Repeat=$repeat; Case=$currentCase
                        Milliseconds=[long]$Matches[1]; Entries=[long]$workers*2000000
                    })
                }
                if ($line -match '^RESULT .* failed=([1-9]\d*)') { throw "Failed Go writes: $line" }
            }
            $caseCount = @($results | Where-Object { $_.Language -eq $language -and $_.Workers -eq $workers -and $_.Repeat -eq $repeat }).Count
            if ($caseCount -ne 10) { throw "Expected ten benchmark cases, found $caseCount" }
            $results | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath (Join-Path $outputPath 'results.csv')
        }
    }
}
Write-Output ("RESULTS " + (Join-Path $outputPath 'results.csv'))
