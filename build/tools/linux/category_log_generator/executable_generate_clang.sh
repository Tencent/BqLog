#!/bin/bash
set -euo pipefail

rm -rf CMakeFiles
mkdir -p CMakeFiles;
cd CMakeFiles;
CC=clang CXX=clang++ cmake -DTARGET_PLATFORM:STRING=linux -DCMAKE_BUILD_TYPE=RelWithDebInfo ../../../../../tools/category_log_generator/ ;
cmake --build . --parallel
cmake --build . --target install
cd ..;