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

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../CMake_utils.txt)
endif()

#make __cplusplus correct in Visual Studio
if (MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus /utf-8 /bigobj")
    add_compile_options(/W4 /WX)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(EXTRA_FLAGS "-Wshorten-64-to-32")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(EXTRA_FLAGS "-Wlogical-op -Wduplicated-branches -Warray-bounds")
    endif()

    add_compile_options(
            -Wall -Wextra -pedantic -Werror
            -Wundef -Wshadow -Wstrict-aliasing
            -Wcast-qual -Wconversion -Wsign-conversion
            -Wnull-dereference -Wformat=2 -Wuninitialized
            ${EXTRA_FLAGS}
    )
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
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src ${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/src)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PUB_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/include)
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty ${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/thirdparty)

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

if(TARGET_PLATFORM STREQUAL "android" OR TARGET_PLATFORM STREQUAL "ios")
    # build shared library
    add_library(BqLogUnitTest
            SHARED
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
    # build executable
    add_executable(BqLogUnitTest
            ${SRC_LIST}
            ${TEST_LIST}
            ${THIRD_PARTY_LIST}
            ${INC_LIST}
            )
endif()



if(TARGET_PLATFORM STREQUAL "android")
    find_library(log-lib log)
    target_link_libraries(BqLogUnitTest ${log-lib} )
    find_library(android-lib android)
    target_link_libraries(BqLogUnitTest ${android-lib} )
elseif(TARGET_PLATFORM STREQUAL "linux")
    target_link_libraries(BqLogUnitTest pthread dl)
elseif(TARGET_PLATFORM STREQUAL "unix")
    target_link_libraries(BqLogUnitTest PRIVATE pthread)
    find_library(EXECINFO_LIBRARY execinfo)
    if(EXECINFO_LIBRARY)
        target_link_libraries(BqLogUnitTest PRIVATE ${EXECINFO_LIBRARY})
    endif()
elseif(TARGET_PLATFORM STREQUAL "win64")
    if (MSVC)
    else()
        target_link_libraries(BqLogUnitTest PRIVATE -ldbghelp)
    endif()
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT BqLogUnitTest)
endif()


