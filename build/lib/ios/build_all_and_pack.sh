#!/bin/zsh

rm -rf XCodeProj
mkdir XCodeProj
pushd "XCodeProj" >/dev/null
cmake ../../../../src \
    -G Xcode \
    -DCMAKE_TOOLCHAIN_FILE=../ios.toolchain.cmake \
    -DPLATFORM=OS64 \
    -DDEPLOYMENT_TARGET=9.0 \
    -DBUILD_LIB_TYPE=dynamic_lib \
    -DAPPLE_LIB_FORMAT=framework \
    -DTARGET_PLATFORM:STRING=ios

BUILD_CONFIGS=(Debug MinSizeRel Release RelWithDebInfo)
for build_config in ${BUILD_CONFIGS[@]}
do
    cmake --build . --config $build_config
    if [ $? -eq 0 ]; then
        echo "Build succeeded."
    else
        echo "Build failed."
        exit 1
    fi
    cmake --install . --config $build_config
done

cmake ../../../../src \
    -G Xcode \
    -DCMAKE_TOOLCHAIN_FILE=../ios.toolchain.cmake \
    -DPLATFORM=OS64 \
    -DDEPLOYMENT_TARGET=9.0 \
    -DBUILD_LIB_TYPE=static_lib \
    -DAPPLE_LIB_FORMAT=a \
    -DTARGET_PLATFORM:STRING=ios

BUILD_CONFIGS=(Debug MinSizeRel Release RelWithDebInfo)
for build_config in ${BUILD_CONFIGS[@]}
do
    cmake --build . --config $build_config
    if [ $? -eq 0 ]; then
        echo "Build succeeded."
    else
        echo "Build failed."
        exit 1
    fi
    cmake --install . --config $build_config
done

cmake ../../../../src \
    -G Xcode \
    -DCMAKE_TOOLCHAIN_FILE=../ios.toolchain.cmake \
    -DPLATFORM=OS64 \
    -DDEPLOYMENT_TARGET=9.0 \
    -DBUILD_LIB_TYPE=dynamic_lib \
    -DAPPLE_LIB_FORMAT=dylib \
    -DTARGET_PLATFORM:STRING=ios

BUILD_CONFIGS=(Debug MinSizeRel Release RelWithDebInfo)
for build_config in ${BUILD_CONFIGS[@]}
do
    cmake --build . --config $build_config
    if [ $? -eq 0 ]; then
        echo "Build succeeded."
    else
        echo "Build failed."
        exit 1
    fi
    cmake --install . --config $build_config
done

popd >/dev/null

rm -rf pack
mkdir pack
pushd "pack" >/dev/null

cmake ../../../../pack -DTARGET_PLATFORM:STRING=ios -DPACKAGE_NAME:STRING=bqlog-lib
cmake --build . --target package
popd >/dev/null

echo "---------"
echo "Finished!"
echo "---------"
