@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Usage: build_win64.bat <path_to_BqLog.lib> [x64|arm64]
set "BQLOG_LIB=%~1"
set "ARCH=%~2"
if "%ARCH%"=="" set "ARCH=x64"

if "%BQLOG_LIB%"=="" (
    echo Usage: %~nx0 ^<path_to_BqLog.lib^> [x64^|arm64]
    exit /b 1
)

set "SCRIPT_DIR=%~dp0"
set "PLUGIN_DIR=%SCRIPT_DIR%..\..\..\plugin\godot"
set "BUILD_DIR=%SCRIPT_DIR%build_win_%ARCH%"

if /I "%ARCH%"=="arm64" (
    set "VS_ARCH=ARM64"
    set "OUT_SUBDIR=arm64"
) else (
    set "VS_ARCH=x64"
    set "OUT_SUBDIR=x86_64"
)

set "EXTRA_ARGS="
if not "%GODOT_CPP_DIR%"=="" (
    set "EXTRA_ARGS=%EXTRA_ARGS% -DGODOT_CPP_DIR=%GODOT_CPP_DIR%"
)
if not "%BQLOG_GODOT_CMAKE_ARGS%"=="" (
    set "EXTRA_ARGS=%EXTRA_ARGS% %BQLOG_GODOT_CMAKE_ARGS%"
)

cmake -S "%PLUGIN_DIR%" -B "%BUILD_DIR%" -A %VS_ARCH% ^
    -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
    -DCMAKE_CXX_FLAGS="/MP" ^
    -DBQLOG_LIB_PATH="%BQLOG_LIB%" ^
    -DBQLOG_GODOT_OUTPUT_NAME=BqLogGodot ^
    %EXTRA_ARGS% || exit /b 1

cmake --build "%BUILD_DIR%" --config RelWithDebInfo --parallel || exit /b 1

set "OUT_DIR=%SCRIPT_DIR%out\windows\%OUT_SUBDIR%"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
copy /Y "%BUILD_DIR%\RelWithDebInfo\BqLogGodot.dll" "%OUT_DIR%\" || exit /b 1
if exist "%BUILD_DIR%\RelWithDebInfo\BqLogGodot.pdb" copy /Y "%BUILD_DIR%\RelWithDebInfo\BqLogGodot.pdb" "%OUT_DIR%\"

echo Built Windows (%OUT_SUBDIR%) Godot plugin in %OUT_DIR%
exit /b 0
