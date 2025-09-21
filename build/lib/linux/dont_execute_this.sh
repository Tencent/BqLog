#!/usr/bin/env bash
set -euo pipefail
# ------------------------------------------------------------
# Unified build/generate/pack driver for Linux
#
# Usage (all parameters optional; missing ones will be prompted):
#   dont_execute_this.sh all [arch] [compiler] [java] [node]
#   dont_execute_this.sh build [arch] [compiler] [java] [node] [static_lib|dynamic_lib]
#   dont_execute_this.sh gen-vsproj [arch] [compiler] [java] [node] [static_lib|dynamic_lib]
#   dont_execute_this.sh pack [arch]
#
# Normalized values:
#   arch      : arm64 | x86 | x86_64 | native
#   compiler  : gcc | clang
#   java,node : ON | OFF
#   lib type  : static_lib | dynamic_lib
# ------------------------------------------------------------

# Ensure running under bash
if [[ -z "${BASH_VERSION:-}" ]]; then
  echo "Please run with bash: bash $0 ..."
  exit 2
fi

BUILD_JOBS="${BUILD_JOBS:-10}"

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." >/dev/null 2>&1 && pwd)"
SRC_DIR="${REPO_ROOT}/src"
PACK_DIR="${REPO_ROOT}/pack"

BuildConfigs=(Debug MinSizeRel RelWithDebInfo Release)

ACTION="${1:-}"
ARG1="${2:-}"
ARG2="${3:-}"
ARG3="${4:-}"
ARG4="${5:-}"
ARG5="${6:-}"

ARCH_PARAM=""
COMPILER_TYPE=""
JAVA_SUPPORT=""
NODE_API_SUPPORT=""
BUILD_LIB_TYPE=""

to_upper() { echo "$1" | tr '[:lower:]' '[:upper:]'; }
to_lower() { echo "$1" | tr '[:upper:]' '[:lower:]'; }

normalize_arch() {
  case "$(to_lower "$1")" in
    x86_64) echo "x86_64" ;;
    x86)    echo "x86" ;;
    arm64)  echo "arm64" ;;
    native) echo "native" ;;
    *)      echo "" ;;
  esac
}
normalize_compiler() {
  case "$(to_lower "$1")" in
    gcc)   echo "gcc" ;;
    clang) echo "clang" ;;
    *)     echo "" ;;
  esac
}
normalize_onoff() {
  case "$(to_lower "$1")" in
    on|yes|1)  echo "ON" ;;
    off|no|0)  echo "OFF" ;;
    *)         echo "" ;;
  esac
}
normalize_build_lib_type() {
  case "$(to_lower "$1")" in
    static_lib)  echo "static_lib" ;;
    dynamic_lib) echo "dynamic_lib" ;;
    *)           echo "" ;;
  esac
}

ask_arch() {
  echo
  echo "Select architecture:"
  echo "  [N] native"
  echo "  [X] x86_64"
  echo "  [E] x86"
  echo "  [A] arm64"
  while true; do
    read -r -p "Enter choice (N/X/E/A): " c
    case "$(to_upper "$c")" in
      N) ARCH_PARAM="native";  break ;;
      X) ARCH_PARAM="x86_64";  break ;;
      E) ARCH_PARAM="x86";     break ;;
      A) ARCH_PARAM="arm64";   break ;;
    esac
  done
}
detect_host_arch() {
  case "$(uname -m)" in
    aarch64) ARCH_PARAM="arm64" ;;
    x86_64)  ARCH_PARAM="x86_64" ;;
    i386|i486|i586|i686) ARCH_PARAM="x86" ;;
    *)       ARCH_PARAM="native" ;;
  esac
}
ask_compiler() {
  echo
  echo "Select compiler:"
  echo "  [G] GCC"
  echo "  [C] Clang"
  while true; do
    read -r -p "Enter choice (G/C): " c
    case "$(to_upper "$c")" in
      G) COMPILER_TYPE="gcc";   break ;;
      C) COMPILER_TYPE="clang"; break ;;
    esac
  done
}
ask_yes_no() {
  local prompt="$1"
  while true; do
    read -r -p "$prompt (Y/N): " c
    case "$(to_upper "$c")" in
      Y) echo "ON";  break ;;
      N) echo "OFF"; break ;;
    esac
  done
}
ask_build_lib_type() {
  echo
  echo "Select library type:"
  echo "  [S] static library"
  echo "  [D] dynamic library"
  while true; do
    read -r -p "Enter choice (S/D): " c
    case "$(to_upper "$c")" in
      S) BUILD_LIB_TYPE="static_lib";  break ;;
      D) BUILD_LIB_TYPE="dynamic_lib"; break ;;
    esac
  done
}

