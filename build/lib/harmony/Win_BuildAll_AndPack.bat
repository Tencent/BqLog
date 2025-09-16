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
				 rem move /Y ..\..\..\..\..\install\dynamic_lib\lib\%%j\%%p\libBqLog.so ..\..\..\..\..\install\dynamic_lib\lib\%%j\%%p\libBqLog_Symbol.so
				 rem %ANDROID_NDK_ROOT%\toolchains\llvm\prebuilt\%HOST_TAG%\bin\llvm-strip.exe -s ..\..\..\..\..\install\dynamic_lib\lib\%%j\%%p\libBqLog_Symbol.so -o ..\..\..\..\..\install\dynamic_lib\lib\%%j\%%p\libBqLog.so
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
rem if exist "pack" rd /s /q "pack"
rem md pack
rem pushd pack
rem 
rem cmake ..\..\..\..\pack -DTARGET_PLATFORM:STRING=harmony -DPACKAGE_NAME:STRING=bqlog-lib
rem cmake --build . --target package
rem if errorlevel 1 goto :fail
rem popd

 
echo ---------
echo Finished!
echo ---------