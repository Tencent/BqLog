#!/bin/zsh

cd "$(dirname "$0")"
mkdir XCodeProj
cd XCodeProj

cmake -DTARGET_PLATFORM:STRING=mac ../../../../benchmark/cpp -G "Xcode"
cd .. 

