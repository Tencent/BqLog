#!/bin/zsh

rm -rf XCodeProj
mkdir XCodeProj
cd XCodeProj

cmake -DTARGET_PLATFORM:STRING=mac ../../../../../tools/category_log_generator/ -G "Xcode"
xcodebuild -project BqLog_CategoryLogGenerator.xcodeproj -scheme BqLog_CategoryLogGenerator -configuration RelWithDebInfo
cmake --build . --target install
cd .. 

