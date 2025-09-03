#!/bin/zsh
set -e
set -o pipefail

CPP_VER_PARAM=${1:-17}
mkdir -p XCodeProj
cd XCodeProj

require_lldb() {
  if ! command -v lldb >/dev/null 2>&1; then
    echo "lldb not found. Please install Xcode Command Line Tools: xcode-select --install"
    exit 127
  fi
}

run_under_lldb_with_core() {
  local exe_path="$1"

  require_lldb

  local exebase="$(basename "$exe_path")"
  local core_dir="$PWD/cores"
  mkdir -p "$core_dir"
  local timestamp
  timestamp="$(date +%Y%m%d-%H%M%S)"
  local core_path="$core_dir/${exebase}.${timestamp}.core"

  echo "Running under LLDB: $exe_path"
  set +e
  lldb -Q --batch \
    -o "settings set auto-confirm true" \
    -o "process handle SIGABRT -p true -s false -n false" \
    -o "process handle SIGSEGV -p true -s false -n false" \
    -o "process handle SIGBUS  -p true -s false -n false" \
    -o "process handle SIGILL  -p true -s false -n false" \
    -o "process handle SIGTRAP -p true -s false -n false" \
    -o "breakpoint set --name __assert_rtn" \
    -o "breakpoint set --name __abort_with_payload" \
    -o "breakpoint set --name __pthread_kill" \
    -o "target stop-hook add -o \"thread backtrace all\" -o \"register read\" -o \"image list\" -o \"shell echo Core saved to $core_path\" -o \"quit 1\"" \
    -o "run" \
    -o "script import lldb,sys;pr=lldb.process;ec=pr.GetExitStatus() if pr and pr.IsValid() else 0; sys.exit(0 if ec==0 else 1)" \
    -- "$exe_path"
  local lldb_ec=$?
  set -e

  if [ $lldb_ec -eq 0 ]; then
    echo "Test succeeded."
  else
    echo "Test failed under LLDB. If an assert/crash occurred, core should be at: $core_path"
    return $lldb_ec
  fi
}

# Debug build
cmake ../../../../test -DTARGET_PLATFORM:STRING=mac -DCPP_VER=$CPP_VER_PARAM -G "Xcode"
xcodebuild -project BqLogUnitTest.xcodeproj -scheme BqLogUnitTest -configuration Debug OTHER_LDFLAGS="-ld_classic"
run_under_lldb_with_core "./Debug/BqLogUnitTest"

# RelWithDebInfo build
cmake ../../../../test -DTARGET_PLATFORM:STRING=mac -DCPP_VER=$CPP_VER_PARAM -G "Xcode"
xcodebuild -project BqLogUnitTest.xcodeproj -scheme BqLogUnitTest -configuration RelWithDebInfo OTHER_LDFLAGS="-ld_classic"
run_under_lldb_with_core "./RelWithDebInfo/BqLogUnitTest"

cd ..