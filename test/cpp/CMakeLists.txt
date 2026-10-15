cmake_minimum_required(VERSION 3.22)

project(BqLogUnitTest)

if(NOT DEFINED CPP_VER)
    set(CMAKE_CXX_STANDARD 17)
    message("Use default C++ 17")
else()
    set(CMAKE_CXX_STANDARD ${CPP_VER})
    message("Use custom C++ ${CPP_VER}")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED True)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
OPTION(JAVA_SUPPORT "Support Java" OFF)

set(BQ_ENABLE_ASAN_UPPER "")
if(DEFINED ENV{BQ_ENABLE_ASAN})
    string(TOUPPER "$ENV{BQ_ENABLE_ASAN}" BQ_ENABLE_ASAN_UPPER)
endif()

if(DEFINED ENV{BQ_ENABLE_ASAN} AND BQ_ENABLE_ASAN_UPPER MATCHES "^(TRUE|ON|1)$")
    message(STATUS "BQ_ENABLE_ASAN is TRUE (case-insensitive), enabling Address Sanitizer")
    if(MSVC)
        add_compile_options(/fsanitize=address)
        # Windows ASan does not support Debug Runtime (/MDd), must use Release Runtime (/MD)
        foreach(flag_var
            CMAKE_C_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_C_FLAGS_RELWITHDEBINFO
            CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            if(${flag_var} MATCHES "/MDd")
                string(REPLACE "/MDd" "/MD" ${flag_var} "${${flag_var}}")
            endif()
        endforeach()
    else()
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
        if(APPLE)
            set(CMAKE_XCODE_ATTRIBUTE_CLANG_ADDRESS_SANITIZER YES)
        endif()
    endif()
    add_compile_definitions(BQ_ASAN_ENABLED)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt)
endif()

#make __cplusplus correct in Visual Studio
if (MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus /utf-8 /bigobj")
    add_compile_options(/W4 /WX)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND EXTRA_FLAGS -Wshorten-64-to-32)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        list(APPEND EXTRA_FLAGS -Wlogical-op)
        list(APPEND EXTRA_FLAGS -Wduplicated-branches)
        list(APPEND EXTRA_FLAGS -Warray-bounds)
    endif()

    add_compile_options(
            -Wall -Wextra -pedantic -Werror
            -Wundef -Wshadow -Wstrict-aliasing
            -Wcast-qual -Wconversion -Wsign-conversion
            -Wnull-dereference -Wuninitialized
            ${EXTRA_FLAGS}
    )
    if (TARGET_PLATFORM STREQUAL "ohos" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wno-unused-command-line-argument -Wno-error=unused-command-line-argument)
    endif()
endif()

#log test use template to generate millions test case, that makes object files become bigger
if (MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /bigobj")
elseif(CMAKE_COMPILER_IS_GNUCXX)
	include(CheckCXXCompilerFlag)
	check_cxx_compiler_flag("-Wa,-mbig-obj" GNU_BIG_OBJ_FLAG_ENABLE)
	if (${GNU_BIG_OBJ_FLAG_ENABLE})
    string(APPEND CMAKE_CXX_FLAGS " -Wa,-mbig-obj")
	endif()
endif()

############################################################
# Config Source Files
############################################################
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/src)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PUB_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/include)
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty ${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/thirdparty)

############################################################
# Search Include Dirs
############################################################
COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${SRC_DIR}")
COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${PUB_INC_DIR}")
COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${THIRD_PARTY_DIR}")
COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${TEST_DIR}")

############################################################
# Search Source Files
############################################################
COLLECT_SRC_FILES(SRC_LIST "${SRC_DIR}")
COLLECT_SRC_FILES(INC_LIST "${PUB_INC_DIR}")
COLLECT_SRC_FILES(THIRD_PARTY_LIST "${THIRD_PARTY_DIR}")
COLLECT_SRC_FILES(TEST_LIST "${TEST_DIR}")

add_compile_definitions(BQ_UNIT_TEST)
option(BQ_TEST_DISABLE_FILE_LOCK "Test a platform without a cross-process file lock" OFF)
if(BQ_TEST_DISABLE_FILE_LOCK)
    add_compile_definitions(BQ_TEST_DISABLE_FILE_LOCK)
