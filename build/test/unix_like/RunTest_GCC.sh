#!/bin/sh
set -e

CPP_VER_PARAM=${1:-17}

mkdir -p CMakeFiles
cd CMakeFiles

# Best-effort enable core dumps; harmless if unsupported.
ulimit -c unlimited 2>/dev/null || true

# Check LLDB availability
have_lldb=0
if command -v lldb >/dev/null 2>&1; then
  have_lldb=1
fi

run_with_lldb() {
  exe="$1"

  exebase="$(basename "$exe")"
  core_dir="$PWD/cores"
  mkdir -p "$core_dir"
  ts="$(date +%Y%m%d-%H%M%S)"
  core_path="$core_dir/${exebase}.${ts}.core"

  echo "Running under LLDB: $exe"
  echo "On assert/crash, core will be saved to: $core_path"

  # Temporarily disable errexit to capture LLDB exit status reliably.
  set +e
  lldb -Q --batch \
    -o "settings set auto-confirm true" \
    -o "settings set stop-disassembly-display never" \
    -o "process handle SIGABRT -p true -s false -n false" \
    -o "process handle SIGSEGV -p true -s false -n false" \
    -o "process handle SIGBUS  -p true -s false -n false" \
    -o "process handle SIGILL  -p true -s false -n false" \
    -o "process handle SIGTRAP -p true -s false -n false" \
    -o "breakpoint set --name __assert" \
    -o "breakpoint set --name __assert_rtn" \
    -o "breakpoint set --name __assert_fail" \
    -o "breakpoint set --name abort" \
    -o "breakpoint set --name __pthread_kill" \
    -o "target stop-hook add -o 'thread backtrace all' -o 'register read' -o 'image list' -o 'process save-core $core_path' -o 'shell echo Core saved to $core_path' -o 'quit 1'" \
    -o "run" \
    -o "script import lldb,sys; pr=lldb.process; ec=pr.GetExitStatus() if pr and pr.IsValid() else 0; sys.exit(0 if ec==0 else 1)" \
    -- "$exe"
  lldb_ec=$?
  set -e

  if [ $lldb_ec -eq 0 ]; then
    echo "Test succeeded under LLDB."
    return 0
  else
    echo "Test failed under LLDB (exit $lldb_ec). If a crash/assert occurred, core should be at: $core_path"
    return $lldb_ec
  fi
}

run_normally() {
  exe="$1"
  echo "Running normally (no debugger): $exe"
  set +e
  "$exe"
  ec=$?
  set -e
  if [ $ec -eq 0 ]; then
    echo "Test succeeded."
    return 0
  else
    echo "Test failed with exit code $ec."
    return $ec
  fi
}

build_and_run() {
  build_type="$1"

  echo "=== Building ($build_type) with gcc ==="
  cmake -DTARGET_PLATFORM:STRING=unix -DCMAKE_BUILD_TYPE="$build_type" -DCPP_VER="$CPP_VER_PARAM" \
        -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
        ../../../../test
  make

  echo "=== Running BqLogUnitTest ($build_type) ==="
  if [ "$have_lldb" -eq 1 ]; then
    run_with_lldb "./BqLogUnitTest"
  else
    run_normally "./BqLogUnitTest"
  fi
}

build_and_run "Debug"
build_and_run "RelWithDebInfo"

cd ..