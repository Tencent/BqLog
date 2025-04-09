#!/bin/sh

CPP_VER_PARAM=${1:-17}
mkdir CMakeFiles;
cd CMakeFiles;
CC=clang CXX=clang++ cmake -DTARGET_PLATFORM:STRING=unix -DCMAKE_BUILD_TYPE=Debug -DCPP_VER=$CPP_VER_PARAM ../../../../test;
make;
./BqLogUnitTest
exit_code=$?
if [ $exit_code -eq 0 ]; then
    echo "Test succeeded."
else
    echo "Test failed."
    exit 1
fi
CC=clang CXX=clang++ cmake -DTARGET_PLATFORM:STRING=unix -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCPP_VER=$CPP_VER_PARAM ../../../../test;
make;
./BqLogUnitTest
exit_code=$?
if [ $exit_code -eq 0 ]; then
    echo "Test succeeded."
else
    echo "Test failed."
    exit 1
fi
cd ..;