@echo off
set "ARCH_PARAM=%~1"

if /I "%ARCH_PARAM%"=="arm64" (
  set "ARCH_PARAM=ARM64"
) else if /I "%ARCH_PARAM%"=="x64" (
  set "ARCH_PARAM=x64"
) else (
  set "ARCH_PARAM="
)

echo "Architecture: %ARCH_PARAM%"

set "GEN_PLATFORM_ARG="
if defined ARCH_PARAM set "GEN_PLATFORM_ARG=-A %ARCH_PARAM%"

cd category_log_generator
call ./GenerateExecutable.bat %ARCH_PARAM%
if errorlevel 1 exit /b %errorlevel%
cd ../log_decoder
call ./GenerateExecutable.bat %ARCH_PARAM%
if errorlevel 1 exit /b %errorlevel%
cd ..

rd /s/q pack
md pack
cd pack

cmake ../../../../pack %ARCH_PARAM% -DTARGET_PLATFORM:STRING=win64 -DPACKAGE_NAME:STRING=bqlog-tools
cmake --build . --target package
cd ..
