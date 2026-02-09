@echo off
setlocal EnableDelayedExpansion

set "DIR=%~dp0"
set "PROJECT_ROOT=%DIR%..\..\.."
set "BUILD_LIB_DIR=%PROJECT_ROOT%\build\lib\win64"
set "TEST_SRC_DIR=%PROJECT_ROOT%\test\java"
set "ARTIFACTS_DIR=%PROJECT_ROOT%\artifacts"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=RelWithDebInfo"

echo ===== Building BqLog Dynamic Library (Windows) =====
pushd "%BUILD_LIB_DIR%"
call dont_execute_this.bat build native msvc ON OFF dynamic_lib
popd

echo ===== Building Java Test Jar =====
if not exist "%DIR%VSProj" md "%DIR%VSProj"
pushd "%DIR%VSProj"
cmake "%TEST_SRC_DIR%" -DTARGET_PLATFORM:STRING=win64
cmake --build . 
popd

set "JAR_PATH=%ARTIFACTS_DIR%\test\java\BqLogJavaTest.jar"
set "LIB_PATH=%ARTIFACTS_DIR%\dynamic_lib\lib\%CONFIG%"

if not exist "%JAR_PATH%" (
    echo Error: Test Jar not found at %JAR_PATH%
    exit /b 1
)

if not exist "%LIB_PATH%" (
    if exist "%ARTIFACTS_DIR%\dynamic_lib\lib\Release" (
        set "LIB_PATH=%ARTIFACTS_DIR%\dynamic_lib\lib\Release"
    ) else if exist "%ARTIFACTS_DIR%\dynamic_lib\lib\Debug" (
        set "LIB_PATH=%ARTIFACTS_DIR%\dynamic_lib\lib\Debug"
    ) else (
        echo Error: Lib Path not found at %LIB_PATH%
        exit /b 1
    )
)

echo Running Java Test on Windows...
echo Lib Path: %LIB_PATH%
java -Djava.library.path="%LIB_PATH%" -jar "%JAR_PATH%"