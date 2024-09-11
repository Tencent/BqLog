#!/bin/zsh

mkdir XCodeProj_Static_framework
cd XCodeProj_Static_framework

cmake -DTARGET_PLATFORM:STRING=mac -DJAVA_SUPPORT=ON -DBUILD_TYPE:STRING=static_lib -DAPPLE_LIB_FORMAT:STRING=framework ../../../../src -G "Xcode"
cd .. 