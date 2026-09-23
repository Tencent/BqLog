@echo off
setlocal EnableDelayedExpansion

rem Run Go wrapper tests on Windows.
rem Usage: run_win.bat [CONFIG]
rem   CONFIG     : RelWithDebInfo (default) / Release / Debug ...
rem Requires: Visual Studio (msvc), cmake, go and gcc (for cgo) in PATH.

set "DIR=%~dp0"
set "PROJECT_ROOT=%DIR%..\..\.."
set "BUILD_LIB_DIR=%PROJECT_ROOT%\build\lib\win64"
set "TEST_SRC_DIR=%PROJECT_ROOT%\test\go"
set "WRAPPER_DIR=%PROJECT_ROOT%\wrapper\go"
set "ARTIFACTS_DIR=%PROJECT_ROOT%\artifacts"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=RelWithDebInfo"

where go >nul 2>nul || (echo Error: go not found in PATH & exit /b 1)
where gcc >nul 2>nul || (echo Error: gcc not found in PATH, required by cgo & exit /b 1)

echo ===== Building BqLog Dynamic Library (Windows) =====
rem cgo requires gcc in PATH, but a gcc/ninja in PATH makes cmake abandon the
rem Visual Studio default generator and pick Ninja, so pin the VS generator
rem explicitly (vswhere has a fixed install location; do not use
rem %ProgramFiles(x86)% which may not survive non-cmd parent shells).
set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=1 delims=." %%v in (`"%VSWHERE%" -latest -prerelease -property installationVersion`) do set "VS_MAJOR=%%v"
)
if "%VS_MAJOR%"=="17" set "VS_YEAR=2022"
if "%VS_MAJOR%"=="18" set "VS_YEAR=2026"
if defined VS_YEAR set "CMAKE_GENERATOR=Visual Studio %VS_MAJOR% %VS_YEAR%"
pushd "%BUILD_LIB_DIR%"
call dont_execute_this.bat build native msvc OFF OFF OFF dynamic_lib ON || exit /b 1
popd

set "LIB_OUT=%ARTIFACTS_DIR%\dynamic_lib\lib\%CONFIG%"
if not exist "%LIB_OUT%" (
    if exist "%ARTIFACTS_DIR%\dynamic_lib\lib\Release" (
        set "LIB_OUT=%ARTIFACTS_DIR%\dynamic_lib\lib\Release"
    ) else if exist "%ARTIFACTS_DIR%\dynamic_lib\lib\Debug" (
        set "LIB_OUT=%ARTIFACTS_DIR%\dynamic_lib\lib\Debug"
    ) else (
        echo Error: Lib Path not found at %LIB_OUT%
        exit /b 1
    )
)

echo ===== Building and Running Go Test =====
rem cgo links against the freshly built artifacts directly via CGO_LDFLAGS
set "CGO_ENABLED=1"
set "CGO_LDFLAGS=-L%LIB_OUT:\=/%"
set "PATH=%LIB_OUT%;%PATH%"
pushd "%WRAPPER_DIR%"
go vet ./... || exit /b 1
go test -count=1 -timeout 120s ./... || exit /b 1
popd
pushd "%TEST_SRC_DIR%"
if exist bqlog_go_test.exe del /q bqlog_go_test.exe
go build -o bqlog_go_test.exe ./src/bq/test || exit /b 1
.\bqlog_go_test.exe
set "TEST_EXIT=%ERRORLEVEL%"
popd
exit /b %TEST_EXIT%
