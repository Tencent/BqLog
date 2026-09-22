#!/bin/zsh
# Run Go wrapper tests on macOS.
# Usage: run_mac.sh [CONFIG]
# Requires: go, cmake and clang (cgo) in PATH.
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]:-$0}" )" && pwd )"
PROJECT_ROOT="$DIR/../../.."
BUILD_LIB_DIR="$PROJECT_ROOT/build/lib/mac"
TEST_SRC_DIR="$PROJECT_ROOT/test/go"
WRAPPER_DIR="$PROJECT_ROOT/wrapper/go"
ARTIFACTS_DIR="$PROJECT_ROOT/artifacts"

CONFIG=${1:-RelWithDebInfo}

command -v go >/dev/null 2>&1 || { echo "Error: go not found in PATH"; exit 1; }

echo "===== Building BqLog Dynamic Library (macOS, GO_SUPPORT=ON) ====="
pushd "$BUILD_LIB_DIR" > /dev/null
# dont_execute_this.sh build [java] [node] [python] [lib_type] [format] [go]
./dont_execute_this.sh build OFF OFF OFF dynamic_lib dylib ON
popd > /dev/null

LIB_OUT="$ARTIFACTS_DIR/dynamic_lib/lib/$CONFIG"
if [ ! -d "$LIB_OUT" ]; then
    if [ -d "$ARTIFACTS_DIR/dynamic_lib/lib/Release" ]; then
        LIB_OUT="$ARTIFACTS_DIR/dynamic_lib/lib/Release"
    elif [ -d "$ARTIFACTS_DIR/dynamic_lib/lib/Debug" ]; then
        LIB_OUT="$ARTIFACTS_DIR/dynamic_lib/lib/Debug"
    else
        echo "Error: Lib Path not found at $LIB_OUT"
        exit 1
    fi
fi

echo "===== Staging native library for cgo ====="
mkdir -p "$WRAPPER_DIR/lib"
cp -f "$LIB_OUT"/libBqLog.dylib "$WRAPPER_DIR/lib/libbqlog.dylib"
cp -f "$LIB_OUT"/libBqLog.dylib "$WRAPPER_DIR/lib/libBqLog.dylib"

echo "===== Building and Running Go Test ====="
pushd "$WRAPPER_DIR" > /dev/null
export CGO_ENABLED=1
go vet ./...
popd > /dev/null
pushd "$TEST_SRC_DIR" > /dev/null
export DYLD_LIBRARY_PATH="$WRAPPER_DIR/lib:${DYLD_LIBRARY_PATH:-}"
go build -o bqlog_go_test ./src/bq/test
./bqlog_go_test
popd > /dev/null
