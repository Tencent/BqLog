#!/bin/csh

mkdir CMakeFiles;
cd CMakeFiles;
CC=clang CXX=clang++ cmake -DTARGET_PLATFORM:STRING=unix -DCMAKE_BUILD_TYPE=RelWithDebInfo ../../../../../tools/category_log_generator/ ;
make;
cd ..;