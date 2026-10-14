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
# Assembles runtimes/<rid>/native/ for the NuGet package out of the downloaded platform
# artifacts. The C# bindings ask the runtime for "BqLog" (wrapper/csharp/src/bq/lib_def.cs),
# so each platform needs its own file name in its own directory.
#
# Usage: bash collect_runtimes.sh <artifacts_dir> <output_dir>
set -euo pipefail

ARTIFACTS="${1:?usage: collect_runtimes.sh <artifacts_dir> <output_dir>}"
OUTPUT="${2:?usage: collect_runtimes.sh <artifacts_dir> <output_dir>}"

artifact_dir() {
    local dir
    dir="$(find "${ARTIFACTS}" -maxdepth 3 -type d -name "$1_*" -print -quit)"
    if [ -z "${dir}" ]; then
        echo "collect_runtimes: no artifact directory named $1_* under ${ARTIFACTS}" >&2
        exit 1
    fi
    echo "${dir}"
}

# cp follows the versioned symlink, which is what a package needs: NuGet flattens symlinks
# into empty files, so packaging libBqLog.so itself would ship a broken library.
place() {
    local artifact="$1" rid="$2" name="$3" pattern="$4"
    local dir src
    dir="$(artifact_dir "${artifact}")"
    src="$(find "${dir}" -type f -name "${pattern}" -print -quit)"
    if [ -z "${src}" ]; then
        echo "collect_runtimes: ${pattern} not found under ${dir}" >&2
        exit 1
    fi
    mkdir -p "${OUTPUT}/runtimes/${rid}/native"
    cp "${src}" "${OUTPUT}/runtimes/${rid}/native/${name}"
    echo "collect_runtimes: ${rid}/native/${name} <- ${src}"
}

# The MSVC build, not the MinGW one: a .NET process on Windows already has the MSVC runtime.
place windows_x86_64_libs          win-x64     BqLog.dll      'BqLog.dll'
place windows_arm64_libs           win-arm64   BqLog.dll      'BqLog.dll'
place linux_x86_64_libs            linux-x64   libBqLog.so    'libBqLog.so.*'
place linux_arm64_libs             linux-arm64 libBqLog.so    'libBqLog.so.*'

# The macOS library is universal, so the same file serves both architectures.
place macos_universal_binary_libs  osx-x64     libBqLog.dylib 'libBqLog.*.dylib'
place macos_universal_binary_libs  osx-arm64   libBqLog.dylib 'libBqLog.*.dylib'

echo "collect_runtimes: done"
