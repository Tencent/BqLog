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

      REM Strip symbols for dynamic lib (same as Android: move original to symbols, strip back to lib)
      if /I "%%L"=="dynamic_lib" (
        set "BASE_INSTALL_DIR=..\..\..\..\install\dynamic_lib"
        set "SOURCE_SO=!BASE_INSTALL_DIR!\lib\%%C\%%T\libBqLog.so"
        set "SYMBOL_SO=!BASE_INSTALL_DIR!\symbols\%%C\%%T\libBqLog.so"

        for %%I in ("!SYMBOL_SO!") do set "SYMBOL_DIR=%%~dpI"
        if not exist "!SYMBOL_DIR!" mkdir "!SYMBOL_DIR!"

        echo [Stripping] Moving !SOURCE_SO! to !SYMBOL_SO!

        if exist "!SOURCE_SO!" (
          move /Y "!SOURCE_SO!" "!SYMBOL_SO!" >nul

          for %%I in ("!SOURCE_SO!") do set "LIB_DIR=%%~dpI"
          if not exist "!LIB_DIR!" mkdir "!LIB_DIR!"

          set "STRIP_EXE=%NDK_PATH%\llvm\bin\llvm-strip.exe"
          if not exist "!STRIP_EXE!" set "STRIP_EXE=%NDK_PATH%\llvm\bin\llvm-strip"
          "!STRIP_EXE!" -s "!SYMBOL_SO!" -o "!SOURCE_SO!" || goto :error
        ) else (
          echo Error: !SOURCE_SO! not found, skip strip.
        )
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

REM Extract har symbols: copy the native .so from har build intermediates to symbols/har/{abi}/
set "HAR_NATIVE_DIR=bqlog\build\default\intermediates\cmake\default\obj"
set "SYMBOL_HAR_DIR=..\..\..\..\install\dynamic_lib\symbols\har"
if exist "!HAR_NATIVE_DIR!" (
  for /D %%A in ("!HAR_NATIVE_DIR!\*") do (
    set "ABI_NAME=%%~nxA"
    set "HAR_SO=%%A\libBqLog.so"
    if exist "!HAR_SO!" (
      set "DEST_DIR=!SYMBOL_HAR_DIR!\!ABI_NAME!"
      if not exist "!DEST_DIR!" mkdir "!DEST_DIR!"
      copy /Y "!HAR_SO!" "!DEST_DIR!\libBqLog.so" >nul
      echo [HAR Symbols] Copied !HAR_SO! to !DEST_DIR!\libBqLog.so
    )
  )
) else (
  echo Warning: HAR native intermediates dir not found at !HAR_NATIVE_DIR!, skip har symbol extraction.
)

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