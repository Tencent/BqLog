cmake_minimum_required(VERSION 3.22)

# The distribution entry point: one configure/build/install covers the library and the
# tools. Every sub-project still builds on its own, which is what the platform drivers use.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/submodules/BqCommon/CMake_utils.txt)
else()
    include(${CMAKE_CURRENT_SOURCE_DIR}/CMake_utils.txt)
endif()

#Parse version from version.cpp
bq_parse_version_from_cpp("${CMAKE_CURRENT_SOURCE_DIR}/src/bq_log/global/version.cpp")

project(BqLog VERSION ${BQ_VERSION} LANGUAGES CXX)

# A driver script or a package manager passes TARGET_PLATFORM; detect it otherwise.
bq_default_target_platform()

set(BUILD_LIB_TYPE "static_lib" CACHE STRING "Choose the build type")
set_property(CACHE BUILD_LIB_TYPE PROPERTY STRINGS "static_lib" "dynamic_lib" "both")

OPTION(BQ_WERROR "Treat compiler warnings as errors" ON)
OPTION(BQ_BUILD_LIB "Build the library" ON)
OPTION(BQ_BUILD_TOOLS "Build the command line tools" ON)

if(BQ_BUILD_LIB)
    add_subdirectory(src)
endif()

if(BQ_BUILD_TOOLS)
    add_subdirectory(tools/category_log_generator)
    add_subdirectory(tools/log_decoder)
endif()
