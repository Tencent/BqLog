@echo off
setlocal EnableDelayedExpansion

set "DIR=%~dp0"
set "PROJECT_ROOT=%DIR%..\..\.."
set "BUILD_LIB_DIR=%PROJECT_ROOT%\build\lib\win64"
set "TEST_SRC_DIR=%PROJECT_ROOT%\test\csharp"
set "ARTIFACTS_DIR=%PROJECT_ROOT%\artifacts"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=RelWithDebInfo"

echo ===== System Architecture Info =====
echo PROCESSOR_ARCHITECTURE: %PROCESSOR_ARCHITECTURE%
echo PROCESSOR_ARCHITEW6432: %PROCESSOR_ARCHITEW6432%

echo ===== Building BqLog Dynamic Library (Windows) =====
pushd "%BUILD_LIB_DIR%"
call dont_execute_this.bat build native msvc OFF OFF dynamic_lib
popd

echo ===== Building C# Test (.NET 6.0 SDK) =====
if not exist "%DIR%Build" md "%DIR%Build"
pushd "%DIR%Build"

REM Uses dotnet build with SDK-style project for native ARM64 support
REM (unlike .NET Framework which runs under x64 emulation on ARM64)
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

REM Check native DLL architecture
echo ===== Native DLL Architecture =====
for %%f in ("%LIB_PATH%\*.dll") do (
    echo Checking: %%~nxf
    powershell -Command "$path='%%f'; $bytes=[System.IO.File]::ReadAllBytes($path); $peOffset=[BitConverter]::ToInt32($bytes,60); $machine=[BitConverter]::ToUInt16($bytes,$peOffset+4); switch($machine){ 0x8664{'x64'} 0x14c{'x86'} 0xaa64{'ARM64'} default{'Unknown:'+$machine.ToString('X')} }"
)

set "PATH=%LIB_PATH%;%PATH%"

if exist "%EXE_PATH%" (
    "%EXE_PATH%"
) else if exist "%DLL_PATH%" (
    dotnet "%DLL_PATH%"
) else (
    echo Error: C# Test Executable/DLL not found at %EXE_PATH% or %DLL_PATH%
    exit /b 1
)