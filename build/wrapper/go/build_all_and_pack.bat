@echo off
setlocal EnableDelayedExpansion

rem Build the Go wrapper: native library (GO_SUPPORT=ON), then go vet +
rem go build against the freshly built artifacts.
rem Release CI packages sources with module/ and publishes go_dist.
rem This development build/test command never publishes.
rem Usage: build_all_and_pack.bat [CONFIG]

set "DIR=%~dp0"
set "PROJECT_ROOT=%DIR%..\..\.."

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=RelWithDebInfo"

call "%PROJECT_ROOT%\build\test\go\run_win.bat" %CONFIG%
exit /b %ERRORLEVEL%
