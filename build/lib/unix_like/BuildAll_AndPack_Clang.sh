#!/bin/sh
set -euo pipefail

./Dont_Execute_This.sh dynamic_lib clang clang++
./Dont_Execute_This.sh static_lib clang clang++

rm -rf pack
mkdir pack
cd pack

cmake ../../../../pack -DTARGET_PLATFORM:STRING=unix -DPACKAGE_NAME:STRING=bqlog-lib
cmake --build . --target package
cd ..
