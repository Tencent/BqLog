#!/bin/sh
set -euo pipefail

UNIX_VERSION=${1:-}

rm -rf "../../../artifacts"
rm -rf "../../../install"

cd category_log_generator
./executable_generate_gcc.sh
cd ../log_decoder
./executable_generate_gcc.sh
cd ..

rm -rf pack
mkdir -p pack
cd pack

cmake ../../../../pack -DTARGET_PLATFORM:STRING=unix -DPACKAGE_NAME:STRING=bqlog-tools -DBQ_UNIX_VERSION=$UNIX_VERSION
cmake --build . --target package
cd ..
