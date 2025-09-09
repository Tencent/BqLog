#!/bin/bash
set -euo pipefail

BUILD_TYPE=$1
COMPILER_TYPE_C=$2
COMPILER_TYPE_CXX=$3

CONFIG_TYPE=(Debug MinSizeRel RelWithDebInfo Release)
for i in "${!CONFIG_TYPE[@]}"; do
    config_type="${CONFIG_TYPE[$i]}"
    rm -rf $BUILD_TYPE/$config_type
    mkdir $BUILD_TYPE/$config_type
    pushd "$BUILD_TYPE/$config_type" >/dev/null
    CC=$COMPILER_TYPE_C CXX=$COMPILER_TYPE_CXX cmake ../../../../../src \
    -DJAVA_SUPPORT=ON \
    -DTARGET_PLATFORM:STRING=linux \
	  -DCMAKE_BUILD_TYPE=$config_type \
	  -DBUILD_TYPE=$BUILD_TYPE
      
	cmake --build . --parallel
    cmake --build . --target install
    popd >/dev/null
done

echo "---------"
echo "Finished!"
echo "---------"
