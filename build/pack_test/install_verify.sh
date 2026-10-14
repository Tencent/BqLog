#!/usr/bin/env bash
# Copyright (C) 2026 Tencent.
#
# BQLOG is licensed under the Apache License, Version 2.0.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#
# Verifies that a standard install is consumable, which is the layout a package manager
# produces: find_package(BqLog) and pkg-config must each locate the headers and the library,
# and a program built either way must run.
#
# Usage: bash install_verify.sh [platform]     (default: mac on macOS, linux elsewhere)
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

case "$(uname -s)" in
    Darwin) DEFAULT_PLATFORM="mac" ;;
    *)      DEFAULT_PLATFORM="linux" ;;
esac
PLATFORM="${1:-$DEFAULT_PLATFORM}"
CXX="${CXX:-c++}"
GENERATOR="${GENERATOR:-Ninja}"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

PREFIX="$WORK/prefix"
echo "install_verify: platform=${PLATFORM} generator=${GENERATOR} prefix=${PREFIX}"

# 1. Install the distribution the way a package manager does: standard layout, both forms.
cmake -S "$REPO_ROOT" -B "$WORK/build" -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DTARGET_PLATFORM="$PLATFORM" \
    -DBUILD_LIB_TYPE=both \
    -DBQ_WERROR=OFF \
    -DBQ_BUILD_TOOLS=OFF \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build "$WORK/build" --parallel
cmake --install "$WORK/build"

# The C++20 module interface unit ships with the headers and has no prebuilt BMI.
test -f "$PREFIX/include/bq_log/bq.log.cppm" \
    || { echo "install_verify: bq.log.cppm is missing from the install" >&2; exit 1; }

cat > "$WORK/consumer.cpp" <<'CPP'
#include <bq_log/bq_log.h>

int main() {
    bq::log log = bq::log::create_log("install_verify",
        "appenders_config.ConsoleAppender.type=console\n"
        "appenders_config.ConsoleAppender.time_zone=localtime\n"
        "appenders_config.ConsoleAppender.levels=[all]\n"
        "log.thread_mode=sync");
    if (!log.is_valid()) {
        return 1;
    }
    log.info("bqlog installed and linked");
    log.force_flush();
    return 0;
}
CPP

run_consumer() {
    if [ "$(uname -s)" = "Darwin" ]; then
        DYLD_LIBRARY_PATH="$PREFIX/lib" "$1"
    else
        LD_LIBRARY_PATH="$PREFIX/lib" "$1"
    fi
}

# 2. find_package: the CMake consumer, through the imported targets of both forms.
mkdir -p "$WORK/cmake_consumer"
cat > "$WORK/cmake_consumer/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.22)
project(consumer LANGUAGES CXX)

find_package(BqLog REQUIRED)

add_executable(consumer_shared ../consumer.cpp)
target_link_libraries(consumer_shared PRIVATE bqlog::BqLogShared)

add_executable(consumer_static ../consumer.cpp)
target_link_libraries(consumer_static PRIVATE bqlog::BqLogStatic)
CMAKE

cmake -S "$WORK/cmake_consumer" -B "$WORK/cmake_consumer/build" -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$PREFIX"
cmake --build "$WORK/cmake_consumer/build" --parallel

run_consumer "$WORK/cmake_consumer/build/consumer_shared"
run_consumer "$WORK/cmake_consumer/build/consumer_static"
echo "install_verify: find_package ok (bqlog::BqLogShared, bqlog::BqLogStatic)"

# 3. pkg-config: the consumers that are not CMake.
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
pkg-config --exists bqlog \
    || { echo "install_verify: pkg-config cannot find bqlog" >&2; exit 1; }
# shellcheck disable=SC2046
"$CXX" -std=c++17 $(pkg-config --cflags bqlog) "$WORK/consumer.cpp" \
    $(pkg-config --libs bqlog) -o "$WORK/consumer_pc"
run_consumer "$WORK/consumer_pc"
echo "install_verify: pkg-config ok (bqlog $(pkg-config --modversion bqlog))"

echo "install_verify: all checks passed"
