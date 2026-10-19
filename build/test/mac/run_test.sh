#!/bin/zsh
set -e
CPP_VER_PARAM=${1:-17}
mkdir -p XCodeProj
cd XCodeProj

cmake ../../../../test/cpp -DTARGET_PLATFORM:STRING=mac -DCPP_VER=$CPP_VER_PARAM -G "Xcode"
xcodebuild -project BqLogUnitTest.xcodeproj -scheme BqLogUnitTest -configuration Debug
./Debug/BqLogUnitTest
exit_code=$?
if [ $exit_code -eq 0 ]; then
    echo "Test succeeded."
else
    echo "Test failed."
    exit 1
fi

cmake ../../../../test/cpp -DTARGET_PLATFORM:STRING=mac -DCPP_VER=$CPP_VER_PARAM -G "Xcode"
xcodebuild -project BqLogUnitTest.xcodeproj -scheme BqLogUnitTest -configuration RelWithDebInfo
./RelWithDebInfo/BqLogUnitTest
exit_code=$?
if [ $exit_code -eq 0 ]; then
    echo "Test succeeded."
else
    echo "Test failed."
    exit 1
fi
cd ..;

# The module entry test lives in a build tree of its own: cmake builds C++20 modules with the
# Ninja and Visual Studio generators only, and the Xcode generator above cannot produce that
# target at all.
if [ "$CPP_VER_PARAM" -ge 20 ]; then
    mkdir -p NinjaProj
    cd NinjaProj
    # The module target needs a toolchain that implements C++20 modules. Prefer the upstream
    # LLVM that CI installs (brew install llvm); fall back to whatever clang++ is in PATH.
    MODULE_CXX="$(command -v clang++)"
    MODULE_CC="$(command -v clang)"
    if command -v brew >/dev/null 2>&1; then
        LLVM_PREFIX="$(brew --prefix llvm 2>/dev/null || true)"
        if [ -n "$LLVM_PREFIX" ] && [ -x "$LLVM_PREFIX/bin/clang++" ]; then
            MODULE_CXX="$LLVM_PREFIX/bin/clang++"
            MODULE_CC="$LLVM_PREFIX/bin/clang"
        fi
    fi
    echo "Module entry test compiler: $MODULE_CXX"
    cmake ../../../../test/cpp -DTARGET_PLATFORM:STRING=mac -DCPP_VER=$CPP_VER_PARAM -DCMAKE_BUILD_TYPE=Debug -G Ninja \
        -DCMAKE_C_COMPILER="$MODULE_CC" -DCMAKE_CXX_COMPILER="$MODULE_CXX"
    cmake --build . --target BqLogModuleUnitTest --parallel
    ./BqLogModuleUnitTest
    cd ..;
    echo "Module entry test succeeded."
fi
