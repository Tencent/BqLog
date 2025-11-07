#!/bin/sh
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

set -eu

# Configurations
BuildConfigs="Debug MinSizeRel RelWithDebInfo Release"

# Argument parsing
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

to_lower() { printf "%s" "$1" | tr '[:upper:]' '[:lower:]'; }
to_upper() { printf "%s" "$1" | tr '[:lower:]' '[:upper:]'; }
trim() { printf "%s" "$1" | awk '{$1=$1; print}'; }

normalize_arch() {
  case "$(to_lower "$1")" in
    arm64) echo "arm64" ;;
    x86) echo "x86" ;;
    x86_64|amd64|x64) echo "x86_64" ;;
    native)
      os="$(uname -s 2>/dev/null || echo unknown)"
      m="$(uname -m 2>/dev/null || echo unknown)"
      p="$(uname -p 2>/dev/null || true)"
      mach_arch=""
      if command -v sysctl >/dev/null 2>&1; then
        mach_arch="$(sysctl -n hw.machine_arch 2>/dev/null || true)"
      fi
      isainfo_n=""
      if command -v isainfo >/dev/null 2>&1; then
        isainfo_n="$(isainfo -n 2>/dev/null || true)"
      fi

      arch_guess="$m"
      case "$os" in
        SunOS)
          case "$m" in
            i86pc)
              case "$isainfo_n" in
                amd64) arch_guess="x86_64" ;;
                i386)  arch_guess="x86" ;;
                *)     arch_guess="x86_64" ;;
              esac
              ;;
            sun4u|sun4v)
              arch_guess="sparc64"
              ;;
          esac
          ;;
        NetBSD)
          if [ "$m" = "evbarm" ] || echo "$m" | grep -qi 'arm'; then
            a="${mach_arch:-$p}"
            case "$a" in
              aarch64) arch_guess="arm64" ;;
              earmv7hf*|earmv7*) arch_guess="armv7" ;;
              arm*) arch_guess="arm" ;;
            esac
          fi
          ;;
        FreeBSD|DragonFly|OpenBSD)
          case "$m" in
            aarch64|arm64) arch_guess="arm64" ;;
          esac
          ;;
        Darwin)
          case "$m" in
            arm64) arch_guess="arm64" ;;
            x86_64) arch_guess="x86_64" ;;
          esac
          ;;
      esac

      case "$arch_guess" in
        x86_64|amd64)            echo "x86_64" ;;
        i386|i486|i586|i686|i86pc|i86) echo "x86" ;;
        aarch64|arm64)           echo "arm64" ;;
        armv8*|armv7*|armv6*|armv5*|arm|earm*|earmv*) echo "arm" ;;
        ppc64le)                 echo "ppc64le" ;;
        ppc64|powerpc64|ppc64*)  echo "ppc64" ;;
        ppc|powerpc|powerpcspe)  echo "ppc" ;;
        mips64el)                echo "mips64el" ;;
        mips64)                  echo "mips64" ;;
        mips|mipsel|mipseb)      echo "mips" ;;
        riscv64|riscv64gc)       echo "riscv64" ;;
        s390x)                   echo "s390x" ;;
        sparc64|sun4u|sun4v)     echo "sparc64" ;;
        sparc)                   echo "sparc" ;;
        ia64)                    echo "ia64" ;;
        *)
          echo "$arch_guess"
          ;;
      esac
      ;;
    "") echo "" ;;
    *) echo "" ;;
  esac
}
normalize_compiler() {
  case "$(to_lower "$1")" in
    gcc|g) echo "gcc" ;;
    clang|c) echo "clang" ;;
    *) echo "" ;;
  esac
}
normalize_onoff() {
  case "$(to_upper "$(trim "$1")")" in
    ON|YES|TRUE|1|Y) echo "ON" ;;
    OFF|NO|FALSE|0|N) echo "OFF" ;;
    *) echo "" ;;
  esac
}
normalize_build_lib_type() {
  case "$(to_lower "$1")" in
    static_lib|static|s) echo "static_lib" ;;
    dynamic_lib|shared|dynamic|d) echo "dynamic_lib" ;;
    *) echo "" ;;
  esac
}

# I/O helpers: print to tty/stderr; read from tty when possible
_prompt() {
  msg="$1"
  if [ -w /dev/tty ] 2>/dev/null; then
    printf "%s" "$msg" > /dev/tty
  else
    printf "%s" "$msg" >&2
  fi
}
_read_tty() {
  varname="$1"
  if [ -r /dev/tty ] 2>/dev/null; then
    IFS= read -r "$varname" < /dev/tty
  else
    IFS= read -r "$varname"
  fi
}

