#!/bin/zsh

mkdir XCodeProj_Static_a
cd XCodeProj_Static_a

cmake -DTARGET_PLATFORM:STRING=mac -DJAVA_SUPPORT=ON -DBUILD_TYPE:STRING=static_lib -DAPPLE_LIB_FORMAT:STRING=a ../../../../src -G "Xcode"
cd .. 