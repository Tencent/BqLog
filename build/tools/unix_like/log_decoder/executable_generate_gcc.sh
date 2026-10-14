#!/bin/sh
set -euo pipefail

rm -rf CMakeFiles
mkdir -p CMakeFiles;
cd CMakeFiles;
CC=gcc CXX=g++ cmake -DTARGET_PLATFORM:STRING=unix -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=../../../../../install/tools -DCMAKE_INSTALL_DOCDIR=. ../../../../../tools/log_decoder/ ;
cmake --build . --parallel 4
cmake --build . --target install
cd ..;