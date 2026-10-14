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
# Verifies the C++20 module interface unit: the installed .cppm is precompiled into a BMI, a
# consumer imports it and runs. --precompile on purpose: the cmake route needs clang-scan-deps.
#
# Usage: bash module_verify.sh [platform]      (default: mac on macOS, linux elsewhere)
#        CXX=clang++ bash module_verify.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

case "$(uname -s)" in
    # The import below links libBqLog.a; a single-form Apple build yields BqLog.framework instead.
    Darwin) DEFAULT_PLATFORM="mac"; SYSTEM_LIBS=""; APPLE_FORMAT="-DAPPLE_LIB_FORMAT=a" ;;
    *)      DEFAULT_PLATFORM="linux"; SYSTEM_LIBS="-lpthread -ldl"; APPLE_FORMAT="" ;;
esac
PLATFORM="${1:-$DEFAULT_PLATFORM}"
CXX="${CXX:-clang++}"

if ! command -v "$CXX" >/dev/null 2>&1; then
    echo "module_verify: $CXX not found; set CXX to a compiler with C++20 module support" >&2
    exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
PREFIX="$WORK/prefix"

echo "module_verify: platform=${PLATFORM} compiler=${CXX} prefix=${PREFIX}"

# 1. Install the library the way a package manager does.
cmake -S "$REPO_ROOT" -B "$WORK/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DTARGET_PLATFORM="$PLATFORM" \
    -DBUILD_LIB_TYPE=static_lib \
    -DBQ_WERROR=OFF \
    -DBQ_BUILD_TOOLS=OFF \
    $APPLE_FORMAT \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" > "$WORK/configure.log" 2>&1
cmake --build "$WORK/build" --parallel >> "$WORK/configure.log" 2>&1
cmake --install "$WORK/build" >> "$WORK/configure.log" 2>&1

# 2. The module interface unit ships with the headers; no BMI is prebuilt.
test -f "$PREFIX/include/bq_log/bq.log.cppm" \
    || { echo "module_verify: bq.log.cppm is missing from the install" >&2; exit 1; }

# 3. The interface unit compiles on its own. A member definition must not sit in an export
# block, which is what the BQ_MODULE_EXPORT_BEGIN/END markers keep track of.
"$CXX" -std=c++20 -x c++-module --precompile \
    -I"$PREFIX/include" \
    "$PREFIX/include/bq_log/bq.log.cppm" \
    -o "$WORK/bq.log.pcm"

# 4. A module unit also needs an object file: it carries the module initializer.
"$CXX" -std=c++20 -c "$WORK/bq.log.pcm" -o "$WORK/bq.log.o"

# 5. The consumer imports the module instead of including the header.
cat > "$WORK/consumer.cpp" <<'CPP'
import bq.log;

int main() {
    bq::log log = bq::log::create_log("module_verify",
        "appenders_config.ConsoleAppender.type=console\n"
        "appenders_config.ConsoleAppender.time_zone=localtime\n"
        "appenders_config.ConsoleAppender.levels=[all]\n"
        "log.thread_mode=sync");
    if (!log.is_valid()) {
        return 1;
    }
    log.info("bqlog module import works");
    log.force_flush();
    return 0;
}
CPP

"$CXX" -std=c++20 -fmodule-file=bq.log="$WORK/bq.log.pcm" \
    -I"$PREFIX/include" -c "$WORK/consumer.cpp" -o "$WORK/consumer.o"

# shellcheck disable=SC2086
"$CXX" "$WORK/bq.log.o" "$WORK/consumer.o" "$PREFIX/lib/libBqLog.a" \
    $SYSTEM_LIBS -o "$WORK/consumer"

"$WORK/consumer"
echo "module_verify: import bq.log ok"
