cmake_minimum_required(VERSION 3.22)
project(BqLogCSharpTest NONE) # Disable language check

# Output Dir
set(ARTIFACTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/test/csharp")
file(MAKE_DIRECTORY ${ARTIFACTS_DIR})

# 1. Scan Sources
set(WRAPPER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../wrapper/csharp/src")
file(GLOB_RECURSE WRAPPER_SRC_LIST "${WRAPPER_SRC_DIR}/*.cs")

set(TEST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(GLOB_RECURSE TEST_SRC_LIST "${TEST_SRC_DIR}/*.cs")

set(ALL_CS_SOURCES ${WRAPPER_SRC_LIST} ${TEST_SRC_LIST})

# 2. Build XML for .csproj
set(CSHARP_SOURCES_XML "")
foreach(SRC ${ALL_CS_SOURCES})
    # Convert CMake path to native path format if needed, but forward slash works in .csproj usually
    set(CSHARP_SOURCES_XML "${CSHARP_SOURCES_XML}    <Compile Include=\"${SRC}\" />\n")
endforeach()

# 3. Configure the .csproj file
# We set CMAKE_RUNTIME_OUTPUT_DIRECTORY to pass it to the .csproj template
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}")
configure_file("BqLogTest.csproj.in" "BqLogCSharpTest.csproj" @ONLY)

# 4. Add Custom Target to run 'dotnet build'
add_custom_target(BqLogCSharpTest ALL
    COMMAND dotnet build "${CMAKE_CURRENT_BINARY_DIR}/BqLogCSharpTest.csproj" -c Release
    COMMENT "Building C# Test Project via dotnet CLI"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

message(STATUS "C# Test Project generated: ${CMAKE_CURRENT_BINARY_DIR}/BqLogCSharpTest.csproj")
message(STATUS "Output Directory: ${ARTIFACTS_DIR}")