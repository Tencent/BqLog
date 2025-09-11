#!/bin/bash
set -euo pipefail

pushd "category_log_generator" >/dev/null
./GenerateExecutable_Clang.sh
popd >/dev/null
pushd "log_decoder" >/dev/null
./GenerateExecutable_Clang.sh
popd >/dev/null

rm -rf pack
mkdir pack
pushd "pack" >/dev/null

cmake ../../../../pack -DTARGET_PLATFORM:STRING=linux -DPACKAGE_NAME:STRING=bqlog-tools
cmake --build . --target package
popd >/dev/null
