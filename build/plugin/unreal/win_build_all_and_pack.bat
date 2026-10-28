@echo off
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%\..\..\..") do set "ROOT_DIR=%%~fI"
set "VERSION_FILE=%ROOT_DIR%\src\bq_log\global\version.cpp"
set "TMP_DIR=%ROOT_DIR%\artifacts\plugin\unreal\tmp_unreal_package"
set "TARGET_DIR=%TMP_DIR%\BqLog"
set "PUBLIC_DIR=%TARGET_DIR%\Source\BqLog\Public"
set "PRIVATE_DIR=%TARGET_DIR%\Source\BqLog\Private"
set "DIST_DIR=%ROOT_DIR%\dist"
set "THIRDPARTY_DIR=%TARGET_DIR%\Source\ThirdParty\BqLog"
set "CORE_DIR=%PRIVATE_DIR%\BqLogCore"

if not exist "%DIST_DIR%" mkdir "%DIST_DIR%" >nul
if not exist "%TMP_DIR%" mkdir "%TMP_DIR%" >nul

set "VER_OUT=%TMP_DIR%\__version.txt"
del /q "%VER_OUT%" 2>nul
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$c=Get-Content -Raw -LiteralPath '%VERSION_FILE%'; $q=[char]34; if($c -match 'BQ_LOG_VERSION\s*=\s*([^;]+);'){ $v=$Matches[1].Trim(); ($v.Trim($q)) }" > "%VER_OUT%"

set /p "VERSION=" < "%VER_OUT%"
del /q "%VER_OUT%" 2>nul

if not defined VERSION (
  echo ERROR: Failed to parse version from "%VERSION_FILE%".
  exit /b 1
)

if exist "..\..\..\artifacts" rmdir /s /q "..\..\..\artifacts"
if exist "..\..\..\install" rmdir /s /q "..\..\..\install"

echo Parsed VERSION=%VERSION%

for %%T in (ue4 ue5) do (
    if exist "%TMP_DIR%" rmdir /s /q "%TMP_DIR%"
    mkdir "%TARGET_DIR%" >nul
    mkdir "%PUBLIC_DIR%" >nul
    mkdir "%PRIVATE_DIR%" >nul

    if not exist "%DIST_DIR%" mkdir "%DIST_DIR%" >nul

    robocopy "%ROOT_DIR%\plugin\unreal\BqLog" "%TARGET_DIR%" /E >nul
    if errorlevel 8 exit /b 8

    rmdir /S /Q "%TARGET_DIR%\Binaries"
    if errorlevel 8 exit /b 8

    REM BqLog core: headers -> Source/ThirdParty/BqLog/include (Fab attribution).
    REM LICENSE lives under ThirdParty too. .cpp implementation tree is copied
    REM into Source/BqLog/Private/BqLogCore/ so UBT picks it up via the standard
    REM module source scan -- no ConditionalAddModuleDirectory (4.26+ only)
    REM needed, so the plugin is buildable on UE 4.22 through UE 5.x.
    mkdir "%THIRDPARTY_DIR%\include" >nul 2>&1
    mkdir "%CORE_DIR%" >nul 2>&1

    robocopy "%ROOT_DIR%\include" "%THIRDPARTY_DIR%\include" /E >nul
    if errorlevel 8 exit /b 8

    robocopy "%ROOT_DIR%\src\bq_log" "%CORE_DIR%\bq_log" /E >nul
    if errorlevel 8 exit /b 8

    robocopy "%ROOT_DIR%\src\bq_common" "%CORE_DIR%\bq_common" /E >nul
    if errorlevel 8 exit /b 8

    copy /Y "%ROOT_DIR%\LICENSE*" "%THIRDPARTY_DIR%\" >nul 2>&1

    if not exist "%CORE_DIR%\IOS" mkdir "%CORE_DIR%\IOS" >nul
    if not exist "%CORE_DIR%\Mac" mkdir "%CORE_DIR%\Mac" >nul

    if exist "%CORE_DIR%\bq_common\platform\ios_misc.mm" move /Y "%CORE_DIR%\bq_common\platform\ios_misc.mm" "%CORE_DIR%\IOS\" >nul
    if exist "%CORE_DIR%\bq_common\platform\mac_misc.mm" move /Y "%CORE_DIR%\bq_common\platform\mac_misc.mm" "%CORE_DIR%\Mac\" >nul

    if exist "%PUBLIC_DIR%\BqLog_h_%%T.txt" move /Y "%PUBLIC_DIR%\BqLog_h_%%T.txt" "%PUBLIC_DIR%\BqLog.h" >nul
    pushd "%PUBLIC_DIR%" && del /q /f *.txt >nul 2>&1 & popd

    REM Replace version in .uplugin
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
      "$v='%VERSION%'; $ue='%%T'; $parts=$v.Split('.'); $vint=[int]$parts[0]*10000+[int]$parts[1]*100+[int]$parts[2]; $f='%TARGET_DIR%\BqLog.uplugin'; $c=Get-Content $f -Raw; $c=$c -replace '\"Version\": 1', ('\"Version\": '+$vint); $c=$c -replace '\"VersionName\": \"1.0\"', ('\"VersionName\": \"'+$v+'\"'); if($ue -eq 'ue4'){ $c=$c -replace '\"EngineVersion\": \"5.8\"', '\"EngineVersion\": \"4.27\"'; $c=$c -replace '\"PlatformAllowList\"', '\"WhitelistPlatforms\"'; $c=$c -replace '\"PlatformDenyList\"', '\"BlacklistPlatforms\"'; $c=$c -replace '\"LinuxArm64\"', '\"LinuxAArch64\"' }; Set-Content $f $c -NoNewline"

    REM Remove .gitkeep files before zipping
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-ChildItem -Path '%TARGET_DIR%' -Recurse -Filter '.gitkeep' | Remove-Item -Force"

    if exist "%DIST_DIR%\bqlog-unreal-plugin-%VERSION%-%%T.zip" del /q "%DIST_DIR%\bqlog-unreal-plugin-%VERSION%-%%T.zip"
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
      "Compress-Archive -Path '%TARGET_DIR%' -DestinationPath '%DIST_DIR%\bqlog-unreal-plugin-%VERSION%-%%T.zip' -Force"
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
      "$zipPath = '%DIST_DIR%\bqlog-unreal-plugin-%VERSION%-%%T.zip'; if (-not (Test-Path -LiteralPath $zipPath)) { throw \"Missing file: $zipPath\" }; $hash = Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath; $line = $hash.Hash.ToLowerInvariant() + '  ' + [System.IO.Path]::GetFileName($zipPath); [System.IO.File]::WriteAllText($zipPath + '.sha256', $line, [System.Text.Encoding]::ASCII)"
    echo Created %DIST_DIR%\bqlog-unreal-plugin-%VERSION%-%%T.zip
)

exit /b 0