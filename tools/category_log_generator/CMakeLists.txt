cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt)
endif()

#Parse version from version.cpp
bq_parse_version_from_cpp("${CMAKE_CURRENT_SOURCE_DIR}/../../src/bq_log/global/version.cpp")

project(BqLog_CategoryLogGenerator VERSION ${BQ_VERSION} LANGUAGES CXX)

if(NOT DEFINED CPP_VER)
    set(CMAKE_CXX_STANDARD 17)
    message("Use default C++ 17")
else()
    set(CMAKE_CXX_STANDARD ${CPP_VER})
    message("Use custom C++ ${CPP_VER}")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED True)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

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
            -Wnull-dereference -Wformat=2 -Wuninitialized
            ${EXTRA_FLAGS}
    )
endif()

############################################################
# Config Source Dirs
############################################################
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/src)
set(TOOLS_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PUB_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/include)
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty ${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/thirdparty)
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

set(ARTIFACTS_OUTPUT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/tools")
set(INSTALL_OUTPUT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../install/tools")

message("Artifacts Directory:${ARTIFACTS_OUTPUT_BASE_DIR}/${ARTIFACTS_SUB_DIR}")
message("Dist Directory:${INSTALL_OUTPUT_BASE_DIR}/${INSTALL_SUB_DIR}")

set(ARTIFACTS_SUB_DIR "bin/$<CONFIG>")
set(INSTALL_SUB_DIR "bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ARTIFACTS_OUTPUT_BASE_DIR}/${ARTIFACTS_SUB_DIR}")

#universal binary for mac os, support both Intel cpu and Apple cpu
if(TARGET_PLATFORM STREQUAL "mac")
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
endif()

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
        target_link_libraries(BqLog_CategoryLogGenerator PRIVATE -ldbghelp)
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




#Ensure the target carries version/soname metadata
set_target_properties(BqLog_CategoryLogGenerator PROPERTIES
        VERSION ${BQ_VERSION}
        SOVERSION ${BQ_SOVERSION}
)

set(CMAKE_INSTALL_PREFIX "${INSTALL_OUTPUT_BASE_DIR}")


install(TARGETS BqLog_CategoryLogGenerator
        ARCHIVE DESTINATION "${INSTALL_SUB_DIR}"
        LIBRARY DESTINATION "${INSTALL_SUB_DIR}"
        RUNTIME DESTINATION "${INSTALL_SUB_DIR}"
        FRAMEWORK DESTINATION "${INSTALL_SUB_DIR}"
)

install(FILES "${ARTIFACTS_OUTPUT_BASE_DIR}/${ARTIFACTS_SUB_DIR}/BqLog_CategoryLogGenerator.pdb"
        DESTINATION "${INSTALL_SUB_DIR}"
        OPTIONAL
)

file(GLOB README_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../*.md"
)

install(FILES ${README_FILES}
        DESTINATION "."
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../docs"
        DESTINATION "."
)