ensure_common_params() {
  [[ -z "$ARCH_PARAM"       ]] && ask_arch
  [[ -z "$COMPILER_TYPE"    ]] && ask_compiler
  [[ -z "$JAVA_SUPPORT"     ]] && JAVA_SUPPORT="$(ask_yes_no "Enable Java/JNI support?")"
  [[ -z "$NODE_API_SUPPORT" ]] && NODE_API_SUPPORT="$(ask_yes_no "Enable Node-API (Node.js) support?")"
  ensure_resolved_toolchain
}
ensure_arch_only() {
  detect_host_arch
  [[ -z "$ARCH_PARAM" ]] && ARCH_PARAM="native"
}

# Parse args and normalize (legacy alias handling)
if [[ "${ACTION:-}" == "dynamic_lib" || "${ACTION:-}" == "static_lib" ]]; then
  BUILD_LIB_TYPE="$(normalize_build_lib_type "$ACTION")"
  ACTION="build"
fi
[[ -z "${ACTION:-}" ]] && ACTION="all"

ARCH_PARAM="$(normalize_arch "$ARG1")"
COMPILER_TYPE="$(normalize_compiler "$ARG2")"
JAVA_SUPPORT="$(normalize_onoff "$ARG3")"
NODE_API_SUPPORT="$(normalize_onoff "$ARG4")"
BUILD_LIB_TYPE="$(normalize_build_lib_type "$ARG5")"

# Echo parsed params summary so "no output" never happens
echo "Parsed params:"
echo "  ACTION:        ${ACTION:-}"
echo "  ARCH_PARAM:    ${ARCH_PARAM:-<unset>}"
echo "  COMPILER_TYPE: ${COMPILER_TYPE:-<unset>}"
echo "  JAVA_SUPPORT:  ${JAVA_SUPPORT:-<unset>}"
echo "  NODE_API:      ${NODE_API_SUPPORT:-<unset>}"
echo "  BUILD_LIB_TYPE:${BUILD_LIB_TYPE:-<unset>}"

# Resolve C++ compiler and CMake cross arguments + set USER_DEF_ARCH
RESOLVED_CXX_BIN=""
ARCH_ARGS=() # bash array
RESOLVE_OK=0

has_prog() { command -v "$1" >/dev/null 2>&1; }

resolve_cxx_and_arch_args() {
  local comp="$1" arch="$2"
  RESOLVE_OK=1
  ARCH_ARGS=()

  # USER_DEF_ARCH env for CMake
  if [[ "$arch" == "native" || -z "$arch" ]]; then
    unset USER_DEF_ARCH || true
  else
    export USER_DEF_ARCH="$arch"
  fi

  if [[ "$comp" == "gcc" ]]; then
    case "$arch" in
      native)
        RESOLVED_CXX_BIN="g++"
        ;;
      x86_64)
        if [[ "$(uname -m)" == "x86_64" ]]; then
          RESOLVED_CXX_BIN="g++"; ARCH_ARGS=(-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=x86_64)
        elif has_prog x86_64-linux-gnu-g++; then
          RESOLVED_CXX_BIN="x86_64-linux-gnu-g++"; ARCH_ARGS=(-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=x86_64)
        else
          echo "Toolchain missing: x86_64-linux-gnu-g++" >&2; RESOLVE_OK=0
        fi
        ;;
      x86)
        if has_prog i686-linux-gnu-g++; then
          RESOLVED_CXX_BIN="i686-linux-gnu-g++"; ARCH_ARGS=(-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=x86)
        else
          # fallback to -m32 if available (may still fail if 32-bit libs missing)
          RESOLVED_CXX_BIN="g++"; ARCH_ARGS=(-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=x86 -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_EXE_LINKER_FLAGS=-m32 -DCMAKE_SHARED_LINKER_FLAGS=-m32)
        fi
        ;;
      arm64)
        if has_prog aarch64-linux-gnu-g++; then
          RESOLVED_CXX_BIN="aarch64-linux-gnu-g++"; ARCH_ARGS=(-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64)
        else
          echo "Toolchain missing: aarch64-linux-gnu-g++" >&2; RESOLVE_OK=0
        fi
        ;;
      *)
        RESOLVED_CXX_BIN="g++"
        ;;
    esac
  elif [[ "$comp" == "clang" ]]; then
    RESOLVED_CXX_BIN="clang++"
    case "$arch" in
      native) ;;
      x86_64) ARCH_ARGS=(-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=x86_64 -DCMAKE_CXX_COMPILER_TARGET=x86_64-linux-gnu) ;;
      x86)    ARCH_ARGS=(-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=x86 -DCMAKE_CXX_COMPILER_TARGET=i686-linux-gnu -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_EXE_LINKER_FLAGS=-m32 -DCMAKE_SHARED_LINKER_FLAGS=-m32) ;;
      arm64)
        ARCH_ARGS=(-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DCMAKE_CXX_COMPILER_TARGET=aarch64-linux-gnu)
        if has_prog ld.lld; then
          ARCH_ARGS+=(-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld)
        fi
        [[ -d /usr/aarch64-linux-gnu ]] && ARCH_ARGS+=(-DCMAKE_SYSROOT=/usr/aarch64-linux-gnu)
        ;;
      *) ;;
    esac
  else
    echo "Unsupported compiler: $comp" >&2
    RESOLVE_OK=0
  fi
}

