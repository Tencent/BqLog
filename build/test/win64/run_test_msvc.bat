@echo off
set CPP_VER_PARAM=%1
if "%CPP_VER_PARAM%"=="" set CPP_VER_PARAM=17

md VSProj
cd VSProj

cmake ..\..\..\..\test\cpp -DTARGET_PLATFORM:STRING=win64 -DJAVA_SUPPORT=ON -DCPP_VER=%CPP_VER_PARAM% -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
.\Debug\BqLogUnitTest.exe
set exitcode=%ERRORLEVEL%
rem bq_module_test_enabled: the configuration builds the module test, so its binary must exist.
if exist bq_module_test_enabled if not exist .\Debug\BqLogModuleUnitTest.exe echo BqLogModuleUnitTest.exe is missing although the module test is enabled for this build.
if exist bq_module_test_enabled if not exist .\Debug\BqLogModuleUnitTest.exe if %exitcode%==0 set exitcode=1
if exist .\Debug\BqLogModuleUnitTest.exe .\Debug\BqLogModuleUnitTest.exe
if exist .\Debug\BqLogModuleUnitTest.exe if %ERRORLEVEL% NEQ 0 set exitcode=%ERRORLEVEL%

echo "If your did not see output with color, and there are some strange characters like \033, please run Terminal Windows With Administrative Privileges, then type command below, and run script again"
echo "reg add HKEY_CURRENT_USER\Console /v VirtualTerminalLevel /t REG_DWORD /d 0x00000001 /f"

IF %exitcode% NEQ 0 (
    echo BqLogUnitTest failed with exit code %exitcode%
    exit /b %exitcode%
)

cmake ..\..\..\..\test\cpp -DTARGET_PLATFORM:STRING=win64 -DJAVA_SUPPORT=ON -DCPP_VER=%CPP_VER_PARAM% -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build . --config RelWithDebInfo
.\RelWithDebInfo\BqLogUnitTest.exe
set exitcode=%ERRORLEVEL%
if exist bq_module_test_enabled if not exist .\RelWithDebInfo\BqLogModuleUnitTest.exe echo BqLogModuleUnitTest.exe is missing although the module test is enabled for this build.
if exist bq_module_test_enabled if not exist .\RelWithDebInfo\BqLogModuleUnitTest.exe if %exitcode%==0 set exitcode=1
if exist .\RelWithDebInfo\BqLogModuleUnitTest.exe .\RelWithDebInfo\BqLogModuleUnitTest.exe
if exist .\RelWithDebInfo\BqLogModuleUnitTest.exe if %ERRORLEVEL% NEQ 0 set exitcode=%ERRORLEVEL%

echo "If your did not see output with color, and there are some strange characters like \033, please run Terminal Windows With Administrative Privileges, then type command below, and run script again"
echo "reg add HKEY_CURRENT_USER\Console /v VirtualTerminalLevel /t REG_DWORD /d 0x00000001 /f"

IF %exitcode% NEQ 0 (
    echo BqLogUnitTest failed with exit code %exitcode%
    exit /b %exitcode%
)

cd ..
pause
