#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$DIR/../../.."
BUILD_LIB_DIR="$PROJECT_ROOT/build/lib/linux"
TEST_SRC_DIR="$PROJECT_ROOT/test/csharp"
ARTIFACTS_DIR="$PROJECT_ROOT/artifacts"

CONFIG=${1:-RelWithDebInfo}

# Enable ASan for debugging
export EXTRA_CMAKE_ARGS="-DBQ_ENABLE_ASAN=ON"

# Find and preload libasan (required for ASan to work with P/Invoke)
ASAN_LIB=$(gcc -print-file-name=libasan.so)
if [ -f "$ASAN_LIB" ]; then
    echo "Preloading ASan: $ASAN_LIB"
    export LD_PRELOAD="$ASAN_LIB"
fi

echo "===== Building BqLog Dynamic Library (Linux) ====="
pushd "$BUILD_LIB_DIR" > /dev/null
./dont_execute_this.sh build native clang OFF OFF dynamic_lib
popd > /dev/null

echo "===== Building C# Test Executable ====="
mkdir -p "$DIR/Build"
pushd "$DIR/Build" > /dev/null
cmake "$TEST_SRC_DIR" -DTARGET_PLATFORM:STRING=linux
cmake --build .
popd > /dev/null

# Artifacts are directly output to artifacts/test/csharp by the csproj
EXE_PATH="$ARTIFACTS_DIR/test/csharp/BqLogTest" # dotnet build uses ProjectName as binary name
DLL_PATH="$ARTIFACTS_DIR/test/csharp/BqLogTest.dll"

# Check if the output name matches the project name in CMakeLists (BqLogCSharpTest.csproj -> BqLogCSharpTest.dll)
EXE_PATH="$ARTIFACTS_DIR/test/csharp/BqLogCSharpTest"
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

echo "Running C# Test on Linux..."
echo "Lib Path: $LIB_PATH"

export LD_LIBRARY_PATH="$LIB_PATH:$LD_LIBRARY_PATH"

if [ -f "$EXE_PATH" ]; then
    "$EXE_PATH"
elif [ -f "$DLL_PATH" ]; then
    dotnet "$DLL_PATH"
else
    echo "Error: C# Test Executable/DLL not found at $EXE_PATH or $DLL_PATH"
    exit 1
fi