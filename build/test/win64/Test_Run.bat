@echo off
set VS_PATH="%VS_PATH%"

IF NOT EXIST %VS_PATH%\devenv.com (
	echo "Please set the VS_PATH environment variable to the correct Visual Studio installation path, pointing to the directory where devenv.com is located."
	pause
	GOTO :EOF
)
set VS_PATH=%VS_PATH:~1,-1%

md VSProj
cd VSProj

cmake ..\..\..\..\test -DTARGET_PLATFORM:STRING=win64 -DCMAKE_GENERATOR_PLATFORM=x64 -DJAVA_SUPPORT=ON

echo "%VS_PATH%\devenv.com"
call "%VS_PATH%\devenv.com" ./BqLogUnitTest.sln /Rebuild "RelWithDebInfo" /Project "./BqLogUnitTest.vcxproj" /Out Build.log

.\RelWithDebInfo\BqLogUnitTest.exe

cd ..

echo "If your did not see output with color, and there are some strange characters like \033, please run Terminal Windows With Administrative Privileges, then type command below, and run script again"
echo "reg add HKEY_CURRENT_USER\Console /v VirtualTerminalLevel /t REG_DWORD /d 0x00000001 /f"


pause
