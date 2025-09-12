#!/bin/sh
set -euo pipefail

cd category_log_generator
./GenerateExecutable_GCC.sh
cd ../log_decoder
./GenerateExecutable_GCC.sh
cd ..

rm -rf pack
mkdir pack
cd pack

cmake ../../../../pack -DTARGET_PLATFORM:STRING=unix -DPACKAGE_NAME:STRING=bqlog-tools
cmake --build . --target package
if errorlevel 1 goto :fail
cd ..
