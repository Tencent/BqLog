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
      FRAMEWORK_PATH="../../../../install/${BUILD_LIB_TYPE}/stage/${TARGET_PLATFORM}/lib/${build_config}/BqLog.framework"
      if [ ! -d "${FRAMEWORK_PATH}" ]; then
        # It may be that some platforms were not built for this config, which is acceptable.
        continue
      fi
      XCFRAMEWORK_ARGS+=("-framework" "${FRAMEWORK_PATH}")

      # Deduce SDK name from platform to find the dSYM path inside the build directory (XCodeProj)
      SDK_NAME=""
      case ${TARGET_PLATFORM} in
        OS64) SDK_NAME="iphoneos" ;;
        SIMULATOR64COMBINED) SDK_NAME="iphonesimulator" ;;
        TVOS) SDK_NAME="appletvos" ;;
        SIMULATORARM64_TVOS) SDK_NAME="appletvsimulator" ;;
        VISIONOS) SDK_NAME="xros" ;;
        SIMULATOR_VISIONOS) SDK_NAME="xrsimulator" ;;
        WATCHOS) SDK_NAME="watchos" ;;
        SIMULATOR_WATCHOSCOMBINED) SDK_NAME="watchsimulator" ;;
      esac

      if [[ -n "${SDK_NAME}" ]]; then
        DSYM_PATH="${FRAMEWORK_PATH}.dSYM"
        if [[ -d "${DSYM_PATH}" ]]; then
          DSYM_PATH_ABS=${DSYM_PATH:A}
          echo "Found dSYM at ${DSYM_PATH} -> ${DSYM_PATH_ABS}"
          XCFRAMEWORK_ARGS+=("-debug-symbols" "${DSYM_PATH_ABS}")
        else
          # Don't warn for Release/MinSizeRel as they don't produce dSYMs
          if [[ "${build_config}" != "Release" && "${build_config}" != "MinSizeRel" ]]; then
            echo "Warning: dSYM not found at ${DSYM_PATH}"
          fi
        fi
      fi
    done

    if [ ${#XCFRAMEWORK_ARGS[@]} -eq 0 ]; then
        echo "No frameworks found for configuration: ${build_config}. Skipping."
        continue
    fi

    OUTPUT_PATH="../../../../install/${BUILD_LIB_TYPE}/lib/${build_config}/BqLog.xcframework"
    rm -rf "${OUTPUT_PATH}"
    echo "Creating XCFramework for configuration: ${build_config}:"
    xcodebuild -create-xcframework "${XCFRAMEWORK_ARGS[@]}" -output "${OUTPUT_PATH}"
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