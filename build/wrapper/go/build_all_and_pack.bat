@echo off
setlocal EnableDelayedExpansion

rem Build the Go wrapper: native library (GO_SUPPORT=ON) staged into
rem wrapper/go/lib, then go vet + go build.
rem Go packages are distributed as source via go modules, so there is no
rem binary packing step here.
rem Usage: build_all_and_pack.bat [CONFIG]

set "DIR=%~dp0"
set "PROJECT_ROOT=%DIR%..\..\.."

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=RelWithDebInfo"

call "%PROJECT_ROOT%\build\test\go\run_win.bat" %CONFIG%
exit /b %ERRORLEVEL%
