cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt)
endif()

project(BqLogCrossBenchmark CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT TARGET_PLATFORM)
    set(TARGET_PLATFORM win64)
endif()

if (CMAKE_GENERATOR MATCHES "Visual Studio")
    string(APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus /utf-8")
endif()

########################################################################
# Third-party logging libraries (fetched at configure time)
########################################################################
include(FetchContent)

set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE)

set(WITH_GFLAGS OFF CACHE BOOL "" FORCE)
set(WITH_GTEST OFF CACHE BOOL "" FORCE)
set(WITH_GMOCK OFF CACHE BOOL "" FORCE)
set(WITH_UNWIND OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(glog_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glog
    GIT_REPOSITORY https://github.com/google/glog.git
    GIT_TAG v0.7.1
    GIT_SHALLOW TRUE)

set(QUILL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QUILL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(QUILL_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(quill
    GIT_REPOSITORY https://github.com/odygrd/quill.git
    GIT_TAG v11.1.0
    GIT_SHALLOW TRUE)

FetchContent_Declare(fmtlog
    GIT_REPOSITORY https://github.com/MengRao/fmtlog.git
    GIT_TAG main
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(spdlog glog quill fmtlog)

########################################################################
# BqLog static library (same build as benchmark/cpp)
########################################################################
set(BQLOG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_subdirectory("${BQLOG_ROOT}/src/" "${PROJECT_BINARY_DIR}/StaticLibProj")

########################################################################
# Benchmark executables
########################################################################
add_executable(bench_bqlog bench_bqlog.cpp)
target_include_directories(bench_bqlog PRIVATE "${BQLOG_ROOT}/include")
target_link_libraries(bench_bqlog PRIVATE BqLog)

add_executable(bench_spdlog bench_spdlog.cpp)
target_link_libraries(bench_spdlog PRIVATE spdlog::spdlog)

add_executable(bench_glog bench_glog.cpp)
target_link_libraries(bench_glog PRIVATE glog::glog)

add_executable(bench_fmtlog bench_fmtlog.cpp)
target_include_directories(bench_fmtlog PRIVATE ${fmtlog_SOURCE_DIR})
target_link_libraries(bench_fmtlog PRIVATE fmtlog-static)

add_executable(bench_quill bench_quill.cpp)
target_link_libraries(bench_quill PRIVATE quill::quill)
