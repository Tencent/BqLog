#!/bin/bash

mkdir CMakeFiles;
cd CMakeFiles;
CC=clang CXX=clang++ cmake -DTARGET_PLATFORM:STRING=linux -DCMAKE_BUILD_TYPE=Debug ../../../../test;
make;
./BqLogUnitTest
exit_code=$?
if [ $exit_code -eq 0 ]; then
    echo "Test succeeded."
else
    echo "Test failed."
    exit 1
fi

CC=clang CXX=clang++ cmake -DTARGET_PLATFORM:STRING=linux -DCMAKE_BUILD_TYPE=RelWithDebInfo ../../../../test;
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