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
if defined ARCH_PARAM set "VS_ARCH_ARG=|%ARCH_PARAM%"

rd /s/q VSProj
md VSProj
cd VSProj

cmake ..\..\..\..\..\tools\category_log_generator\ -DTARGET_PLATFORM:STRING=win64 %GEN_PLATFORM_ARG% || exit /b 1

cmake --build . --config RelWithDebInfo --parallel %BUILD_JOBS% || exit /b 1
cmake --install . --config RelWithDebInfo || exit /b 1
cd ..
