param(
    [string]$Executable
)

$ErrorActionPreference = "Continue"
$timeoutSeconds = 600 # 10 minutes

# Locate CDB
$cdb = (Get-Command cdb.exe -ErrorAction SilentlyContinue).Source
if (-not $cdb) {
    $candidates = @(
        "C:\Program Files (x86)\Windows Kits\10\Debuggers\arm64\cdb.exe",
        "C:\Program Files (x86)\Windows Kits\11\Debuggers\arm64\cdb.exe",
        "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe",
        "C:\Program Files (x86)\Windows Kits\11\Debuggers\x64\cdb.exe",
        "C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\cdb.exe",
        "C:\Program Files (x86)\Windows Kits\11\Debuggers\x86\cdb.exe"
    )
    foreach ($p in $candidates) { if (Test-Path $p) { $cdb = $p; break } }
}

if ($cdb) {
    Write-Host "Running $Executable under CDB with watchdog (Timeout: ${timeoutSeconds}s)..."
    
    # Create CDB command script to avoid quoting hell
    $cdbScript = Join-Path $PSScriptRoot "cdb_commands.txt"
    "sxe -c `"kv; q`" av`r`ng`r`nq" | Set-Content -Path $cdbScript -Encoding Ascii

    # -o: debug child processes
    # -G: ignore segment end breakpoint
    # -cf: run commands from file
    $cdbArgs = @(
        "-o",
        "-G",
        "-cf",
        $cdbScript,
        $Executable
    )
    $proc = Start-Process -FilePath $cdb -ArgumentList $cdbArgs -PassThru -NoNewWindow
} else {
    Write-Host "CDB not found. Running $Executable directly with watchdog (Timeout: ${timeoutSeconds}s)..."
    $proc = Start-Process -FilePath $Executable -PassThru -NoNewWindow
}

$sw = [System.Diagnostics.Stopwatch]::StartNew()

while (-not $proc.HasExited) {
    Start-Sleep -Seconds 2
    
    if ($sw.Elapsed.TotalSeconds -gt $timeoutSeconds) {
        Write-Host "!!! TIMEOUT DETECTED ($timeoutSeconds seconds) !!!"
        
        if ($cdb) {
            # ... existing attach logic ...
        } else {
            Write-Host "CDB not found. Trying GDB..."
            $gdb = (Get-Command gdb.exe -ErrorAction SilentlyContinue).Source
            if ($gdb) {
                Write-Host "Attaching GDB to analyze hang..."
                & $gdb -p $proc.Id -batch -ex "thread apply all bt" -ex "quit"
            } else {
                Write-Host "CDB and GDB not found. Cannot print stack traces."
            }
        }
        
        Stop-Process -Id $proc.Id -Force
        Write-Error "Process $Executable killed due to timeout."
        exit 1
    }
}

if ($proc.ExitCode -ne 0) {
    exit $proc.ExitCode
}
