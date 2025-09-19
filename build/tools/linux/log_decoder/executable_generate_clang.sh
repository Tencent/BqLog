#!/bin/bash
set -euo pipefail

rm -rf CMakeFiles
mkdir CMakeFiles;
cd CMakeFiles;
CC=clang CXX=clang++ cmake -DTARGET_PLATFORM:STRING=linux -DCMAKE_BUILD_TYPE=RelWithDebInfo ../../../../../tools/log_decoder/ ;
cmake --build . --parallel
cmake --build . --target install
cd ..;