cmake_minimum_required(VERSION 3.22)

set(TARGET_PLATFORM "win64" CACHE STRING "Choose the target platform")
set_property(CACHE TARGET_PLATFORM PROPERTY STRINGS "win64" "mac" "ios" "android" "linux" "unix")

set(BUILD_LIB_TYPE "static_lib" CACHE STRING "Choose the build type")
set_property(CACHE BUILD_LIB_TYPE PROPERTY STRINGS "static_lib" "dynamic_lib")

if(TARGET_PLATFORM STREQUAL "mac" OR TARGET_PLATFORM STREQUAL "ios")
set(APPLE_LIB_FORMAT "framework" CACHE STRING "Apple build format")
set_property(CACHE APPLE_LIB_FORMAT PROPERTY STRINGS "framework" "dylib" "a")
endif()

OPTION(JAVA_SUPPORT "Support Java" OFF)
OPTION(NODE_API_SUPPORT "Support Node-API(NodeJs)" OFF)
OPTION(BQ_ENABLE_ASAN "Enable Address Sanitizer" OFF)

if(BQ_ENABLE_ASAN)
    if(MSVC)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        link_libraries(-fsanitize=address)
    endif()
    add_compile_definitions(BQ_ASAN_ENABLED)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../CMake_utils.txt)
endif()
#Parse version from version.cpp
bq_parse_version_from_cpp("${CMAKE_CURRENT_SOURCE_DIR}/../src/bq_log/global/version.cpp")

project(BqLog VERSION ${BQ_VERSION} LANGUAGES CXX)
BQ_DETECT_ARCH(DETECTED_ARCH)


if(NOT DEFINED CPP_VER)
    set(CMAKE_CXX_STANDARD 17)
    message(STATUS "Use default C++ 17")
else()
    set(CMAKE_CXX_STANDARD ${CPP_VER})
    message(STATUS "Use custom C++ ${CPP_VER}")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /D_HAS_EXCEPTIONS=0 /GR- /utf-8")
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W4 /WX)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND EXTRA_FLAGS -Wshorten-64-to-32)
        list(APPEND EXTRA_FLAGS -Wno-error=unused-command-line-argument)
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
    if (TARGET_PLATFORM STREQUAL "ohos" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wno-unused-command-line-argument -Wno-error=unused-command-line-argument)
    endif()
endif()
if(ANDROID_STL STREQUAL "none")
    add_compile_definitions(BQ_NO_LIBCPP=1)
endif()


#make __cplusplus correct in Visual Studio
if (CMAKE_GENERATOR MATCHES "Visual Studio")
    string(APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus")
endif()

############################################################
# Config Source Dirs
############################################################
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../../submodules/BqCommon/src)
set(PUB_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/../../../submodules/BqCommon/include)
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty ${CMAKE_CURRENT_SOURCE_DIR}/../../../submodules/BqCommon/thirdparty)
############################################################
# Search Include Dirs
############################################################
COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${SRC_DIR}")
COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${PUB_INC_DIR}")
COLLECT_INCLUDE_DIRS(ALL_INC_DIRS "${THIRD_PARTY_DIR}")
############################################################
# Search Source Files
############################################################
COLLECT_SRC_FILES(SRC_LIST "${SRC_DIR}")
COLLECT_SRC_FILES(INC_LIST "${PUB_INC_DIR}")
COLLECT_SRC_FILES(THIRD_PARTY_LIST "${THIRD_PARTY_DIR}")

############################################################
# IDE Sources Group
############################################################
GROUP_FOR_IDE("${SRC_LIST}" "src")
GROUP_FOR_IDE("${THIRD_PARTY_LIST}" "thirdparty")
GROUP_FOR_IDE("${INC_LIST}" "include")

include_directories(
    ${ALL_INC_DIRS}
)

