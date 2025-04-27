@echo off
set VS_PATH="%VS_PATH%"

IF NOT EXIST %VS_PATH%\devenv.com (
	echo "Please set the VS_PATH environment variable to the correct Visual Studio installation path, pointing to the directory where devenv.com is located."
	pause
	GOTO :EOF
)
set VS_PATH=%VS_PATH:~1,-1%

set BUILD_TOOL=
set MSBUILD_PATH=
set DEVENV_PATH=%VS_PATH%\devenv.com

set MSBUILD_PATH="%VS_PATH%\..\..\MSBuild\Current\Bin\MSBuild.exe"
if exist !MSBUILD_PATH! (
    set BUILD_TOOL=MSBUILD
)

if not defined BUILD_TOOL (
    set VSWHERE="C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "%VSWHERE%" (
        for /f "tokens=*" %%i in ('"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath') do set VS_INSTALL_PATH=%%i
        if defined VS_INSTALL_PATH (
            set MSBUILD_PATH=%VS_INSTALL_PATH%\MSBuild\Current\Bin\MSBuild.exe
            if exist "!MSBUILD_PATH!" (
                set BUILD_TOOL=MSBUILD
            )
        )
    )
)

if not defined BUILD_TOOL (
    set MSBUILD_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    if exist !MSBUILD_PATH! (
        set BUILD_TOOL=MSBUILD
    )
)

if not defined BUILD_TOOL (
    set BUILD_TOOL=DEVENV
)

set ARCH_PARAM=x64
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" set ARCH_PARAM=ARM64

set CPP_VER_PARAM=%1
if "%CPP_VER_PARAM%"=="" set CPP_VER_PARAM=17

md VSProj
cd VSProj

cmake ..\..\..\..\test -DTARGET_PLATFORM:STRING=win64 -DJAVA_SUPPORT=ON -DCPP_VER=%CPP_VER_PARAM% -T ClangCl

if "%BUILD_TOOL%"=="MSBUILD" (
    echo Using MSBuild: !MSBUILD_PATH!
    call "!MSBUILD_PATH!" ./BqLogUnitTest.sln /t:Rebuild /p:Configuration=Debug /p:Platform=%ARCH_PARAM% /out Build.log
) else (
    echo Using devenv.com: %DEVENV_PATH%
    call "%DEVENV_PATH%" ./BqLogUnitTest.sln /Rebuild "Debug" /Project "./BqLogUnitTest.vcxproj" /Out Build.log
)

.\Debug\BqLogUnitTest.exe
set exitcode=%ERRORLEVEL%

echo "If your did not see output with color, and there are some strange characters like \033, please run Terminal Windows With Administrative Privileges, then type command below, and run script again"
echo "reg add HKEY_CURRENT_USER\Console /v VirtualTerminalLevel /t REG_DWORD /d 0x00000001 /f"

IF %exitcode% NEQ 0 (
    echo BqLogUnitTest failed with exit code %exitcode%
    exit /b %exitcode%
)

if "%BUILD_TOOL%"=="MSBUILD" (
    call "!MSBUILD_PATH!" ./BqLogUnitTest.sln /t:Rebuild /p:Configuration=RelWithDebInfo /p:Platform=%ARCH_PARAM% /out Build.log
) else (
    call "%DEVENV_PATH%" ./BqLogUnitTest.sln /Rebuild "RelWithDebInfo" /Project "./BqLogUnitTest.vcxproj" /Out Build.log
)

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
