@echo off
setlocal enabledelayedexpansion

set "VS_PATH=%VS_PATH%"
set "BUILD_TYPE=%1"
set "COMPILER_TYPE=%2"

if not exist "%VS_PATH%\devenv.com" (
    echo Please set the VS_PATH environment variable to the correct Visual Studio installation path, pointing to the directory where devenv.com is located.
    pause
    goto :EOF
)

set "PROJ_DIR=VSProj_%BUILD_TYPE%"
if exist "%PROJ_DIR%" rd /s /q "%PROJ_DIR%"
md "%PROJ_DIR%"
cd "%PROJ_DIR%"

set CL=/MP

if /i "%COMPILER_TYPE%"=="clang" (
    cmake ..\..\..\..\src -DTARGET_PLATFORM:STRING=win64 -DCMAKE_GENERATOR_PLATFORM=x64 -DJAVA_SUPPORT=ON -DBUILD_TYPE=%BUILD_TYPE% -T ClangCl
) else (
    cmake ..\..\..\..\src -DTARGET_PLATFORM:STRING=win64 -DCMAKE_GENERATOR_PLATFORM=x64 -DJAVA_SUPPORT=ON -DBUILD_TYPE=%BUILD_TYPE%
)

if errorlevel 1 goto :fail

set CONFIGS=Debug MinSizeRel RelWithDebInfo Release

for %%c in (%CONFIGS%) do (
    set "CUR_CFG=%%c"
    call "%VS_PATH%\devenv.com" ./BqLog.sln /Rebuild "!CUR_CFG!" /Project "./BqLog.vcxproj" /Out Build.log
    cmake --install . --config !CUR_CFG!

    if errorlevel 1 goto :fail
)

cd ..
echo "---------"
echo "Finished!"
echo "---------"
exit /b 0