#!/bin/zsh

mkdir -p XCodeProj
cd XCodeProj

cmake ../../../../../tools/log_decoder/ -DTARGET_PLATFORM:STRING=mac -G "Xcode"
cd ..

