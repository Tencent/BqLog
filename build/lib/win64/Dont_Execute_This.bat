@echo off
setlocal enabledelayedexpansion

set "VS_PATH=%VS_PATH%"
set "BUILD_TYPE=%1"
set "COMPILER_TYPE=%2"
set "ARCH_PARAM=%~3"

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

if not exist "%VS_PATH%\devenv.com" (
    echo Please set the VS_PATH environment variable to the correct Visual Studio installation path, pointing to the directory where devenv.com is located.
    pause
    goto :EOF
)

set "PROJ_DIR=VSProj_%BUILD_TYPE%"
if exist "%PROJ_DIR%" rd /s /q "%PROJ_DIR%"
md "%PROJ_DIR%"
cd "%PROJ_DIR%"


if /i "%COMPILER_TYPE%"=="clang" (
    cmake ..\..\..\..\src -DTARGET_PLATFORM:STRING=win64 %GEN_PLATFORM_ARG% -DJAVA_SUPPORT=ON -DBUILD_TYPE=%BUILD_TYPE% -T ClangCl
) else (
    cmake ..\..\..\..\src -DTARGET_PLATFORM:STRING=win64 %GEN_PLATFORM_ARG% -DJAVA_SUPPORT=ON -DBUILD_TYPE=%BUILD_TYPE%
)

if errorlevel 1 goto :fail

set CONFIGS=Debug MinSizeRel RelWithDebInfo Release

for %%c in (%CONFIGS%) do (
    set "CUR_CFG=%%c"
    call "%VS_PATH%\devenv.com" ./BqLog.sln /Rebuild "!CUR_CFG!|%ARCH_PARAM%" /Project "./BqLog.vcxproj" /Out Build.log
    cmake --install . --config !CUR_CFG!

    if errorlevel 1 goto :fail
)

cd ..
echo "---------"
echo "Finished!"
echo "---------"
exit /b 0