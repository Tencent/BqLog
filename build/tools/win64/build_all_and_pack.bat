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

if exist "..\..\..\artifacts" rmdir /s /q "..\..\..\artifacts"
if exist "..\..\..\install" rmdir /s /q "..\..\..\install"

cd category_log_generator
call ./executable_generate.bat %ARCH_PARAM% || exit /b 1
cd ../log_decoder
call ./executable_generate.bat %ARCH_PARAM% || exit /b 1
cd ..

rd /s/q pack
md pack
cd pack

cmake ../../../../pack %GEN_PLATFORM_ARG% -DTARGET_PLATFORM:STRING=win64 -DPACKAGE_NAME:STRING=bqlog-tools || exit /b 1
cmake --build . --target package || exit /b 1
cd ..
