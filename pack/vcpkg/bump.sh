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
# Writes the version and the SHA512 of a release into ports/bqlog.
#
# Usage: bash bump.sh <version> [tarball]
#   bash bump.sh 2.7.1                    downloads the Release_2.7.1 archive from GitHub
#   bash bump.sh 2.7.1 Release_2.7.1.tar.gz
set -euo pipefail

VERSION="${1:?usage: bump.sh <version> [tarball]}"
TARBALL="${2:-}"
PORT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/ports/bqlog"

if [ -z "$TARBALL" ]; then
    TARBALL="$(mktemp)"
    trap 'rm -f "$TARBALL"' EXIT
    curl -fsSL "https://github.com/Tencent/BqLog/archive/refs/tags/Release_${VERSION}.tar.gz" -o "$TARBALL"
fi

if command -v sha512sum >/dev/null 2>&1; then
    SHA512="$(sha512sum "$TARBALL" | cut -d' ' -f1)"
else
    SHA512="$(shasum -a 512 "$TARBALL" | cut -d' ' -f1)"
fi

sed -i.bak -E "s#^(    SHA512 ).*#\1${SHA512}#" "${PORT_DIR}/portfile.cmake"
sed -i.bak -E "s#^(  \"version\": )\".*\"#\1\"${VERSION}\"#" "${PORT_DIR}/vcpkg.json"
rm -f "${PORT_DIR}/portfile.cmake.bak" "${PORT_DIR}/vcpkg.json.bak"

# sed exits 0 even when nothing matched, which would leave the placeholder and ship a broken port.
grep -qF "SHA512 ${SHA512}" "${PORT_DIR}/portfile.cmake" \
    || { echo "bump: the SHA512 was not written into ${PORT_DIR}/portfile.cmake" >&2; exit 1; }
grep -qF "\"version\": \"${VERSION}\"" "${PORT_DIR}/vcpkg.json" \
    || { echo "bump: the version was not written into ${PORT_DIR}/vcpkg.json" >&2; exit 1; }

echo "bump: bqlog ${VERSION}"
echo "bump: sha512 ${SHA512}"
