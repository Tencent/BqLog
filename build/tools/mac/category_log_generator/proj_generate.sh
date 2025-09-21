#!/bin/zsh

mkdir XCodeProj
cd XCodeProj

cmake ../../../../../tools/category_log_generator/ -DTARGET_PLATFORM:STRING=mac -G "Xcode"
cd ..

