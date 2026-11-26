#!/usr/bin/env bash
set -euo pipefail

# Usage: ./build_android.sh <android_ndk_root> <bqlog_jni_dir>
# <bqlog_jni_dir> must contain <abi>/libBqLog.so for arm64-v8a, armeabi-v7a, x86_64, x86.
NDK_ROOT="${1:?Usage: $0 <android_ndk_root> <bqlog_jni_dir>}"
BQLOG_JNI_DIR="${2:?Usage: $0 <android_ndk_root> <bqlog_jni_dir>}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLUGIN_DIR="$(cd "${SCRIPT_DIR}/../../../plugin/godot" && pwd)"

ABIS=("arm64-v8a" "armeabi-v7a" "x86_64" "x86")
EXTRA_ARGS=()
if [[ -n "${GODOT_CPP_DIR:-}" ]]; then
    EXTRA_ARGS+=("-DGODOT_CPP_DIR=${GODOT_CPP_DIR}")
fi

for ABI in "${ABIS[@]}"; do
    BQLOG_SO="${BQLOG_JNI_DIR}/${ABI}/libBqLog.so"
    if [[ ! -f "${BQLOG_SO}" ]]; then
        echo "ERROR: Missing ${BQLOG_SO}" >&2
        exit 1
    fi
    BUILD_DIR="${SCRIPT_DIR}/build_android_${ABI}"
    OUT_DIR="${SCRIPT_DIR}/out/android/${ABI}"

    cmake -S "${PLUGIN_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_TOOLCHAIN_FILE="${NDK_ROOT}/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="${ABI}" \
        -DANDROID_PLATFORM=android-21 \
        -DANDROID_STL=c++_shared \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DBQLOG_LIB_PATH="${BQLOG_SO}" \
        -DBQLOG_GODOT_OUTPUT_NAME=BqLogGodot \
        ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}

    cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

    mkdir -p "${OUT_DIR}"
    cp -Lf "${BUILD_DIR}/libBqLogGodot.so" "${OUT_DIR}/libBqLogGodot.so"
    echo "Built Android (${ABI}) Godot plugin in ${OUT_DIR}"
done
