@echo off
setlocal enabledelayedexpansion

rem Unknown keys are downgraded to a warning on purpose: the config names both SpaceInEmptyBraces
rem (clang-format 22 and later) and SpaceInEmptyBlock (18 to 21), and every version knows only one
rem of them. Refusing to run on the other one would make this depend on the installed version.

set "root_dir=%~dp0"

set "dirs=src include demo benchmark test tools"


for %%d in (%dirs%) do (
    echo "%~dp0%%d"
    pushd "%~dp0%%d"
    for /r  %%f in (*.cpp *.h *.cppm) do (
        echo Formatting %%f
        clang-format -Wno-error=unknown -i "%%f"
    )
    popd
)

endlocal
echo Done!
pause
