@echo off
SETLOCAL ENABLEEXTENSIONS


md makeProj
cd makeProj

cmake ..\..\..\..\wrapper\java -G "Unix Makefiles"

echo Please Ensure "make.exe" is in your PATH.

make

cd ..
pause
