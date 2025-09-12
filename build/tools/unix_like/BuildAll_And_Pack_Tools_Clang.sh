#!/bin/sh
set -euo pipefail

cd category_log_generator
./GenerateExecutable_Clang.sh
cd ../log_decoder
./GenerateExecutable_Clang.sh
cd ..

rm -rf pack
mkdir pack
cd pack

cmake ../../../../pack -DTARGET_PLATFORM:STRING=unix -DPACKAGE_NAME:STRING=bqlog-tools
cmake --build . --target package
if errorlevel 1 exit /b %errorlevel%
cd ..
