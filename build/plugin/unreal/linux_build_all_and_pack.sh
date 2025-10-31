#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/../../.." && pwd)
VERSION_FILE="$ROOT_DIR/src/bq_log/global/version.cpp"
VERSION=$(grep -Eo 'BQ_LOG_VERSION\s*=\s*"[^"]+"' "$VERSION_FILE" | sed -E 's/.*"([^"]+)"/\1/')
TMP_DIR="$ROOT_DIR/artifacts/plugin/unreal/tmp_unreal_package"
TARGET_DIR="$TMP_DIR/BqLog"
PUBLIC_DIR="$TARGET_DIR/Source/BqLog/Public"
PRIVATE_DIR="$TARGET_DIR/Source/BqLog/Private"
DIST_DIR="$ROOT_DIR/dist"
ZIP_NAME="bqlog-unreal-plugin-$VERSION.zip"

rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR" "$DIST_DIR"
cp -R "$ROOT_DIR/plugin/unreal/BqLog" "$TMP_DIR/"
mkdir -p "$PUBLIC_DIR" "$PRIVATE_DIR"
cp -R "$ROOT_DIR/include/." "$PUBLIC_DIR/"
cp -R "$ROOT_DIR/src/bq_log" "$PRIVATE_DIR/"
cp -R "$ROOT_DIR/src/bq_common" "$PRIVATE_DIR/"
mkdir -p "$PRIVATE_DIR/IOS" "$PRIVATE_DIR/Mac"
if [[ -f "$PRIVATE_DIR/bq_common/platform/ios_misc.mm" ]]; then
  mv "$PRIVATE_DIR/bq_common/platform/ios_misc.mm" "$PRIVATE_DIR/IOS/"
fi
if [[ -f "$PRIVATE_DIR/bq_common/platform/mac_misc.mm" ]]; then
  mv "$PRIVATE_DIR/bq_common/platform/mac_misc.mm" "$PRIVATE_DIR/Mac/"
fi
rm -f "$DIST_DIR/$ZIP_NAME"
(cd "$TMP_DIR" && zip -r "$DIST_DIR/$ZIP_NAME" "BqLog" >/dev/null)
echo "Created $DIST_DIR/$ZIP_NAME"