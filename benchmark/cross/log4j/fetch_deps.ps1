$ErrorActionPreference = 'Stop'
# Downloads the Log4j2 benchmark dependencies from Maven Central into lib/
$dir = Split-Path $MyInvocation.MyCommand.Path
$lib = Join-Path $dir 'lib'
New-Item -ItemType Directory -Force -Path $lib | Out-Null
$deps = @(
    'https://repo1.maven.org/maven2/org/apache/logging/log4j/log4j-api/2.26.0/log4j-api-2.26.0.jar',
    'https://repo1.maven.org/maven2/org/apache/logging/log4j/log4j-core/2.26.0/log4j-core-2.26.0.jar',
    'https://repo1.maven.org/maven2/com/lmax/disruptor/4.0.0/disruptor-4.0.0.jar'
)
foreach ($url in $deps) {
    $name = Split-Path $url -Leaf
    $dest = Join-Path $lib $name
    if (-not (Test-Path $dest)) {
        Write-Host "downloading $name"
        Invoke-WebRequest -Uri $url -OutFile $dest
    }
}
Write-Host 'done'
