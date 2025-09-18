@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Wrapper: build both static and dynamic libraries and then package.
rem All parameter handling is delegated to Dont_Execute_This.bat.
rem You can still pass optional args: [arch] [compiler] [java] [node]
rem Example: BuildAll_AndPack.bat x64 msvc on off

call ".\Dont_Execute_This.bat" all %* || exit /b 1
exit /b 0