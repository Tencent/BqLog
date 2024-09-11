#!/bin/csh

mkdir CMakeFiles;
cd CMakeFiles;
CC=clang CXX=clang++ cmake -DTARGET_PLATFORM:STRING=unix -DCMAKE_BUILD_TYPE=RelWithDebInfo ../../../../../tools/log_decoder/ ;
make;
cd ..;