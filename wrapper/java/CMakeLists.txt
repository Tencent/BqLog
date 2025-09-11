cmake_minimum_required(VERSION 3.22)

set(PROJ_NAME "BqLogWrapper_Java")
message(${PROJ_NAME})

# Optional common utils include
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt)
endif()

# Parse version from version.cpp (this should define BQ_LOG_VERSION or related variables)
bq_parse_version_from_cpp("${CMAKE_CURRENT_SOURCE_DIR}/../../src/bq_log/global/version.cpp")

# Fallbacks if BQ_LOG_VERSION is not defined by the parser
if(NOT DEFINED BQ_LOG_VERSION)
    if(DEFINED BQ_VERSION)
        set(BQ_LOG_VERSION "${BQ_VERSION}")
    elseif(DEFINED PROJECT_VERSION)
        set(BQ_LOG_VERSION "${PROJECT_VERSION}")
    elseif(DEFINED PROJ_VERSION)
        set(BQ_LOG_VERSION "${PROJ_VERSION}")
    else()
        set(BQ_LOG_VERSION "0.0.0")
    endif()
endif()
message(STATUS "BQ_LOG_VERSION=${BQ_LOG_VERSION}")

project(${PROJ_NAME} VERSION ${BQ_LOG_VERSION})

# Java compile flags: UTF-8, suppress warnings, include debug info for IDE symbol mapping
set(CMAKE_JAVA_COMPILE_FLAGS "-encoding" "UTF-8" "-nowarn" "-g")

############################################################
# Search Source Files
############################################################
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(GLOB_RECURSE SRC_LIST "${SRC_DIR}/*.java")

############################################################
# IDE Sources Group
############################################################
foreach(FILE ${SRC_LIST})
    message(${FILE})
    get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")
    string(REPLACE "/" "\\" GROUP "${GROUP}")
    set(GROUP "Sources${GROUP}")
    source_group("${GROUP}" FILES "${FILE}")
endforeach()

# Unified output directory for artifacts
set(ARTIFACTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/wrapper/java")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}")

# Build jars
find_package(Java REQUIRED)
include(UseJava)

# Prepare custom manifest with library version
set(MANIFEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/manifest")
file(MAKE_DIRECTORY "${MANIFEST_DIR}")
set(MANIFEST_FILE "${MANIFEST_DIR}/manifest.mf")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/manifest.mf.in" "${MANIFEST_FILE}" @ONLY)

# Binary classes jar with version in file name and custom manifest
add_jar(
    BqLogWrapper_Java
    SOURCES ${SRC_LIST}
    OUTPUT_DIR "${ARTIFACTS_DIR}"
    OUTPUT_NAME "BqLogWrapper_Java-${BQ_LOG_VERSION}"
    MANIFEST "${MANIFEST_FILE}"
)
message("Jar file generated at directory: (ProjectRoot)/artifacts/wrapper/java")

# Sources jar 
set(SOURCES_JAR_PATH "${ARTIFACTS_DIR}/BqLogWrapper_Java-${BQ_LOG_VERSION}-sources.jar")
add_custom_command(
    OUTPUT "${SOURCES_JAR_PATH}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARTIFACTS_DIR}"
    COMMAND "${Java_JAR_EXECUTABLE}" cfm "BqLogWrapper_Java-${BQ_LOG_VERSION}-sources.jar" "${MANIFEST_FILE}" -C "${SRC_DIR}" .
    DEPENDS ${SRC_LIST} "${MANIFEST_FILE}"
    WORKING_DIRECTORY "${ARTIFACTS_DIR}"
    COMMENT "Creating sources jar: ${SOURCES_JAR_PATH}"
    VERBATIM
)
add_custom_target(BqLogWrapper_Java_sources ALL DEPENDS "${SOURCES_JAR_PATH}")
add_dependencies(BqLogWrapper_Java_sources BqLogWrapper_Java)

# Javadoc 
set(JAVADOC_DIR "${ARTIFACTS_DIR}/javadoc")
set(JAVADOC_JAR_PATH "${ARTIFACTS_DIR}/BqLogWrapper_Java-${BQ_LOG_VERSION}-javadoc.jar")
add_custom_command(
    OUTPUT "${JAVADOC_JAR_PATH}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${JAVADOC_DIR}"
    COMMAND "${Java_JAVADOC_EXECUTABLE}" -encoding UTF-8 -quiet -Xdoclint:none
            -d "${JAVADOC_DIR}"
            ${SRC_LIST}
    COMMAND "${Java_JAR_EXECUTABLE}" cfm "BqLogWrapper_Java-${BQ_LOG_VERSION}-javadoc.jar" "${MANIFEST_FILE}" -C "${JAVADOC_DIR}" .
    DEPENDS ${SRC_LIST} "${MANIFEST_FILE}"
    WORKING_DIRECTORY "${ARTIFACTS_DIR}"
    COMMENT "Generating javadoc and packaging: ${JAVADOC_JAR_PATH}"
    VERBATIM
)
add_custom_target(BqLogWrapper_Java_javadoc ALL DEPENDS "${JAVADOC_JAR_PATH}")
add_dependencies(BqLogWrapper_Java_javadoc BqLogWrapper_Java)

# Custom target for JNI header generation (classpath uses the versioned jar)
add_custom_command(
    OUTPUT "log_invoker.h"
    COMMAND echo "Generating JNI header file"
    COMMAND javac -encoding UTF-8
            -classpath "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/BqLogWrapper_Java-${BQ_LOG_VERSION}.jar"
            -h "${CMAKE_CURRENT_SOURCE_DIR}/../../src/bq_log/api/"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/bq/impl/log_invoker.java"
    DEPENDS BqLogWrapper_Java
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating JNI header file"
)
add_custom_target(generate_jni_headers ALL DEPENDS "log_invoker.h")
add_dependencies(generate_jni_headers BqLogWrapper_Java)

set(BQ_LOG_INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../install/wrapper")
set(BQ_LOG_DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../dist")

install(
        FILES
            "${ARTIFACTS_DIR}/BqLogWrapper_Java-${BQ_LOG_VERSION}.jar"
            "${ARTIFACTS_DIR}/BqLogWrapper_Java-${BQ_LOG_VERSION}-sources.jar"
            "${ARTIFACTS_DIR}/BqLogWrapper_Java-${BQ_LOG_VERSION}-javadoc.jar"
        DESTINATION "${BQ_LOG_INSTALL_DIR}"
    )
install(
        FILES
            "${ARTIFACTS_DIR}/BqLogWrapper_Java-${BQ_LOG_VERSION}.jar"
            "${ARTIFACTS_DIR}/BqLogWrapper_Java-${BQ_LOG_VERSION}-sources.jar"
            "${ARTIFACTS_DIR}/BqLogWrapper_Java-${BQ_LOG_VERSION}-javadoc.jar"
        DESTINATION "${BQ_LOG_DIST_DIR}"
    )