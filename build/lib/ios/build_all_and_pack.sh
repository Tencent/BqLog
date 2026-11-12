#!/bin/zsh
set -e -u -o pipefail
rm -rf XCodeProj
mkdir -p XCodeProj
pushd "XCodeProj" >/dev/null

REPO_ROOT="${PWD:A}/../../../.."
REPO_ROOT="${REPO_ROOT:A}"

BUILD_LIB_TYPES=(dynamic_lib static_lib)
APPLE_LIB_FORMATS=(framework framework)
BUILD_CONFIGS=(Debug MinSizeRel Release RelWithDebInfo)
TARGET_PLATFORMS=(OS64 SIMULATOR64COMBINED TVOS SIMULATORARM64_TVOS VISIONOS SIMULATOR_VISIONOS WATCHOS SIMULATOR_WATCHOSCOMBINED)

typeset -A SLICE_ID=(
  OS64                      ios-arm64
  SIMULATOR64COMBINED       ios-arm64_x86_64-simulator
  TVOS                      tvos-arm64
  SIMULATORARM64_TVOS       tvos-arm64-simulator
  VISIONOS                  xros-arm64
  SIMULATOR_VISIONOS        xros-arm64-simulator
  WATCHOS                   watchos-arm64_32_armv7k
  SIMULATOR_WATCHOSCOMBINED watchos-arm64_x86_64-simulator
)

rm -rf "${REPO_ROOT}/artifacts"

for (( i=1; i<=${#BUILD_LIB_TYPES[@]}; i++ )); do
  BUILD_LIB_TYPE=${BUILD_LIB_TYPES[i]}
  APPLE_LIB_FORMAT=${APPLE_LIB_FORMATS[i]}
  rm -rf "${REPO_ROOT}/install/${BUILD_LIB_TYPE}"
  for TARGET_PLATFORM in ${TARGET_PLATFORMS[@]}; do
      echo "== Configure/Build for PLATFORM=${TARGET_PLATFORM} =="
      rm -f CMakeCache.txt
      rm -rf CMakeFiles *.xcodeproj(N)
      # dsymutil keeps the files of the previous platform in an existing .dSYM
      rm -rf "${REPO_ROOT}/artifacts/${BUILD_LIB_TYPE}"
      case ${TARGET_PLATFORM} in
        VISIONOS|SIMULATOR_VISIONOS) DEPLOYMENT_TARGET_ARG=() ;;
        *) DEPLOYMENT_TARGET_ARG=(-DDEPLOYMENT_TARGET=9.0) ;;
      esac
      # CMAKE_INSTALL_* reproduces the prebuilt tarball shape, install/<libtype>/lib/<config>;
      # the CMakeLists itself only supplies the standard GNUInstallDirs defaults.
      cmake ../../../../src \
            -G Xcode \
            -DCMAKE_TOOLCHAIN_FILE=../ios.toolchain.cmake \
            -DPLATFORM=${TARGET_PLATFORM} \
            "${DEPLOYMENT_TARGET_ARG[@]}" \
            -DCPP_VER=14 \
            -DBUILD_LIB_TYPE=$BUILD_LIB_TYPE \
            -DAPPLE_LIB_FORMAT=$APPLE_LIB_FORMAT \
            -DTARGET_PLATFORM:STRING=ios \
            -DCMAKE_INSTALL_PREFIX=../../../../install/$BUILD_LIB_TYPE \
            -DCMAKE_INSTALL_LIBDIR='lib/$<CONFIG>' \
            -DCMAKE_INSTALL_DOCDIR=..

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
    OUT_DIR="../../../../install/${BUILD_LIB_TYPE}/lib/${build_config}"
    DSYM_OUT="${OUT_DIR}/dSYMs"
    rm -rf "${DSYM_OUT}"
    for TARGET_PLATFORM in ${TARGET_PLATFORMS[@]}; do
      FRAMEWORK_PATH="../../../../install/${BUILD_LIB_TYPE}/stage/${TARGET_PLATFORM}/lib/${build_config}/BqLog.framework"
      if [ ! -d "${FRAMEWORK_PATH}" ]; then
        # It may be that some platforms were not built for this config, which is acceptable.
        continue
      fi
      # No -debug-symbols: Xcode fails the build when a declared DebugSymbolsPath is missing
      XCFRAMEWORK_ARGS+=("-framework" "${FRAMEWORK_PATH}")

      [[ "${BUILD_LIB_TYPE}" == "dynamic_lib" ]] || continue
      DSYM_PATH="${FRAMEWORK_PATH}.dSYM"
      DWARF_FILES=( "${DSYM_PATH}"/Contents/Resources/DWARF/*(N) )
      if (( ${#DWARF_FILES} )); then
        mkdir -p "${DSYM_OUT}/${SLICE_ID[$TARGET_PLATFORM]}"
        ditto "${DSYM_PATH}" "${DSYM_OUT}/${SLICE_ID[$TARGET_PLATFORM]}/BqLog.framework.dSYM"
      elif [[ "${build_config}" == "Debug" || "${build_config}" == "RelWithDebInfo" ]]; then
        echo "ERROR: dSYM missing or empty for ${TARGET_PLATFORM}/${build_config}: ${DSYM_PATH}"
        exit 1
      fi
    done

    if [ ${#XCFRAMEWORK_ARGS[@]} -eq 0 ]; then
        echo "No frameworks found for configuration: ${build_config}. Skipping."
        continue
    fi

    OUTPUT_PATH="${OUT_DIR}/BqLog.xcframework"
    rm -rf "${OUTPUT_PATH}"
    echo "Creating XCFramework for configuration: ${build_config}:"
    xcodebuild -create-xcframework "${XCFRAMEWORK_ARGS[@]}" -output "${OUTPUT_PATH}"

    if [[ -d "${DSYM_OUT}" ]]; then
      ( cd "${DSYM_OUT}" && for d in */BqLog.framework.dSYM; do dwarfdump --uuid "$d"; done > UUIDs.txt )
    fi
  done
  rm -rf ../../../../install/${BUILD_LIB_TYPE}/stage
done

python3 "${REPO_ROOT}/.github/scripts/check_apple_symbols.py" ios-libs "${REPO_ROOT}/install"



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
