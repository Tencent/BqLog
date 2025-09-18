cmake_minimum_required(VERSION 3.22)


set(PROJ_NAME "BqLogWrapper_Java")

message(${PROJ_NAME})

project(${PROJ_NAME} NONE)
set(CMAKE_JAVA_COMPILE_FLAGS "-encoding" "UTF-8")

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

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/wrapper/java")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/wrapper/java")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/wrapper/java")

set(CMAKE_JAVA_COMPILE_FLAGS "-encoding" "UTF-8" "-g" "-parameters" "-nowarn")

# build library
find_package(Java REQUIRED)
INCLUDE(UseJava)
add_jar(BqLogWrapper_Java ${SRC_LIST} OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/wrapper/java")

message("Jar file generated at directory:(ProjectRoot)/artifacts/wrapper/java")

# Custom target for JNI header generation
add_custom_command(
    OUTPUT "log_invoker.h"
    COMMAND echo "Generating JNI header file"
    COMMAND javac -encoding UTF-8 -classpath ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/BqLogWrapper_Java.jar -h ${CMAKE_CURRENT_SOURCE_DIR}/../../src/bq_log/api/ ${CMAKE_CURRENT_SOURCE_DIR}/src/bq/impl/log_invoker.java
    DEPENDS BqLogWrapper_Java
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating JNI header file"
)

add_custom_target(generate_jni_headers ALL DEPENDS "log_invoker.h")

# Ensure the jar target is built before the JNI headers are generated
add_dependencies(generate_jni_headers BqLogWrapper_Java)
