#!/bin/bash
# Nintendo Switch (devkitPro/libnx) unit-test build for BqLog.
#
# Usage (mirrors build/test/linux/run_test_gcc.sh):
#   ./run_test.sh [cpp_ver]        # default: 17
#
# Must be run inside the devkitA64 toolchain container
# (ghcr.io/pippocao/bqlog/debian:switch-devkita64), which provides the
# DEVKITPRO environment variable plus cmake/ninja.
#
# The resulting BqLogUnitTest.elf only runs on real Switch hardware (or an
# emulator), so this script stops at compile+link: a successfully linked ELF
# is the CI pass criterion. A homebrew .nro wrapper is additionally produced
# via nacptool+elf2nro when those tools are available (bonus, non-fatal).
set -e

if [ -z "${DEVKITPRO:-}" ]; then
    echo "Environment variable DEVKITPRO is not found! Build cancelled!"
    exit 1
fi

TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake"
if [ ! -f "$TOOLCHAIN_FILE" ]; then
    echo "Toolchain file not found: $TOOLCHAIN_FILE"
    exit 1
fi

# nacptool/elf2nro (optional .nro packaging) live here.
if [ -d "$DEVKITPRO/tools/bin" ]; then
    export PATH="$DEVKITPRO/tools/bin:$PATH"
fi

CPP_VER_PARAM=${1:-17}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
cd "$SCRIPT_DIR"
mkdir -p CMakeFiles
cd CMakeFiles

# devkitPro's Switch.cmake sets CMAKE_EXECUTABLE_SUFFIX to ".elf".
verify_elf() {
    local cfg="$1"
    if [ ! -f BqLogUnitTest.elf ]; then
        echo "Test failed: BqLogUnitTest.elf was not produced for $cfg."
        exit 1
    fi
    echo "Test succeeded: $cfg ELF linked: $(pwd)/BqLogUnitTest.elf"
    echo "SKIP-RUN: switch binaries require real hardware; compile+link is the CI pass criterion"
}

# Bonus: wrap the ELF as a homebrew .nro (non-fatal if it cannot be made).
make_nro() {
    if command -v nacptool >/dev/null 2>&1 && command -v elf2nro >/dev/null 2>&1; then
        if nacptool --create "BqLogUnitTest" "BqLog" "1.0.0" BqLogUnitTest.nacp \
            && elf2nro BqLogUnitTest.elf BqLogUnitTest.nro --nacp=BqLogUnitTest.nacp; then
            echo "NRO produced: $(pwd)/BqLogUnitTest.nro"
        else
            echo "nacptool/elf2nro failed; skipping .nro packaging (non-fatal)."
        fi
    else
        echo "nacptool/elf2nro not found; skipping .nro packaging (non-fatal)."
    fi
}

# 1) Debug
cmake -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DTARGET_PLATFORM:STRING=switch \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCPP_VER="$CPP_VER_PARAM" \
    ../../../../test/cpp
cmake --build . -- -j"$(nproc)"
verify_elf Debug
make_nro

# 2) RelWithDebInfo
cmake -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DTARGET_PLATFORM:STRING=switch \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCPP_VER="$CPP_VER_PARAM" \
    ../../../../test/cpp
cmake --build . -- -j"$(nproc)"
verify_elf RelWithDebInfo
make_nro

cd ..
echo "Test succeeded."
