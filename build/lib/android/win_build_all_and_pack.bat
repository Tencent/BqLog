@echo off
setlocal ENABLEEXTENSIONS
setlocal ENABLEDELAYEDEXPANSION

If not Defined ANDROID_NDK_ROOT (
    Echo ANDROID_NDK_ROOT is not defined! Build Cancelled!
    goto :fail
)
If not exist "%ANDROID_NDK_ROOT%" (
    Echo Folder %ANDROID_NDK_ROOT% Is Not Found! Build Cancelled!
    goto :fail
)

Echo Android NDK Found: %ANDROID_NDK_ROOT%

set ANDROID_PLATFORM="android-21"

REM HOST_TAG Detect
set "HOST_TAG=windows-x86_64"
if exist "%ANDROID_NDK_ROOT%\toolchains\llvm\prebuilt\windows-arm64\bin" (
    set "HOST_TAG=windows-arm64"
    echo Using NDK host toolchain: windows-arm64
) else (
    if exist "%ANDROID_NDK_ROOT%\toolchains\llvm\prebuilt\windows-x86_64\bin" (
        set "HOST_TAG=windows-x86_64"
        echo Using NDK host toolchain: windows-x86_64
    ) else (
        echo ERROR: Neither windows-arm64 nor windows-x86_64 toolchain found!
        goto :fail
    )
)

if exist "..\..\..\artifacts" rmdir /s /q "..\..\..\artifacts"
if exist "..\..\..\install" rmdir /s /q "..\..\..\install"

set BUILD_TARGET[0]=armeabi-v7a
set BUILD_TARGET[1]=arm64-v8a
set BUILD_TARGET[2]=x86_64
set BUILD_TARGET[3]=x86

set BUILD_CONFIGS[0]=Debug
set BUILD_CONFIGS[1]=MinSizeRel
set BUILD_CONFIGS[2]=RelWithDebInfo
set BUILD_CONFIGS[3]=Release

for /l %%a in (0,1,3) do (
    for /f "usebackq delims== tokens=1-2" %%i in (`set BUILD_TARGET[%%a]`) do (
       set "CURRENT_ABI=%%j"
       echo ========================================
       echo Building for ABI: !CURRENT_ABI!
       echo ========================================

       if exist "!CURRENT_ABI!" rd /s /q "!CURRENT_ABI!"
       md "!CURRENT_ABI!"
       pushd "!CURRENT_ABI!"

       for /l %%b in (0,1,3) do (
          for /f "usebackq delims== tokens=1-2" %%o in (`set BUILD_CONFIGS[%%b]`) do (
             set "CURRENT_CONFIG=%%p"
             echo [Config: !CURRENT_CONFIG!]

             if exist "!CURRENT_CONFIG!" rd /s /q "!CURRENT_CONFIG!"
             md "!CURRENT_CONFIG!"
             pushd "!CURRENT_CONFIG!"

             cmake ..\..\..\..\..\src^
              -G="MinGW Makefiles"^
              -DANDROID_ABI=!CURRENT_ABI!^
              -DANDROID_PLATFORM=%ANDROID_PLATFORM%^
              -DANDROID_NDK="%ANDROID_NDK_ROOT%"^
              -DCMAKE_BUILD_TYPE="!CURRENT_CONFIG!"^
              -DBUILD_LIB_TYPE=dynamic_lib^
              -DCMAKE_TOOLCHAIN_FILE="%ANDROID_NDK_ROOT%/build/cmake/android.toolchain.cmake"^
              -DTARGET_PLATFORM:STRING=android^
              -DANDROID_STL=none^
              -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON^
              || exit /b 1

              cmake --build . -- -j10 || exit /b 1
              cmake --build . --target install || exit /b 1

              set "BASE_INSTALL_DIR=..\..\..\..\..\install\dynamic_lib"
              set "SOURCE_SO=!BASE_INSTALL_DIR!\lib\!CURRENT_CONFIG!\!CURRENT_ABI!\libBqLog.so"
              set "SYMBOL_SO=!BASE_INSTALL_DIR!\symbols\!CURRENT_CONFIG!\!CURRENT_ABI!\libBqLog.so"

              for %%A in ("!SYMBOL_SO!") do set "SYMBOL_DIR=%%~dpA"
              if not exist "!SYMBOL_DIR!" mkdir "!SYMBOL_DIR!"

              echo [Stripping] Moving !SOURCE_SO! to !SYMBOL_SO!

              if exist "!SOURCE_SO!" (
                  move /Y "!SOURCE_SO!" "!SYMBOL_SO!" >nul

                  for %%A in ("!SOURCE_SO!") do set "LIB_DIR=%%~dpA"
                  if not exist "!LIB_DIR!" mkdir "!LIB_DIR!"

                  "!ANDROID_NDK_ROOT!\toolchains\llvm\prebuilt\%HOST_TAG%\bin\llvm-strip.exe" -s "!SYMBOL_SO!" -o "!SOURCE_SO!"
              ) else (
                  echo Error: !SOURCE_SO! not found, skip strip.
              )

             popd
          )
       )
       popd
    )
)

echo Building Android AAR...
pushd gradle_proj
call gradlew.bat assemble || exit /b 1
popd

if exist "pack" rd /s /q "pack"
md pack
pushd pack
cmake ..\..\..\..\pack -DTARGET_PLATFORM:STRING=android -DPACKAGE_NAME:STRING=bqlog-lib || exit /b 1
cmake --build . --target package || exit /b 1
popd

echo ---------
echo Finished!
echo ---------
exit /b 0

:fail
echo Build Failed!
exit /b 1