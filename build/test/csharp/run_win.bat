@echo off
setlocal EnableDelayedExpansion

set "DIR=%~dp0"
set "PROJECT_ROOT=%DIR%..\..\.."
set "BUILD_LIB_DIR=%PROJECT_ROOT%\build\lib\win64"
set "TEST_SRC_DIR=%PROJECT_ROOT%\test\csharp"
set "ARTIFACTS_DIR=%PROJECT_ROOT%\artifacts"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=RelWithDebInfo"

echo ===== Building BqLog Dynamic Library (Windows) =====
pushd "%BUILD_LIB_DIR%"
call dont_execute_this.bat build native msvc OFF OFF dynamic_lib
popd

echo ===== Building C# Test Executable =====
if not exist "%DIR%Build" md "%DIR%Build"
pushd "%DIR%Build"

REM Note: Do NOT pass -A for C# projects. C# defaults to AnyCPU which works on all platforms.
cmake "%TEST_SRC_DIR%"
cmake --build . --config %CONFIG%
popd

set "EXE_PATH=%ARTIFACTS_DIR%\test\csharp\BqLogCSharpTest.exe"
set "DLL_PATH=%ARTIFACTS_DIR%\test\csharp\BqLogCSharpTest.dll"
set "LIB_PATH=%ARTIFACTS_DIR%\dynamic_lib\lib\%CONFIG%"

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

echo Running C# Test on Windows...
echo Lib Path: %LIB_PATH%
set "PATH=%LIB_PATH%;%PATH%"

if exist "%EXE_PATH%" (
    "%EXE_PATH%"
) else if exist "%DLL_PATH%" (
    dotnet "%DLL_PATH%"
) else (
    echo Error: C# Test Executable/DLL not found at %EXE_PATH% or %DLL_PATH%
    exit /b 1
)