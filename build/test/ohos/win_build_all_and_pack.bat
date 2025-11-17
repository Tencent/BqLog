@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM Check OHOS_SDK
if "%OHOS_SDK%"=="" (
  echo Environment variable OHOS_SDK is not found! Build cancelled!
  exit /b 1
)

set "NDK_PATH=%OHOS_SDK%\native"

REM Detect number of CPU cores for parallel build
if not defined PARALLEL_JOBS set "PARALLEL_JOBS=%NUMBER_OF_PROCESSORS%"

echo NDK: %NDK_PATH%
echo Parallel jobs: %PARALLEL_JOBS%

for %%T in (x86_64) do (
  for %%C in (Debug) do (
    for %%L in (static_lib) do (
      if exist "cmake_build" rmdir /s /q "cmake_build"
      mkdir "cmake_build"
      pushd "cmake_build"

      REM Convert NDK path to POSIX style for CMake
      set "NDK_POSIX=%NDK_PATH:\=/%"

      cmake ../../../../test -G "Unix Makefiles" ^
        -DOHOS_ARCH=%%T ^
        -DCMAKE_BUILD_TYPE=%%C ^
        -DCMAKE_TOOLCHAIN_FILE=!NDK_POSIX!/build/cmake/ohos.toolchain.cmake ^
        -DTARGET_PLATFORM:STRING=ohos ^
        -DOHOS_STL=c++_static || goto :error

      REM Build and install
      cmake --build . -- -j%PARALLEL_JOBS% || goto :error
      popd
    )
  )
)

echo ---------
echo Finished!
echo ---------
exit /b 0

:error
echo Build failed with error level %errorlevel%.
exit /b %errorlevel%