endif()
if(DEFINED ENV{GITHUB_ACTIONS} AND "$ENV{GITHUB_ACTIONS}" STREQUAL "true")
    message(STATUS "Detected: running inside GitHub Actions")
    add_compile_definitions(BQ_IN_GITHUB_ACTIONS=1)
endif()

if(DEFINED ENV{BQ_LOW_PERFORMANCE_TESTS} AND "$ENV{BQ_LOW_PERFORMANCE_TESTS}" STREQUAL "true")
    message(STATUS "Detected: low performance GitHub Actions")
    add_compile_definitions(BQ_UNITE_TEST_LOW_PERFORMANCE_MODE=1)
endif()


############################################################
# IDE Sources Group
############################################################

GROUP_FOR_IDE("${SRC_LIST}" "src")
GROUP_FOR_IDE("${TEST_LIST}" "test")
GROUP_FOR_IDE("${THIRD_PARTY_LIST}" "thirdparty")
GROUP_FOR_IDE("${INC_LIST}" "include")


include_directories(
    ${ALL_INC_DIRS}
)

if(TARGET_PLATFORM STREQUAL "android" OR TARGET_PLATFORM STREQUAL "ohos")
    # build static library
    add_executable(BqLogUnitTest
            ${SRC_LIST}
            ${TEST_LIST}
            ${THIRD_PARTY_LIST}
            ${INC_LIST})
elseif(TARGET_PLATFORM STREQUAL "ios")
    add_library(BqLogUnitTest
        STATIC
        ${SRC_LIST}
        ${TEST_LIST}
        ${THIRD_PARTY_LIST}
        ${INC_LIST})
else()
    if(JAVA_SUPPORT)
		if(DEFINED ENV{JAVA_HOME})
			message("JAVA_HOME: $ENV{JAVA_HOME}")
		else()
			message(FATAL_ERROR "can not find environment variable JAVA_HOME")
		endif()
		add_compile_definitions(BQ_JAVA)
		include_directories($ENV{JAVA_HOME}/include)
        file(GLOB JNI_SUBDIRS "$ENV{JAVA_HOME}/include/*")
        foreach(DIR IN LISTS JNI_SUBDIRS)
            if(IS_DIRECTORY ${DIR})
                include_directories(${DIR})
            endif()
        endforeach()
	endif()
    # The library itself, compiled once: both test entries link it, the include entry's
    # binary and the module entry's. Everything that is not test code belongs to it.
    add_library(bqlog_lib OBJECT ${SRC_LIST} ${THIRD_PARTY_LIST} ${INC_LIST})

    # build executable
    add_executable(BqLogUnitTest ${TEST_LIST})
    target_link_libraries(BqLogUnitTest PRIVATE bqlog_lib)
endif()



if(TARGET_PLATFORM STREQUAL "android")
    find_library(log-lib log)
    target_link_libraries(BqLogUnitTest PRIVATE ${log-lib})
    find_library(android-lib android)
    target_link_libraries(BqLogUnitTest PRIVATE ${android-lib})
elseif(TARGET_PLATFORM STREQUAL "ohos")
    find_library(HILOG_LIB hilog_ndk.z REQUIRED)
    target_link_libraries(BqLogUnitTest
            PRIVATE
            ${HILOG_LIB}
            dl
    )
elseif(TARGET_PLATFORM STREQUAL "linux")
    target_link_libraries(bqlog_lib PRIVATE pthread dl)
elseif(TARGET_PLATFORM STREQUAL "unix")
    target_link_libraries(bqlog_lib PRIVATE pthread)
    find_library(EXECINFO_LIBRARY execinfo)
    if(EXECINFO_LIBRARY)
        target_link_libraries(bqlog_lib PRIVATE ${EXECINFO_LIBRARY})
    endif()
elseif(TARGET_PLATFORM STREQUAL "win64")
    if (MSVC)
    else()
        target_link_libraries(bqlog_lib PRIVATE -ldbghelp)
    endif()
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT BqLogUnitTest)
endif()

