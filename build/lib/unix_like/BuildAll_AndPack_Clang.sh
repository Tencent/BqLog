#!/bin/sh
set -euo pipefail

UNIX_VERSION=$1

./Dont_Execute_This.sh dynamic_lib clang clang++
./Dont_Execute_This.sh static_lib clang clang++

rm -rf pack
mkdir pack
cd pack

cmake ../../../../pack -DTARGET_PLATFORM:STRING=unix -DPACKAGE_NAME:STRING=bqlog-lib -DBQ_UNIX_VERSION=$UNIX_VERSION
cmake --build . --target package
cd ..
