@echo off
setlocal enabledelayedexpansion


set "root_dir=%~dp0"

set "dirs=src include demo benchmark test tools"


for %%d in (%dirs%) do (
    echo "%~dp0%%d"
    pushd "%~dp0%%d"
    for /r  %%f in (*.cpp *.h) do (
        echo Formatting %%f
        clang-format -i "%%f"
    )
    popd
)

endlocal
echo Done!
pause
