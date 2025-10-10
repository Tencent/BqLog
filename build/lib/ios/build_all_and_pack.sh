#!/bin/zsh
set -e -u -o pipefail
rm -rf XCodeProj
mkdir XCodeProj
pushd "XCodeProj" >/dev/null

BUILD_LIB_TYPES=(dynamic_lib dynamic_lib static_lib static_lib)
APPLE_LIB_FORMATS=(framework dylib framework a)
BUILD_CONFIGS=(Debug MinSizeRel Release RelWithDebInfo)

for (( i=1; i<=${#BUILD_LIB_TYPES[@]}; i++ )); do
    BUILD_LIB_TYPE=${BUILD_LIB_TYPES[i]}
    APPLE_LIB_FORMAT=${APPLE_LIB_FORMATS[i]}

    cmake ../../../../src \
        -G Xcode \
        -DCMAKE_TOOLCHAIN_FILE=../ios.toolchain.cmake \
        -DPLATFORM=OS64 \
        -DDEPLOYMENT_TARGET=9.0 \
        -DBUILD_LIB_TYPE=$BUILD_LIB_TYPE \
        -DAPPLE_LIB_FORMAT=$APPLE_LIB_FORMAT \
        -DTARGET_PLATFORM:STRING=ios

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