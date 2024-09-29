#!/bin/zsh

mkdir XCodeProj
cd XCodeProj

cmake ../../../../test -DTARGET_PLATFORM:STRING=mac -G "Xcode"
xcodebuild -project BqLogUnitTest.xcodeproj -scheme BqLogUnitTest -configuration Debug
./Debug/BqLogUnitTest
exit_code=$?
cd ..;
if [ $exit_code -eq 0 ]; then
    echo "Test succeeded."
else
    echo "Test failed."
    exit 1
fi

