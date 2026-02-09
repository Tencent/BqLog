#!/bin/sh
set -e
DIR="$( cd "$( dirname "$0" )" && pwd )"
PROJECT_ROOT="$DIR/../../.."
BUILD_LIB_DIR="$PROJECT_ROOT/build/lib/unix_like"
TEST_SRC_DIR="$PROJECT_ROOT/test/java"
ARTIFACTS_DIR="$PROJECT_ROOT/artifacts"

CONFIG=${1:-RelWithDebInfo}

echo "===== Building BqLog Dynamic Library (Unix) ====="
(
    cd "$BUILD_LIB_DIR" || exit 1
    # dont_execute_this.sh build [arch] [compiler] [java] [node] [static_lib|dynamic_lib]
    chmod +x ./dont_execute_this.sh
    ./dont_execute_this.sh build native clang ON OFF dynamic_lib
)

echo "===== Building Java Test Jar ====="
mkdir -p "$DIR/VSProj"
(
    cd "$DIR/VSProj" || exit 1
    cmake "$TEST_SRC_DIR" -DTARGET_PLATFORM:STRING=unix
    cmake --build .
)

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

echo "Running Java Test on Unix..."
echo "Lib Path: $LIB_PATH"
java -Djava.library.path="$LIB_PATH" -jar "$JAR_PATH"
