@echo off

set VS_PATH="%VS_PATH%"

set BUILD_TYPE=%1
set COMPILER_TYPE=%2

IF NOT EXIST %VS_PATH%\devenv.com (
	echo "Please set the VS_PATH environment variable to the correct Visual Studio installation path, pointing to the directory where devenv.com is located."
	pause
	GOTO :EOF
)

set VS_PATH=%VS_PATH:~1,-1%

rd /s/q VSProj_%BUILD_TYPE%
md VSProj_%BUILD_TYPE%
cd VSProj_%BUILD_TYPE%

IF "%COMPILER_TYPE%"=="clang" (
	cmake ..\..\..\..\src -DTARGET_PLATFORM:STRING=win64 -DCMAKE_GENERATOR_PLATFORM=x64 -DJAVA_SUPPORT=ON -DBUILD_TYPE=%BUILD_TYPE% -T ClangCl
)^
ELSE (
	cmake ..\..\..\..\src -DTARGET_PLATFORM:STRING=win64 -DCMAKE_GENERATOR_PLATFORM=x64 -DJAVA_SUPPORT=ON -DBUILD_TYPE=%BUILD_TYPE%
)



echo "VS COMMAND TOOLS PATH:"
echo "%VS_PATH%\devenv.com"


set BUILD_TYPE[0]=Debug
set BUILD_TYPE[1]=Release
set BUILD_TYPE[2]=RelWithDebInfo
set BUILD_TYPE[3]=MinSizeRel

rem %%p : BUILD_TYPE
for /l %%a in (0,1,3) do (
	for /f "usebackq delims== tokens=1-2" %%o in (`set BUILD_TYPE[%%a]`) do (
		call "%VS_PATH%\devenv.com" ./BqLog.sln /Rebuild "%%p" /Project "./BqLog.vcxproj" /Out Build.log
		cmake --install . --config %%p
		if exist "..\..\..\..\artifacts\%BUILD_TYPE%\win64\%%p\BqLog.pdb" (
			echo PDB exists. Copying to ..\..\..\..\dist\%BUILD_TYPE%\win64\%%p\BqLog.pdb"...
			copy /Y "..\..\..\..\artifacts\%BUILD_TYPE%\win64\%%p\BqLog.pdb" "..\..\..\..\dist\%BUILD_TYPE%\win64\%%p\BqLog.pdb"
			echo File copied successfully.
		)
	)
)

cd ..

echo "---------"
echo "Finished!"
echo "---------"
