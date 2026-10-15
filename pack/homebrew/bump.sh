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
# Writes the url and sha256 of a release into Formula/bqlog.rb.
#
# Usage: bash bump.sh <version> [tarball]
#   bash bump.sh 2.7.0                    downloads the Release_2.7.0 archive from GitHub
#   bash bump.sh 2.7.0 Release_2.7.0.tar.gz
# BQ_FORMULA_URL overrides the url written into the formula (CI points it at a local file).
set -euo pipefail

VERSION="${1:?usage: bump.sh <version> [tarball]}"
TARBALL="${2:-}"
FORMULA="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/Formula/bqlog.rb"
URL="${BQ_FORMULA_URL:-https://github.com/Tencent/BqLog/archive/refs/tags/Release_${VERSION}.tar.gz}"

if [ -z "$TARBALL" ]; then
    TARBALL="$(mktemp)"
    trap 'rm -f "$TARBALL"' EXIT
    curl -fsSL "https://github.com/Tencent/BqLog/archive/refs/tags/Release_${VERSION}.tar.gz" -o "$TARBALL"
fi

if command -v sha256sum >/dev/null 2>&1; then
    SHA256="$(sha256sum "$TARBALL" | cut -d' ' -f1)"
else
    SHA256="$(shasum -a 256 "$TARBALL" | cut -d' ' -f1)"
fi

sed -i.bak -E "s#^(  url )\".*\"#\1\"${URL}\"#; s#^(  sha256 )\".*\"#\1\"${SHA256}\"#" "$FORMULA"
rm -f "${FORMULA}.bak"

# sed exits 0 even when nothing matched, which would leave the placeholder and ship a broken formula.
grep -qF "url \"${URL}\"" "$FORMULA" || { echo "bump: the url was not written into ${FORMULA}" >&2; exit 1; }
grep -qF "sha256 \"${SHA256}\"" "$FORMULA" || { echo "bump: the sha256 was not written into ${FORMULA}" >&2; exit 1; }

echo "bump: ${URL}"
echo "bump: sha256 ${SHA256}"
