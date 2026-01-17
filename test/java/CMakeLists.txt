cmake_minimum_required(VERSION 3.22)
project(BqLogJavaTest)

find_package(Java REQUIRED)
include(UseJava)

# Include Wrapper Sources (Reuse source code directly to create a standalone test jar)
set(WRAPPER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../wrapper/java/src")
file(GLOB_RECURSE WRAPPER_SRC_LIST "${WRAPPER_SRC_DIR}/*.java")

# Include Test Sources
set(TEST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(GLOB_RECURSE TEST_SRC_LIST "${TEST_SRC_DIR}/*.java")

# Output Dir
set(ARTIFACTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/test/java")
file(MAKE_DIRECTORY ${ARTIFACTS_DIR})

# Build the Test Jar
# We include both wrapper and test sources.
add_jar(
    BqLogJavaTest
    SOURCES ${WRAPPER_SRC_LIST} ${TEST_SRC_LIST}
    OUTPUT_DIR "${ARTIFACTS_DIR}"
    OUTPUT_NAME "BqLogJavaTest"
    ENTRY_POINT "bq.test.test_main"
)

message(STATUS "Java Test Jar configured: ${ARTIFACTS_DIR}/BqLogJavaTest.jar")
