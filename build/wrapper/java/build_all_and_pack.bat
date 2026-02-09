@echo off
SETLOCAL ENABLEEXTENSIONS

if exist "..\..\..\artifacts" rmdir /s /q "..\..\..\artifacts"
if exist "..\..\..\install" rmdir /s /q "..\..\..\install"

md makeProj
cd makeProj

cmake ..\..\..\..\wrapper\java -G "Unix Makefiles" || exit /b 1
cmake --build . --target install || exit /b 1


cd ..

rd /s/q pack
md pack
cd pack

cmake ../../../../pack %GEN_PLATFORM_ARG% -DTARGET_PLATFORM:STRING=all -DPACKAGE_NAME:STRING=bqlog-java-wrapper || exit /b 1
cmake --build . --target package || exit /b 1
cd ..
