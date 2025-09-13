@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "BUILD_JOBS=10"

set "BUILD_TYPE=%~1"
set "COMPILER_TYPE=%~2"
set "ARCH_PARAM=%~3"

if /I "%ARCH_PARAM%"=="arm64" (
  set "ARCH_PARAM=ARM64"
) else if /I "%ARCH_PARAM%"=="x64" (
  set "ARCH_PARAM=x64"
) else (
  set "ARCH_PARAM="
)

echo "Architecture: %ARCH_PARAM%"

set "VS_GEN_PLATFORM_ARG="
if defined ARCH_PARAM set "VS_GEN_PLATFORM_ARG=-A %ARCH_PARAM%"

set "PROJ_DIR=VSProj_%BUILD_TYPE%"
if exist "%PROJ_DIR%" rd /s /q "%PROJ_DIR%" || exit /b 1
md "%PROJ_DIR%" || exit /b 1
cd "%PROJ_DIR%" || exit /b 1

rem 预配置：
if /I "%COMPILER_TYPE%"=="clang" (
  cmake ..\..\..\..\src -DTARGET_PLATFORM:STRING=win64 %VS_GEN_PLATFORM_ARG% -DJAVA_SUPPORT=ON -DBUILD_TYPE=%BUILD_TYPE% -T ClangCl || exit /b 1
) else if /I "%COMPILER_TYPE%"=="mingw" (
  rem no-op for MinGW here
) else (
  cmake ..\..\..\..\src -DTARGET_PLATFORM:STRING=win64 %VS_GEN_PLATFORM_ARG% -DJAVA_SUPPORT=ON -DBUILD_TYPE=%BUILD_TYPE% || exit /b 1
)

set "CONFIGS=Debug MinSizeRel RelWithDebInfo Release"

for %%c in (%CONFIGS%) do (
  set "CUR_CFG=%%c"

  if /I "%COMPILER_TYPE%"=="mingw" (
    cmake ..\..\..\..\src -G "MinGW Makefiles" -DTARGET_PLATFORM:STRING=win64 -DJAVA_SUPPORT=ON -DCMAKE_BUILD_TYPE=!CUR_CFG! -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DBUILD_TYPE=%BUILD_TYPE% || exit /b 1
    cmake --build . --parallel %BUILD_JOBS% || exit /b 1
  ) else (
    cmake --build . --config !CUR_CFG! --parallel %BUILD_JOBS% || exit /b 1
  )

  cmake --install . --config !CUR_CFG! || exit /b 1
)

cd ..
echo "---------"
echo "Finished!"
echo "---------"
exit /b 0