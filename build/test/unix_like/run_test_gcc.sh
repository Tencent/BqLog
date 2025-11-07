#!/bin/sh
set -e
CPP_VER_PARAM=${1:-17}
mkdir -p CMakeFiles
cd CMakeFiles

CC=clang CXX=clang++ cmake -DTARGET_PLATFORM:STRING=unix -DCMAKE_BUILD_TYPE=Debug -DCPP_VER=$CPP_VER_PARAM ../../../../test
make
./BqLogUnitTest
exit_code=$?
if [ $exit_code -ne 0 ]; then
    echo "Test failed."
    exit 1
fi

CC=gcc CXX=g++ cmake -DTARGET_PLATFORM:STRING=unix -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCPP_VER=$CPP_VER_PARAM ../../../../test
make
./BqLogUnitTest
exit_code=$?
if [ $exit_code -ne 0 ]; then
    echo "Test failed."
    exit 1
fi
cd ..