ask_arch() {
  echo
  echo "Select architecture:"
  echo "  [A] arm64"
  echo "  [X] x86"
  echo "  [Z] x86_64 (amd64/x64)"
  echo "  [N] native (autodetect)"
  while :; do
    _prompt "Enter choice (A/X/Z/N): "
    _read_tty c_raw
    c="$(to_lower "$(trim "$c_raw")")"
    case "$c" in
      a|arm64) ARCH_PARAM="arm64"; break ;;
      x|x86) ARCH_PARAM="x86"; break ;;
      z|x86_64|amd64|x64) ARCH_PARAM="x86_64"; break ;;
      n|native) ARCH_PARAM="$(normalize_arch native)"; break ;;
    esac
  done
}
ask_compiler() {
  echo
  echo "Select compiler:"
  echo "  [G] GCC"
  echo "  [C] Clang"
  while :; do
    _prompt "Enter choice (G/C or gcc/clang): "
    _read_tty c_raw
    c="$(to_lower "$(trim "$c_raw")")"
    case "$c" in
      g|gcc) COMPILER_TYPE="gcc"; break ;;
      c|clang) COMPILER_TYPE="clang"; break ;;
    esac
  done
}
ask_yes_no() {
  prompt="$1"
  while :; do
    _prompt "$prompt (Y/N): "
    _read_tty c_raw
    c="$(to_upper "$(trim "$c_raw")")"
    set -- $c
    c="$1"
    case "$c" in
      Y|YES|ON|TRUE|1) echo "ON"; break ;;
      N|NO|OFF|FALSE|0) echo "OFF"; break ;;
    esac
  done
}
ask_build_lib_type() {
  echo
  echo "Select library type:"
  echo "  [S] static_lib"
  echo "  [D] dynamic_lib"
  while :; do
    _prompt "Enter choice (S/D): "
    _read_tty c_raw
    c="$(to_lower "$(trim "$c_raw")")"
    case "$c" in
      s|static|static_lib) BUILD_LIB_TYPE="static_lib"; break ;;
      d|dynamic|shared|dynamic_lib) BUILD_LIB_TYPE="dynamic_lib"; break ;;
    esac
  done
}

ensure_common_params() {
  [ -z "$ARCH_PARAM" ] && ask_arch
  [ -z "$COMPILER_TYPE" ] && ask_compiler
  [ -z "$JAVA_SUPPORT" ] && JAVA_SUPPORT=$(ask_yes_no "Enable Java/JNI support?")
  [ -z "$NODE_API_SUPPORT" ] && NODE_API_SUPPORT=$(ask_yes_no "Enable Node-API (Node.js) support?")
  echo
  echo "Parameters:"
  echo "  ARCH             : $ARCH_PARAM"
  echo "  COMPILER         : $COMPILER_TYPE"
  echo "  JAVA_SUPPORT     : $JAVA_SUPPORT"
  echo "  NODE_API_SUPPORT : $NODE_API_SUPPORT"
  echo
}
ensure_arch_only() {
  ARCH_PARAM="$(normalize_arch native)"
  [ -z "$ARCH_PARAM" ] && ask_arch
}

# Parse/normalize
if [ "$ACTION" = "dynamic_lib" ]; then BUILD_LIB_TYPE="dynamic_lib"; ACTION="build"; fi
if [ "$ACTION" = "static_lib" ]; then BUILD_LIB_TYPE="static_lib"; ACTION="build"; fi
[ -z "$ACTION" ] && ACTION="all"

if [ -n "$ARG1" ]; then ARCH_PARAM="$(normalize_arch "$ARG1")"; fi
COMPILER_TYPE="$(normalize_compiler "$ARG2")"
JAVA_SUPPORT="$(normalize_onoff "$ARG3")"
NODE_API_SUPPORT="$(normalize_onoff "$ARG4")"
BUILD_LIB_TYPE="$(normalize_build_lib_type "$ARG5")"

