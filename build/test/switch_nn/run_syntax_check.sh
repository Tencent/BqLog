#!/bin/bash
# Nintendo Switch (official Nintendo SDK, nn::) syntax check for BqLog.
#
# The official SDK is NDA-only, so this harness cannot build against it here.
# Instead it cross-compiles every BqLog translation unit with devkitA64 GCC
# (aarch64, like the Switch) at -D__NX__ against the PUBLIC open-ead/nnheaders
# declarations (pinned commit below), which mirror the nn:: SDK signatures.
# This validates syntax/types only, against public nnheaders -- NOT the real
# NDA SDK -- and links nothing (SDK libraries are unavailable).
#
# Must be run inside the devkitpro/devkita64 container, e.g.:
#   docker run --rm -v <repo>:/src -w /src devkitpro/devkita64 \
#       bash build/test/switch_nn/run_syntax_check.sh
#
# Env knobs:
#   BQ_NNHEADERS_DIR  where to clone/cache nnheaders (default: /tmp/bqlog_nnheaders_<commit>)
#   CXX               cross compiler (default: aarch64-none-elf-g++)

set -uo pipefail

NNHEADERS_COMMIT=2556add5202f25754c7350f03bfd4dd29f4e4a2b
NNHEADERS_DIR="${BQ_NNHEADERS_DIR:-/tmp/bqlog_nnheaders_${NNHEADERS_COMMIT}}"
CXX="${CXX:-aarch64-none-elf-g++}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

if ! command -v "$CXX" >/dev/null 2>&1; then
    export PATH="/opt/devkitpro/devkitA64/bin:$PATH"
fi
if ! command -v "$CXX" >/dev/null 2>&1; then
    echo "ERROR: $CXX not found; run inside the devkitpro/devkita64 container." >&2
    exit 1
fi

# 1) clone nnheaders at the pinned commit (cached between runs)
if [ ! -d "$NNHEADERS_DIR/include" ]; then
    echo "Cloning open-ead/nnheaders@$NNHEADERS_COMMIT ..."
    rm -rf "$NNHEADERS_DIR" "$NNHEADERS_DIR.tmp"
    clone_ok=0
    for attempt in 1 2 3; do
        if git clone --quiet https://github.com/open-ead/nnheaders "$NNHEADERS_DIR.tmp"; then
            clone_ok=1
            break
        fi
        echo "clone attempt $attempt failed, retrying ..."
        rm -rf "$NNHEADERS_DIR.tmp"
        sleep 2
    done
    if [ "$clone_ok" -ne 1 ]; then
        echo "ERROR: failed to clone nnheaders (network access required)." >&2
        exit 1
    fi
    git -C "$NNHEADERS_DIR.tmp" checkout --quiet "$NNHEADERS_COMMIT" || {
        echo "ERROR: failed to checkout nnheaders commit $NNHEADERS_COMMIT." >&2
        rm -rf "$NNHEADERS_DIR.tmp"
        exit 1
    }
    mv "$NNHEADERS_DIR.tmp" "$NNHEADERS_DIR"
fi
echo "nnheaders: $NNHEADERS_DIR ($(git -C "$NNHEADERS_DIR" rev-parse HEAD))"

# 2) flags: mirror devkitPro's arch settings and BqLog's strict warnings.
#    nnheaders is included with -isystem so its own warnings are not promoted
#    to errors; BqLog code itself gets the full project warning set.
ARCH_FLAGS="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -ftls-model=local-exec -ffunction-sections -fdata-sections"
COMMON_FLAGS="-std=c++17 -fno-exceptions -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden -D__NX__ -isystem $NNHEADERS_DIR/include"
SRC_WARN_FLAGS="-Wall -Wextra -pedantic -Werror -Wundef -Wshadow -Wstrict-aliasing -Wcast-qual -Wconversion -Wsign-conversion -Wnull-dereference -Wformat=2 -Wuninitialized -Wlogical-op -Wduplicated-branches -Warray-bounds"
TEST_WARN_FLAGS="-Wall -Wextra -pedantic -Werror -Wundef -Wshadow -Wstrict-aliasing -Wcast-qual -Wconversion -Wsign-conversion -Wnull-dereference -Wuninitialized -Wlogical-op -Wduplicated-branches -Warray-bounds"

# Include roots mirror COLLECT_INCLUDE_DIRS in CMake_utils.txt: the source
# tree roots and the include/ root only (never leaf dirs, so libc headers
# like <assert.h> are not shadowed by BqLog's own headers).
SRC_INCLUDES=("-I$REPO_ROOT/src" "-I$REPO_ROOT/include" "-I$REPO_ROOT/thirdparty")
TEST_INCLUDES=("${SRC_INCLUDES[@]}" "-I$REPO_ROOT/test/cpp")

total=0
failed=0
failed_files=()

compile_dir() {
    local src_dir="$1"
    shift
    local extra_flags=("$@")
    local f
    while IFS= read -r f; do
        total=$((total + 1))
        if ! "$CXX" $ARCH_FLAGS $COMMON_FLAGS "${extra_flags[@]}" -c "$f" -o /dev/null; then
            failed=$((failed + 1))
            failed_files+=("$f")
        fi
    done < <(find "$src_dir" -name '*.cpp' | sort)
}

echo "Compiling src/**/*.cpp (-D__NX__) ..."
compile_dir "$REPO_ROOT/src" "${SRC_INCLUDES[@]}" $SRC_WARN_FLAGS

echo "Compiling test/cpp/**/*.cpp (-D__NX__ -DBQ_UNIT_TEST) ..."
compile_dir "$REPO_ROOT/test/cpp" "${TEST_INCLUDES[@]}" -DBQ_UNIT_TEST $TEST_WARN_FLAGS

echo "----------------------------------------------------------------"
if [ "$failed" -ne 0 ]; then
    echo "SYNTAX CHECK FAILED: $failed of $total translation units failed:"
    printf '  %s\n' "${failed_files[@]}"
    exit 1
fi
echo "SYNTAX CHECK PASSED: all $total translation units compiled (nnheaders@$NNHEADERS_COMMIT)."
