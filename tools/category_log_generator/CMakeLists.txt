cmake_minimum_required(VERSION 3.22)

project(BqLog_CategoryLogGenerator)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_OSX_ARCHITECTURES "$(ARCHS_STANDARD)")

if(WIN32)
	#VS为了兼容性__cplusplus宏一直是199711，这里我们需要正确的值来判断c++版本
	string(APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus /utf-8")
endif()

############################################################
# Config Source Dirs
############################################################
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../submodules/BqCommon/src)
set(TOOLS_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PUB_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${CMAKE_CURRENT_SOURCE_DIR}/../../submodules/BqCommon/include)
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty ${CMAKE_CURRENT_SOURCE_DIR}/../../submodules/BqCommon/thirdparty)
############################################################
# Search Include Dirs
############################################################
COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${SRC_DIR}")
COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${TOOLS_DIR}")
COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${PUB_INC_DIR}")
COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${THIRD_PARTY_DIR}")
############################################################
# Search Sources
############################################################
COLLECT_SRC_FILES(SRC_LIST "${SRC_DIR}")
COLLECT_SRC_FILES(TOOLS_LIST "${TOOLS_DIR}")
COLLECT_SRC_FILES(INC_LIST "${PUB_INC_DIR}")
COLLECT_SRC_FILES(THIRD_PARTY_LIST "${THIRD_PARTY_DIR}")

add_compile_definitions(BQ_TOOLS)
add_compile_definitions(BQ_NO_DYNAMIC_LIB_CALL)

############################################################
# 保证VS里面的源文件分组和实际的目录一致
############################################################
GROUP_FOR_IDE("${SRC_LIST}" "src")
GROUP_FOR_IDE("${TOOLS_LIST}" "category_log_generator")
GROUP_FOR_IDE("${THIRD_PARTY_LIST}" "thirdparty")
GROUP_FOR_IDE("${INC_LIST}" "include")

include_directories(
    ${ALL_INC_DIRS}
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../bin/tools/${TARGET_PLATFORM}${BITS_SUFFIX})

# generate executable
add_executable(BqLog_CategoryLogGenerator
	${TOOLS_LIST}
	${SRC_LIST}
	${INC_LIST}
	${THIRD_PARTY_LIST}
)


# set start up project
if(TARGET_PLATFORM STREQUAL "win64")
    if (MSVC)
    else()
        target_link_libraries(BqLogUnitTest PRIVATE -ldbghelp)
    endif()
   set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT BqLog_CategoryLogGenerator)
elseif(TARGET_PLATFORM STREQUAL "linux")
    target_link_libraries(BqLog_CategoryLogGenerator PRIVATE pthread)
elseif(TARGET_PLATFORM STREQUAL "unix")
    target_link_libraries(BqLog_CategoryLogGenerator PRIVATE pthread)
    find_library(EXECINFO_LIBRARY execinfo)
    if(EXECINFO_LIBRARY)
        target_link_libraries(BqLog_CategoryLogGenerator PRIVATE ${EXECINFO_LIBRARY})
    endif()
endif()