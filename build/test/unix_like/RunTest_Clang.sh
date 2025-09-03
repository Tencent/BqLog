#!/bin/sh
set -e

CPP_VER_PARAM=${1:-17}

mkdir -p CMakeFiles
cd CMakeFiles

# Detect FreeBSD
is_freebsd=0
if command -v sysctl >/dev/null 2>&1; then
  if sysctl -n kern.ostype 2>/dev/null | grep -qi freebsd; then
    is_freebsd=1
  fi
fi

# Make sure core dumps are enabled
# (ulimit is per-shell; apply unconditionally, it's harmless elsewhere)
ulimit -c unlimited 2>/dev/null || true

# FreeBSD-specific: ensure core dumps are actually produced and named predictably
if [ "$is_freebsd" -eq 1 ]; then
  echo "Configuring FreeBSD core dump settings..."
  if [ "$(id -u)" -eq 0 ]; then
    sysctl kern.coredump=1 || true
    sysctl kern.corefile=core.%P || true
    sysctl kern.sugid_coredump=1 || true
  else
    # Use sudo if available; ignore failures (best-effort)
    if command -v sudo >/dev/null 2>&1; then
      sudo sysctl kern.coredump=1 || true
      sudo sysctl kern.corefile=core.%P || true
      sudo sysctl kern.sugid_coredump=1 || true
    fi
  fi
fi

echo "Core files will be searched as: core, core.*, *.core, *.core.*, BqLogUnitTest.core* in $(pwd)"
rm -f core core.* *.core *.core.* BqLogUnitTest.core* 2>/dev/null || true

have_lldb=0
have_gdb=0
if command -v lldb >/dev/null 2>&1; then
  have_lldb=1
fi
# Fix redirection bug (>/dev/null)
if command -v gdb >/dev/null 2>&1; then
  have_gdb=1
fi

find_latest_core() {
  # Try common patterns across BSD/Linux/macOS
  latest=""
  latest=$(
    (
      ls -t core 2>/dev/null
      ls -t core.* 2>/dev/null
      ls -t *.core 2>/dev/null
      ls -t *.core.* 2>/dev/null
      ls -t BqLogUnitTest.core* 2>/dev/null
    ) 2>/dev/null | head -n 1
  )
  echo "$latest"
}

analyze_core() {
  exe="$1"
  corefile="$2"

  echo "Analyzing core dump: $corefile"

  if [ "$have_lldb" -eq 1 ]; then
    echo "Using LLDB to print all threads' backtraces..."
    # The -c <core> form works well on FreeBSD. Keep batch mode (-b). Be tolerant to failures.
    lldb -b -c "$corefile" -- "$exe" \
      -o "settings set stop-disassembly-display never" \
      -o "thread list" \
      -o "thread backtrace all" \
      -o "quit" || true
    return
  fi

  if [ "$have_gdb" -eq 1 ]; then
    echo "Using GDB to print all threads' backtraces..."
    gdb --batch --quiet \
      -ex "set pagination off" \
      -ex "info threads" \
      -ex "thread apply all bt full" \
      -ex "quit" \
      "$exe" "$corefile" || true
    return
  fi

  # On non-FreeBSD, don't treat missing debuggers as an error; just inform.
  if [ "$is_freebsd" -eq 1 ]; then
    echo "No debugger (lldb/gdb) found. Please install lldb or gdb to analyze core files on FreeBSD."
  else
    echo "No debugger (lldb/gdb) found. Skipping stacktrace (best-effort on non-FreeBSD)."
  fi
}

run_with_debugger() {
  exe="$1"

  if [ "$have_lldb" -eq 1 ]; then
    echo "Running with LLDB to capture backtrace..."
    # Run the program; when it crashes LLDB should stop and we print all threads' backtraces.
    lldb -b -- "$exe" \
      -o "settings set stop-disassembly-display never" \
      -o "run" \
      -o "thread backtrace all" \
      -o "quit" || true
    return
  fi

  if [ "$have_gdb" -eq 1 ]; then
    echo "Running with GDB to capture backtrace..."
    gdb --batch --quiet \
      -ex "set pagination off" \
      -ex "run" \
      -ex "info threads" \
      -ex "thread apply all bt full" \
      -ex "quit" \
      "$exe" || true
    return
  fi

  if [ "$is_freebsd" -eq 1 ]; then
    echo "No debugger (lldb/gdb) found. Cannot run under debugger on FreeBSD."
  else
    echo "No debugger (lldb/gdb) found. Skipping debugger run (best-effort on non-FreeBSD)."
  fi
}

build_and_run() {
  build_type="$1"

  echo "=== Building ($build_type) ==="
  CC=clang CXX=clang++ cmake -DTARGET_PLATFORM:STRING=unix -DCMAKE_BUILD_TYPE="$build_type" -DCPP_VER="$CPP_VER_PARAM" ../../../../test
  make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 2)"

  rm -f core core.* *.core *.core.* BqLogUnitTest.core* 2>/dev/null || true

  echo "=== Running BqLogUnitTest ($build_type) ==="
  ./BqLogUnitTest
  exit_code=$?

  if [ $exit_code -ne 0 ]; then
    echo "Test failed with exit code $exit_code."

    CORE_FILE=$(find_latest_core)
    if [ -n "$CORE_FILE" ] && [ -f "$CORE_FILE" ]; then
      analyze_core "./BqLogUnitTest" "$CORE_FILE"
    else
      if [ "$is_freebsd" -eq 1 ]; then
        echo "No core dump found in $(pwd) on FreeBSD."
        echo "Running under debugger to capture stack traces directly..."
        run_with_debugger "./BqLogUnitTest"
      else
        echo "No core dump found in $(pwd) (non-FreeBSD). Skipping debugger run (best-effort)."
      fi
    fi
    # Still return the test failure (this script is for CI). Not reporting stacktrace is not an additional error.
    exit 1
  fi
}

build_and_run "Debug"
build_and_run "RelWithDebInfo"

cd ..