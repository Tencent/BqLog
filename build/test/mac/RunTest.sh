#!/bin/zsh
set -e

CPP_VER_PARAM=${1:-17}
mkdir -p XCodeProj
cd XCodeProj

# Helper: run a test binary; if it fails, print stack trace via LLDB, then exit 1
run_with_stacktrace() {
  local exe_path="$1"

  echo "Running: $exe_path"
  set +e
  "$exe_path"
  local exit_code=$?
  set -e

  if [ $exit_code -eq 0 ]; then
    echo "Test succeeded."
    return 0
  fi

  echo "Test failed with exit code $exit_code. Attempting to capture stack trace with LLDB..."
  if command -v lldb >/dev/null 2>&1; then
    # Run under LLDB in batch mode to capture a backtrace.
    # Note: This will re-run the test binary under the debugger.
    lldb -Q --batch -o "run" -o "bt all" -o "register read" -o "image list" -- "$exe_path" || true
  else
    echo "lldb not found. Install Xcode Command Line Tools (xcode-select --install) to enable stack traces."
  fi

  exit 1
}

# Debug build (with classic linker via LDFLAGS if desired by your setup)
cmake ../../../../test -DTARGET_PLATFORM:STRING=mac -DCPP_VER=$CPP_VER_PARAM -G "Xcode" OTHER_LDFLAGS="-ld_classic"
xcodebuild -project BqLogUnitTest.xcodeproj -scheme BqLogUnitTest -configuration Debug
run_with_stacktrace "./Debug/BqLogUnitTest"

# RelWithDebInfo build (pass classic linker here if needed for this configuration)
cmake ../../../../test -DTARGET_PLATFORM:STRING=mac -DCPP_VER=$CPP_VER_PARAM -G "Xcode"
xcodebuild -project BqLogUnitTest.xcodeproj -scheme BqLogUnitTest -configuration RelWithDebInfo OTHER_LDFLAGS="-ld_classic"
run_with_stacktrace "./RelWithDebInfo/BqLogUnitTest"

cd ..