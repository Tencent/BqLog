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