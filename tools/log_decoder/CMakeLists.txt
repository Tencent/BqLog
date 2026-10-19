cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt)
endif()

#Parse version from version.cpp
bq_parse_version_from_cpp("${CMAKE_CURRENT_SOURCE_DIR}/../../src/bq_log/global/version.cpp")

project(BqLog_LogDecoder VERSION ${BQ_VERSION} LANGUAGES CXX)

include(GNUInstallDirs)

# A CI or local build that only needs the binary must not need write access to /usr/local:
# keep the default prefix inside the build tree unless the caller passed one.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install path prefix" FORCE)
endif()

# A driver script or a package manager passes TARGET_PLATFORM; detect it otherwise.
bq_default_target_platform()

# Packagers turn this off so a new compiler warning cannot fail a package build.
OPTION(BQ_WERROR "Treat compiler warnings as errors" ON)

# The tools ship next to the library, so their code has to be position independent too.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")
endif()

bq_set_warning_flags()

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

set(ARTIFACTS_SUB_DIR "bin/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ARTIFACTS_OUTPUT_BASE_DIR}/${ARTIFACTS_SUB_DIR}")

message("Artifacts Directory:${ARTIFACTS_OUTPUT_BASE_DIR}/${ARTIFACTS_SUB_DIR}")

# Universal on Apple unless the caller chose the architectures; a package manager must not get one.
if(TARGET_PLATFORM STREQUAL "mac" AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
endif()

# generate executable
add_executable(BqLog_LogDecoder
        ${TOOLS_LIST}
        ${SRC_LIST}
        ${INC_LIST}
        ${THIRD_PARTY_LIST}
)


# set start up project
if(TARGET_PLATFORM STREQUAL "win64")
    if (MSVC)
    else()
        target_link_libraries(BqLog_LogDecoder PRIVATE -ldbghelp)
    endif()
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT BqLog_LogDecoder)
elseif(TARGET_PLATFORM STREQUAL "linux")
    target_link_libraries(BqLog_LogDecoder PRIVATE pthread)
elseif(TARGET_PLATFORM STREQUAL "unix")
    target_link_libraries(BqLog_LogDecoder PRIVATE pthread)
    find_library(EXECINFO_LIBRARY execinfo)
    if(EXECINFO_LIBRARY)
        target_link_libraries(BqLog_LogDecoder PRIVATE ${EXECINFO_LIBRARY})
    endif()
endif()




#Ensure the target carries version/soname metadata
set_target_properties(BqLog_LogDecoder PROPERTIES
        VERSION ${BQ_VERSION}
        SOVERSION ${BQ_SOVERSION}
)

install(TARGETS BqLog_LogDecoder
        ARCHIVE DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        FRAMEWORK DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(FILES "${ARTIFACTS_OUTPUT_BASE_DIR}/${ARTIFACTS_SUB_DIR}/BqLog_LogDecoder.pdb"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        OPTIONAL
)

file(GLOB README_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../*.md"
)

install(FILES ${README_FILES}
        DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../docs"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)











