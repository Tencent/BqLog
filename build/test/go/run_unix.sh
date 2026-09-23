#!/bin/sh
# Run Go wrapper tests on unix-like systems (FreeBSD etc.).
# Usage: run_unix.sh [gcc|clang] [CONFIG]
# Requires: go, cmake and a C compiler (cgo) in PATH.
set -e
DIR="$( cd "$( dirname "$0" )" && pwd )"
PROJECT_ROOT="$DIR/../../.."
BUILD_LIB_DIR="$PROJECT_ROOT/build/lib/unix_like"
TEST_SRC_DIR="$PROJECT_ROOT/test/go"
WRAPPER_DIR="$PROJECT_ROOT/wrapper/go"
ARTIFACTS_DIR="$PROJECT_ROOT/artifacts"

COMPILER=${1:-clang}
CONFIG=${2:-RelWithDebInfo}

command -v go >/dev/null 2>&1 || { echo "Error: go not found in PATH"; exit 1; }

echo "===== Building BqLog Dynamic Library (Unix, $COMPILER, GO_SUPPORT=ON) ====="
(
    cd "$BUILD_LIB_DIR" || exit 1
    chmod +x ./dont_execute_this.sh
    ./dont_execute_this.sh build native "$COMPILER" OFF OFF OFF dynamic_lib ON
)

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
(
    cd "$WRAPPER_DIR" || exit 1
    go vet ./...
    go test -count=1 -timeout 120s ./...
)
cd "$TEST_SRC_DIR" || exit 1
go build -o bqlog_go_test ./src/bq/test
./bqlog_go_test
