#!/bin/bash

mkdir CMakeFiles;
cd CMakeFiles;
# Enable core dumps
ulimit -c unlimited
export CORE_PATTERN=core.dump
CC=gcc CXX=g++ cmake -DTARGET_PLATFORM:STRING=linux -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address" ../../../../test;
make;
./BqLogUnitTest
exit_code=$?
cd ..;
# Check if core dump was generated
if [ -f "core.dump" ]; then
    echo "Core dump generated."
else
    echo "No core dump generated."
fi
if [ $exit_code -eq 0 ]; then
    echo "Test succeeded."
else
    echo "Test failed."
    exit 1
fi