cmake_minimum_required(VERSION 3.22)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt)
endif()

project(BqLogDemo)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
#VS为了兼容性__cplusplus宏一直是199711，这里我们需要正确的值来判断c++版本
if (CMAKE_GENERATOR MATCHES "Visual Studio")
    string(APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus /utf-8")
endif()

############################################################
# Config Source Dirs
############################################################
set(DEMO_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PUB_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/include)

############################################################
# Search Include Dirs
############################################################
COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${DEMO_DIR}")
COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${PUB_INC_DIR}")
############################################################
# Search Source Files
############################################################
COLLECT_SRC_FILES(DEMO_LIST "${DEMO_DIR}")
COLLECT_SRC_FILES(INC_LIST "${PUB_INC_DIR}")

############################################################
# 保证IDE里面的源文件分组和实际的目录一致
############################################################
GROUP_FOR_IDE("${DEMO_LIST}" "demo")
GROUP_FOR_IDE("${INC_LIST}" "include")


include_directories(
    ${ALL_INC_DIRS}
)

set(BUILD_TYPE "static_lib")

if(TARGET_PLATFORM STREQUAL "linux" OR TARGET_PLATFORM STREQUAL "unix")
		set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/demo/${TARGET_PLATFORM}/${CMAKE_BUILD_TYPE}")
		set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/demo/${TARGET_PLATFORM}/${CMAKE_BUILD_TYPE}")
		set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/demo/${TARGET_PLATFORM}/${CMAKE_BUILD_TYPE}")
else()
		set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/demo/${TARGET_PLATFORM}")
		set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/demo/${TARGET_PLATFORM}")
		set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/demo/${TARGET_PLATFORM}")
endif()

if(TARGET_PLATFORM STREQUAL "android" OR TARGET_PLATFORM STREQUAL "ios")
    # build shared library
    add_library(BqLogDemo
            SHARED
			${DEMO_LIST}
			${INC_LIST})
else()
    # build executable
	add_executable(BqLogDemo
		${DEMO_LIST}
		${INC_LIST})
endif()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../src/" "${PROJECT_BINARY_DIR}/StaticLibProj")



if(TARGET_PLATFORM STREQUAL "android")
    find_library(log-lib log)
    target_link_libraries(BqLogDemo ${log-lib} "BqLog")
elseif(TARGET_PLATFORM STREQUAL "linux")
    target_link_libraries(BqLogDemo PRIVATE pthread dl "BqLog")
elseif(TARGET_PLATFORM STREQUAL "unix")
    target_link_libraries(BqLogDemo PRIVATE pthread dl "BqLog")
    find_library(EXECINFO_LIBRARY execinfo)
    if(EXECINFO_LIBRARY)
        target_link_libraries(BqLogDemo PRIVATE ${EXECINFO_LIBRARY})
    endif()
elseif(TARGET_PLATFORM STREQUAL "win64")
    # 设置启动工程
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT BqLogDemo)
	target_link_libraries(BqLogDemo "BqLog")
else()
    target_link_libraries(BqLogDemo "BqLog")
endif()

