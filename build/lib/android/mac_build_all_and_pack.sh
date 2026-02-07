#!/usr/bin/env bash
set -euo pipefail

# Check if ANDROID_NDK_ROOT is set, fallback to ANDROID_NDK_HOME if not.
if [[ -z "${ANDROID_NDK_ROOT:-}" && -n "${ANDROID_NDK_HOME:-}" ]]; then
  export ANDROID_NDK_ROOT="$ANDROID_NDK_HOME"
fi

if [[ -z "${ANDROID_NDK_ROOT:-}" ]]; then
  echo "Environment variable ANDROID_NDK_ROOT is not found! Build cancelled!"
  exit 1
fi

if [[ ! -d "$ANDROID_NDK_ROOT" ]]; then
  echo "The folder specified by ANDROID_NDK_ROOT does not exist! Build cancelled!"
  echo "$ANDROID_NDK_ROOT"
  exit 1
fi

# Only support macOS
uname_s="$(uname -s)"
if [[ "$uname_s" != "Darwin" ]]; then
  echo "This script is for macOS only. Current OS: $uname_s"
  exit 1
fi

echo "NDK: $ANDROID_NDK_ROOT"

pushd "gradle_proj" >/dev/null
chmod +x ./gradlew
./gradlew assemble
popd >/dev/null

rm -rf pack
mkdir -p pack
pushd "pack" >/dev/null
cmake ../../../../pack -DTARGET_PLATFORM:STRING=android -DPACKAGE_NAME:STRING=bqlog-lib
cmake --build . --target package
popd >/dev/null

echo "---------"
echo "Finished!"
echo "---------"