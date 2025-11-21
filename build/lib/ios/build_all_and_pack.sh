#!/bin/zsh
set -e -u -o pipefail
rm -rf XCodeProj
mkdir -p XCodeProj
pushd "XCodeProj" >/dev/null

BUILD_LIB_TYPES=(dynamic_lib static_lib)
APPLE_LIB_FORMATS=(framework framework)
BUILD_CONFIGS=(Debug MinSizeRel Release RelWithDebInfo)
TARGET_PLATFORMS=(OS64 SIMULATOR64COMBINED TVOS SIMULATORARM64_TVOS VISIONOS SIMULATOR_VISIONOS WATCHOS SIMULATOR_WATCHOSCOMBINED)

rm -rf "../../../artifacts"
rm -rf "../../../install"

for (( i=1; i<=${#BUILD_LIB_TYPES[@]}; i++ )); do
  BUILD_LIB_TYPE=${BUILD_LIB_TYPES[i]}
  APPLE_LIB_FORMAT=${APPLE_LIB_FORMATS[i]}
  rm -rf "../../../../install/${BUILD_LIB_TYPE}"
  for TARGET_PLATFORM in ${TARGET_PLATFORMS[@]}; do
      echo "== Configure/Build for PLATFORM=${TARGET_PLATFORM} =="
      rm -f CMakeCache.txt
      rm -rf CMakeFiles *.xcodeproj(N)
      cmake ../../../../src \
            -G Xcode \
            -DCMAKE_TOOLCHAIN_FILE=../ios.toolchain.cmake \
            -DPLATFORM=${TARGET_PLATFORM} \
            -DDEPLOYMENT_TARGET=13.0 \
            -DBUILD_LIB_TYPE=$BUILD_LIB_TYPE \
            -DAPPLE_LIB_FORMAT=$APPLE_LIB_FORMAT \
            -DTARGET_PLATFORM:STRING=ios

      for build_config in ${BUILD_CONFIGS[@]}; do
            cmake --build . --config $build_config
            if [ $? -eq 0 ]; then
                echo "Build succeeded."
            else
                echo "Build failed."
                exit 1
            fi
            cmake --install . --config $build_config
      done
      STAGE_DIR="../../../../install/${BUILD_LIB_TYPE}/stage/${TARGET_PLATFORM}"
      rm -rf ${STAGE_DIR}
      mkdir -p ${STAGE_DIR}
      mv ../../../../install/${BUILD_LIB_TYPE}/lib ${STAGE_DIR}
  done

  for build_config in ${BUILD_CONFIGS[@]}; do
    XCFRAMEWORK_ARGS=()
    for TARGET_PLATFORM in ${TARGET_PLATFORMS[@]}; do
      XCFRAMEWORK_ARGS+=("-framework" "../../../../install/${BUILD_LIB_TYPE}/stage/${TARGET_PLATFORM}/lib/${build_config}/BqLog.framework")
    done
    echo "Creating XCFramework for configuration: ${build_config}:"
    echo "Cmd : xcodebuild -create-xcframework ${XCFRAMEWORK_ARGS[@]} -output ../../../../install/${BUILD_LIB_TYPE}/lib/${build_config}/BqLog.xcframework"
    xcodebuild -create-xcframework "${XCFRAMEWORK_ARGS[@]}" -output "../../../../install/${BUILD_LIB_TYPE}/lib/${build_config}/BqLog.xcframework"
  done
  rm -rf ../../../../install/${BUILD_LIB_TYPE}/stage
done




popd >/dev/null

rm -rf pack
mkdir -p pack
pushd "pack" >/dev/null

cmake ../../../../pack -DTARGET_PLATFORM:STRING=ios -DPACKAGE_NAME:STRING=bqlog-lib
cmake --build . --target package
popd >/dev/null

echo "---------"
echo "Finished!"
echo "---------"