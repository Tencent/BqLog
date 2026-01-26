#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$DIR/../../.."
BUILD_LIB_DIR="$PROJECT_ROOT/build/lib/mac"
TEST_SRC_DIR="$PROJECT_ROOT/test/java"
ARTIFACTS_DIR="$PROJECT_ROOT/artifacts"

CONFIG=${1:-RelWithDebInfo}

echo "===== Building BqLog Dynamic Library (Mac) ====="
pushd "$BUILD_LIB_DIR" > /dev/null
# dont_execute_this.sh build [java] [node] [static_lib|dynamic_lib] [framework|dylib|a]
./dont_execute_this.sh build ON OFF dynamic_lib dylib
popd > /dev/null

echo "===== Building Java Test Jar ====="
mkdir -p "$DIR/VSProj"
pushd "$DIR/VSProj" > /dev/null
cmake "$TEST_SRC_DIR" -DTARGET_PLATFORM:STRING=mac
cmake --build .
popd > /dev/null

JAR_PATH="$ARTIFACTS_DIR/test/java/BqLogJavaTest.jar"
LIB_PATH="$ARTIFACTS_DIR/dynamic_lib/lib/$CONFIG"

if [ ! -f "$JAR_PATH" ]; then
    echo "Error: Test Jar not found at $JAR_PATH"
    exit 1
fi

if [ ! -d "$LIB_PATH" ]; then
    # Fallback to other configs if requested one not found
    if [ -d "$ARTIFACTS_DIR/dynamic_lib/lib/Release" ]; then
        LIB_PATH="$ARTIFACTS_DIR/dynamic_lib/lib/Release"
    elif [ -d "$ARTIFACTS_DIR/dynamic_lib/lib/Debug" ]; then
        LIB_PATH="$ARTIFACTS_DIR/dynamic_lib/lib/Debug"
    else
        echo "Error: Lib Path not found at $LIB_PATH"
        exit 1
    fi
fi

echo "Running Java Test on Mac..."
echo "Lib Path: $LIB_PATH"

# Add ASan support for Mac
if [[ "$OSTYPE" == "darwin"* ]]; then
    BQ_ENABLE_ASAN_UPPER=$(echo "$BQ_ENABLE_ASAN" | tr '[:lower:]' '[:upper:]')
    if [[ "$BQ_ENABLE_ASAN_UPPER" == "TRUE" || "$BQ_ENABLE_ASAN_UPPER" == "ON" || "$BQ_ENABLE_ASAN_UPPER" == "1" ]]; then
        ASAN_LIB=$(clang -print-file-name=libclang_rt.asan_osx_dynamic.dylib)
        if [[ "$ASAN_LIB" == /* ]]; then
            echo "ASan enabled, pre-loading $ASAN_LIB"
            export DYLD_INSERT_LIBRARIES="$ASAN_LIB"
        fi
    fi
fi

java -Djava.library.path="$LIB_PATH" -jar "$JAR_PATH"