############################################################
# C++20 module entry
############################################################
# `import bq.log;` is exercised by a binary of its own: a translation unit may not import
# and include the same library, so it cannot live in BqLogUnitTest. The test runners start
# it after BqLogUnitTest and CTest registers it, so it is compiled and run wherever a
# toolchain can build modules; one that cannot skips it and says so.
OPTION(BQ_MODULE_TEST "Build and run the C++20 module entry test" ON)

set(BQ_MODULE_TOOLCHAIN_OK FALSE)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.28)
    if(MSVC AND MSVC_VERSION GREATER_EQUAL 1934)
        set(BQ_MODULE_TOOLCHAIN_OK TRUE)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16)
        set(BQ_MODULE_TOOLCHAIN_OK TRUE)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14)
        set(BQ_MODULE_TOOLCHAIN_OK TRUE)
    endif()
endif()

# Only where the test binary is an executable that can be run: iOS builds a static
# library, and whether the module is shipped to the mobile platforms is a packaging
# question of its own.
if(BQ_MODULE_TEST AND DEFINED CPP_VER AND CPP_VER GREATER_EQUAL 20 AND BQ_MODULE_TOOLCHAIN_OK
        AND CMAKE_GENERATOR MATCHES "Ninja|Visual Studio"
        AND TARGET bqlog_lib)
    message(STATUS "Module entry test: enabled (import bq.log)")

    # PUBLIC: an import from another target needs the file set in the interface
    add_library(bqlog_module STATIC)
    target_include_directories(bqlog_module PRIVATE ${ALL_INC_DIRS})
    target_sources(bqlog_module PUBLIC FILE_SET CXX_MODULES BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../../include
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../include/bq_log/bq.log.cppm)
    # the module has to see the headers exactly as the test sources do
    target_compile_definitions(bqlog_module PRIVATE BQ_UNIT_TEST)

    # The generator also writes test_category_log.cppm, so the category log is exercised through a
    # module entry too. It has to be built before the test binary, hence the dependency.
    set(BQ_MODULE_TEST_LIBS "")
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_category_log.cppm)
        add_library(bqlog_test_category_module STATIC)
        target_include_directories(bqlog_test_category_module PRIVATE ${ALL_INC_DIRS})
        target_sources(bqlog_test_category_module PUBLIC FILE_SET CXX_MODULES BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
                FILES ${CMAKE_CURRENT_SOURCE_DIR}/test_category_log.cppm)
        # the module has to see the headers exactly as the test sources do, and it imports bq.log
        target_compile_definitions(bqlog_test_category_module PRIVATE BQ_UNIT_TEST)
        target_link_libraries(bqlog_test_category_module PRIVATE bqlog_module)
        list(APPEND BQ_MODULE_TEST_LIBS bqlog_test_category_module)
    endif()

    add_executable(BqLogModuleUnitTest ${CMAKE_CURRENT_SOURCE_DIR}/../module/module_test.cpp)
    target_link_libraries(BqLogModuleUnitTest PRIVATE bqlog_lib bqlog_module ${BQ_MODULE_TEST_LIBS})
    # a source file that only imports is not scanned by default; CMP0155 would scan the
    # whole project, this says so for this target alone
    set_property(TARGET BqLogModuleUnitTest PROPERTY CXX_SCAN_FOR_MODULES ON)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC's modules are still a TS, and the flag is needed on both sides.
        target_compile_options(bqlog_module PRIVATE -fmodules-ts)
        target_compile_options(BqLogModuleUnitTest PRIVATE -fmodules-ts)
        foreach(_bq_module_test_lib IN LISTS BQ_MODULE_TEST_LIBS)
            target_compile_options(${_bq_module_test_lib} PRIVATE -fmodules-ts)
        endforeach()
    endif()

    enable_testing()
    add_test(NAME module_entry COMMAND BqLogModuleUnitTest)

    # the runner scripts require BqLogModuleUnitTest whenever this marker exists
    file(WRITE "${CMAKE_BINARY_DIR}/bq_module_test_enabled" "BqLogModuleUnitTest\n")
else()
    file(REMOVE "${CMAKE_BINARY_DIR}/bq_module_test_enabled")
    message(STATUS "Module entry test: skipped (needs C++20, CMake 3.28+, a toolchain with modules, and a Ninja or VS generator; -DBQ_MODULE_TEST=ON enables it once include/bq_log/bq.log.cppm is finished)")
endif()


