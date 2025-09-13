@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Fixed parallel build level
set "BUILD_JOBS=10"

rem Args: 1=BUILD_TYPE, 2=COMPILER_TYPE (msvc|clang|mingw), 3=ARCH (x64|arm64, optional)
set "BUILD_TYPE=%~1"
set "COMPILER_TYPE=%~2"
set "ARCH_PARAM=%~3"

rem Normalize ARCH_PARAM from user input
if /I "%ARCH_PARAM%"=="arm64" (
  set "ARCH_PARAM=ARM64"
) else if /I "%ARCH_PARAM%"=="x64" (
  set "ARCH_PARAM=x64"
)

rem If no arch provided, default to host architecture
if not defined ARCH_PARAM (
  set "HOST_ARCH="
  if /I "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "HOST_ARCH=ARM64"
  if /I "%PROCESSOR_ARCHITECTURE%"=="AMD64" set "HOST_ARCH=x64"
  if not defined HOST_ARCH if /I "%PROCESSOR_ARCHITEW6432%"=="ARM64" set "HOST_ARCH=ARM64"
  if not defined HOST_ARCH if /I "%PROCESSOR_ARCHITEW6432%"=="AMD64" set "HOST_ARCH=x64"
  if defined HOST_ARCH set "ARCH_PARAM=%HOST_ARCH%"
)

echo Architecture: %ARCH_PARAM%

rem Visual Studio generator platform arg (only used by MSVC/ClangCL generators)
set "VS_GEN_PLATFORM_ARG="
if defined ARCH_PARAM set "VS_GEN_PLATFORM_ARG=-A %ARCH_PARAM%"

rem Create a fresh build directory per BUILD_TYPE
set "PROJ_DIR=VSProj_%BUILD_TYPE%"
if exist "%PROJ_DIR%" rd /s /q "%PROJ_DIR%" || exit /b 1
md "%PROJ_DIR%" || exit /b 1
cd "%PROJ_DIR%" || exit /b 1

rem Pre-configure for MSVC/ClangCL (multi-config). MinGW config happens per-configuration below.
if /I "%COMPILER_TYPE%"=="clang" (
  cmake ..\..\..\..\src -DTARGET_PLATFORM:STRING=win64 %VS_GEN_PLATFORM_ARG% -DJAVA_SUPPORT=ON -DBUILD_TYPE=%BUILD_TYPE% -T ClangCl || exit /b 1
) else if /I "%COMPILER_TYPE%"=="mingw" (
  rem no-op for MinGW here (single-config generators will be configured inside the loop)
) else (
  rem Default to MSVC (Visual Studio generator)
  cmake ..\..\..\..\src -DTARGET_PLATFORM:STRING=win64 %VS_GEN_PLATFORM_ARG% -DJAVA_SUPPORT=ON -DBUILD_TYPE=%BUILD_TYPE% || exit /b 1
)

rem Build all configurations
set "CONFIGS=Debug MinSizeRel RelWithDebInfo Release"

for %%c in (%CONFIGS%) do (
  set "CUR_CFG=%%c"

  if /I "%COMPILER_TYPE%"=="mingw" (
    rem MinGW with LLVM/Clang (installed in the workflow). Use Ninja, no -S, per-config configure.
    rem If ARCH_PARAM is set, pass a target triple; otherwise rely on the host/default MSYS2 environment.
    set "MINGW_TARGET_ARGS="
    if /I "%ARCH_PARAM%"=="ARM64" (
      set "MINGW_TARGET_ARGS=-DCMAKE_C_COMPILER_TARGET=aarch64-w64-windows-gnu -DCMAKE_CXX_COMPILER_TARGET=aarch64-w64-windows-gnu -DCMAKE_SYSTEM_PROCESSOR=aarch64"
    ) else if /I "%ARCH_PARAM%"=="x64" (
      set "MINGW_TARGET_ARGS=-DCMAKE_C_COMPILER_TARGET=x86_64-w64-windows-gnu -DCMAKE_CXX_COMPILER_TARGET=x86_64-w64-windows-gnu -DCMAKE_SYSTEM_PROCESSOR=x86_64"
    )

    cmake ..\..\..\..\src -G "Ninja" ^
      -DTARGET_PLATFORM:STRING=win64 -DJAVA_SUPPORT=ON -DBUILD_TYPE=%BUILD_TYPE% ^
      -DCMAKE_BUILD_TYPE=!CUR_CFG! ^
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_RC_COMPILER=llvm-rc ^
      -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld ^
      !MINGW_TARGET_ARGS! || exit /b 1

    if not exist build.ninja exit /b 1
    cmake --build . --parallel %BUILD_JOBS% || exit /b 1

  ) else (
    rem MSVC/ClangCL: build per configuration (multi-config generators)
    cmake --build . --config !CUR_CFG! --parallel %BUILD_JOBS% || exit /b 1
  )

  rem Install artifacts for each configuration
  cmake --install . --config !CUR_CFG! || exit /b 1
)

cd ..
echo ---------
echo Finished!
echo ---------
exit /b 0