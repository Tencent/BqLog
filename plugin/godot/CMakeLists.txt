cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt")
    include("${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt")
endif()

bq_parse_version_from_cpp("${CMAKE_CURRENT_SOURCE_DIR}/../../src/bq_log/global/version.cpp")

project(BqLogGodot VERSION ${BQ_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BQ_WERROR "Treat compiler warnings as errors in BqLogGodot sources" ON)
option(BQLOG_GODOT_ENABLE_TEST_HELPER "Compile internal C++ adapter test helper class (test builds only)" OFF)
set(GODOT_CPP_DIR "" CACHE PATH "Path to an existing godot-cpp checkout (optional; fetched if empty)")
set(GODOT_CPP_GIT_TAG "godot-4.2-stable" CACHE STRING "godot-cpp git tag to fetch when GODOT_CPP_DIR is empty")
set(BQLOG_LIB_PATH "" CACHE FILEPATH "Explicit path to the prebuilt BqLog shared library (or .lib on Windows)")
set(BQLOG_LIB_DIR "" CACHE PATH "Directory containing prebuilt BqLog shared library")
set(BQLOG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include" CACHE PATH "Path to BqLog public headers")
set(BQLOG_GODOT_OUTPUT_NAME "BqLogGodot" CACHE STRING "Output base name of the GDExtension library")

# Pull in godot-cpp (local checkout or FetchContent).
if(GODOT_CPP_DIR AND EXISTS "${GODOT_CPP_DIR}/CMakeLists.txt")
    message(STATUS "Using local godot-cpp from: ${GODOT_CPP_DIR}")
    add_subdirectory("${GODOT_CPP_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/godot-cpp" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(
        godot-cpp
        GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
        GIT_TAG ${GODOT_CPP_GIT_TAG}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(godot-cpp)
endif()

# Locate the BqLog dynamic library to link against (shared singleton across GDScript/C#/C++).
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

set(BQLOG_GODOT_SOURCES
    src/register_types.h
    src/register_types.cpp
    src/bq_log_godot_console.h
    src/bq_log_godot_console.cpp
    src/bq_log_godot_encoder.h
    src/bq_log_godot_encoder.cpp
    src/bq_log_godot.h
    src/bq_log_godot.cpp
    src/bq_category_log_godot.h
    src/bq_category_log_godot.cpp
)

add_library(BqLogGodot SHARED ${BQLOG_GODOT_SOURCES})

target_include_directories(BqLogGodot
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${BQLOG_INCLUDE_DIR}"
)

target_link_libraries(BqLogGodot
    PRIVATE
        godot::cpp
        "${BQLOG_LINK_LIB}"
)

target_compile_definitions(BqLogGodot PRIVATE BQ_DYNAMIC_LIB_IMPORT)
if(BQLOG_GODOT_ENABLE_TEST_HELPER)
    target_compile_definitions(BqLogGodot PRIVATE BQLOG_GODOT_ENABLE_TEST_HELPER=1)
endif()

if(MSVC)
    # GDCLASS() macro expands unused parameters; suppress C4100 just like -Wno-unused-parameter on GCC/Clang.
    target_compile_options(BqLogGodot PRIVATE /W4 /wd4100 /utf-8 /Zc:__cplusplus)
    if(BQ_WERROR)
        target_compile_options(BqLogGodot PRIVATE /WX)
    endif()
else()
    target_compile_options(BqLogGodot PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
    if(BQ_WERROR)
        target_compile_options(BqLogGodot PRIVATE -Werror)
    endif()
endif()

set_target_properties(BqLogGodot PROPERTIES
    OUTPUT_NAME "${BQLOG_GODOT_OUTPUT_NAME}"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
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
