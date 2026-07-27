#!/bin/bash
# PS5 (Prospero) unit-test build for BqLog.
#
# Usage (mirrors build/test/linux/run_test_gcc.sh):
#   ./run_test.sh [cpp_ver]        # default: 17
#
# Must be run inside the PS5 toolchain container
# (ghcr.io/pippocao/bqlog/debian:ps5-sdk), which provides the
# PS5_PAYLOAD_SDK environment variable plus clang-18/cmake/ninja.
#
# The resulting BqLogUnitTest ELF only runs on real PS5 hardware, so this
# script stops at compile+link: a successfully linked ELF is the CI pass
# criterion.
set -e

if [ -z "${PS5_PAYLOAD_SDK:-}" ]; then
    echo "Environment variable PS5_PAYLOAD_SDK is not found! Build cancelled!"
    exit 1
fi

TOOLCHAIN_FILE="$PS5_PAYLOAD_SDK/toolchain/prospero.cmake"
if [ ! -f "$TOOLCHAIN_FILE" ]; then
    echo "Toolchain file not found: $TOOLCHAIN_FILE"
    exit 1
fi

CPP_VER_PARAM=${1:-17}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
cd "$SCRIPT_DIR"
mkdir -p CMakeFiles
cd CMakeFiles

# The Prospero toolchain produces a plain ELF named BqLogUnitTest.
verify_elf() {
    local cfg="$1"
    local elf=""
    for candidate in BqLogUnitTest BqLogUnitTest.elf; do
        if [ -f "$candidate" ]; then
            elf="$candidate"
            break
        fi
    done
    if [ -z "$elf" ]; then
        echo "Test failed: BqLogUnitTest ELF was not produced for $cfg."
        exit 1
    fi
    echo "Test succeeded: $cfg ELF linked: $(pwd)/$elf"
    echo "SKIP-RUN: ps5 binaries require real hardware; compile+link is the CI pass criterion"
}

# 1) Debug
cmake -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DTARGET_PLATFORM:STRING=ps5 \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCPP_VER="$CPP_VER_PARAM" \
    ../../../../test/cpp
cmake --build . -- -j"$(nproc)"
verify_elf Debug

# 2) RelWithDebInfo
cmake -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DTARGET_PLATFORM:STRING=ps5 \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCPP_VER="$CPP_VER_PARAM" \
    ../../../../test/cpp
cmake --build . -- -j"$(nproc)"
verify_elf RelWithDebInfo

cd ..
echo "Test succeeded."
