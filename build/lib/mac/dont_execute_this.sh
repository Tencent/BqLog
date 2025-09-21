#!/bin/zsh
#
# Usage (all parameters optional; missing ones will be prompted):
#   dont_execute_this.sh all  [java] [node]
#   dont_execute_this.sh build [java] [node] [static_lib|dynamic_lib] [framework|dylib|a]
#   dont_execute_this.sh gen-vsproj [java] [node] [static_lib|dynamic_lib] [framework|dylib|a]
#   dont_execute_this.sh pack
#
# Normalized values:
#   format    : framework | dylib | a
#   java,node : ON | OFF
#   lib type  : static_lib | dynamic_lib
# ------------------------------------------------------------

set -e -u -o pipefail

: ${BUILD_JOBS:=10}

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" >/dev/null 2>&1 && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." >/dev/null 2>&1 && pwd)"
SRC_DIR="${REPO_ROOT}/src"
DIST_DIR="${REPO_ROOT}/dist"
PACK_DIR="${REPO_ROOT}/pack"

typeset -a BuildConfigs
BuildConfigs=("Debug" "MinSizeRel" "RelWithDebInfo" "Release")

# Args (no compiler, no arch; format is last)
ACTION="${1:-}"
ARG1="${2:-}"  # java
ARG2="${3:-}"  # node
ARG3="${4:-}"  # build_lib_type
ARG4="${5:-}"  # format (framework|dylib|a)

JAVA_SUPPORT=""
NODE_API_SUPPORT=""
BUILD_LIB_TYPE=""
APPLE_LIB_FORMAT=""

# Helpers (zsh-native lowercase)
to_lower() { print -r -- "${1:-}" | tr '[:upper:]' '[:lower:]'; }
to_upper() { print -r -- "${1:-}" | tr '[:lower:]' '[:upper:]'; }

normalize_onoff() {
  local v="${1:-}"; v="${v:l}"  # zsh lowercase
  case "$v" in
    on|yes|y|1|true)  echo "ON" ;;
    off|no|n|0|false) echo "OFF" ;;
    *)                echo "" ;;
  esac
}
normalize_build_lib_type() {
  local v="${1:-}"; v="${v:l}"
  case "$v" in
    static_lib|dynamic_lib) echo "$v" ;;
    *) echo "" ;;
  esac
}
normalize_format() {
  local v="${1:-}"; v="${v:l}"
  case "$v" in
    framework|dylib|a) echo "$v" ;;
    *) echo "" ;;
  esac
}

ask_yes_no() {
  local prompt="$1" c
  while true; do
    if [ -t 0 ]; then
      printf "%s (Y/N): " "$prompt" >/dev/tty
    else
      printf "%s (Y/N): " "$prompt" >&2
    fi
    read -r c
    case "$(to_upper "$c")" in
      Y) echo "ON";  break ;;
      N) echo "OFF"; break ;;
    esac
  done
}
ask_build_lib_type() {
  echo
  echo "Select library type:"
  echo "  [S] static_lib"
  echo "  [D] dynamic_lib"
  while true; do
    printf "Enter choice (S/D): "
    read -r c
    case "$(to_upper "$c")" in
      S) BUILD_LIB_TYPE="static_lib";  break ;;
      D) BUILD_LIB_TYPE="dynamic_lib"; break ;;
    esac
  done
}
ask_format() {
  # If N-API is ON and dynamic lib, force dylib without prompting
  if [[ "${NODE_API_SUPPORT:-}" == "ON" && "${BUILD_LIB_TYPE:-}" == "dynamic_lib" ]]; then
    echo
    echo "Node-API is ON: dynamic library format is forced to 'dylib' (framework will not be produced)."
    APPLE_LIB_FORMAT="dylib"
    return
  fi
  echo
  echo "Select Apple build format:"
  echo "  [F] framework"
  echo "  [D] dylib"
  echo "  [A] a (static .a)"
  while true; do
    printf "Enter choice (F/D/A): "
    read -r c
    case "$(to_upper "$c")" in
      F) APPLE_LIB_FORMAT="framework"; break ;;
      D) APPLE_LIB_FORMAT="dylib";     break ;;
      A) APPLE_LIB_FORMAT="a";         break ;;
    esac
  done
}

ensure_common_params() {
  [[ -z "${JAVA_SUPPORT:-}"     ]] && JAVA_SUPPORT="$(ask_yes_no "Enable Java/JNI support?")"
  [[ -z "${NODE_API_SUPPORT:-}" ]] && NODE_API_SUPPORT="$(ask_yes_no "Enable Node-API (Node.js) support?")"
}

determine_generator() {
  local want="${APPLE_GEN:-}"
  case "$(to_lower "$want")" in
    xcode) echo "Xcode"; return ;;
    make|unix|makefiles) echo "Unix Makefiles"; return ;;
  esac
  if command -v xcodebuild >/dev/null 2>&1; then echo "Xcode"; else echo "Unix Makefiles"; fi
}

