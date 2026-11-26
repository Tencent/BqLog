cmake_minimum_required(VERSION 3.22)
project(BqLogGodotCppTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(GODOT_CPP_DIR "" CACHE PATH "Path to an existing godot-cpp checkout")
set(GODOT_CPP_GIT_TAG "godot-4.2-stable" CACHE STRING "godot-cpp git tag to fetch when GODOT_CPP_DIR is empty")
set(BQLOG_LIB_PATH "" CACHE FILEPATH "Explicit path to the prebuilt BqLog shared library (or .lib on Windows)")
set(BQLOG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include" CACHE PATH "Path to BqLog public headers")
set(BQLOG_GODOT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../plugin/godot/include" CACHE PATH "Path to BqLog Godot C++ adapter headers")

if(GODOT_CPP_DIR AND EXISTS "${GODOT_CPP_DIR}/CMakeLists.txt")
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

if(NOT BQLOG_LIB_PATH OR NOT EXISTS "${BQLOG_LIB_PATH}")
    message(FATAL_ERROR "BQLOG_LIB_PATH must point to the prebuilt BqLog shared library.")
endif()

add_library(BqLogGodotCppTest SHARED src/cpp_adapter_test.cpp)

target_include_directories(BqLogGodotCppTest
    PRIVATE
        "${BQLOG_GODOT_INCLUDE_DIR}"
)
target_include_directories(BqLogGodotCppTest
    SYSTEM PRIVATE
        "${BQLOG_INCLUDE_DIR}"
)
target_link_libraries(BqLogGodotCppTest
    PRIVATE
        godot::cpp
        "${BQLOG_LIB_PATH}"
)
target_compile_definitions(BqLogGodotCppTest PRIVATE BQ_DYNAMIC_LIB_IMPORT)

if(MSVC)
    target_compile_options(BqLogGodotCppTest PRIVATE /W4 /wd4100 /WX /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(BqLogGodotCppTest PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter -Werror)
endif()

set_target_properties(BqLogGodotCppTest PROPERTIES
    OUTPUT_NAME "BqLogGodotCppTest"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

if(APPLE)
    set_target_properties(BqLogGodotCppTest PROPERTIES
        INSTALL_NAME_DIR "@rpath"
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@loader_path"
    )
elseif(UNIX)
    set_target_properties(BqLogGodotCppTest PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN"
    )
endif()
