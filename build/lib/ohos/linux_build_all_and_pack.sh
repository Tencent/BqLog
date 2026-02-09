#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${OHOS_SDK:-}" ]]; then
  echo "Environment variable OHOS_SDK is not found! Build cancelled!"
  exit 1
fi

# Only support Linux
uname_s="$(uname -s)"
if [[ "$uname_s" != "Linux" ]]; then
  echo "This script is for Linux only. Current OS: $uname_s"
  exit 1
fi

NDK_PATH="$OHOS_SDK/native"

# Detect number of CPU cores for parallel build
PARALLEL_JOBS="${PARALLEL_JOBS:-$(command -v nproc >/dev/null 2>&1 && nproc || echo 8)}"

BUILD_TARGET=(armeabi-v7a arm64-v8a x86_64)
BUILD_CONFIGS=(Debug RelWithDebInfo MinSizeRel Release)
BUILD_LIB_TYPES=(static_lib dynamic_lib)

echo "NDK: $NDK_PATH"
echo "Parallel jobs: $PARALLEL_JOBS"

rm -rf "../../../artifacts"
rm -rf "../../../install"

for build_target in "${BUILD_TARGET[@]}"; do
  for build_config in "${BUILD_CONFIGS[@]}"; do
    for build_lib_type in "${BUILD_LIB_TYPES[@]}"; do
      rm -rf "cmake_build"
      mkdir -p "cmake_build"
      pushd "cmake_build" >/dev/null
      cmake ../../../../src \
        -DOHOS_ARCH="$build_target" \
        -DCMAKE_BUILD_TYPE="$build_config" \
        -DBUILD_LIB_TYPE="$build_lib_type" \
        -DCMAKE_TOOLCHAIN_FILE="$NDK_PATH/build/cmake/ohos.toolchain.cmake" \
        -DTARGET_PLATFORM:STRING=ohos \
        -DOHOS_STL=c++_static

      # Build and install
      cmake --build . -- -j"${PARALLEL_JOBS}"
      cmake --build . --target install

      # Strip symbols (same as Android: move original to symbols, strip back to lib)
      if [ "$build_lib_type" == "dynamic_lib" ]; then
        SOURCE_SO=../../../../install/dynamic_lib/lib/"$build_config"/"$build_target"/libBqLog.so
        SYMBOL_SO=../../../../install/dynamic_lib/symbols/"$build_config"/"$build_target"/libBqLog.so
        mkdir -p "$(dirname "$SYMBOL_SO")"
        echo "[Stripping] Moving $SOURCE_SO to $SYMBOL_SO"
        mv "$SOURCE_SO" "$SYMBOL_SO"
        "$NDK_PATH/llvm/bin/llvm-strip" -s "$SYMBOL_SO" -o "$SOURCE_SO"
      fi
      popd >/dev/null
    done
  done
done

pushd "harmonyOS" >/dev/null
hvigorw clean --no-daemon
hvigorw assembleHar --mode module -p module=bqlog@default -p product=default --no-daemon -p buildMode=release --no-parallel
mkdir -p ../../../../install/dynamic_lib
cp -f bqlog/build/default/outputs/default/bqlog.har ../../../../install/dynamic_lib/bqlog.har

# Extract har symbols: copy the native .so from har build intermediates to symbols/har/{abi}/
HAR_NATIVE_DIR="bqlog/build/default/intermediates/cmake/default/obj"
SYMBOL_HAR_DIR="../../../../install/dynamic_lib/symbols/har"
if [ -d "$HAR_NATIVE_DIR" ]; then
  for abi_dir in "$HAR_NATIVE_DIR"/*/; do
    abi_name="$(basename "$abi_dir")"
    har_so="$abi_dir/libBqLog.so"
    if [ -f "$har_so" ]; then
      dest_dir="$SYMBOL_HAR_DIR/$abi_name"
      mkdir -p "$dest_dir"
      cp -f "$har_so" "$dest_dir/libBqLog.so"
      echo "[HAR Symbols] Copied $har_so to $dest_dir/libBqLog.so"
    fi
  done
else
  echo "Warning: HAR native intermediates dir not found at $HAR_NATIVE_DIR, skip har symbol extraction."
fi

popd >/dev/null

# Package the library
rm -rf pack
mkdir -p pack
pushd "pack" >/dev/null
cmake ../../../../pack -DTARGET_PLATFORM:STRING=ohos -DPACKAGE_NAME:STRING=bqlog-lib
cmake --build . --target package
popd >/dev/null

echo "---------"
echo "Finished!"
echo "---------"