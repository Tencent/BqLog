#!/usr/bin/env bash
set -euo pipefail

# Usage: ./build_ios.sh <path_to_BqLog.xcframework>
BQLOG_XCFW="${1:?Usage: $0 <path_to_BqLog.xcframework>}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
PLUGIN_DIR="${REPO_ROOT}/plugin/godot"
TOOLCHAIN="${REPO_ROOT}/build/lib/ios/ios.toolchain.cmake"
OUT_DIR="${SCRIPT_DIR}/out/ios"

if [[ -z "${GODOT_CPP_DIR:-}" ]]; then
    GODOT_CPP_TAG="${GODOT_CPP_GIT_TAG:-godot-4.2-stable}"
    GODOT_CPP_CACHE="${SCRIPT_DIR}/_godot_cpp_${GODOT_CPP_TAG}"
    if [[ ! -f "${GODOT_CPP_CACHE}/CMakeLists.txt" ]]; then
        rm -rf "${GODOT_CPP_CACHE}"
        mkdir -p "${GODOT_CPP_CACHE}"
        curl -fsSL --retry 5 --retry-delay 3 \
            "https://github.com/godotengine/godot-cpp/archive/refs/tags/${GODOT_CPP_TAG}.tar.gz" \
            | tar -xz --strip-components=1 -C "${GODOT_CPP_CACHE}"
    fi
    GODOT_CPP_DIR="${GODOT_CPP_CACHE}"
fi
EXTRA_ARGS=("-DGODOT_CPP_DIR=${GODOT_CPP_DIR}")

build_slice() {
    local PLATFORM="$1"
    local SLICE_DIR="$2"
    local BQLOG_FW="${BQLOG_XCFW}/${SLICE_DIR}/BqLog.framework"
    if [[ ! -d "${BQLOG_FW}" ]]; then
        echo "ERROR: Missing BqLog slice: ${BQLOG_FW}" >&2
        exit 1
    fi
    local BUILD_DIR="${SCRIPT_DIR}/build_ios_${PLATFORM}"
    cmake -S "${PLUGIN_DIR}" -B "${BUILD_DIR}" \
        -G Xcode \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
        -DPLATFORM="${PLATFORM}" \
        -DDEPLOYMENT_TARGET=12.0 \
        -DBQLOG_LIB_PATH="${BQLOG_FW}/BqLog" \
        -DBQLOG_GODOT_OUTPUT_NAME=BqLogGodot \
        ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}

    cmake --build "${BUILD_DIR}" --config RelWithDebInfo --parallel "$(sysctl -n hw.ncpu)"
}

build_slice "OS64" "ios-arm64"
build_slice "SIMULATOR64COMBINED" "ios-arm64_x86_64-simulator"

mkdir -p "${OUT_DIR}"
rm -rf "${OUT_DIR}/BqLogGodot.xcframework"
xcodebuild -create-xcframework \
    -framework "${SCRIPT_DIR}/build_ios_OS64/RelWithDebInfo-iphoneos/BqLogGodot.framework" \
    -framework "${SCRIPT_DIR}/build_ios_SIMULATOR64COMBINED/RelWithDebInfo-iphonesimulator/BqLogGodot.framework" \
    -output "${OUT_DIR}/BqLogGodot.xcframework"

echo "Built iOS BqLogGodot.xcframework in ${OUT_DIR}"
