cmake_minimum_required(VERSION 3.22)

option(USE_FOR_IOS "Use This Wrapper For iOS" OFF)
option(USE_FOR_UNITY_IL2CPP "Use This Wrapper For Unity IL2CPP" OFF)

set(PROJ_NAME "BqLogWrapper_CSharp")

message(${PROJ_NAME})



project(${PROJ_NAME} CSharp)
set(CMAKE_CSharp_FLAGS "/platform:anycpu /unsafe")

############################################################
# Search Source Files
############################################################
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE SRC_LIST "${SRC_DIR}/*.cs")


############################################################
# IDE Sources Group
############################################################
foreach(FILE ${SRC_LIST}) 
    message(${FILE})
    get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)
    get_filename_component(FILE_NAME "${FILE}" NAME)
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")
    string(REPLACE "/" "\\" GROUP "Sources/bq_log/${GROUP}")
	set_source_files_properties("${FILE}"
        PROPERTIES VS_CSHARP_Link "${GROUP}/${FILE_NAME}")
endforeach()


set(OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/wrapper/csharp")

message("OutPut Directory:${OUTPUT_DIRECTORY}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}")

# build library
add_library(${PROJ_NAME} SHARED ${SRC_LIST})

set_property(TARGET ${PROJ_NAME} PROPERTY VS_DOTNET_TARGET_FRAMEWORK_VERSION "v3.5")
set_property(TARGET ${PROJ_NAME} PROPERTY VS_DOTNET_REFERENCES
    "System"
    "System.Core"
    "System.Data"
    "System.Data.DataSetExtensions"
    "System.Xml"
    "System.Xml.Linq")






