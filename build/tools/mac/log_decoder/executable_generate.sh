#!/bin/zsh

rm -rf XCodeProj
mkdir -p XCodeProj
cd XCodeProj

cmake -DTARGET_PLATFORM:STRING=mac -DCMAKE_INSTALL_PREFIX=../../../../../install/tools -DCMAKE_INSTALL_DOCDIR=. ../../../../../tools/log_decoder/ -G "Xcode"
xcodebuild -project BqLog_LogDecoder.xcodeproj -scheme BqLog_LogDecoder -configuration RelWithDebInfo
cmake --build . --target install
cd .. 

