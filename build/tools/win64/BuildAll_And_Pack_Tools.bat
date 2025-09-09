@echo off

cd category_log_generator
call ./GenerateExecutable.bat
if errorlevel 1 goto :fail
cd ../log_decoder
call ./GenerateExecutable.bat
if errorlevel 1 goto :fail
cd ..

rd /s/q pack
md pack
cd pack

cmake ../../../../pack -DTARGET_PLATFORM:STRING=win64 -DPACKAGE_NAME:STRING=bqlog-tools
cmake --build . --target package
cd ..
