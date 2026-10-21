#!/bin/sh
set -e
CPP_VER_PARAM=${1:-17}
# the remaining arguments go to the test binary, e.g. --timeout-minutes=45
[ $# -gt 0 ] && shift || true
mkdir -p CMakeFiles
cd CMakeFiles

CC=gcc CXX=g++ cmake -DTARGET_PLATFORM:STRING=unix -DCMAKE_BUILD_TYPE=Debug -DCPP_VER=$CPP_VER_PARAM ../../../../test/cpp
make
if [ "$BQ_LOW_PERFORMANCE_TESTS" != "true" ]; then
    ./BqLogUnitTest "$@"
    exit_code=$?
    # bq_module_test_enabled: the configuration builds the module test, so its binary must exist.
    if [ -f bq_module_test_enabled ]; then
        if [ -x ./BqLogModuleUnitTest ]; then
            ./BqLogModuleUnitTest || exit_code=$?
        else
            echo "BqLogModuleUnitTest is missing although the module test is enabled for this build." >&2
            exit_code=1
        fi
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
./BqLogUnitTest "$@"
exit_code=$?
# bq_module_test_enabled: the configuration builds the module test, so its binary must exist.
if [ -f bq_module_test_enabled ]; then
    if [ -x ./BqLogModuleUnitTest ]; then
        ./BqLogModuleUnitTest || exit_code=$?
    else
        echo "BqLogModuleUnitTest is missing although the module test is enabled for this build." >&2
        exit_code=1
    fi
fi
if [ $exit_code -ne 0 ]; then
    echo "Test failed."
    exit 1
fi
cd ..
