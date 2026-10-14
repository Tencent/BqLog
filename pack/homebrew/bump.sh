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
# Points the formulas at a published release: fetches the checksum of the tag archive and
# writes it, together with the url, into every formula under Formula/.
#
# Usage: bash bump.sh <version> [formula_dir]
#   bash bump.sh 2.7.0
set -euo pipefail

VERSION="${1:?usage: bump.sh <version> [formula_dir]}"
FORMULA_DIR="${2:-Formula}"
TAG="Release_${VERSION}"
URL="https://github.com/Tencent/BqLog/archive/refs/tags/${TAG}.tar.gz"

if command -v sha256sum >/dev/null 2>&1; then
    SHA256="$(curl -fsSL "$URL" | sha256sum | cut -d' ' -f1)"
else
    SHA256="$(curl -fsSL "$URL" | shasum -a 256 | cut -d' ' -f1)"
fi

echo "bump: ${TAG}"
echo "bump: ${SHA256}"

shopt -s nullglob
formulas=("${FORMULA_DIR}"/*.rb)
shopt -u nullglob

if [ ${#formulas[@]} -eq 0 ]; then
    echo "bump: no formula under ${FORMULA_DIR}" >&2
    exit 1
fi

for formula in "${formulas[@]}"; do
    sed -i.bak -E "s#^(  url )\".*\"#\1\"${URL}\"#" "$formula"
    sed -i.bak -E "s#^(  sha256 )\".*\"#\1\"${SHA256}\"#" "$formula"
    rm -f "${formula}.bak"
    echo "bump: updated ${formula}"
done
