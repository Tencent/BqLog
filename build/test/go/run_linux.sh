#!/bin/bash
# Run Go wrapper tests on Linux.
# Usage: run_linux.sh [gcc|clang] [CONFIG]
# Requires: go, cmake and a C compiler (cgo) in PATH.
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$DIR/../../.."
BUILD_LIB_DIR="$PROJECT_ROOT/build/lib/linux"
TEST_SRC_DIR="$PROJECT_ROOT/test/go"
WRAPPER_DIR="$PROJECT_ROOT/wrapper/go"
ARTIFACTS_DIR="$PROJECT_ROOT/artifacts"

COMPILER=${1:-clang}
CONFIG=${2:-RelWithDebInfo}

command -v go >/dev/null 2>&1 || { echo "Error: go not found in PATH"; exit 1; }

echo "===== Building BqLog Dynamic Library (Linux, $COMPILER, GO_SUPPORT=ON) ====="
pushd "$BUILD_LIB_DIR" > /dev/null
./dont_execute_this.sh build native "$COMPILER" OFF OFF OFF dynamic_lib ON
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

echo "===== Building and Running Go Test ====="
# cgo links against the freshly built artifacts directly via CGO_LDFLAGS
export CGO_ENABLED=1
export CGO_LDFLAGS="-L$LIB_OUT"
export LD_LIBRARY_PATH="$LIB_OUT:${LD_LIBRARY_PATH:-}"
pushd "$WRAPPER_DIR" > /dev/null
go vet ./...
go test -count=1 -timeout 120s ./...
popd > /dev/null
pushd "$TEST_SRC_DIR" > /dev/null
go build -o bqlog_go_test ./src/bq/test
./bqlog_go_test
popd > /dev/null
