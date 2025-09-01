#!/bin/sh
set -e

CPP_VER_PARAM=${1:-17}

mkdir -p CMakeFiles
cd CMakeFiles

ulimit -c unlimited

if command -v sysctl >/dev/null 2>&1 && sysctl -n kern.ostype 2>/dev/null | grep -qi freebsd; then
  echo "Configuring FreeBSD core dump settings..."
  if [ "$(id -u)" -eq 0 ]; then
    sysctl kern.coredump=1 || true
    sysctl kern.corefile=core.%P || true
    sysctl kern.sugid_coredump=1 || true
  else
    sudo sysctl kern.coredump=1 || true
    sudo sysctl kern.corefile=core.%P || true
    sudo sysctl kern.sugid_coredump=1 || true
  fi
fi

echo "Core files will be searched as: core, core.*, *.core in $(pwd)"

rm -f core core.* *.core 2>/dev/null || true

have_lldb=0
have_gdb=0
if command -v lldb >/dev/null 2>&1; then
  have_lldb=1
fi
if command -v gdb >/devnull 2>&1; then
  have_gdb=1
fi

find_latest_core() {
  latest=""
  latest=$( (ls -t core 2>/dev/null; ls -t core.* 2>/dev/null; ls -t *.core 2>/dev/null) 2>/dev/null | head -n 1)
  echo "$latest"
}

analyze_core() {
  exe="$1"
  corefile="$2"

  echo "Analyzing core dump: $corefile"

  if [ "$have_lldb" -eq 1 ]; then
    echo "Using LLDB to print all threads' backtraces..."
    lldb -b -o "target create -c $corefile $exe" \
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

  echo "No debugger (lldb/gdb) found. Please install lldb or gdb to analyze core files."
}

run_with_debugger() {
  exe="$1"

  if [ "$have_lldb" -eq 1 ]; then
    echo "Running with LLDB to capture backtrace..."
    lldb -b -o "run" -o "thread backtrace all" -o "quit" -- "$exe" || true
    return
  fi

  if [ "$have_gdb" -eq 1 ]; then
    echo "Running with GDB to capture backtrace..."
    gdb --batch --quiet \
      -ex "run" \
      -ex "info threads" \
      -ex "thread apply all bt full" \
      -ex "quit" \
      "$exe" || true
    return
  fi

  echo "No debugger (lldb/gdb) found. Cannot run under debugger."
}

build_and_run() {
  build_type="$1"

  echo "=== Building ($build_type) ==="
  CC=clang CXX=clang++ cmake -DTARGET_PLATFORM:STRING=unix -DCMAKE_BUILD_TYPE="$build_type" -DCPP_VER="$CPP_VER_PARAM" ../../../../test
  make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 2)"

  rm -f core core.* *.core 2>/dev/null || true

  echo "=== Running BqLogUnitTest ($build_type) ==="
  ./BqLogUnitTest
  exit_code=$?

  if [ $exit_code -ne 0 ]; then
    echo "Test failed with exit code $exit_code."

    CORE_FILE=$(find_latest_core)
    if [ -n "$CORE_FILE" ] && [ -f "$CORE_FILE" ]; then
      analyze_core "./BqLogUnitTest" "$CORE_FILE"
    else
      echo "No core dump found in $(pwd)"
      echo "Running under debugger to capture stack traces directly..."
      run_with_debugger "./BqLogUnitTest"
    fi
    exit 1
  fi
}

build_and_run "Debug"
build_and_run "RelWithDebInfo"

cd ..