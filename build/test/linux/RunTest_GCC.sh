#!/bin/bash

mkdir CMakeFiles;
cd CMakeFiles;
CC=gcc CXX=g++ cmake -DTARGET_PLATFORM:STRING=linux -DCMAKE_BUILD_TYPE=RelWithDebInfo ../../../../test;
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