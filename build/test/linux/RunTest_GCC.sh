#!/bin/bash

CPP_VER_PARAM=${1:-17}
mkdir CMakeFiles;
cd CMakeFiles;

ulimit -c unlimited
echo "Setting core dump pattern to 'core.%p'..."
sudo sysctl -w kernel.core_pattern=core.%p || echo "Failed to set core pattern (might lack permissions)."
echo "Running BqLogUnitTest directly..."

CC=gcc CXX=g++ cmake -DTARGET_PLATFORM:STRING=linux -DCMAKE_BUILD_TYPE=Debug -DCPP_VER="$CPP_VER_PARAM" ../../../../test;
make;
./BqLogUnitTest
exit_code=$?
if [ $exit_code -eq 0 ]; then
    echo "Test succeeded."
else
    CORE_FILE=$(ls core* 2>/dev/null | head -n 1)
    if [ -n "$CORE_FILE" ]; then
        echo "Core dump detected at $CORE_FILE, analyzing with GDB..."
        gdb --batch --quiet \
            -ex "thread apply all bt full" \
            -ex "quit" \
            ./BqLogUnitTest "$CORE_FILE"
    else
        echo "No core dump generated in current directory."
        echo "Running with GDB to capture stack trace directly..."
        gdb --batch --quiet \
            -ex "run" \
            -ex "thread apply all bt full" \
            -ex "quit" \
            ./BqLogUnitTest
    fi
    echo "Test failed."
    exit 1
fi

CC=gcc CXX=g++ cmake -DTARGET_PLATFORM:STRING=linux -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCPP_VER="$CPP_VER_PARAM" ../../../../test;
make;
./BqLogUnitTest
exit_code=$?
if [ $exit_code -eq 0 ]; then
    echo "Test succeeded."
else
    CORE_FILE=$(ls core* 2>/dev/null | head -n 1)
    if [ -n "$CORE_FILE" ]; then
        echo "Core dump detected at $CORE_FILE, analyzing with GDB..."
        gdb --batch --quiet \
            -ex "thread apply all bt full" \
            -ex "quit" \
            ./BqLogUnitTest "$CORE_FILE"
    else
        echo "No core dump generated in current directory."
        echo "Running with GDB to capture stack trace directly..."
        gdb --batch --quiet \
            -ex "run" \
            -ex "thread apply all bt full" \
            -ex "quit" \
            ./BqLogUnitTest
    fi
    echo "Test failed."
    exit 1
fi
cd ..;