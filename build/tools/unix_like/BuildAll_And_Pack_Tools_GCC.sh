#!/bin/sh
set -euo pipefail

UNIX_VERSION=${1:-}

cd category_log_generator
./GenerateExecutable_GCC.sh
cd ../log_decoder
./GenerateExecutable_GCC.sh
cd ..

rm -rf pack
mkdir pack
cd pack

cmake ../../../../pack -DTARGET_PLATFORM:STRING=unix -DPACKAGE_NAME:STRING=bqlog-tools -DBQ_UNIX_VERSION=$UNIX_VERSION
cmake --build . --target package
cd ..
