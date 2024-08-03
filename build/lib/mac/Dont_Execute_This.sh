BUILD_TYPE=$1
APPLE_FORMAT=$2

CONFIG_TYPE=(Debug MinSizeRel RelWithDebInfo Release)
for config_type in ${CONFIG_TYPE[@]}
do
    rm -rf XCodeProj_${BUILD_TYPE}_${APPLE_FORMAT}
    mkdir XCodeProj_${BUILD_TYPE}_${APPLE_FORMAT}
    cd XCodeProj_${BUILD_TYPE}_${APPLE_FORMAT}

    cmake -DTARGET_PLATFORM:STRING=mac -DJAVA_SUPPORT=ON -DBUILD_TYPE:STRING=$BUILD_TYPE -DAPPLE_LIB_FORMAT:STRING=$APPLE_FORMAT ../../../../src -G "Xcode"
    xcodebuild -project BqLog.xcodeproj -scheme ALL_BUILD -configuration $config_type
    cmake --install . --config $config_type
    cd ..
done

echo "---------"
echo "Finished!"
echo "---------"
