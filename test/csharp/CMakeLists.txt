cmake_minimum_required(VERSION 3.22)

# Enable C# Language support
# On Windows + VS Generator, this uses MSBuild/CSC (targeting .NET Framework by default).
# On Linux/Mac + Makefile Generator, we must use custom commands for Mono (mcs) as CMake's native C# support is VS-only.

if(CMAKE_GENERATOR MATCHES "Visual Studio")
    project(BqLogCSharpTest CSharp)
else()
    project(BqLogCSharpTest NONE)
endif()

set(ARTIFACTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/test/csharp")
file(MAKE_DIRECTORY ${ARTIFACTS_DIR})

set(WRAPPER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../wrapper/csharp/src")
file(GLOB_RECURSE WRAPPER_SRC_LIST "${WRAPPER_SRC_DIR}/*.cs")

set(TEST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(GLOB_RECURSE TEST_SRC_LIST "${TEST_SRC_DIR}/*.cs")

if(CMAKE_GENERATOR MATCHES "Visual Studio")
    add_executable(BqLogCSharpTest ${WRAPPER_SRC_LIST} ${TEST_SRC_LIST})

    set_target_properties(BqLogCSharpTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${ARTIFACTS_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ARTIFACTS_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${ARTIFACTS_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${ARTIFACTS_DIR}"
    )

    # For Visual Studio Generator
    set_property(TARGET BqLogCSharpTest PROPERTY VS_GLOBAL_AllowUnsafeBlocks "true")
    # Target .NET Framework 4.7.2 for maximum compatibility on Windows
    set_property(TARGET BqLogCSharpTest PROPERTY VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.7.2")
    # Use AnyCPU to support both x64 and ARM64
    set_property(TARGET BqLogCSharpTest PROPERTY VS_GLOBAL_PlatformTarget "AnyCPU")
else()
    # For Mono on Mac/Linux
    
    # 1. Find Mono Compiler
    find_program(MCS_EXECUTABLE mcs)
    if(NOT MCS_EXECUTABLE)
        message(FATAL_ERROR "Mono compiler (mcs) not found. Please install Mono (e.g. 'brew install mono' on macOS).")
    endif()

    set(OUTPUT_EXE "${ARTIFACTS_DIR}/BqLogCSharpTest.exe")
    
    # 2. Define Custom Command to compile
    # Note: We use ${WRAPPER_SRC_LIST} and ${TEST_SRC_LIST} which are lists of files.
    add_custom_command(
        OUTPUT "${OUTPUT_EXE}"
        COMMAND ${MCS_EXECUTABLE} -target:exe -out:"${OUTPUT_EXE}" -unsafe ${WRAPPER_SRC_LIST} ${TEST_SRC_LIST}
        DEPENDS ${WRAPPER_SRC_LIST} ${TEST_SRC_LIST}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building C# Test with Mono (mcs)"
    )
    
    # 3. Define Custom Target to drive the build
    add_custom_target(BqLogCSharpTest ALL DEPENDS "${OUTPUT_EXE}")
endif()

message(STATUS "C# Test Project Configured: ${ARTIFACTS_DIR}/BqLogCSharpTest.exe")