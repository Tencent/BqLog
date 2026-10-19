#!/bin/bash
set -euo pipefail

rm -rf CMakeFiles
mkdir -p CMakeFiles;
cd CMakeFiles;
CC=gcc CXX=g++ cmake -DTARGET_PLATFORM:STRING=linux -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=../../../../../install/tools -DCMAKE_INSTALL_DOCDIR=. ../../../../../tools/log_decoder/ ;
cmake --build . --parallel
cmake --build . --target install
cd ..;