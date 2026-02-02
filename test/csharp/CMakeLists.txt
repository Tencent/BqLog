cmake_minimum_required(VERSION 3.22)

# C# Test Project
# On Windows: Use dotnet build with SDK-style project for native ARM64 support
# On Linux/Mac: Use Mono compiler (mcs)

project(BqLogCSharpTest NONE)

set(ARTIFACTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/test/csharp")
file(MAKE_DIRECTORY ${ARTIFACTS_DIR})

set(WRAPPER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../wrapper/csharp/src")
file(GLOB_RECURSE WRAPPER_SRC_LIST "${WRAPPER_SRC_DIR}/*.cs")

set(TEST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(GLOB_RECURSE TEST_SRC_LIST "${TEST_SRC_DIR}/*.cs")

if(WIN32)
    # ==========================================================================
    # Windows: Use dotnet build with SDK-style project
    # This provides native ARM64 support (unlike .NET Framework which runs under x64 emulation)
    # ==========================================================================
    find_program(DOTNET_EXECUTABLE dotnet)
    if(NOT DOTNET_EXECUTABLE)
        message(FATAL_ERROR "dotnet not found. Please install .NET 6.0 SDK or later.")
    endif()
    
    # Generate source file list for csproj
    set(CSHARP_SOURCES_XML "")
    foreach(SRC_FILE ${WRAPPER_SRC_LIST} ${TEST_SRC_LIST})
        string(APPEND CSHARP_SOURCES_XML "    <Compile Include=\"${SRC_FILE}\" />\n")
    endforeach()
    
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}")
    
    # Configure the csproj from template
    set(CSPROJ_FILE "${CMAKE_CURRENT_BINARY_DIR}/BqLogTest.csproj")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/BqLogTest.csproj.in"
        "${CSPROJ_FILE}"
        @ONLY
    )
    
    set(OUTPUT_DLL "${ARTIFACTS_DIR}/BqLogTest.dll")
    
    add_custom_command(
        OUTPUT "${OUTPUT_DLL}"
        COMMAND ${DOTNET_EXECUTABLE} build "${CSPROJ_FILE}" -c Release -o "${ARTIFACTS_DIR}"
        DEPENDS ${WRAPPER_SRC_LIST} ${TEST_SRC_LIST} "${CSPROJ_FILE}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building C# Test with dotnet build (.NET 6.0, native ARM64 support)"
    )
    
    add_custom_target(BqLogCSharpTest ALL DEPENDS "${OUTPUT_DLL}")
    
    message(STATUS "C# Test Project Configured (dotnet): ${ARTIFACTS_DIR}/BqLogTest.dll")
else()
    # ==========================================================================
    # Linux/Mac: Use Mono compiler (mcs)
    # ==========================================================================
    find_program(MCS_EXECUTABLE mcs)
    if(NOT MCS_EXECUTABLE)
        message(FATAL_ERROR "Mono compiler (mcs) not found. Please install Mono (e.g. 'brew install mono' on macOS).")
    endif()

    set(OUTPUT_EXE "${ARTIFACTS_DIR}/BqLogCSharpTest.exe")
    
    add_custom_command(
        OUTPUT "${OUTPUT_EXE}"
        COMMAND ${MCS_EXECUTABLE} -target:exe -out:"${OUTPUT_EXE}" -unsafe ${WRAPPER_SRC_LIST} ${TEST_SRC_LIST}
        DEPENDS ${WRAPPER_SRC_LIST} ${TEST_SRC_LIST}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building C# Test with Mono (mcs)"
    )
    
    add_custom_target(BqLogCSharpTest ALL DEPENDS "${OUTPUT_EXE}")
    
    message(STATUS "C# Test Project Configured (Mono): ${ARTIFACTS_DIR}/BqLogCSharpTest.exe")
endif()