set(ARTIFACTS_OUTPUT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../artifacts/${BUILD_LIB_TYPE}")
set(INSTALL_OUTPUT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../install/${BUILD_LIB_TYPE}")

if(TARGET_PLATFORM STREQUAL "android")
    set(ARTIFACTS_SUB_DIR "lib/$<CONFIG>/${ANDROID_ABI}")
elseif(TARGET_PLATFORM STREQUAL "ohos")
    set(ARTIFACTS_SUB_DIR "lib/$<CONFIG>/${OHOS_ARCH}")
else()
	set(ARTIFACTS_SUB_DIR "lib/$<CONFIG>")
endif()

if(JAVA_SUPPORT)
	if(DEFINED ENV{JAVA_HOME})
		message(STATUS "JAVA_HOME: $ENV{JAVA_HOME}")
	else()
		message(FATAL_ERROR "can not find environment variable JAVA_HOME")
	endif()
	add_compile_definitions(BQ_JAVA)
	include_directories($ENV{JAVA_HOME}/include)
    message(STATUS "AddInclude: $ENV{JAVA_HOME}/include")
    file(GLOB JNI_SUBDIRS "$ENV{JAVA_HOME}/include/*")
    foreach(DIR IN LISTS JNI_SUBDIRS)
        if(IS_DIRECTORY ${DIR})
            message(STATUS "AddInclude: ${DIR}")
            include_directories(${DIR})
         endif()
    endforeach()
endif()
if(NODE_API_SUPPORT)
	add_compile_definitions(BQ_NAPI)
    add_compile_definitions(NAPI_VERSION=6)  #best compability, for nodejs version >= 12
    set(_NODE_API_HEADERS_FOUND OFF)
    set(_NODE_API_HEADERS_DIR "")

    # 1) ENV: NODE_MODULES_ROOT/node-api-headers/include
    if(DEFINED ENV{NODE_MODULES_ROOT} AND NOT "$ENV{NODE_MODULES_ROOT}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{NODE_MODULES_ROOT}" _NMR)
        if(EXISTS "${_NMR}/node-api-headers/include/node_api.h")
            set(_NODE_API_HEADERS_DIR "${_NMR}/node-api-headers/include")
            set(_NODE_API_HEADERS_FOUND ON)
        endif()
    endif()

    # 2) node -p "require.resolve('node-api-headers/package.json')" from CMAKE_SOURCE_DIR (then PROJECT_SOURCE_DIR)
    if(NOT _NODE_API_HEADERS_FOUND)
        find_program(NODE_EXECUTABLE node)
        if(NOT NODE_EXECUTABLE)
            set(NODE_EXECUTABLE "node")
        endif()
        if(NODE_EXECUTABLE)
            set(_NODE_RESOLVE_JS [[require('path').dirname(require.resolve('node-api-headers/package.json'))]])

            execute_process(
                COMMAND "${NODE_EXECUTABLE}" -p "${_NODE_RESOLVE_JS}"
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                RESULT_VARIABLE _RES
                OUTPUT_VARIABLE _PKG_DIR
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if(_RES EQUAL 0 AND NOT "${_PKG_DIR}" STREQUAL "")
                file(TO_CMAKE_PATH "${_PKG_DIR}" _PKG_DIR_CMAKE)
                if(EXISTS "${_PKG_DIR_CMAKE}/include/node_api.h")
                    set(_NODE_API_HEADERS_DIR "${_PKG_DIR_CMAKE}/include")
                    set(_NODE_API_HEADERS_FOUND ON)
                endif()
            endif()

            if(NOT _NODE_API_HEADERS_FOUND AND NOT "${PROJECT_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
                execute_process(
                    COMMAND "${NODE_EXECUTABLE}" -p "${_NODE_RESOLVE_JS}"
                    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
                    RESULT_VARIABLE _RES2
                    OUTPUT_VARIABLE _PKG_DIR2
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                )
                if(_RES2 EQUAL 0 AND NOT "${_PKG_DIR2}" STREQUAL "")
                    file(TO_CMAKE_PATH "${_PKG_DIR2}" _PKG_DIR2_CMAKE)
                    if(EXISTS "${_PKG_DIR2_CMAKE}/include/node_api.h")
                        set(_NODE_API_HEADERS_DIR "${_PKG_DIR2_CMAKE}/include")
                        set(_NODE_API_HEADERS_FOUND ON)
                    endif()
                endif()
            endif()
        endif()
    endif()

    if(_NODE_API_HEADERS_FOUND)
        include_directories(${_NODE_API_HEADERS_DIR})
    else()
        message(FATAL_ERROR
            "Node-API headers not found.\n"
            "Please run: npm i node-api-headers\n"
            "and set the environment variable NODE_MODULES_ROOT to your node_modules directory.\n"
            "Expected header: %ENV{NODE_MODULES_ROOT}/node-api-headers/include/node_api.h"
        )
    endif()
endif()



set(INSTALL_SUB_DIR "${ARTIFACTS_SUB_DIR}")

message(STATUS "Artifacts Directory:${ARTIFACTS_OUTPUT_BASE_DIR}/${ARTIFACTS_SUB_DIR}")
message(STATUS "Dist Directory:${INSTALL_OUTPUT_BASE_DIR}/${INSTALL_SUB_DIR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ARTIFACTS_OUTPUT_BASE_DIR}/${ARTIFACTS_SUB_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ARTIFACTS_OUTPUT_BASE_DIR}/${ARTIFACTS_SUB_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${ARTIFACTS_OUTPUT_BASE_DIR}/${ARTIFACTS_SUB_DIR}")

#universal binary for mac os, support both Intel cpu and Apple cpu
if(TARGET_PLATFORM STREQUAL "mac")
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
endif()

if(BUILD_LIB_TYPE STREQUAL "static_lib")
    # build static library
    add_library(BqLog STATIC ${SRC_LIST} ${THIRD_PARTY_LIST} ${INC_LIST})
else()
    # build dynamic library
    add_compile_definitions(BQ_DYNAMIC_LIB)
    if(NODE_API_SUPPORT)
        add_library(BqLog MODULE ${SRC_LIST} ${THIRD_PARTY_LIST} ${INC_LIST})
        set_target_properties(BqLog PROPERTIES PREFIX "" SUFFIX ".node")
    else()
        add_library(BqLog SHARED ${SRC_LIST} ${THIRD_PARTY_LIST} ${INC_LIST})
    endif()
endif()


if(TARGET_PLATFORM STREQUAL "android")
    find_library(log-lib log)
    target_link_libraries(BqLog ${log-lib} )
    find_library(android-lib android)
    target_link_libraries(BqLog ${android-lib} )
    target_link_options(BqLog PRIVATE "-Wl,-z,max-page-size=16384")
elseif(TARGET_PLATFORM STREQUAL "linux")
    target_link_libraries(BqLog PRIVATE pthread)
elseif(TARGET_PLATFORM STREQUAL "unix")
    target_link_libraries(BqLog PRIVATE pthread)
    find_library(EXECINFO_LIBRARY execinfo)
    if(EXECINFO_LIBRARY)
        target_link_libraries(BqLog PRIVATE ${EXECINFO_LIBRARY})
    endif()
elseif(TARGET_PLATFORM STREQUAL "ohos")
    find_library(ACE_NAPI_LIB ace_napi.z REQUIRED)
    find_library(HILOG_LIB hilog_ndk.z REQUIRED)
    target_link_libraries(BqLog
            PRIVATE
            ${ACE_NAPI_LIB}
            ${HILOG_LIB}
            dl
    )
elseif(TARGET_PLATFORM STREQUAL "mac")
    set_xcode_property(BqLog DEBUG_INFORMATION_FORMAT "dwarf-with-dsym" "ALL")
    if(APPLE_LIB_FORMAT STREQUAL "framework")
        set_target_properties(BqLog PROPERTIES
            FRAMEWORK TRUE
            MACOSX_FRAMEWORK_IDENTIFIER com.tencent.BqLog
            PRODUCT_BUNDLE_IDENTIFIER com.tencent.BqLog
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.tencent.BqLog
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
        )
    endif()
    if(NODE_API_SUPPORT)
        target_link_options(BqLog PRIVATE "-undefined" "dynamic_lookup")
    endif()
elseif(TARGET_PLATFORM STREQUAL "ios")
    target_link_libraries(BqLog PRIVATE "-framework Security")
    target_link_libraries(BqLog PRIVATE "-framework UIKit")
    
    set_xcode_property(BqLog DEBUG_INFORMATION_FORMAT "dwarf-with-dsym" "ALL")
    
    if(APPLE_LIB_FORMAT STREQUAL "framework")
        set_target_properties(BqLog PROPERTIES
            FRAMEWORK TRUE
            MACOSX_FRAMEWORK_IDENTIFIER com.tencent.BqLog
            PRODUCT_BUNDLE_IDENTIFIER com.tencent.BqLog
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.tencent.BqLog
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
        )
    endif()
elseif(TARGET_PLATFORM STREQUAL "win64")
    if (MSVC)
    else()
        target_link_libraries(BqLog PRIVATE -ldbghelp)
    endif()
    if(NODE_API_SUPPORT)
        #find node.lib
        # Discover Node version via `node -p process.versions.node`
        execute_process(
        COMMAND "${NODE_EXECUTABLE}" -p "process.versions.node"
            RESULT_VARIABLE NODE_CMD_RC
            OUTPUT_VARIABLE NODEJS_VERSION
            ERROR_VARIABLE NODE_CMD_ERR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
)

        # Determine arch subdir used by node-gyp cache
        set(_node_arch "${DETECTED_ARCH}")
        if(_node_arch STREQUAL "x86_64")
            set(_node_arch "x64")
        endif()

        # Candidate cache paths (new node-gyp cache and legacy .node-gyp)
        if(DEFINED ENV{LOCALAPPDATA})
            file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}/node-gyp/Cache/${NODEJS_VERSION}/${_node_arch}" _gyp_cache_new)
        endif()
        if(DEFINED ENV{USERPROFILE})
            file(TO_CMAKE_PATH "$ENV{USERPROFILE}/.node-gyp/${NODEJS_VERSION}/${_node_arch}" _gyp_cache_legacy)
        endif()
        message(STATUS "$ENV{LOCALAPPDATA}/node-gyp/Cache/${NODEJS_VERSION}/${_node_arch}")
        message(STATUS "$ENV{USERPROFILE}/.node-gyp/${NODEJS_VERSION}/${_node_arch}")

        # Choose library names per toolchain
        if(MINGW)
            set(_node_lib_names libnode.dll.a)
        else()
            set(_node_lib_names node node.lib)
        endif()

        find_library(NODEJS_IMPORT_LIB
            NAMES ${_node_lib_names}
            PATHS
            "${_gyp_cache_new}"
            "${_gyp_cache_legacy}"
            NO_DEFAULT_PATH)

        unset(_gyp_cache_new)
        unset(_gyp_cache_legacy)
        unset(_node_arch)
        unset(_node_lib_names)

        if(NOT NODEJS_IMPORT_LIB OR NOT EXISTS "${NODEJS_IMPORT_LIB}")
            message(FATAL_ERROR
            "Node import library not found. "
            "or install via `npx node-gyp install`.")
        endif()
        message(STATUS "Using Node import library: ${NODEJS_IMPORT_LIB}")
        target_link_libraries(BqLog PRIVATE "${NODEJS_IMPORT_LIB}")
    endif()
endif()

add_custom_command(TARGET BqLog POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${ARTIFACTS_OUTPUT_BASE_DIR}/include"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../include" "${ARTIFACTS_OUTPUT_BASE_DIR}/include"
    COMMENT "Copying include headers to install folder"
)
if(BUILD_LIB_TYPE STREQUAL "dynamic_lib")
    add_custom_command(TARGET BqLog POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ARTIFACTS_OUTPUT_BASE_DIR}/include/bq_common/platform/build_type_dynamic_lib_import.h" "${ARTIFACTS_OUTPUT_BASE_DIR}/include/bq_common/platform/build_type.h"
        COMMENT "Switch include headers to dynamic lib mode"
    )
    if(TARGET_PLATFORM STREQUAL "mac" OR TARGET_PLATFORM STREQUAL "ios")
        if(APPLE_LIB_FORMAT STREQUAL "framework")
            add_custom_command(TARGET BqLog POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ARTIFACTS_OUTPUT_BASE_DIR}/include/bq_common/platform/build_type_dynamic_lib_import.h" "$<TARGET_FILE_DIR:BqLog>/Headers/bq_common/platform/build_type.h"
                COMMENT "Switch Framework headers to dynamic lib mode"
            )
        endif()
    endif()
endif()

#Ensure the target carries version/soname metadata
if(NOT TARGET_PLATFORM STREQUAL "ohos")
    set_target_properties(BqLog PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endif()

set(CMAKE_INSTALL_PREFIX "${INSTALL_OUTPUT_BASE_DIR}")


install(TARGETS BqLog
    ARCHIVE DESTINATION "${INSTALL_SUB_DIR}"
    LIBRARY DESTINATION "${INSTALL_SUB_DIR}"
    RUNTIME DESTINATION "${INSTALL_SUB_DIR}"
    FRAMEWORK DESTINATION "${INSTALL_SUB_DIR}"
)

install(DIRECTORY "${ARTIFACTS_OUTPUT_BASE_DIR}/${INSTALL_SUB_DIR}/"
    DESTINATION "${INSTALL_SUB_DIR}"
    FILES_MATCHING PATTERN "*.dSYM"
)

install(FILES "${ARTIFACTS_OUTPUT_BASE_DIR}/${INSTALL_SUB_DIR}/BqLog.pdb"
        DESTINATION "${INSTALL_SUB_DIR}"
        OPTIONAL
)

file(GLOB MD_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../*.md"
)

install(FILES ${MD_FILES}
    DESTINATION ".."
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../docs"
    DESTINATION ".."
)

install(DIRECTORY "${ARTIFACTS_OUTPUT_BASE_DIR}/include"
    DESTINATION "."
)



