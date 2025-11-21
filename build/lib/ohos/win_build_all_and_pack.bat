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

if exist "..\..\..\artifacts" rmdir /s /q "..\..\..\artifacts"
if exist "..\..\..\install" rmdir /s /q "..\..\..\install"

for %%T in (armeabi-v7a arm64-v8a x86_64) do (
  for %%C in (Debug RelWithDebInfo MinSizeRel Release) do (
    for %%L in (static_lib dynamic_lib) do (
      if exist "cmake_build" rmdir /s /q "cmake_build"
      mkdir "cmake_build"
      pushd "cmake_build"

      REM Convert NDK path to POSIX style for CMake
      set "NDK_POSIX=%NDK_PATH:\=/%"

      cmake ../../../../src -G "Unix Makefiles" -DOHOS_ARCH=%%T -DCMAKE_BUILD_TYPE=%%C -DBUILD_LIB_TYPE=%%L -DCMAKE_TOOLCHAIN_FILE="!NDK_POSIX!/build/cmake/ohos.toolchain.cmake" -DTARGET_PLATFORM:STRING=ohos -DOHOS_STL=c++_static || goto :error

      REM Build and install
      cmake --build . -- -j%PARALLEL_JOBS% || goto :error
      cmake --build . --target install || goto :error

      REM Strip symbols for dynamic lib
      if /I "%%L"=="dynamic_lib" (
        set "SOURCE_SO=..\..\..\..\install\dynamic_lib\lib\%%C\%%T\libBqLog.so"
        set "STRIP_SO=..\..\..\..\install\dynamic_lib\lib_strip\%%C\%%T\libBqLog.so"
        for %%I in ("!STRIP_SO!") do set "DEST_DIR=%%~dpI"
        if not exist "!DEST_DIR!" mkdir "!DEST_DIR!"
        echo SOURCE_SO:!SOURCE_SO!

        set "STRIP_EXE=%NDK_PATH%\llvm\bin\llvm-strip.exe"
        if not exist "!STRIP_EXE!" set "STRIP_EXE=%NDK_PATH%\llvm\bin\llvm-strip"
        "!STRIP_EXE!" -s "!SOURCE_SO!" -o "!STRIP_SO!" || goto :error
      )

      popd
    )
  )
)

pushd "harmonyOS" || goto :error
call hvigorw clean --no-daemon || goto :error
call hvigorw assembleHar --mode module -p module=bqlog@default -p product=default --no-daemon -p buildMode=release --no-parallel || goto :error
if not exist "..\..\..\..\install\dynamic_lib" mkdir "..\..\..\..\install\dynamic_lib"
copy /Y "bqlog\build\default\outputs\default\bqlog.har" "..\..\..\..\install\dynamic_lib\bqlog.har" >nul || goto :error
popd

REM Package the library
if exist "pack" rmdir /s /q "pack"
mkdir "pack"
pushd "pack"
cmake ../../../../pack -DTARGET_PLATFORM:STRING=ohos -DPACKAGE_NAME:STRING=bqlog-lib || goto :error
cmake --build . --target package || goto :error
popd

echo ---------
echo Finished!
echo ---------
exit /b 0

:error
echo Build failed with error level %errorlevel%.
exit /b %errorlevel%