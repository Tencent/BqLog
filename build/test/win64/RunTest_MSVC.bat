@echo off
set VS_PATH="%VS_PATH%"

IF NOT EXIST %VS_PATH%\devenv.com (
	echo "Please set the VS_PATH environment variable to the correct Visual Studio installation path, pointing to the directory where devenv.com is located."
	pause
	GOTO :EOF
)
set VS_PATH=%VS_PATH:~1,-1%

set CPP_VER_PARAM=%1
if "%CPP_VER_PARAM%"=="" set CPP_VER_PARAM=17

md VSProj
cd VSProj

cmake ..\..\..\..\test -DTARGET_PLATFORM:STRING=win64 -DJAVA_SUPPORT=ON -DCPP_VER=%CPP_VER_PARAM%

echo "%VS_PATH%\devenv.com"
call "%VS_PATH%\devenv.com" ./BqLogUnitTest.sln /Rebuild "Debug" /Project "./BqLogUnitTest.vcxproj" /Out Build.log

.\Debug\BqLogUnitTest.exe
set exitcode=%ERRORLEVEL%

echo "If your did not see output with color, and there are some strange characters like \033, please run Terminal Windows With Administrative Privileges, then type command below, and run script again"
echo "reg add HKEY_CURRENT_USER\Console /v VirtualTerminalLevel /t REG_DWORD /d 0x00000001 /f"

IF %exitcode% NEQ 0 (
    echo BqLogUnitTest failed with exit code %exitcode%
    exit /b %exitcode%
)

call "%VS_PATH%\devenv.com" ./BqLogUnitTest.sln /Rebuild "RelWithDebInfo" /Project "./BqLogUnitTest.vcxproj" /Out Build.log

.\RelWithDebInfo\BqLogUnitTest.exe
set exitcode=%ERRORLEVEL%

echo "If your did not see output with color, and there are some strange characters like \033, please run Terminal Windows With Administrative Privileges, then type command below, and run script again"
echo "reg add HKEY_CURRENT_USER\Console /v VirtualTerminalLevel /t REG_DWORD /d 0x00000001 /f"

IF %exitcode% NEQ 0 (
    echo BqLogUnitTest failed with exit code %exitcode%
    exit /b %exitcode%
)

cd ..
pause
