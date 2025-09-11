@echo off
SETLOCAL ENABLEEXTENSIONS


md makeProj
cd makeProj

cmake ..\..\..\..\wrapper\java -G "Unix Makefiles"
cmake --build . --target install

if errorlevel 1 goto :fail

cd ..
