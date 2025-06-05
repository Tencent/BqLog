#!/bin/zsh

CPP_VER_PARAM=${1:-17}
mkdir XCodeProj
cd XCodeProj

cmake ../../../../test -DTARGET_PLATFORM:STRING=mac -DCPP_VER=$CPP_VER_PARAM -G "Xcode" OTHER_LDFLAGS="-ld_classic"
xcodebuild -project BqLogUnitTest.xcodeproj -scheme BqLogUnitTest -configuration Debug
./Debug/BqLogUnitTest
exit_code=$?
if [ $exit_code -eq 0 ]; then
    echo "Test succeeded."
else
    echo "Test failed."
    exit 1
fi

cmake ../../../../test -DTARGET_PLATFORM:STRING=mac -DCPP_VER=$CPP_VER_PARAM -G "Xcode"
xcodebuild -project BqLogUnitTest.xcodeproj -scheme BqLogUnitTest -configuration RelWithDebInfo OTHER_LDFLAGS="-ld_classic"
./RelWithDebInfo/BqLogUnitTest
exit_code=$?
if [ $exit_code -eq 0 ]; then
    echo "Test succeeded."
else
    echo "Test failed."
    exit 1
fi
cd ..;

