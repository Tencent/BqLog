#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$DIR/../../.."
BUILD_LIB_DIR="$PROJECT_ROOT/build/lib/linux"
TEST_SRC_DIR="$PROJECT_ROOT/test/java"
ARTIFACTS_DIR="$PROJECT_ROOT/artifacts"

CONFIG=${1:-RelWithDebInfo}

echo "===== Building BqLog Dynamic Library (Linux) ====="
pushd "$BUILD_LIB_DIR" > /dev/null
# dont_execute_this.sh build [arch] [compiler] [java] [node] [static_lib|dynamic_lib]
./dont_execute_this.sh build native gcc ON OFF dynamic_lib
popd > /dev/null

echo "===== Building Java Test Jar ====="
mkdir -p "$DIR/VSProj"
pushd "$DIR/VSProj" > /dev/null
cmake "$TEST_SRC_DIR" -DTARGET_PLATFORM:STRING=linux
cmake --build .
popd > /dev/null

JAR_PATH="$ARTIFACTS_DIR/test/java/BqLogJavaTest.jar"
LIB_PATH="$ARTIFACTS_DIR/dynamic_lib/lib/$CONFIG"

if [ ! -f "$JAR_PATH" ]; then
    echo "Error: Test Jar not found at $JAR_PATH"
    exit 1
fi

if [ ! -d "$LIB_PATH" ]; then
    if [ -d "$ARTIFACTS_DIR/dynamic_lib/lib/Release" ]; then
        LIB_PATH="$ARTIFACTS_DIR/dynamic_lib/lib/Release"
    elif [ -d "$ARTIFACTS_DIR/dynamic_lib/lib/Debug" ]; then
        LIB_PATH="$ARTIFACTS_DIR/dynamic_lib/lib/Debug"
    else
        echo "Error: Lib Path not found at $LIB_PATH"
        exit 1
    fi
fi

echo "Running Java Test on Linux..."
echo "Lib Path: $LIB_PATH"
java -Djava.library.path="$LIB_PATH" -jar "$JAR_PATH"