#!/usr/bin/env bash
set -euo pipefail

# Usage: ./build_linux.sh <path_to_libBqLog.so> [x86_64|arm64]
BQLOG_LIB="${1:?Usage: $0 <path_to_libBqLog.so> [x86_64|arm64]}"
ARCH="${2:-$(uname -m)}"
case "$ARCH" in
    aarch64|arm64) ARCH="arm64" ;;
    x86_64|amd64) ARCH="x86_64" ;;
esac

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLUGIN_DIR="$(cd "${SCRIPT_DIR}/../../../plugin/godot" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build_linux_${ARCH}"
OUT_DIR="${SCRIPT_DIR}/out/linux/${ARCH}"

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
    -DBQLOG_LIB_PATH="${BQLOG_LIB}" \
    -DBQLOG_GODOT_OUTPUT_NAME=BqLogGodot \
    "${EXTRA_ARGS[@]}"

cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

mkdir -p "${OUT_DIR}"
cp -Lf "${BUILD_DIR}/libBqLogGodot.so" "${OUT_DIR}/libBqLogGodot.so"
echo "Built Linux (${ARCH}) Godot plugin in ${OUT_DIR}"
