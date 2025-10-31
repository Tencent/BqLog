@echo off
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%\..\..\..") do set "ROOT_DIR=%%~fI"
set "VERSION_FILE=%ROOT_DIR%\src\bq_log\global\version.cpp"
for /f "usebackq tokens=* delims=" %%I in (`powershell -NoProfile -Command "(Select-String -Pattern 'BQ_LOG_VERSION\s*=\s*\"([^\"]+)\"' -Path '%VERSION_FILE%').Matches[0].Groups[1].Value"`) do set "VERSION=%%I"
set "TMP_DIR=%ROOT_DIR%\artifacts\plugin\unreal\tmp_unreal_package"
set "TARGET_DIR=%TMP_DIR%\BqLog"
set "PUBLIC_DIR=%TARGET_DIR%\Source\BqLog\Public"
set "PRIVATE_DIR=%TARGET_DIR%\Source\BqLog\Private"
set "DIST_DIR=%ROOT_DIR%\dist"
set "ZIP_PATH=%DIST_DIR%\bqlog-unreal-plugin-%VERSION%.zip"

if exist "%TMP_DIR%" rmdir /s /q "%TMP_DIR%"
mkdir "%TARGET_DIR%" >nul
mkdir "%PUBLIC_DIR%" >nul
mkdir "%PRIVATE_DIR%" >nul
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%" >nul

robocopy "%ROOT_DIR%\plugin\unreal\BqLog" "%TARGET_DIR%" /E >nul
if %ERRORLEVEL% GEQ 8 exit /b %ERRORLEVEL%

robocopy "%ROOT_DIR%\include" "%PUBLIC_DIR%" /E >nul
if %ERRORLEVEL% GEQ 8 exit /b %ERRORLEVEL%

robocopy "%ROOT_DIR%\src\bq_log" "%PRIVATE_DIR%\bq_log" /E >nul
if %ERRORLEVEL% GEQ 8 exit /b %ERRORLEVEL%

robocopy "%ROOT_DIR%\src\bq_common" "%PRIVATE_DIR%\bq_common" /E >nul
if %ERRORLEVEL% GEQ 8 exit /b %ERRORLEVEL%

if not exist "%PRIVATE_DIR%\IOS" mkdir "%PRIVATE_DIR%\IOS" >nul
if not exist "%PRIVATE_DIR%\Mac" mkdir "%PRIVATE_DIR%\Mac" >nul

if exist "%PRIVATE_DIR%\bq_common\platform\ios_misc.mm" move /Y "%PRIVATE_DIR%\bq_common\platform\ios_misc.mm" "%PRIVATE_DIR%\IOS\" >nul
if exist "%PRIVATE_DIR%\bq_common\platform\mac_misc.mm" move /Y "%PRIVATE_DIR%\bq_common\platform\mac_misc.mm" "%PRIVATE_DIR%\Mac\" >nul

if exist "%ZIP_PATH%" del /q "%ZIP_PATH%"
powershell -NoProfile -Command "Compress-Archive -Path '%TARGET_DIR%' -DestinationPath '%ZIP_PATH%' -Force"
echo Created %ZIP_PATH%