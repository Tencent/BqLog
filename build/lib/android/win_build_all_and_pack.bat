@echo off
SETLOCAL ENABLEEXTENSIONS

If not Defined ANDROID_NDK_ROOT (
    Echo ANDROID_NDK_ROOT is not defined! Build Cancelled!
	goto :fail
)
If not exist %ANDROID_NDK_ROOT% (
    Echo Folder %ANDROID_NDK_ROOT% Is Not Found! Build Cancelled!
	goto :fail
)

Echo Android NDK Found:%ANDROID_NDK_ROOT%

set ANDROID_PLATFORM="android-21"

echo Android NDK found: %ANDROID_NDK_ROOT%

REM Try to use windows-arm64 toolchain if it exists, otherwise fallback to windows-x86_64
set "HOST_TAG=windows-x86_64"
if exist "%ANDROID_NDK_ROOT%\toolchains\llvm\prebuilt\windows-arm64\bin" (
    set "HOST_TAG=windows-arm64"
    echo Using NDK host toolchain: windows-arm64
) else (
    if exist "%ANDROID_NDK_ROOT%\toolchains\llvm\prebuilt\windows-x86_64\bin" (
        set "HOST_TAG=windows-x86_64"
        echo Using NDK host toolchain: windows-x86_64
    ) else (
        echo ERROR: Neither windows-arm64 nor windows-x86_64 toolchain found in your NDK. Build cancelled!
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

rem %%j : BUILD_TARGET
rem %%p : BUILD_CONFIGS
for /l %%a in (0,1,3) do (
    for /f "usebackq delims== tokens=1-2" %%i in (`set BUILD_TARGET[%%a]`) do (
		rd /s /q %%j
		md %%j
		pushd %%j
		for /l %%a in (0,1,3) do (
			for /f "usebackq delims== tokens=1-2" %%o in (`set BUILD_CONFIGS[%%a]`) do (
				md %%p
				pushd %%p
				cmake ..\..\..\..\..\src^
				 -G="MinGW Makefiles"^
				 -DANDROID_ABI=%%j^
				 -DANDROID_PLATFORM=%ANDROID_PLATFORM%^
				 -DANDROID_NDK=%ANDROID_NDK_ROOT%^
				 -DCMAKE_BUILD_TYPE="%%p"^
				 -DBUILD_LIB_TYPE=dynamic_lib^
				 -DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK_ROOT%/build/cmake/android.toolchain.cmake^
				 -DTARGET_PLATFORM:STRING=android^
				 -DANDROID_STL=none^
				 || exit /b 1
				 
				 cmake --build . -- -j10 || exit /b 1
                 cmake --build . --target install || exit /b 1
                 for %%F in ("..\..\..\..\..\install\dynamic_lib\lib_strip\%%p\%%j\libBqLog.so") do if not exist "%%~dpF" mkdir "%%~dpF"Z
                 "%ANDROID_NDK_ROOT%\toolchains\llvm\prebuilt\%HOST_TAG%\bin\llvm-strip.exe" -s "..\..\..\..\..\install\dynamic_lib\lib\%%p\%%j\libBqLog.so" -o "..\..\..\..\..\install\dynamic_lib\lib_strip\%%p\%%j\libBqLog.so"

			)
			popd
		)
    )
	popd
)

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