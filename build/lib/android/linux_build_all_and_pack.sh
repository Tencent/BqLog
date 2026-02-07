#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${ANDROID_NDK_ROOT:-}" ]]; then
  echo "Environment Variable ANDROID_NDK_ROOT Is Not Found! Build Cancelled!!"
  exit 1
fi

if [[ ! -d "$ANDROID_NDK_ROOT" ]]; then
  echo "The folder in ANDROID_NDK_ROOT does not exist! Build Cancelled!!"
  echo "$ANDROID_NDK_ROOT"
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