#!/bin/zsh

mkdir XCodeProj_Dynamic_dylib
cd XCodeProj_Dynamic_dylib

cmake -DTARGET_PLATFORM:STRING=mac -DJAVA_SUPPORT=ON -DBUILD_TYPE:STRING=dynamic_lib -DAPPLE_LIB_FORMAT:STRING=dylib ../../../../src -G "Xcode"
cd .. 