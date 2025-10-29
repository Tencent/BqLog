cmake_minimum_required(VERSION 3.22)

set(PROJ_NAME "BqLogWrapper_NodeJs")
message(${PROJ_NAME})

# Optional common utils include
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt)
endif()

# Parse version from version.cpp (this should define BQ_LOG_VERSION or related variables)
bq_parse_version_from_cpp("${CMAKE_CURRENT_SOURCE_DIR}/../../src/bq_log/global/version.cpp")

# Fallbacks if BQ_LOG_VERSION is not defined by the parser
if(NOT DEFINED BQ_LOG_VERSION)
    if(DEFINED BQ_VERSION)
        set(BQ_LOG_VERSION "${BQ_VERSION}")
    elseif(DEFINED PROJECT_VERSION)
        set(BQ_LOG_VERSION "${PROJECT_VERSION}")
    elseif(DEFINED PROJ_VERSION)
        set(BQ_LOG_VERSION "${PROJ_VERSION}")
    else()
        set(BQ_LOG_VERSION "0.0.0")
    endif()
endif()
message(STATUS "BQ_LOG_VERSION=${BQ_LOG_VERSION}")

project(${PROJ_NAME} VERSION ${BQ_LOG_VERSION})

# Paths (adjust as needed)
set(ARTIFACTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/wrapper/nodejs")
set(INSTALL_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/../../install/wrapper_nodejs")

# Read package.json (same directory) to compute npm pack filename: <name>-<version>.tgz
set(PKG_JSON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/package.json")
if(NOT EXISTS "${PKG_JSON_PATH}")
  message(FATAL_ERROR "package.json not found at: ${PKG_JSON_PATH}")
endif()

set(PKG_JSON "${CMAKE_CURRENT_SOURCE_DIR}/package.json")
if(NOT EXISTS "${PKG_JSON}")
  message(FATAL_ERROR "package.json not found: ${PKG_JSON}")
endif()

# Write Version
file(READ "${PKG_JSON}" _pkg_raw)
string(REGEX REPLACE "\"version\"[ \t]*:[ \t]*\"[^\"]+\"" "\"version\": \"${CMAKE_PROJECT_VERSION}\"" _pkg_new "${_pkg_raw}")

if(NOT _pkg_raw STREQUAL _pkg_new)
  file(WRITE "${PKG_JSON}" "${_pkg_new}")
  message(STATUS "Updated package.json version -> ${CMAKE_PROJECT_VERSION}")
else()
  message(STATUS "package.json version already ${CMAKE_PROJECT_VERSION}")
endif()

if(WIN32)
  # use cmd /c npm，to avoid “no such file or directory” in Unix Makefiles 
  set(NPM cmd /c npm)
else()
  set(NPM npm)
endif()

# Build pipeline (runs during 'cmake --build'):
# 2) npm run build
# 3) npm pack into ARTIFACTS_DIR
add_custom_target(nodejs_pack ALL
  COMMAND "${CMAKE_COMMAND}" -E chdir "${CMAKE_CURRENT_SOURCE_DIR}" ${NPM} i -D typescript @types/node
  COMMAND "${CMAKE_COMMAND}" -E chdir "${CMAKE_CURRENT_SOURCE_DIR}" ${NPM} run build
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARTIFACTS_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E chdir "${CMAKE_CURRENT_SOURCE_DIR}" ${NPM} pack --pack-destination "${ARTIFACTS_DIR}"
  USES_TERMINAL
)

# Install step: copyfrom ARTIFACTS_DIR to INSTALL_DIR
install(CODE
  "
  file(MAKE_DIRECTORY \"${INSTALL_DIR}\")
  if(EXISTS \"${ARTIFACTS_DIR}\")
    message(STATUS \"Installing all artifacts from ${ARTIFACTS_DIR} -> ${INSTALL_DIR}\")
    file(COPY \"${ARTIFACTS_DIR}/\" DESTINATION \"${INSTALL_DIR}\")
  else()
    message(FATAL_ERROR \"Artifacts dir not found: ${ARTIFACTS_DIR}. Build first.\")
  endif()
  "
)



file(GLOB MD_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../*.md"
)

install(FILES ${MD_FILES}
        DESTINATION "${INSTALL_DIR}/.."
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../docs"
        DESTINATION "${INSTALL_DIR}/.."
)