#!/bin/zsh

mkdir -p XCodeProj
cd XCodeProj

cmake ../../../../test/cpp -DTARGET_PLATFORM:STRING=mac -G "Xcode"
cd .. 

