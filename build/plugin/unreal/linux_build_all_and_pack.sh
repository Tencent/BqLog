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

rm -rf "../../../artifacts"
rm -rf "../../../install"

for ue_version in "${UE_VERSIONS[@]}"; do
  rm -rf "$TMP_DIR"
  mkdir -p "$TMP_DIR"

  cp -R "$ROOT_DIR/plugin/unreal/BqLog" "$TMP_DIR/"
  rm -rf "$TARGET_DIR/Binaries"
  mkdir -p "$PUBLIC_DIR" "$PRIVATE_DIR"

  # BqLog core: headers go under Source/ThirdParty/BqLog/include
  # (kept in the ThirdParty tree for Fab's third-party attribution requirement;
  # LICENSE also lives here). The .cpp implementation tree is copied into
  # Source/BqLog/Private/BqLogCore/ so it falls under UBT's standard module
  # source scan — no ConditionalAddModuleDirectory (4.26+ only) needed, which
  # keeps the plugin buildable on UE 4.22 through UE 5.x with one Build.cs.
  THIRDPARTY_DIR="$TARGET_DIR/Source/ThirdParty/BqLog"
  CORE_DIR="$PRIVATE_DIR/BqLogCore"
  mkdir -p "$THIRDPARTY_DIR/include"
  mkdir -p "$CORE_DIR"
  cp -R "$ROOT_DIR/include/." "$THIRDPARTY_DIR/include/"
  cp -R "$ROOT_DIR/src/bq_log" "$CORE_DIR/"
  cp -R "$ROOT_DIR/src/bq_common" "$CORE_DIR/"
  cp "$ROOT_DIR/LICENSE"* "$THIRDPARTY_DIR/"

  mkdir -p "$CORE_DIR/IOS" "$CORE_DIR/Mac"
  if [[ -f "$CORE_DIR/bq_common/platform/ios_misc.mm" ]]; then
    mv -f "$CORE_DIR/bq_common/platform/ios_misc.mm" "$CORE_DIR/IOS/"
  fi
  if [[ -f "$CORE_DIR/bq_common/platform/mac_misc.mm" ]]; then
    mv -f "$CORE_DIR/bq_common/platform/mac_misc.mm" "$CORE_DIR/Mac/"
  fi

  if [[ -f "$PUBLIC_DIR/BqLog_h_${ue_version}.txt" ]]; then
    mv -f "$PUBLIC_DIR/BqLog_h_${ue_version}.txt" "$PUBLIC_DIR/BqLog.h"
  fi

  rm -f "$PUBLIC_DIR"/*.txt 2>/dev/null || true

  # Replace version in .uplugin
  VERSION_MAJOR=$(echo "$VERSION" | cut -d. -f1)
  VERSION_MINOR=$(echo "$VERSION" | cut -d. -f2)
  VERSION_PATCH=$(echo "$VERSION" | cut -d. -f3)
  VERSION_INT=$(( VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_PATCH ))
  sed -i "s/\"Version\": 1/\"Version\": $VERSION_INT/" "$TARGET_DIR/BqLog.uplugin"
  sed -i "s/\"VersionName\": \"1.0\"/\"VersionName\": \"$VERSION\"/" "$TARGET_DIR/BqLog.uplugin"
  # Replace EngineVersion based on UE major version
  if [[ "$ue_version" == "ue4" ]]; then
    sed -i "s/\"EngineVersion\": \"5.8\"/\"EngineVersion\": \"4.27\"/" "$TARGET_DIR/BqLog.uplugin"
    sed -i 's/"PlatformAllowList"/"WhitelistPlatforms"/g' "$TARGET_DIR/BqLog.uplugin"
    sed -i 's/"PlatformDenyList"/"BlacklistPlatforms"/g' "$TARGET_DIR/BqLog.uplugin"
    sed -i 's/"LinuxArm64"/"LinuxAArch64"/g' "$TARGET_DIR/BqLog.uplugin"
  fi

  # Remove .gitkeep files before zipping
  find "$TARGET_DIR" -name ".gitkeep" -delete

  ZIP_NAME="bqlog-unreal-plugin-${VERSION}-${ue_version}.zip"
  rm -f "$DIST_DIR/$ZIP_NAME"
  (cd "$TMP_DIR" && zip -rq "$DIST_DIR/$ZIP_NAME" "BqLog")
  (cd "$DIST_DIR" && sha256sum "$ZIP_NAME" | tr '[:upper:]' '[:lower:]' > "$ZIP_NAME.sha256")
  echo "Created $DIST_DIR/$ZIP_NAME"
done