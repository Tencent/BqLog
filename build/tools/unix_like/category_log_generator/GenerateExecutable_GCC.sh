#!/bin/bash
set -euo pipefail

rm -rf CMakeFiles
mkdir CMakeFiles;
cd CMakeFiles;
CC=gcc CXX=g++ cmake -DTARGET_PLATFORM:STRING=unix -DCMAKE_BUILD_TYPE=RelWithDebInfo ../../../../../tools/category_log_generator/ ;
cmake --build . --parallel 4
cmake --build . --target install
cd ..;