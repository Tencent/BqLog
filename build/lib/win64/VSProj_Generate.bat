@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Wrapper: generate Visual Studio project files.
rem All parameter handling is delegated to Dont_Execute_This.bat.
rem Optional args: [arch] [msvc|clang] [java] [node] [static_lib|dynamic_lib]
rem Example: VSProj_Generate.bat x64 msvc on off static_lib

call ".\Dont_Execute_This.bat" gen-vsproj %* || exit /b 1
pause
exit /b 0