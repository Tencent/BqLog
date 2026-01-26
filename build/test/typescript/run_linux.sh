#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$DIR/../../.."
BUILD_LIB_DIR="$PROJECT_ROOT/build/lib/linux"
TEST_SRC_DIR="$PROJECT_ROOT/test/typescript"
ARTIFACTS_DIR="$PROJECT_ROOT/artifacts"

CONFIG=${1:-RelWithDebInfo}

echo "===== Installing Node API Headers (Project Root) ====="
pushd "$PROJECT_ROOT" > /dev/null
npm install node-api-headers --no-save
popd > /dev/null

# Tell CMake where to find node-api-headers
export NODE_MODULES_ROOT="$PROJECT_ROOT/wrapper/typescript/node_modules"

echo "===== Building BqLog Dynamic Library (Linux) ====="
pushd "$BUILD_LIB_DIR" > /dev/null
# Node needs native module built with ON flag for node support
./dont_execute_this.sh build native clang OFF ON dynamic_lib
popd > /dev/null

echo "===== Installing TypeScript Test Dependencies ====="
pushd "$TEST_SRC_DIR" > /dev/null
npm install
popd > /dev/null

echo "===== Building TypeScript Wrapper ====="
pushd "$PROJECT_ROOT/wrapper/typescript" > /dev/null
npm install
npm run build
popd > /dev/null

echo "===== Running TypeScript Tests ====="
pushd "$TEST_SRC_DIR" > /dev/null

# Add ASan support for Linux
BQ_ENABLE_ASAN_UPPER=$(echo "$BQ_ENABLE_ASAN" | tr '[:lower:]' '[:upper:]')
if [[ "$BQ_ENABLE_ASAN_UPPER" == "TRUE" || "$BQ_ENABLE_ASAN_UPPER" == "ON" || "$BQ_ENABLE_ASAN_UPPER" == "1" ]]; then
    # Try to find ASan library
    ASAN_LIB=$(gcc -print-file-name=libasan.so)
    if [[ "$ASAN_LIB" != "libasan.so" ]]; then
        echo "ASan enabled, pre-loading $ASAN_LIB"
        export LD_PRELOAD="$ASAN_LIB"
    fi
    # Suppress Node leaks
    SUPP_FILE="$PROJECT_ROOT/test/lsan_suppressions.txt"
    export LSAN_OPTIONS="suppressions=$SUPP_FILE"
    # Node.js (V8) generates SEGVs for internal checks.
    # We must let Node handle them, otherwise ASan kills the process immediately.
    export ASAN_OPTIONS="handle_segv=0:allow_user_segv_handler=1"
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

node run_all_tests.js
popd > /dev/null
