@echo off
SETLOCAL ENABLEEXTENSIONS


md makeProj
cd makeProj

cmake ..\..\..\..\wrapper\typescript -G "Unix Makefiles" || exit /b 1
cmake --build . --target install || exit /b 1


cd ..

rd /s/q pack
md pack
cd pack

cmake ../../../../pack %GEN_PLATFORM_ARG% -DTARGET_PLATFORM:STRING=all -DPACKAGE_NAME:STRING=bqlog-nodejs-npm || exit /b 1
cmake --build . --target package || exit /b 1
cd ..
