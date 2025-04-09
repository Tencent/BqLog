#!/bin/bash

mkdir CMakeFiles;
cd CMakeFiles;


CC=gcc CXX=g++ cmake -DTARGET_PLATFORM:STRING=linux -DCMAKE_BUILD_TYPE=RelWithDebInfo ../../../../test;
make;
ulimit -c unlimited
./BqLogUnitTest
exit_code=$?
echo "Call Exited."
if [ $exit_code -eq 0 ]; then
    echo "RelWithDebInfo Test succeeded."
else
    echo "RelWithDebInfo Test failed with exit code $exit_code."
    # 如果崩溃，查找核心转储文件并用 GDB 输出调用栈
    if [ -f "core" ] || [ -f "core.*" ]; then
        echo "Core dump detected, analyzing with GDB..."
        # 查找核心转储文件（文件名可能因系统而异）
        CORE_FILE=$(ls core* 2>/dev/null | head -n 1)
        if [ -n "$CORE_FILE" ]; then
            gdb --batch --quiet \
                -ex "thread apply all bt full" \
                -ex "quit" \
                ./BqLogUnitTest "$CORE_FILE"
        else
            echo "No core dump file found."
        fi
    else
        echo "No core dump generated."
    fi
    exit 1
fi
cd ..;