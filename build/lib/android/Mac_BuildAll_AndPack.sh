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

NDK_PATH="$ANDROID_NDK_ROOT"

# Try to use darwin-arm64 if available, otherwise fallback to darwin-x86_64
if [[ -d "$NDK_PATH/toolchains/llvm/prebuilt/darwin-arm64/bin" ]]; then
  HOST_TAG="darwin-arm64"
  echo "Using NDK host toolchain: darwin-arm64"
elif [[ -d "$NDK_PATH/toolchains/llvm/prebuilt/darwin-x86_64/bin" ]]; then
  HOST_TAG="darwin-x86_64"
  echo "Using NDK host toolchain: darwin-x86_64"
else
  echo "Neither darwin-arm64 nor darwin-x86_64 toolchain found in NDK. Build cancelled!"
  exit 1
fi

# Detect number of CPU cores for parallel build
PARALLEL_JOBS="${PARALLEL_JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 8)}"

BUILD_TARGET=(armeabi-v7a arm64-v8a x86_64 x86)
BUILD_TYPE=(Debug RelWithDebInfo MinSizeRel Release)

echo "NDK: $NDK_PATH"
echo "HOST_TAG: $HOST_TAG"
echo "Parallel jobs: $PARALLEL_JOBS"

for build_target in "${BUILD_TARGET[@]}"; do
  rm -rf "$build_target"
  mkdir -p "$build_target"
  cd "$build_target"

  # Select minimum ANDROID_API for each ABI (64-bit requires >= 21)
  case "$build_target" in
    arm64-v8a|x86_64) ANDROID_API=21 ;;
    *) ANDROID_API=19 ;;
  esac

  for build_type in "${BUILD_TYPE[@]}"; do
    mkdir -p "$build_type"
    cd "$build_type"

    cmake ../../../../../src \
      -DANDROID_ABI="$build_target" \
      -DANDROID_PLATFORM="android-${ANDROID_API}" \
      -DANDROID_NDK="$NDK_PATH" \
      -DCMAKE_BUILD_TYPE="$build_type" \
      -DBUILD_TYPE=dynamic_lib \
      -DCMAKE_TOOLCHAIN_FILE="$NDK_PATH/build/cmake/android.toolchain.cmake" \
      -DTARGET_PLATFORM:STRING=android \
      -DANDROID_STL=none

    # Build and install
    cmake --build . -- -j"${PARALLEL_JOBS}"
    cmake --build . --target install

    # Strip symbols
    SOURCE_SO=../../../../../install/dynamic_lib/lib/"$build_type"/"$build_target"/libBqLog.so
    STRIP_SO=../../../../../install/dynamic_lib/lib_strip/"$build_type"/"$build_target"/libBqLog.so
    mkdir -p "$(dirname "$STRIP_SO")"
    echo "SOURCE_SO:$SOURCE_SO"
    "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$HOST_TAG/bin/llvm-strip" -s "$SOURCE_SO" -o "$STRIP_SO"

    cd ..
  done
  cd ..
done

pushd "gradle_proj" >/dev/null
chmod +x ./gradlew
./gradlew assemble
popd >/dev/null

# Package the library
rm -rf pack
mkdir -p pack
pushd "pack" >/dev/null
cmake ../../../../pack -DTARGET_PLATFORM:STRING=android -DPACKAGE_NAME:STRING=bqlog-lib
cmake --build . --target package
popd >/dev/null

echo "---------"
echo "Finished!"
echo "---------"