ensure_resolved_toolchain() {
  resolve_cxx_and_arch_args "${COMPILER_TYPE:-}" "${ARCH_PARAM:-}"
  while [[ $RESOLVE_OK -ne 1 ]]; do
    echo
    echo "The selected arch/compiler requires a missing toolchain."
    echo "Options:"
    echo "  [I] Show install hint (Debian/Ubuntu)"
    echo "  [N] Switch to native"
    echo "  [A] Change arch"
    echo "  [C] Change compiler"
    echo "  [Q] Quit"
    read -r -p "Enter choice (I/N/A/C/Q): " c
    case "$(to_upper "$c")" in
      I)
        case "${ARCH_PARAM}" in
          arm64)  echo "sudo apt-get install g++-aarch64-linux-gnu" ;;
          x86_64) echo "sudo apt-get install g++-x86-64-linux-gnu (or use clang with --target)" ;;
          x86)    echo "sudo apt-get install g++-i686-linux-gnu (or ensure 32-bit multilib: sudo apt-get install gcc-multilib g++-multilib)" ;;
        esac
        ;;
      N) ARCH_PARAM="native" ;;
      A) ask_arch ;;
      C) ask_compiler ;;
      Q) exit 2 ;;
    esac
    resolve_cxx_and_arch_args "${COMPILER_TYPE:-}" "${ARCH_PARAM:-}"
  done
}

