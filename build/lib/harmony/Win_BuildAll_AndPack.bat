@echo off
SETLOCAL ENABLEEXTENSIONS

If not Defined OHOS_NDK_ROOT (
    Echo OHOS_NDK_ROOT is not defined! Build Cancelled!
	GOTO :EOF
)
If not exist "%OHOS_NDK_ROOT%" (
    Echo Folder %OHOS_NDK_ROOT% Is Not Found! Build Cancelled!
	GOTO :EOF
)

Echo Android NDK Found:%OHOS_NDK_ROOT%

echo Android NDK found: %OHOS_NDK_ROOT%

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
        goto :EOF
    )
)

set BUILD_TARGET[0]=arm64-v8a
set BUILD_TARGET[1]=x86_64
set BUILD_TYPE[0]=Debug
set BUILD_TYPE[1]=MinSizeRel
set BUILD_TYPE[2]=RelWithDebInfo
set BUILD_TYPE[3]=Release

rem %%j : BUILD_TARGET
rem %%p : BUILD_TYPE
for /l %%a in (0,1) do (
    for /f "usebackq delims== tokens=1-2" %%i in (`set BUILD_TARGET[%%a]`) do (
		rd /s /q %%j
		md %%j
		pushd %%j
		for /l %%a in (0,1,3) do (
			for /f "usebackq delims== tokens=1-2" %%o in (`set BUILD_TYPE[%%a]`) do (
				md %%p
				pushd %%p
				"%OHOS_NDK_ROOT%/native/build-tools/cmake/bin/cmake.exe" ..\..\..\..\..\src^
				 -G="Ninja"^
				 -DOHOS_ARCH=%%j^
				 -DOHOS_PLATFORM=OHOS^
				 -DCMAKE_BUILD_TYPE="%%p"^
				 -DBUILD_TYPE=dynamic_lib^
				 -DCMAKE_TOOLCHAIN_FILE="%OHOS_NDK_ROOT%/native/build/cmake/ohos.toolchain.cmake"^
				 -DTARGET_PLATFORM:STRING=harmony^
				 -DOHOS_STL=c++_shared

				 rem "%OHOS_NDK_ROOT%/native/build-tools/cmake/bin/cmake.exe" --trace -j10
				 "%OHOS_NDK_ROOT%/native/build-tools/cmake/bin/ninja.exe" -f build.ninja
				 move /Y ..\..\..\..\..\install\dynamic_lib\lib\%%j\%%p\libBqLog.so ..\..\..\..\..\install\dynamic_lib\lib\%%j\%%p\libBqLog_Symbol.so
				 %ANDROID_NDK_ROOT%\toolchains\llvm\prebuilt\%HOST_TAG%\bin\llvm-strip.exe -s ..\..\..\..\..\install\dynamic_lib\lib\%%j\%%p\libBqLog_Symbol.so -o ..\..\..\..\..\install\dynamic_lib\lib\%%j\%%p\libBqLog.so
			)
			popd
		)
    )
	popd
)

rem pushd gradle_proj
rem call gradlew.bat assemble
rem if errorlevel 1 goto :fail
rem popd
rem 
if exist "pack" rd /s /q "pack"
md pack
pushd pack

"%OHOS_NDK_ROOT%/native/build-tools/cmake/bin/cmake.exe" ..\..\..\..\pack -DTARGET_PLATFORM:STRING=harmony -DPACKAGE_NAME:STRING=bqlog-lib
"%OHOS_NDK_ROOT%/native/build-tools/cmake/bin/cmake.exe" --build . --target package
if errorlevel 1 goto :fail
popd

 
echo ---------
echo Finished!
echo ---------