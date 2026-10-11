#!/bin/sh
set -e
CPP_VER_PARAM=${1:-17}
mkdir -p CMakeFiles
cd CMakeFiles

CC=gcc CXX=g++ cmake -DTARGET_PLATFORM:STRING=unix -DCMAKE_BUILD_TYPE=Debug -DCPP_VER=$CPP_VER_PARAM ../../../../test/cpp
make
if [ "$BQ_LOW_PERFORMANCE_TESTS" != "true" ]; then
    ./BqLogUnitTest
    exit_code=$?
    if [ -x ./BqLogModuleUnitTest ]; then
        ./BqLogModuleUnitTest || exit_code=$?
    fi
    if [ $exit_code -ne 0 ]; then
        echo "Test failed."
        exit 1
    fi
else
    echo "Skip Debug tests in low performance mode."
fi

CC=gcc CXX=g++ cmake -DTARGET_PLATFORM:STRING=unix -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCPP_VER=$CPP_VER_PARAM ../../../../test/cpp
make
./BqLogUnitTest
exit_code=$?
if [ -x ./BqLogModuleUnitTest ]; then
    ./BqLogModuleUnitTest || exit_code=$?
fi
if [ $exit_code -ne 0 ]; then
    echo "Test failed."
    exit 1
fi
cd ..
