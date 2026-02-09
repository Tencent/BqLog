#!/bin/zsh

cd "$(dirname "$0")"
mkdir -p XCodeProj
cd XCodeProj

cmake -DTARGET_PLATFORM:STRING=mac ../../../../benchmark/cpp -G "Xcode"
cd .. 

