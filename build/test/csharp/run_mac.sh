#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$DIR/../../.."
BUILD_LIB_DIR="$PROJECT_ROOT/build/lib/mac"
TEST_SRC_DIR="$PROJECT_ROOT/test/csharp"
ARTIFACTS_DIR="$PROJECT_ROOT/artifacts"

CONFIG=${1:-RelWithDebInfo}

echo "===== Building BqLog Dynamic Library (macOS) ====="
pushd "$BUILD_LIB_DIR" > /dev/null
# dont_execute_this.sh build [java] [node] [static_lib|dynamic_lib] [framework|dylib|a]
./dont_execute_this.sh build OFF OFF dynamic_lib dylib
popd > /dev/null

echo "===== Building C# Test Executable ====="
mkdir -p "$DIR/Build"
pushd "$DIR/Build" > /dev/null
cmake "$TEST_SRC_DIR" -DTARGET_PLATFORM:STRING=mac -G "Unix Makefiles"
cmake --build .
popd > /dev/null

EXE_PATH="$ARTIFACTS_DIR/test/csharp/BqLogCSharpTest"
EXE_DOT_EXE_PATH="$ARTIFACTS_DIR/test/csharp/BqLogCSharpTest.exe"
DLL_PATH="$ARTIFACTS_DIR/test/csharp/BqLogCSharpTest.dll"

LIB_PATH="$ARTIFACTS_DIR/dynamic_lib/lib/$CONFIG"

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

echo "Running C# Test on macOS..."
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
        # Suppress CoreCLR leaks
        SUPP_FILE="$PROJECT_ROOT/test/lsan_suppressions.txt"
        export LSAN_OPTIONS="suppressions=$SUPP_FILE"
        # CoreCLR generates SEGVs for internal checks.
        export ASAN_OPTIONS="handle_segv=0:allow_user_segv_handler=1"
    fi
fi

export DYLD_LIBRARY_PATH="$LIB_PATH:$DYLD_LIBRARY_PATH"

if [ -f "$EXE_PATH" ]; then
    "$EXE_PATH"
elif [ -f "$EXE_DOT_EXE_PATH" ]; then
    mono "$EXE_DOT_EXE_PATH"
elif [ -f "$DLL_PATH" ]; then
    dotnet "$DLL_PATH"
else
    echo "Error: C# Test Executable/DLL not found at $EXE_PATH or $DLL_PATH"
    exit 1
fi