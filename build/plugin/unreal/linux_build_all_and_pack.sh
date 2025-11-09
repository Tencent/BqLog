#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"

VERSION_FILE="$ROOT_DIR/src/bq_log/global/version.cpp"
VERSION="$(sed -nE 's/.*BQ_LOG_VERSION[[:space:]]*=[[:space:]]*"([^"]+)".*/\1/p' "$VERSION_FILE" | head -n1 || true)"
if [[ -z "${VERSION:-}" ]]; then
  echo "ERROR: Failed to parse version from: $VERSION_FILE" >&2
  exit 1
fi

TMP_DIR="$ROOT_DIR/artifacts/plugin/unreal/tmp_unreal_package"
TARGET_DIR="$TMP_DIR/BqLog"
PUBLIC_DIR="$TARGET_DIR/Source/BqLog/Public"
PRIVATE_DIR="$TARGET_DIR/Source/BqLog/Private"
DIST_DIR="$ROOT_DIR/dist"
UE_VERSIONS=(ue4 ue5)

if ! command -v zip >/dev/null 2>&1; then
  echo "ERROR: 'zip' command not found. Please install zip." >&2
  exit 1
fi

mkdir -p "$DIST_DIR"

for ue_version in "${UE_VERSIONS[@]}"; do
  rm -rf "$TMP_DIR"
  mkdir -p "$TMP_DIR"

  cp -R "$ROOT_DIR/plugin/unreal/BqLog" "$TMP_DIR/"
  rm -rf "$TARGET_DIR/Binaries"
  mkdir -p "$PUBLIC_DIR" "$PRIVATE_DIR"

  cp -R "$ROOT_DIR/include/." "$PUBLIC_DIR/"
  cp -R "$ROOT_DIR/src/bq_log" "$PRIVATE_DIR/"
  cp -R "$ROOT_DIR/src/bq_common" "$PRIVATE_DIR/"

  mkdir -p "$PRIVATE_DIR/IOS" "$PRIVATE_DIR/Mac"
  if [[ -f "$PRIVATE_DIR/bq_common/platform/ios_misc.mm" ]]; then
    mv -f "$PRIVATE_DIR/bq_common/platform/ios_misc.mm" "$PRIVATE_DIR/IOS/"
  fi
  if [[ -f "$PRIVATE_DIR/bq_common/platform/mac_misc.mm" ]]; then
    mv -f "$PRIVATE_DIR/bq_common/platform/mac_misc.mm" "$PRIVATE_DIR/Mac/"
  fi

  if [[ -f "$PUBLIC_DIR/BqLog_h_${ue_version}.txt" ]]; then
    mv -f "$PUBLIC_DIR/BqLog_h_${ue_version}.txt" "$PUBLIC_DIR/BqLog.h"
  fi

  rm -f "$PUBLIC_DIR"/*.txt 2>/dev/null || true

  ZIP_NAME="bqlog-unreal-plugin-${VERSION}-${ue_version}.zip"
  rm -f "$DIST_DIR/$ZIP_NAME"
  (cd "$TMP_DIR" && zip -rq "$DIST_DIR/$ZIP_NAME" "BqLog")
  (cd "$DIST_DIR" && sha256sum "$ZIP_NAME" | tr '[:upper:]' '[:lower:]' > "$ZIP_NAME.sha256")
  echo "Created $DIST_DIR/$ZIP_NAME"
done