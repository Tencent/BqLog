@echo off
SETLOCAL ENABLEEXTENSIONS


md makeProj
cd makeProj

cmake ..\..\..\..\wrapper\java -G "Unix Makefiles"
cmake --build . --target install

echo Please Ensure "make.exe" is in your PATH.

make

cd ..
pause
