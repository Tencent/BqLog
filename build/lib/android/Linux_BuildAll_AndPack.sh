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

NDK_PATH="$ANDROID_NDK_ROOT"

# Detect host architecture for NDK toolchain
uname_m="$(uname -m)"
if [[ "$uname_m" == "aarch64" || "$uname_m" == "arm64" ]]; then
  HOST_TAG="linux-arm64"
elif [[ "$uname_m" == "x86_64" ]]; then
  HOST_TAG="linux-x86_64"
else
  echo "Unsupported host architecture: $uname_m"
  exit 1
fi

# Fallback if NDK doesn't have this prebuilt
if [[ ! -d "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$HOST_TAG/bin" ]]; then
  if [[ -d "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin" ]]; then
    HOST_TAG="linux-x86_64"
  elif [[ -d "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-arm64/bin" ]]; then
    HOST_TAG="linux-arm64"
  else
    echo "No suitable NDK prebuilt toolchain found for this host."
    exit 1
  fi
fi

echo "Using NDK toolchain: $HOST_TAG"

BUILD_TARGET=(armeabi-v7a arm64-v8a x86_64 x86)
BUILD_TYPE=(Debug RelWithDebInfo MinSizeRel Release)

for build_target in "${BUILD_TARGET[@]}"; do
  rm -rf "$build_target"
  mkdir -p "$build_target"
  cd "$build_target"

  for build_type in "${BUILD_TYPE[@]}"; do
    mkdir -p "$build_type"
    cd "$build_type"

    cmake ../../../../../src \
      -DANDROID_ABI="$build_target" \
      -DANDROID_PLATFORM="android-21" \
      -DANDROID_NDK="$NDK_PATH" \
      -DCMAKE_BUILD_TYPE="$build_type" \
      -DBUILD_TYPE=dynamic_lib \
      -DCMAKE_TOOLCHAIN_FILE="$NDK_PATH/build/cmake/android.toolchain.cmake" \
      -DTARGET_PLATFORM:STRING=android \
      -DANDROID_STL=none

    # Build and install (prefer cmake --build over NDK's make path)
    PARALLEL_JOBS="${PARALLEL_JOBS:-$( (command -v nproc >/dev/null && nproc) || (sysctl -n hw.ncpu 2>/dev/null) || echo 8 )}"
    cmake --build . -- -j"${PARALLEL_JOBS}"
    cmake --build . --target install

    # Strip using NDK llvm-strip for the host
    SYMBOL_SO=../../../../../install/dynamic_lib/lib/"$build_target"/"$build_type"/libBqLog_Symbol.so
    FINAL_SO=../../../../../install/dynamic_lib/lib/"$build_target"/"$build_type"/libBqLog.so

    mv -f "$FINAL_SO" "$SYMBOL_SO"
    "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$HOST_TAG/bin/llvm-strip" -s "$SYMBOL_SO" -o "$FINAL_SO"

    cd ..
  done
  cd ..
done

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