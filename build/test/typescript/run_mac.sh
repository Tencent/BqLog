#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$DIR/../../.."
BUILD_LIB_DIR="$PROJECT_ROOT/build/lib/mac"
TEST_SRC_DIR="$PROJECT_ROOT/test/typescript"
ARTIFACTS_DIR="$PROJECT_ROOT/artifacts"

CONFIG=${1:-RelWithDebInfo}

echo "===== Installing Node API Headers (Project Root) ====="
pushd "$PROJECT_ROOT" > /dev/null
npm install node-api-headers --no-save
popd > /dev/null

echo "===== Building BqLog Dynamic Library (macOS) ====="
pushd "$BUILD_LIB_DIR" > /dev/null
# dont_execute_this.sh build [java] [node] [static_lib|dynamic_lib] [framework|dylib|a]
./dont_execute_this.sh build OFF ON dynamic_lib dylib
popd > /dev/null

echo "===== Installing TypeScript Test Dependencies ====="
pushd "$TEST_SRC_DIR" > /dev/null
npm install
popd > /dev/null

echo "===== Building TypeScript Wrapper (for tests) ====="
pushd "$PROJECT_ROOT/wrapper/typescript" > /dev/null
npm install
npm run build
popd > /dev/null

echo "===== Running TypeScript Tests ====="
pushd "$TEST_SRC_DIR" > /dev/null

# Add ASan support for Mac
if [[ "$OSTYPE" == "darwin"* ]]; then
    BQ_ENABLE_ASAN_UPPER=$(echo "$BQ_ENABLE_ASAN" | tr '[:lower:]' '[:upper:]')
    if [[ "$BQ_ENABLE_ASAN_UPPER" == "TRUE" || "$BQ_ENABLE_ASAN_UPPER" == "ON" || "$BQ_ENABLE_ASAN_UPPER" == "1" ]]; then
        ASAN_LIB=$(clang -print-file-name=libclang_rt.asan_osx_dynamic.dylib)
        if [[ "$ASAN_LIB" == /* ]]; then
            echo "ASan enabled, pre-loading $ASAN_LIB"
            export DYLD_INSERT_LIBRARIES="$ASAN_LIB"
            export BQ_ASAN_LIB="$ASAN_LIB"
        fi
        # Suppress Node leaks
        SUPP_FILE="$PROJECT_ROOT/test/lsan_suppressions.txt"
        export LSAN_OPTIONS="suppressions=$SUPP_FILE"
    fi
fi

# Find the .node file
NODE_LIB=$(find "$ARTIFACTS_DIR/dynamic_lib/lib" -name "*.node" 2>/dev/null | head -n 1) || true

if [ -z "$NODE_LIB" ]; then
    echo "Warning: .node file not found via find, checking common locations..."
else
    echo "Found Node Lib: $NODE_LIB"
    # Copy to wrapper/typescript/dist so loader can find it
    DEST_DIR="$PROJECT_ROOT/wrapper/typescript/dist"
    echo "Copying to $DEST_DIR..."
    mkdir -p "$DEST_DIR"
    cp -f "$NODE_LIB" "$DEST_DIR/BqLog.node"
fi

# Use node directly instead of npm test to reduce shell layers that strip DYLD_INSERT_LIBRARIES
node run_all_tests.js
popd > /dev/null
