#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$DIR/../../.."
BUILD_LIB_DIR="$PROJECT_ROOT/build/lib/linux"
TEST_SRC_DIR="$PROJECT_ROOT/test/typescript"
ARTIFACTS_DIR="$PROJECT_ROOT/artifacts"

CONFIG=${1:-RelWithDebInfo}

echo "===== Building BqLog Dynamic Library (Linux) ====="
pushd "$BUILD_LIB_DIR" > /dev/null
# Node needs native module built with ON flag for node support
./dont_execute_this.sh build native clang OFF ON dynamic_lib
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

# Find the .node file
NODE_LIB=$(find "$ARTIFACTS_DIR/dynamic_lib/lib" -name "*.node" 2>/dev/null | head -n 1) || true

if [ -z "$NODE_LIB" ]; then
    echo "Warning: .node file not found via find, checking common locations..."
else
    echo "Found Node Lib: $NODE_LIB"
    export BQ_NODE_ADDON="$NODE_LIB"
fi

npm test
popd > /dev/null
