#!/bin/bash

BUILD_TYPE=$1
COMPILER_TYPE_C=$2
COMPILER_TYPE_CXX=$3

CONFIG_TYPE=(Debug MinSizeRel RelWithDebInfo Release)
for config_type in ${CONFIG_TYPE[@]}
do
    rm -rf $BUILD_TYPE/$config_type
	mkdir $BUILD_TYPE
    mkdir $BUILD_TYPE/$config_type
    cd $BUILD_TYPE/$config_type
    CC=$COMPILER_TYPE_C CXX=$COMPILER_TYPE_CXX cmake ../../../../../src \
    -DJAVA_SUPPORT=ON \
    -DTARGET_PLATFORM:STRING=linux \
	-DCMAKE_BUILD_TYPE=$config_type \
	-DBUILD_TYPE=$BUILD_TYPE
	make;
    if [ $? -eq 0 ]; then
        echo "Build succeeded."
    else
        echo "Build failed."
        exit 1
    fi
	make install;
    cd ../..
done

echo "---------"
echo "Finished!"
echo "---------"
