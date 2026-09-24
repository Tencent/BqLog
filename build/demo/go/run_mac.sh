#!/bin/zsh
# Run Go demo on macOS.
# Usage: run_mac.sh [CONFIG]
# Requires: go, cmake and clang (cgo) in PATH.
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]:-$0}" )" && pwd )"
PROJECT_ROOT="$DIR/../../.."
BUILD_LIB_DIR="$PROJECT_ROOT/build/lib/mac"
DEMO_SRC_DIR="$PROJECT_ROOT/demo/go"
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

echo "===== Building and Running Go Demo ====="
# cgo links against the freshly built artifacts directly via CGO_LDFLAGS
export CGO_ENABLED=1
export CGO_LDFLAGS="-L$LIB_OUT"
export DYLD_LIBRARY_PATH="$LIB_OUT:${DYLD_LIBRARY_PATH:-}"
pushd "$DEMO_SRC_DIR" > /dev/null
go build -o bqlog_go_demo .
./bqlog_go_demo
popd > /dev/null
