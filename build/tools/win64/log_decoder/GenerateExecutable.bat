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

set VS_PATH="%VS_PATH%"

IF NOT EXIST %VS_PATH%\devenv.com (
	echo "Please set the VS_PATH environment variable to the correct Visual Studio installation path, pointing to the directory where devenv.com is located."
	GOTO :fail
)
set VS_PATH=%VS_PATH:~1,-1%

rd /s/q VSProj
md VSProj
cd VSProj

cmake ..\..\..\..\..\tools\log_decoder\ -DTARGET_PLATFORM:STRING=win64 %GEN_PLATFORM_ARG%

cmake --build . --config RelWithDebInfo --parallel %BUILD_JOBS% || exit /b 1
cmake --install . --config RelWithDebInfo || exit /b 1
cd ..
