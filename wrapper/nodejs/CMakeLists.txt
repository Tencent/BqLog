cmake_minimum_required(VERSION 3.15)

project(BqLogNodeJS VERSION 1.4.9 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Node.js addon API
execute_process(
    COMMAND node -p "require('node-addon-api').include"
    OUTPUT_VARIABLE NODE_ADDON_API_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND node -p "require('node-addon-api').gyp"
    OUTPUT_VARIABLE NODE_ADDON_API_GYP
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Include directories
include_directories(
    ${NODE_ADDON_API_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Source files
set(NODEJS_SOURCES
    src/bqlog_node_api.cpp
)

# Create the Node.js addon
add_library(bqlog SHARED ${NODEJS_SOURCES})

# Link to BqLog static library
if(TARGET BqLog)
    target_link_libraries(bqlog BqLog)
else()
    # Try to find pre-built library
    find_library(BQLOG_LIB
        NAMES bqlog BqLog
        PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../dist/dynamic_lib
        PATH_SUFFIXES linux mac win64
    )
    
    if(BQLOG_LIB)
        target_link_libraries(bqlog ${BQLOG_LIB})
    else()
        message(WARNING "BqLog library not found. Make sure to build BqLog first.")
    endif()
endif()

# Node.js addon specific settings
set_target_properties(bqlog PROPERTIES
    PREFIX ""
    SUFFIX ".node"
)

# Platform-specific compiler settings
if(WIN32)
    target_compile_definitions(bqlog PRIVATE _HAS_EXCEPTIONS=1)
    set_target_properties(bqlog PROPERTIES
        LINK_FLAGS "/DELAYLOAD:node.exe"
    )
elseif(APPLE)
    set_target_properties(bqlog PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif()

# Compiler definitions
target_compile_definitions(bqlog PRIVATE
    NAPI_DISABLE_CPP_EXCEPTIONS
    BQ_JAVA=0
)

# Disable exceptions (as recommended for Node.js addons)
if(MSVC)
    target_compile_options(bqlog PRIVATE /EHsc)
else()
    target_compile_options(bqlog PRIVATE -fno-exceptions)
endif()

# Install the addon to build/Release directory (node-gyp convention)
install(TARGETS bqlog
    DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/build/Release
)