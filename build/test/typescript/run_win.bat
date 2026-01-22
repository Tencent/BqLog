@echo off
setlocal EnableDelayedExpansion

set "DIR=%~dp0"
set "PROJECT_ROOT=%DIR%..\..\.."
set "BUILD_LIB_DIR=%PROJECT_ROOT%\build\lib\win64"
set "TEST_SRC_DIR=%PROJECT_ROOT%\test\typescript"
set "ARTIFACTS_DIR=%PROJECT_ROOT%\artifacts"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=RelWithDebInfo"

echo ===== Building BqLog Dynamic Library (Windows) =====
pushd "%BUILD_LIB_DIR%"
call dont_execute_this.bat build native msvc OFF ON dynamic_lib
popd

echo ===== Installing TypeScript Test Dependencies =====
pushd "%TEST_SRC_DIR%"
call npm install
popd

echo ===== Building TypeScript Wrapper (for tests) =====
pushd "%PROJECT_ROOT%\wrapper\typescript"
call npm install
call npm run build
popd

echo ===== Running TypeScript Tests =====
pushd "%TEST_SRC_DIR%"

REM Find .node file
REM In batch, finding file is a bit tricky, assume it is in the standard location
set "NODE_LIB_DIR=%ARTIFACTS_DIR%\dynamic_lib\lib\%CONFIG%"
if not exist "%NODE_LIB_DIR%" (
     if exist "%ARTIFACTS_DIR%\dynamic_lib\lib\Release" (
        set "NODE_LIB_DIR=%ARTIFACTS_DIR%\dynamic_lib\lib\Release"
    ) else if exist "%ARTIFACTS_DIR%\dynamic_lib\lib\Debug" (
        set "NODE_LIB_DIR=%ARTIFACTS_DIR%\dynamic_lib\lib\Debug"
    )
)

for /r "%NODE_LIB_DIR%" %%f in (*.node) do (
    set "BQ_NODE_ADDON=%%f"
    goto :found_node
)

:found_node
if "%BQ_NODE_ADDON%"=="" (
    echo Warning: .node file not found in %NODE_LIB_DIR%
) else (
    echo Found Node Lib: %BQ_NODE_ADDON%
)

call npm test
popd
