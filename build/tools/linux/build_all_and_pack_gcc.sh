#!/bin/bash
set -euo pipefail

rm -rf "../../../artifacts"
rm -rf "../../../install"

pushd "category_log_generator" >/dev/null
./executable_generate_gcc.sh
popd >/dev/null
pushd "log_decoder" >/dev/null
./executable_generate_gcc.sh
popd >/dev/null

rm -rf pack
mkdir -p pack
pushd "pack" >/dev/null

cmake ../../../../pack -DTARGET_PLATFORM:STRING=linux -DPACKAGE_NAME:STRING=bqlog-tools
cmake --build . --target package
popd >/dev/null
