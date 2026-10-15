@echo off
set CPP_VER_PARAM=%1
if "%CPP_VER_PARAM%"=="" set CPP_VER_PARAM=17

md MinGWProj_Clang
cd MinGWProj_Clang

cmake ..\..\..\..\test\cpp -G "MinGW Makefiles" -DTARGET_PLATFORM:STRING=win64 -DCPP_VER=%CPP_VER_PARAM% -DCMAKE_C_COMPILER="C:\Program Files\LLVM\bin\clang.exe" -DCMAKE_CXX_COMPILER="C:\Program Files\LLVM\bin\clang++.exe" -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
.\BqLogUnitTest.exe
set exitcode=%ERRORLEVEL%
rem bq_module_test_enabled: the configuration builds the module test, so its binary must exist.
if exist bq_module_test_enabled if not exist .\BqLogModuleUnitTest.exe echo BqLogModuleUnitTest.exe is missing although the module test is enabled for this build.
if exist bq_module_test_enabled if not exist .\BqLogModuleUnitTest.exe if %exitcode%==0 set exitcode=1
if exist .\BqLogModuleUnitTest.exe .\BqLogModuleUnitTest.exe
if exist .\BqLogModuleUnitTest.exe if %ERRORLEVEL% NEQ 0 set exitcode=%ERRORLEVEL%

echo "If your did not see output with color, and there are some strange characters like \033, please run Terminal Windows With Administrative Privileges, then type command below, and run script again"
echo "reg add HKEY_CURRENT_USER\Console /v VirtualTerminalLevel /t REG_DWORD /d 0x00000001 /f"

IF %exitcode% NEQ 0 (
    echo BqLogUnitTest failed with exit code %exitcode%
    exit /b %exitcode%
)

cmake ..\..\..\..\test\cpp -G "MinGW Makefiles" -DTARGET_PLATFORM:STRING=win64 -DCPP_VER=%CPP_VER_PARAM% -DCMAKE_C_COMPILER="C:\Program Files\LLVM\bin\clang.exe" -DCMAKE_CXX_COMPILER="C:\Program Files\LLVM\bin\clang++.exe" -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build . --config RelWithDebInfo
.\BqLogUnitTest.exe
set exitcode=%ERRORLEVEL%
if exist bq_module_test_enabled if not exist .\BqLogModuleUnitTest.exe echo BqLogModuleUnitTest.exe is missing although the module test is enabled for this build.
if exist bq_module_test_enabled if not exist .\BqLogModuleUnitTest.exe if %exitcode%==0 set exitcode=1
if exist .\BqLogModuleUnitTest.exe .\BqLogModuleUnitTest.exe
if exist .\BqLogModuleUnitTest.exe if %ERRORLEVEL% NEQ 0 set exitcode=%ERRORLEVEL%

echo "If your did not see output with color, and there are some strange characters like \033, please run Terminal Windows With Administrative Privileges, then type command below, and run script again"
echo "reg add HKEY_CURRENT_USER\Console /v VirtualTerminalLevel /t REG_DWORD /d 0x00000001 /f"

IF %exitcode% NEQ 0 (
    echo BqLogUnitTest failed with exit code %exitcode%
    exit /b %exitcode%
)

cd ..
pause
