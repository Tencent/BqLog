cmake_minimum_required(VERSION 3.22)

# Enable C# Language support
# On Windows + VS Generator, this uses MSBuild/CSC (targeting .NET Framework by default).
# On Linux/Mac + Makefile Generator, this uses Mono (mcs).
project(BqLogCSharpTest CSharp)

set(ARTIFACTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/test/csharp")
file(MAKE_DIRECTORY ${ARTIFACTS_DIR})

set(WRAPPER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../wrapper/csharp/src")
file(GLOB_RECURSE WRAPPER_SRC_LIST "${WRAPPER_SRC_DIR}/*.cs")

set(TEST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(GLOB_RECURSE TEST_SRC_LIST "${TEST_SRC_DIR}/*.cs")

add_executable(BqLogCSharpTest ${WRAPPER_SRC_LIST} ${TEST_SRC_LIST})

set_target_properties(BqLogCSharpTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${ARTIFACTS_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ARTIFACTS_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${ARTIFACTS_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${ARTIFACTS_DIR}"
)

# Enable Unsafe Blocks (Required for P/Invoke pointers etc)
if(MSVC)
    # For Visual Studio Generator
    set_property(TARGET BqLogCSharpTest PROPERTY VS_GLOBAL_AllowUnsafeBlocks "true")
    # Target .NET Framework 4.7.2 for maximum compatibility on Windows
    set_property(TARGET BqLogCSharpTest PROPERTY VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.7.2")
else()
    # For Mono (Makefile/Ninja)
    # Pass -unsafe flag to mcs
    target_compile_options(BqLogCSharpTest PRIVATE "-unsafe")
endif()

message(STATUS "C# Test Project Configured: ${ARTIFACTS_DIR}/BqLogCSharpTest.exe")