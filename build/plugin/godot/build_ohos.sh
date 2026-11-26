#!/usr/bin/env bash
set -euo pipefail

# Usage: ./build_ohos.sh <ohos_sdk_dir> <bqlog_ohos_lib_dir>
# <bqlog_ohos_lib_dir> must contain <abi>/libBqLog.so for arm64-v8a, armeabi-v7a, x86_64.
OHOS_SDK_DIR="${1:?Usage: $0 <ohos_sdk_dir> <bqlog_ohos_lib_dir>}"
BQLOG_OHOS_DIR="${2:?Usage: $0 <ohos_sdk_dir> <bqlog_ohos_lib_dir>}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLUGIN_DIR="$(cd "${SCRIPT_DIR}/../../../plugin/godot" && pwd)"
NDK_PATH="${OHOS_SDK_DIR}/native"
PARALLEL_JOBS="${PARALLEL_JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)}"

ABIS=("arm64-v8a" "armeabi-v7a" "x86_64")
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

for ABI in "${ABIS[@]}"; do
    BQLOG_SO="${BQLOG_OHOS_DIR}/${ABI}/libBqLog.so"
    if [[ ! -f "${BQLOG_SO}" ]]; then
        echo "ERROR: Missing ${BQLOG_SO}" >&2
        exit 1
    fi
    BUILD_DIR="${SCRIPT_DIR}/build_ohos_${ABI}"
    OUT_DIR="${SCRIPT_DIR}/out/ohos/${ABI}"

    cmake -S "${PLUGIN_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_TOOLCHAIN_FILE="${NDK_PATH}/build/cmake/ohos.toolchain.cmake" \
        -DOHOS_ARCH="${ABI}" \
        -DOHOS_STL=c++_static \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DBQLOG_LIB_PATH="${BQLOG_SO}" \
        -DBQLOG_GODOT_OUTPUT_NAME=BqLogGodot \
        ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}

    cmake --build "${BUILD_DIR}" --parallel "${PARALLEL_JOBS}"

    mkdir -p "${OUT_DIR}"
    cp -Lf "${BUILD_DIR}/libBqLogGodot.so" "${OUT_DIR}/libBqLogGodot.so"
    if [[ -x "${NDK_PATH}/llvm/bin/llvm-strip" ]]; then
        "${NDK_PATH}/llvm/bin/llvm-strip" -s "${OUT_DIR}/libBqLogGodot.so"
    fi
    echo "Built OpenHarmony (${ABI}) Godot plugin in ${OUT_DIR}"
done
