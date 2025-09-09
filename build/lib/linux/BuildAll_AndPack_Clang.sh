#!/bin/bash
set -euo pipefail

./Dont_Execute_This.sh dynamic_lib clang clang++
./Dont_Execute_This.sh static_lib clang clang++

rm -rf pack
mkdir pack
pushd "pack" >/dev/null

cmake ../../../../pack -DTARGET_PLATFORM:STRING=linux -DPACKAGE_NAME:STRING=bqlog-lib
cmake --build . --target package
popd >/dev/null
