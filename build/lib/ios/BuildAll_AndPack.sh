#!/bin/zsh

rm -rf XCodeProj
mkdir XCodeProj
pushd "XCodeProj" >/dev/null
cmake ../../../../src \
    -G Xcode \
    -DCMAKE_TOOLCHAIN_FILE=../ios.toolchain.cmake \
    -DPLATFORM=OS64 \
    -DDEPLOYMENT_TARGET=9.0 \
    -DBUILD_TYPE=dynamic_lib \
    -DAPPLE_LIB_FORMAT=framework \
    -DTARGET_PLATFORM:STRING=ios

BUILD_TYPE=(Debug MinSizeRel Release RelWithDebInfo)
for build_type in ${BUILD_TYPE[@]}
do
    cmake --build . --config $build_type
    if [ $? -eq 0 ]; then
        echo "Build succeeded."
    else
        echo "Build failed."
        exit 1
    fi
    cmake --install . --config $build_type

    if [ -d "../../../../artifacts/dynamic_lib/ios/${build_type}/BqLog.framework.dSYM" ]; then
        rm -rf "../../../../dist/dynamic_lib/ios/${build_type}/BqLog.framework.dSYM"
        cp -r "../../../../artifacts/dynamic_lib/ios/${build_type}/BqLog.framework.dSYM" "../../../../dist/dynamic_lib/ios/${build_type}/BqLog.framework.dSYM"
    fi
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
