#!/bin/zsh

mkdir XCodeProj
cd XCodeProj

cmake ../../../../test -DTARGET_PLATFORM:STRING=mac -G "Xcode"
cd .. 

