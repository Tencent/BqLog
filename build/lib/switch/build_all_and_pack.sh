#!/usr/bin/env bash
set -euo pipefail
# ------------------------------------------------------------
# Nintendo Switch (devkitPro/libnx) static library build + pack driver
# for BqLog.
#
# Usage:
#   build_all_and_pack.sh [ignored...]
#
# Unlike Linux (arch/compiler/java/node/python), the Switch build needs no
# parameters; extra arguments are accepted and ignored so that the CI
# invocation keeps the same shape as the other platforms.
#
# Must be run inside the devkitA64 toolchain container
# (ghcr.io/pippocao/bqlog/debian:switch-devkita64), which provides the
# DEVKITPRO environment variable plus cmake/ninja.
# ------------------------------------------------------------

if [[ -z "${DEVKITPRO:-}" ]]; then
  echo "Environment variable DEVKITPRO is not found! Build cancelled!"
  exit 1
fi

# Only support Linux
uname_s="$(uname -s)"
if [[ "$uname_s" != "Linux" ]]; then
  echo "This script is for Linux only. Current OS: $uname_s"
  exit 1
fi

TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake"
if [[ ! -f "$TOOLCHAIN_FILE" ]]; then
  echo "Toolchain file not found: $TOOLCHAIN_FILE"
  exit 1
fi

# Detect number of CPU cores for parallel build
PARALLEL_JOBS="${PARALLEL_JOBS:-$(command -v nproc >/dev/null 2>&1 && nproc || echo 8)}"

BUILD_CONFIGS=(Debug MinSizeRel RelWithDebInfo Release)

echo "DEVKITPRO: $DEVKITPRO"
echo "Toolchain: $TOOLCHAIN_FILE"
echo "Parallel jobs: $PARALLEL_JOBS"

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
cd "$SCRIPT_DIR"

rm -rf "../../../artifacts"
rm -rf "../../../install"

# Switch supports static_lib only (src/CMakeLists.txt rejects dynamic_lib).
for build_config in "${BUILD_CONFIGS[@]}"; do
  echo "== Configure/Build for CONFIG=${build_config} (static_lib) =="
  rm -rf "cmake_build"
  mkdir -p "cmake_build"
  pushd "cmake_build" >/dev/null
  cmake ../../../../src \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DTARGET_PLATFORM:STRING=switch \
    -DBUILD_LIB_TYPE=static_lib \
    -DCMAKE_BUILD_TYPE="$build_config"

  # Build and install
  cmake --build . -- -j"${PARALLEL_JOBS}"
  cmake --build . --target install
  popd >/dev/null
done

# Package the library
rm -rf pack
mkdir -p pack
pushd "pack" >/dev/null
cmake ../../../../pack -DTARGET_PLATFORM:STRING=switch -DPACKAGE_NAME:STRING=bqlog-lib
cmake --build . --target package
popd >/dev/null

echo "---------"
echo "Finished!"
echo "---------"
