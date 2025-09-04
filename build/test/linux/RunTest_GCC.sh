#!/bin/bash
set -e
CPP_VER_PARAM=${1:-17}
mkdir -p CMakeFiles
cd CMakeFiles

ulimit -c unlimited || true
echo "Setting core dump pattern to 'core.%p'..."
sudo sysctl -w kernel.core_pattern=core.%p || echo "Failed to set core pattern (might lack permissions)."

gdb_run() {
  local exe="$1"

  # Run under gdb in batch mode:
  # - catch fatal signals and immediately: gcore, backtrace, quit with non-zero
  # - if program exits with non-zero (but no signal), propagate the exit code
  set +e
  gdb --batch --quiet "$exe" \
    -ex "set pagination off" \
    -ex "set confirm off" \
    \
    -ex "catch signal SIGSEGV" \
    -ex "commands" \
    -ex "echo [GDB] Caught SIGSEGV, dumping core and backtrace...\n" \
    -ex "gcore core" \
    -ex "thread apply all bt full" \
    -ex "quit 1" \
    -ex "end" \
    \
    -ex "catch signal SIGABRT" \
    -ex "commands" \
    -ex "echo [GDB] Caught SIGABRT, dumping core and backtrace...\n" \
    -ex "gcore core" \
    -ex "thread apply all bt full" \
    -ex "quit 1" \
    -ex "end" \
    \
    -ex "catch signal SIGBUS" \
    -ex "commands" \
    -ex "echo [GDB] Caught SIGBUS, dumping core and backtrace...\n" \
    -ex "gcore core" \
    -ex "thread apply all bt full" \
    -ex "quit 1" \
    -ex "end" \
    \
    -ex "catch signal SIGFPE" \
    -ex "commands" \
    -ex "echo [GDB] Caught SIGFPE, dumping core and backtrace...\n" \
    -ex "gcore core" \
    -ex "thread apply all bt full" \
    -ex "quit 1" \
    -ex "end" \
    \
    -ex "catch signal SIGILL" \
    -ex "commands" \
    -ex "echo [GDB] Caught SIGILL, dumping core and backtrace...\n" \
    -ex "gcore core" \
    -ex "thread apply all bt full" \
    -ex "quit 1" \
    -ex "end" \
    \
    -ex "run" \
    -ex "set \$ec = \$_exitcode" \
    -ex "if \$ec != 0" \
    -ex "echo [GDB] Program exited with code \$ec\n" \
    -ex "quit \$ec" \
    -ex "end"
  local gdb_ec=$?
  set -e
  return $gdb_ec
}

build_and_test() {
  local build_type="$1"
  echo "==== Building ($build_type) ===="
  CC=gcc CXX=g++ cmake -DTARGET_PLATFORM:STRING=linux -DCMAKE_BUILD_TYPE="$build_type" -DCPP_VER="$CPP_VER_PARAM" ../../../../test
  make -j"$(nproc)"

  echo "==== Running BqLogUnitTest under gdb ($build_type) ===="
  if gdb_run ./BqLogUnitTest; then
    echo "[$build_type] Test succeeded."
  else
    echo "[$build_type] Test failed. If a crash occurred, a core file like core.<pid> should be present."
    echo "core_pattern: $(cat /proc/sys/kernel/core_pattern 2>/dev/null || true)"
    echo "ulimit -c: $(ulimit -c)"
    exit 1
  fi
}

# 1) Debug
build_and_test Debug

# 2) RelWithDebInfo
build_and_test RelWithDebInfo

cd ..
echo "All done."