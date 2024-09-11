#!/bin/bash

mkdir CMakeFiles;
cd CMakeFiles;
CC=gcc CXX=g++ cmake -DTARGET_PLATFORM:STRING=linux -DCMAKE_BUILD_TYPE=RelWithDebInfo ../../../../demo/cpp;
make;
cd ..;