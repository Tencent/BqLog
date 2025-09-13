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

cmake ..\..\..\..\..\tools\category_log_generator\ -DTARGET_PLATFORM:STRING=win64 %GEN_PLATFORM_ARG% || exit /b 1

echo "VS COMMAND TOOLS PATH:"
echo "%VS_PATH%\devenv.com"
call "%VS_PATH%\devenv.com" ./BqLog_CategoryLogGenerator.sln /Rebuild "RelWithDebInfo%VS_ARCH_ARG%" /Project "./BqLog_CategoryLogGenerator.vcxproj" /Out Build.log || exit /b 1
cmake --install . --config RelWithDebInfo || exit /b 1
cd ..