# Parse and normalize (non-interactive params)
[[ -z "${ACTION:-}" ]] && ACTION="all"
JAVA_SUPPORT="$(normalize_onoff "$ARG1")"
NODE_API_SUPPORT="$(normalize_onoff "$ARG2")"
BUILD_LIB_TYPE="$(normalize_build_lib_type "$ARG3")"
APPLE_LIB_FORMAT="$(normalize_format "$ARG4")"

echo "Parsed params:"
echo "  ACTION:            ${ACTION:-}"
echo "  JAVA_SUPPORT:      ${JAVA_SUPPORT:-<unset>}"
echo "  NODE_API_SUPPORT:  ${NODE_API_SUPPORT:-<unset>}"
echo "  BUILD_LIB_TYPE:    ${BUILD_LIB_TYPE:-<unset>}"
echo "  APPLE_LIB_FORMAT:  ${APPLE_LIB_FORMAT:-<unset>}"
echo "  GENERATOR:         $(determine_generator)"

# Always build universal binaries (arm64 + x86_64)
typeset -a ARCH_ARGS
ARCH_ARGS=("-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64")

# Build one (libtype, format) across all configs
build_one_pair() {
  local build_lib_type="$1"   # static_lib | dynamic_lib
  local apple_format="$2"     # framework | dylib | a
  local gen proj_dir
  gen="$(determine_generator)"
  proj_dir="${SCRIPT_DIR}/proj_${build_lib_type}_${apple_format}"

  rm -rf "${proj_dir}"
  mkdir -p "${proj_dir}"
  pushd "${proj_dir}" >/dev/null

  echo
  echo "===== Build Configuration ====="
  echo "  ACTION            : build (${build_lib_type})"
  echo "  JAVA_SUPPORT      : ${JAVA_SUPPORT}"
  echo "  NODE_API_SUPPORT  : ${NODE_API_SUPPORT}"
  echo "  APPLE_LIB_FORMAT  : ${apple_format}"
  echo "  Generator         : ${gen}"
  ((${#ARCH_ARGS[@]})) && echo "  CMake arch args   : ${ARCH_ARGS[*]}"
  echo "================================="
  echo

  if [[ "$gen" == "Xcode" ]]; then
    cmake "${SRC_DIR}" -G "Xcode" \
      -DTARGET_PLATFORM:STRING=mac \
      -DBUILD_LIB_TYPE="${build_lib_type}" \
      -DJAVA_SUPPORT:BOOL="${JAVA_SUPPORT}" \
      -DNODE_API_SUPPORT:BOOL="${NODE_API_SUPPORT}" \
      -DAPPLE_LIB_FORMAT:STRING="${apple_format}" \
      -DCMAKE_INSTALL_PREFIX="${REPO_ROOT}/install" \
      "${ARCH_ARGS[@]}"

    for cfg in "${BuildConfigs[@]}"; do
      echo "  BUILD FOR CONFIG ${build_lib_type} / ${apple_format} : ${cfg}"
      cmake --build . --config "${cfg}" --parallel "${BUILD_JOBS}"
      cmake --install . --config "${cfg}"
    done
  else
    for cfg in "${BuildConfigs[@]}"; do
      echo "  BUILD FOR CONFIG ${build_lib_type} / ${apple_format} : ${cfg}"
      cmake "${SRC_DIR}" -G "Unix Makefiles" \
        -DTARGET_PLATFORM:STRING=mac \
        -DBUILD_LIB_TYPE="${build_lib_type}" \
        -DJAVA_SUPPORT:BOOL="${JAVA_SUPPORT}" \
        -DNODE_API_SUPPORT:BOOL="${NODE_API_SUPPORT}" \
        -DCMAKE_BUILD_TYPE="${cfg}" \
        -DAPPLE_LIB_FORMAT:STRING="${apple_format}" \
        -DCMAKE_INSTALL_PREFIX="${REPO_ROOT}/install" \
        "${ARCH_ARGS[@]}"
      cmake --build . -- -j "${BUILD_JOBS}"
      cmake --install .
    done
  fi

  popd >/dev/null
}

# Generate project (Debug) for one (libtype, format)
gen_project_one() {
  local build_lib_type="$1"
  local apple_format="$2"
  local gen proj_dir
  gen="$(determine_generator)"
  proj_dir="${SCRIPT_DIR}/vsproj_${build_lib_type}_${apple_format}"

  rm -rf "${proj_dir}"
  mkdir -p "${proj_dir}"
  pushd "${proj_dir}" >/dev/null

  echo
  echo "===== Project Generation ====="
  echo "  JAVA_SUPPORT      : ${JAVA_SUPPORT}"
  echo "  NODE_API_SUPPORT  : ${NODE_API_SUPPORT}"
  echo "  BUILD_LIB_TYPE    : ${build_lib_type}"
  echo "  APPLE_LIB_FORMAT  : ${apple_format}"
  echo "  Generator         : ${gen}"
  ((${#ARCH_ARGS[@]})) && echo "  CMake arch args   : ${ARCH_ARGS[*]}"
  echo "================================="
  echo

  if [[ "$gen" == "Xcode" ]]; then
    cmake "${SRC_DIR}" -G "Xcode" \
      -DTARGET_PLATFORM:STRING=mac \
      -DBUILD_TYPE="${build_lib_type}" \
      -DJAVA_SUPPORT:BOOL="${JAVA_SUPPORT}" \
      -DNODE_API_SUPPORT:BOOL="${NODE_API_SUPPORT}" \
      -DAPPLE_LIB_FORMAT:STRING="${apple_format}" \
      "${ARCH_ARGS[@]}"
  else
    cmake "${SRC_DIR}" -G "Unix Makefiles" \
      -DTARGET_PLATFORM:STRING=mac \
      -DBUILD_TYPE="${build_lib_type}" \
      -DJAVA_SUPPORT:BOOL="${JAVA_SUPPORT}" \
      -DNODE_API_SUPPORT:BOOL="${NODE_API_SUPPORT}" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DAPPLE_LIB_FORMAT:STRING="${apple_format}" \
      "${ARCH_ARGS[@]}"
  fi

  popd >/dev/null
}

# Build all combinations (respect N-API rule: dynamic => dylib only when ON)
build_all_combos() {
  if [[ "${NODE_API_SUPPORT:-}" == "ON" ]]; then
    # dynamic: dylib only
    build_one_pair "dynamic_lib" "dylib"
  else
    # dynamic: framework + dylib
    build_one_pair "dynamic_lib" "framework"
    build_one_pair "dynamic_lib" "dylib"
  fi
  # static: framework + a
  build_one_pair "static_lib"  "framework"
  build_one_pair "static_lib"  "a"
}

# CMake-based packaging (CPack), like Windows
do_pack() {
  local gen; gen="$(determine_generator)"

  echo
  echo "===== Packaging ====="
  echo "  TARGET_PLATFORM   : mac"
  echo "  PACKAGE_NAME      : bqlog-lib"
  echo "  Generator         : ${gen}"
  echo "================================="
  echo

  local work_dir="${SCRIPT_DIR}/pack"
  rm -rf "${work_dir}"
  mkdir -p "${work_dir}"
  pushd "${work_dir}" >/dev/null

  if [[ "$gen" == "Xcode" ]]; then
    cmake "${PACK_DIR}" -G "Xcode" \
      -DTARGET_PLATFORM:STRING=mac \
      -DPACKAGE_NAME:STRING=bqlog-lib
    cmake --build . --target package --parallel "${BUILD_JOBS}"
  else
    cmake "${PACK_DIR}" -G "Unix Makefiles" \
      -DTARGET_PLATFORM:STRING=mac \
      -DPACKAGE_NAME:STRING=bqlog-lib
    cmake --build . --target package -- -j "${BUILD_JOBS}"
  fi

  popd >/dev/null
}

# Enforce N-API rule on explicit build/gen requests
enforce_napi_rule_if_needed() {
  if [[ "${NODE_API_SUPPORT:-}" == "ON" && "${BUILD_LIB_TYPE:-}" == "dynamic_lib" && "${APPLE_LIB_FORMAT:-}" != "dylib" ]]; then
    echo "NODE_API_SUPPORT=ON: forcing dynamic_lib format to 'dylib' (was: ${APPLE_LIB_FORMAT:-unset})."
    APPLE_LIB_FORMAT="dylib"
  fi
}

# Dispatch
case "$ACTION" in
  all)
    # If CLI provided ON/OFF, use it; otherwise prompt
    [[ -z "${JAVA_SUPPORT:-}"     ]] && JAVA_SUPPORT="$(ask_yes_no "Enable Java/JNI support?")"
    [[ -z "${NODE_API_SUPPORT:-}" ]] && NODE_API_SUPPORT="$(ask_yes_no "Enable Node-API (Node.js) support?")"
    build_all_combos
    do_pack
    echo "---------"; echo "Finished!"; echo "---------"
    ;;
  build)
    ensure_common_params
    [[ -z "${BUILD_LIB_TYPE:-}"   ]] && ask_build_lib_type
    [[ -z "${APPLE_LIB_FORMAT:-}" ]] && ask_format
    enforce_napi_rule_if_needed
    build_one_pair "$BUILD_LIB_TYPE" "$APPLE_LIB_FORMAT"
    echo "---------"; echo "Finished!"; echo "---------"
    ;;
  gen-vsproj)
    ensure_common_params
    [[ -z "${BUILD_LIB_TYPE:-}"   ]] && ask_build_lib_type
    [[ -z "${APPLE_LIB_FORMAT:-}" ]] && ask_format
    enforce_napi_rule_if_needed
    gen_project_one "$BUILD_LIB_TYPE" "$APPLE_LIB_FORMAT"
    echo "---------"; echo "Finished!"; echo "---------"
    ;;
  pack)
    do_pack
    echo "---------"; echo "Finished!"; echo "---------"
    ;;
  *)
    echo "Unknown action: $ACTION"
    echo "Supported: all | build | gen-vsproj | pack"
    exit 2
    ;;
esac