build_one() {
  BUILD_LIB_TYPE_ARG="$1"
  [ "$BUILD_LIB_TYPE_ARG" != "static_lib" ] && [ "$BUILD_LIB_TYPE_ARG" != "dynamic_lib" ] && { echo "Invalid BUILD_LIB_TYPE: $BUILD_LIB_TYPE_ARG"; exit 2; }
  [ "$COMPILER_TYPE" = "gcc" ] && CC="gcc" CXX="g++"
  [ "$COMPILER_TYPE" = "clang" ] && CC="clang" CXX="clang++"
  SHARED="OFF"; [ "$BUILD_LIB_TYPE_ARG" = "dynamic_lib" ] && SHARED="ON"
  SRC_DIR="$(cd "$(dirname "$0")/../../../src" && pwd)"
  OUT_DIR="build_${ARCH_PARAM}_${COMPILER_TYPE}_${BUILD_LIB_TYPE_ARG}"
  rm -rf "$OUT_DIR"; mkdir "$OUT_DIR"; cd "$OUT_DIR" || exit 1
  echo
  echo "===== Build Configuration ====="
  echo "  ACTION            : build ($BUILD_LIB_TYPE_ARG)"
  echo "  ARCH              : $ARCH_PARAM"
  echo "  COMPILER          : $COMPILER_TYPE"
  echo "  JAVA_SUPPORT      : $JAVA_SUPPORT"
  echo "  NODE_API_SUPPORT  : $NODE_API_SUPPORT"
  echo "  Generator         : Unix Makefiles"
  echo "================================="
  echo
  for cfg in $BuildConfigs; do
    echo "  BUILD FOR CONFIG $BUILD_LIB_TYPE_ARG : $cfg"
    cmake "$SRC_DIR" -G "Unix Makefiles" \
      -DTARGET_PLATFORM:STRING=unix \
      -DBUILD_LIB_TYPE="$BUILD_LIB_TYPE_ARG" \
      -DJAVA_SUPPORT:BOOL="$JAVA_SUPPORT" \
      -DNODE_API_SUPPORT:BOOL="$NODE_API_SUPPORT" \
      -DCMAKE_BUILD_TYPE="$cfg" \
      -DBUILD_SHARED_LIBS="$SHARED" \
      -DCMAKE_C_COMPILER="$CC" \
      -DCMAKE_CXX_COMPILER="$CXX"
    cmake --build . -- -j 4 || exit 1
    cmake --install . || exit 1
  done
  cd ..
}

gen_vsproj() {
  [ "$COMPILER_TYPE" = "gcc" ] && CC="gcc" CXX="g++"
  [ "$COMPILER_TYPE" = "clang" ] && CC="clang" CXX="clang++"
  SHARED="OFF"; [ "$BUILD_LIB_TYPE" = "dynamic_lib" ] && SHARED="ON"
  SRC_DIR="$(cd "$(dirname "$0")/../../../src" && pwd)"
  OUT_DIR="vsproj_${ARCH_PARAM}_${COMPILER_TYPE}_${BUILD_LIB_TYPE}"
  rm -rf "$OUT_DIR"; mkdir "$OUT_DIR"; cd "$OUT_DIR" || exit 1
  echo
  echo "===== VS Project Generation ====="
  echo "  ARCH              : $ARCH_PARAM"
  echo "  COMPILER          : $COMPILER_TYPE"
  echo "  JAVA_SUPPORT      : $JAVA_SUPPORT"
  echo "  NODE_API_SUPPORT  : $NODE_API_SUPPORT"
  echo "  BUILD_LIB_TYPE    : $BUILD_LIB_TYPE"
  echo "  Generator         : Unix Makefiles"
  echo "================================="
  echo
  cmake "$SRC_DIR" -G "Unix Makefiles" \
    -DTARGET_PLATFORM:STRING=unix \
    -DBUILD_LIB_TYPE="$BUILD_LIB_TYPE" \
    -DJAVA_SUPPORT:BOOL="$JAVA_SUPPORT" \
    -DNODE_API_SUPPORT:BOOL="$NODE_API_SUPPORT" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_SHARED_LIBS="$SHARED" \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_CXX_COMPILER="$CXX"
  cd ..
}

do_pack() {
  OUT_DIR="pack_${ARCH_PARAM}"
  rm -rf "$OUT_DIR"; mkdir "$OUT_DIR"; cd "$OUT_DIR" || exit 1
  PACK_DIR="$(cd "$(dirname "$0")/../../../pack" && pwd)"
  echo
  echo "===== Packaging ====="
  echo "  ARCH              : $ARCH_PARAM"
  echo "================================="
  echo
  cmake "$PACK_DIR" -DTARGET_PLATFORM:STRING=unix -DPACKAGE_NAME:STRING=bqlog-lib
  cmake --build . --target package || exit 1
  make package
  cd ..
}

if [ "$ACTION" = "all" ]; then
  ensure_common_params
  build_one dynamic_lib
  build_one static_lib
  do_pack
  echo "---------"; echo "Finished!"; echo "---------"
  exit 0
elif [ "$ACTION" = "build" ]; then
  ensure_common_params
  [ -z "$BUILD_LIB_TYPE" ] && ask_build_lib_type
  build_one "$BUILD_LIB_TYPE"
  echo "---------"; echo "Finished!"; echo "---------"
  exit 0
elif [ "$ACTION" = "gen-vsproj" ]; then
  ensure_common_params
  [ -z "$BUILD_LIB_TYPE" ] && ask_build_lib_type
  gen_vsproj
  echo "---------"; echo "Finished!"; echo "---------"
  exit 0
elif [ "$ACTION" = "pack" ]; then
  ensure_arch_only
  do_pack
  echo "---------"; echo "Finished!"; echo "---------"
  exit 0
else
  echo "Unknown action: $ACTION"
  echo "Supported: all | build | gen-vsproj | pack | dynamic_lib | static_lib"
  exit 2
fi