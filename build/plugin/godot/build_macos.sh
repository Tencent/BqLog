#!/usr/bin/env bash
set -euo pipefail

# Usage: ./build_macos.sh <path_to_universal_libBqLog.dylib>
BQLOG_LIB="${1:?Usage: $0 <path_to_universal_libBqLog.dylib>}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLUGIN_DIR="$(cd "${SCRIPT_DIR}/../../../plugin/godot" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build_macos_universal"
OUT_DIR="${SCRIPT_DIR}/out/macos"

EXTRA_ARGS=()
if [[ -n "${GODOT_CPP_DIR:-}" ]]; then
    EXTRA_ARGS+=("-DGODOT_CPP_DIR=${GODOT_CPP_DIR}")
fi
if [[ -n "${BQLOG_GODOT_CMAKE_ARGS:-}" ]]; then
    # shellcheck disable=SC2206
    EXTRA_ARGS+=(${BQLOG_GODOT_CMAKE_ARGS})
fi

cmake -S "${PLUGIN_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DBQLOG_LIB_PATH="${BQLOG_LIB}" \
    -DBQLOG_GODOT_OUTPUT_NAME=BqLogGodot.macos.universal \
    "${EXTRA_ARGS[@]}"

cmake --build "${BUILD_DIR}" --parallel "$(sysctl -n hw.ncpu)"

mkdir -p "${OUT_DIR}"
cp -Lf "${BUILD_DIR}/libBqLogGodot.macos.universal.dylib" "${OUT_DIR}/libBqLogGodot.macos.universal.dylib"

# Normalize install_name of linked libBqLog (e.g. @rpath/libBqLog.2.dylib) to @loader_path/libBqLog.dylib.
# Exclude the binary's own ID line (libBqLogGodot.*) and re-sign ad-hoc for Apple Silicon.
BQLOG_DEP="$(otool -L "${OUT_DIR}/libBqLogGodot.macos.universal.dylib" | awk '!/libBqLogGodot/ && /libBqLog/{print $1; exit}')"
if [[ -n "${BQLOG_DEP}" && "${BQLOG_DEP}" != "@loader_path/libBqLog.dylib" ]]; then
    install_name_tool -change "${BQLOG_DEP}" "@loader_path/libBqLog.dylib" "${OUT_DIR}/libBqLogGodot.macos.universal.dylib"
    codesign --force --sign - "${OUT_DIR}/libBqLogGodot.macos.universal.dylib"
fi

echo "Built macOS universal Godot plugin in ${OUT_DIR}"
