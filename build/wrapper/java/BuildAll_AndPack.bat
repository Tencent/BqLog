@echo off
SETLOCAL ENABLEEXTENSIONS


md makeProj
cd makeProj

cmake ..\..\..\..\wrapper\java -G "Unix Makefiles"
cmake --build . --target install

if errorlevel 1 exit /b %errorlevel%

cd ..

rd /s/q pack
md pack
cd pack

cmake ../../../../pack %GEN_PLATFORM_ARG% -DTARGET_PLATFORM:STRING=all -DPACKAGE_NAME:STRING=bqlog-java-wrapper
cmake --build . --target package
cd ..
