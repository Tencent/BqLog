@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Wrapper: build both static and dynamic libraries and then package.
rem All parameter handling is delegated to dont_execute_this.bat.
rem You can still pass optional args: [arch] [compiler] [java] [node]
rem Example: BuildAll_AndPack.bat x64 msvc on off

call ".\dont_execute_this.bat" all %* || exit /b 1
exit /b 0