build_one() {
  local BUILD_LIB_TYPE_ARG="$1"
  if [[ "$BUILD_LIB_TYPE_ARG" != "static_lib" && "$BUILD_LIB_TYPE_ARG" != "dynamic_lib" ]]; then
    echo "Invalid BUILD_LIB_TYPE: $BUILD_LIB_TYPE_ARG"
    exit 2
  fi
  ensure_resolved_toolchain

  local SHARED="OFF"
  [[ "$BUILD_LIB_TYPE_ARG" == "dynamic_lib" ]] && SHARED="ON"

  local GEN="Unix Makefiles"

  echo
  echo "===== Build Configuration ====="
  echo "  ACTION            : build (${BUILD_LIB_TYPE_ARG})"
  echo "  ARCH              : ${ARCH_PARAM:-native}"
  echo "  COMPILER          : ${COMPILER_TYPE}"
  echo "  CXX               : ${RESOLVED_CXX_BIN}"
  echo "  JAVA_SUPPORT      : ${JAVA_SUPPORT}"
  echo "  NODE_API_SUPPORT  : ${NODE_API_SUPPORT}"
  echo "  Generator         : ${GEN}"
  ((${#ARCH_ARGS[@]})) && echo "  CMake cross args  : ${ARCH_ARGS[*]}"
  echo "  USER_DEF_ARCH     : ${USER_DEF_ARCH:-}"
  echo "================================="
  echo

  local PROJ_DIR="${SCRIPT_DIR}/VSProj_${BUILD_LIB_TYPE_ARG}"
  rm -rf "${PROJ_DIR}"
  mkdir -p "${PROJ_DIR}"
  pushd "${PROJ_DIR}" >/dev/null

  for cfg in "${BuildConfigs[@]}"; do
    echo "  BUILD FOR CONFIG ${BUILD_LIB_TYPE_ARG} : ${cfg}"
    cmake "${SRC_DIR}" -G "${GEN}" \
      -DTARGET_PLATFORM:STRING=linux \
      -DBUILD_LIB_TYPE="${BUILD_LIB_TYPE_ARG}" \
      -DJAVA_SUPPORT:BOOL="${JAVA_SUPPORT}" \
      -DNODE_API_SUPPORT:BOOL="${NODE_API_SUPPORT}" \
      -DCMAKE_BUILD_TYPE="${cfg}" \
      -DBUILD_SHARED_LIBS="${SHARED}" \
      -DCMAKE_CXX_COMPILER="${RESOLVED_CXX_BIN}" \
      -DCMAKE_INSTALL_PREFIX="${REPO_ROOT}/install" \
      "${ARCH_ARGS[@]}"

    cmake --build . -- -j "${BUILD_JOBS}"
    cmake --install .
  done

  popd >/dev/null
}

gen_vsproj() {
  ensure_resolved_toolchain

  local SHARED="OFF"
  [[ "$BUILD_LIB_TYPE" == "dynamic_lib" ]] && SHARED="ON"

  local GEN="Unix Makefiles"

  echo
  echo "===== VS Project Generation ====="
  echo "  ARCH              : ${ARCH_PARAM:-native}"
  echo "  COMPILER          : ${COMPILER_TYPE}"
  echo "  CXX               : ${RESOLVED_CXX_BIN}"
  echo "  JAVA_SUPPORT      : ${JAVA_SUPPORT}"
  echo "  NODE_API_SUPPORT  : ${NODE_API_SUPPORT}"
  echo "  BUILD_LIB_TYPE    : ${BUILD_LIB_TYPE}"
  echo "  Generator         : ${GEN}"
  ((${#ARCH_ARGS[@]})) && echo "  CMake cross args  : ${ARCH_ARGS[*]}"
  echo "  USER_DEF_ARCH     : ${USER_DEF_ARCH:-}"
  echo "================================="
  echo

  local PROJ_DIR="${SCRIPT_DIR}/VSProj"
  rm -rf "${PROJ_DIR}"
  mkdir -p "${PROJ_DIR}"
  pushd "${PROJ_DIR}" >/dev/null

  cmake "${SRC_DIR}" -G "${GEN}" \
    -DTARGET_PLATFORM:STRING=linux \
    -DBUILD_TYPE="${BUILD_LIB_TYPE}" \
    -DJAVA_SUPPORT:BOOL="${JAVA_SUPPORT}" \
    -DNODE_API_SUPPORT:BOOL="${NODE_API_SUPPORT}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_SHARED_LIBS="${SHARED}" \
    -DCMAKE_CXX_COMPILER="${RESOLVED_CXX_BIN}" \
    -DCMAKE_INSTALL_PREFIX="${REPO_ROOT}/install" \
    "${ARCH_ARGS[@]}"

  popd >/dev/null
}

do_pack() {
  echo
  echo "===== Packaging ====="
  echo "  ARCH              : ${ARCH_PARAM:-native}"
  echo "================================="
  echo

  local PACK_WORK_DIR="${SCRIPT_DIR}/pack"
  rm -rf "${PACK_WORK_DIR}"
  mkdir -p "${PACK_WORK_DIR}"
  pushd "${PACK_WORK_DIR}" >/dev/null

  cmake "${PACK_DIR}" \
    -DTARGET_PLATFORM:STRING=linux \
    -DPACKAGE_NAME:STRING=bqlog-lib

  cmake --build . --target package

  popd >/dev/null
}

# Dispatch
if [[ "$ACTION" == "all" ]]; then
  ensure_common_params
  build_one dynamic_lib
  build_one static_lib
  do_pack
  echo "---------"; echo "Finished!"; echo "---------"
  exit 0
elif [[ "$ACTION" == "build" ]]; then
  ensure_common_params
  [[ -z "$BUILD_LIB_TYPE" ]] && ask_build_lib_type
  build_one "$BUILD_LIB_TYPE"
  echo "---------"; echo "Finished!"; echo "---------"
  exit 0
elif [[ "$ACTION" == "gen-vsproj" ]]; then
  ensure_common_params
  [[ -z "$BUILD_LIB_TYPE" ]] && ask_build_lib_type
  gen_vsproj
  echo "---------"; echo "Finished!"; echo "---------"
  exit 0
elif [[ "$ACTION" == "pack" ]]; then
  ensure_arch_only
  do_pack
  echo "---------"; echo "Finished!"; echo "---------"
  exit 0
else
  echo "Unknown action: $ACTION"
  echo "Supported: all | build | gen-vsproj | pack | dynamic_lib | static_lib"
  exit 2
fi