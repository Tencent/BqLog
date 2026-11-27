cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt")
    include("${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt")
endif()

option(BQ_WERROR "Treat compiler warnings as errors in BqLogGodot sources" ON)
set(GODOT_CPP_DIR "" CACHE PATH "Path to an existing godot-cpp checkout (optional; fetched if empty)")
set(GODOT_CPP_GIT_TAG "godot-4.2-stable" CACHE STRING "godot-cpp git tag to fetch when GODOT_CPP_DIR is empty")
set(BQLOG_LIB_PATH "" CACHE FILEPATH "Explicit path to the prebuilt BqLog shared library (or .lib on Windows)")
set(BQLOG_LIB_DIR "" CACHE PATH "Directory containing prebuilt BqLog shared library")
set(BQLOG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include" CACHE PATH "Path to BqLog public headers")
set(BQLOG_GODOT_OUTPUT_NAME "BqLogGodot" CACHE STRING "Output base name of the GDExtension library")

# Parse version from version.cpp
bq_parse_version_from_cpp("${CMAKE_CURRENT_SOURCE_DIR}/../../src/bq_log/global/version.cpp")

project(BqLogGodot VERSION ${BQ_VERSION} LANGUAGES CXX)
BQ_DETECT_ARCH(DETECTED_ARCH)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

############################################################
# Dependencies (godot-cpp & prebuilt BqLog)
############################################################
if(GODOT_CPP_DIR AND EXISTS "${GODOT_CPP_DIR}/CMakeLists.txt")
    message(STATUS "Using local godot-cpp from: ${GODOT_CPP_DIR}")
    add_subdirectory("${GODOT_CPP_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/godot-cpp" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(
        godot-cpp
        URL "https://github.com/godotengine/godot-cpp/archive/refs/tags/${GODOT_CPP_GIT_TAG}.tar.gz"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(godot-cpp)
endif()

if(BQLOG_LIB_PATH AND EXISTS "${BQLOG_LIB_PATH}")
    set(BQLOG_LINK_LIB "${BQLOG_LIB_PATH}")
else()
    find_library(BQLOG_LINK_LIB
        NAMES BqLog libBqLog
        HINTS "${BQLOG_LIB_DIR}"
        NO_DEFAULT_PATH
    )
    if(NOT BQLOG_LINK_LIB)
        find_library(BQLOG_LINK_LIB NAMES BqLog libBqLog)
    endif()
endif()

if(NOT BQLOG_LINK_LIB)
    message(FATAL_ERROR "BqLog shared library not found. Set BQLOG_LIB_PATH or BQLOG_LIB_DIR.")
endif()
message(STATUS "Linking BqLogGodot against BqLog: ${BQLOG_LINK_LIB}")

############################################################
# Config Source & Include Dirs
############################################################
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PUB_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${SRC_DIR}")
COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${PUB_INC_DIR}")

COLLECT_SRC_FILES(SRC_LIST "${SRC_DIR}")
COLLECT_SRC_FILES(INC_LIST "${PUB_INC_DIR}")

GROUP_FOR_IDE("${SRC_LIST}" "src")
GROUP_FOR_IDE("${INC_LIST}" "include")

############################################################
# Target Definition
############################################################
add_library(BqLogGodot SHARED ${SRC_LIST} ${INC_LIST})

target_include_directories(BqLogGodot PRIVATE ${ALL_INC_DIRS})
# Mark BqLog public headers as SYSTEM so strict -Werror in BqLogGodot does not fail
# on platform-specific fortify warnings in upstream headers (e.g. Android NDK 30).
target_include_directories(BqLogGodot SYSTEM PRIVATE "${BQLOG_INCLUDE_DIR}")

target_link_libraries(BqLogGodot
    PRIVATE
        godot::cpp
        "${BQLOG_LINK_LIB}"
        ${CMAKE_DL_LIBS}
)

target_compile_definitions(BqLogGodot PRIVATE BQ_DYNAMIC_LIB_IMPORT)

if(MSVC)
    # GDCLASS() macro expands unused parameters; suppress C4100 just like -Wno-unused-parameter on GCC/Clang.
    target_compile_options(BqLogGodot PRIVATE /W4 /wd4100 /utf-8 /Zc:__cplusplus)
    if(BQ_WERROR)
        target_compile_options(BqLogGodot PRIVATE /WX)
    endif()
else()
    target_compile_options(BqLogGodot PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
    if(ANDROID)
        target_compile_options(BqLogGodot PRIVATE -Wno-user-defined-warnings)
    endif()
    if(OHOS)
        target_compile_options(BqLogGodot PRIVATE -Wno-unused-command-line-argument -Wno-error=unused-command-line-argument)
    endif()
    if(BQ_WERROR)
        target_compile_options(BqLogGodot PRIVATE -Werror)
    endif()
endif()

set_target_properties(BqLogGodot PROPERTIES
    OUTPUT_NAME "${BQLOG_GODOT_OUTPUT_NAME}"
)

if(APPLE)
    if(IOS)
        set_target_properties(BqLogGodot PROPERTIES
            FRAMEWORK TRUE
            MACOSX_FRAMEWORK_IDENTIFIER "com.tencent.bqlog.godot"
            INSTALL_NAME_DIR "@rpath"
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "@executable_path/Frameworks;@loader_path/Frameworks"
        )
    else()
        set_target_properties(BqLogGodot PROPERTIES
            INSTALL_NAME_DIR "@rpath"
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "@loader_path"
        )
    endif()
elseif(UNIX AND NOT ANDROID AND NOT OHOS)
    set_target_properties(BqLogGodot PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN"
    )
endif()
