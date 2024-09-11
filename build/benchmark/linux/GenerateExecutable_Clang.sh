#!/bin/bash

mkdir CMakeFiles;
cd CMakeFiles;
CC=clang CXX=clang++ cmake -DTARGET_PLATFORM:STRING=linux -DCMAKE_BUILD_TYPE=RelWithDebInfo ../../../../benchmark/cpp;
make;
cd ..;
