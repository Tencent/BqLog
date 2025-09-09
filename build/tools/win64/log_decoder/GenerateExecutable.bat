@echo off

set VS_PATH="%VS_PATH%"

IF NOT EXIST %VS_PATH%\devenv.com (
	echo "Please set the VS_PATH environment variable to the correct Visual Studio installation path, pointing to the directory where devenv.com is located."
	pause
	GOTO :EOF
)
set VS_PATH=%VS_PATH:~1,-1%

rd /s/q VSProj
md VSProj
cd VSProj

cmake ..\..\..\..\..\tools\log_decoder\ -DTARGET_PLATFORM:STRING=win64 -DCMAKE_GENERATOR_PLATFORM=x64


echo "VS COMMAND TOOLS PATH:"
echo "%VS_PATH%\devenv.com"
call "%VS_PATH%\devenv.com" ./BqLog_LogDecoder.sln /Rebuild "Debug" /Project "./BqLog_LogDecoder.vcxproj" /Out Build.log 
call "%VS_PATH%\devenv.com" ./BqLog_LogDecoder.sln /Rebuild "Release" /Project "./BqLog_LogDecoder.vcxproj" /Out Build.log 
call "%VS_PATH%\devenv.com" ./BqLog_LogDecoder.sln /Rebuild "RelWithDebInfo" /Project "./BqLog_LogDecoder.vcxproj" /Out Build.log 
call "%VS_PATH%\devenv.com" ./BqLog_LogDecoder.sln /Rebuild "MinSizeRel" /Project "./BqLog_LogDecoder.vcxproj" /Out Build.log 
cd ..

pause
