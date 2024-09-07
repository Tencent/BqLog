if ! [ "$ANDROID_NDK_ROOT" ]; then
    echo "Environment Variable ANDROID_NDK_ROOT Is Not Found! Build Cancelled!!"
    exit 1
fi

if ! [ -d "$ANDROID_NDK_ROOT" ]; then
    echo "the folder Environment Variable ANDROID_NDK_ROOT Is Not Exist! Build Cancelled!!"
    echo $ANDROID_NDK_ROOT
    exit 1
fi


NDK_PATH=$ANDROID_NDK_ROOT

BUILD_TARGET=(armeabi-v7a arm64-v8a x86_64)
BUILD_TYPE=(Debug MinSizeRel)
for build_target in ${BUILD_TARGET[@]}
do
    rm -rf $build_target
    mkdir $build_target
    cd $build_target
    for build_type in ${BUILD_TYPE[@]}
    do

        mkdir $build_type
        cd $build_type
        cmake ../../../../../src\
        -DANDROID_ABI=$build_target\
         -DANDROID_PLATFORM=android-19\
         -DANDROID_NDK=$NDK_PATH\
         -DCMAKE_BUILD_TYPE=$build_type\
		 -DBUILD_TYPE=dynamic_lib\
         -DCMAKE_TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake\
         -DTARGET_PLATFORM:STRING=android\
		 -DANDROID_STL=none
		 
        $ANDROID_NDK_ROOT/prebuilt/darwin-x86_64/bin/make VERBOSE=1 -j$(sysctl -n hw.logicalcpu)
        if [ $? -eq 0 ]; then
            echo "Build succeeded."
        else
            echo "Build failed."
            exit 1
        fi
        $ANDROID_NDK_ROOT/prebuilt/darwin-x86_64/bin/make install
        mv -f ../../../../../dist/dynamic_lib/android/$build_target/$build_type/libBqLog.so ../../../../../dist/dynamic_lib/android/$build_target/$build_type/libBqLog_Symbol.so
        $ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip -s ../../../../../dist/dynamic_lib/android/$build_target/$build_type/libBqLog_Symbol.so -o ../../../../../dist/dynamic_lib/android/$build_target/$build_type/libBqLog.so
        cd ..
    done
    cd ..
done


echo "---------"
echo "Finished!"
echo "---------"
