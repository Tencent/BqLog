#!/bin/zsh

rm -rf XCodeProj
mkdir -p XCodeProj
cd XCodeProj

cmake -DTARGET_PLATFORM:STRING=mac -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_INSTALL_PREFIX=../../../../../install/tools -DCMAKE_INSTALL_DOCDIR=. ../../../../../tools/log_decoder/ -G "Xcode"
cmake --build . --target install --config RelWithDebInfo
cd .. 

