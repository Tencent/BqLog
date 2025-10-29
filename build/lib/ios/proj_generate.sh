#!/bin/zsh

rm -rf XCodeProj
mkdir -p XCodeProj
pushd "XCodeProj" >/dev/null

cmake ../../../../src \
    -G Xcode \
    -DCMAKE_TOOLCHAIN_FILE=../ios.toolchain.cmake \
    -DPLATFORM=OS64 \
    -DDEPLOYMENT_TARGET=9.0 \
    -DBUILD_LIB_TYPE=dynamic_lib \
    -DAPPLE_LIB_FORMAT=framework \
    -DTARGET_PLATFORM:STRING=ios

popd >/dev/null

echo "---------"
echo "Finished!"
echo "---------"
