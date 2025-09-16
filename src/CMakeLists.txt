cmake_minimum_required(VERSION 3.22)
set(TARGET_PLATFORM "win64" CACHE STRING "Choose the target platform")
set_property(CACHE TARGET_PLATFORM PROPERTY STRINGS "win64" "mac" "ios" "android" "linux" "unix")

set(BUILD_TYPE "static_lib" CACHE STRING "Choose the build type")
set_property(CACHE BUILD_TYPE PROPERTY STRINGS "static_lib" "dynamic_lib")

if(TARGET_PLATFORM STREQUAL "mac" OR TARGET_PLATFORM STREQUAL "ios")
set(APPLE_LIB_FORMAT "framework" CACHE STRING "Apple build format")
set_property(CACHE APPLE_LIB_FORMAT PROPERTY STRINGS "framework" "dylib" "a")
endif()

OPTION(JAVA_SUPPORT "Support Java" OFF)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../CMake_utils.txt)
endif()

#Parse version from version.cpp
bq_parse_version_from_cpp("${CMAKE_CURRENT_SOURCE_DIR}/../src/bq_log/global/version.cpp")

project(BqLog VERSION ${BQ_LOG_VERSION} LANGUAGES CXX)
if(NOT DEFINED CPP_VER)
    set(CMAKE_CXX_STANDARD 17)
    message("Use default C++ 17")
else()
    set(CMAKE_CXX_STANDARD ${CPP_VER})
    message("Use custom C++ ${CPP_VER}")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /D_HAS_EXCEPTIONS=0 /GR-")
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W4 /WX)
elseif(TARGET_PLATFORM STREQUAL "harmony")
    message("set options for harmony")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")
    add_compile_options(-Wall -Wextra -pedantic -fno-exceptions)
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
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/src)
set(PUB_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/include)
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty ${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/thirdparty)
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

set(ARTIFACTS_OUTPUT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../artifacts/${BUILD_TYPE}")
set(INSTALL_OUTPUT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../install/${BUILD_TYPE}")

if(TARGET_PLATFORM STREQUAL "android")
    set(ARTIFACTS_SUB_DIR "lib/${ANDROID_ABI}/$<CONFIG>")
elseif(TARGET_PLATFORM STREQUAL "harmony")
    set(ARTIFACTS_SUB_DIR "lib/${OHOS_ARCH}/$<CONFIG>")
else()
	set(ARTIFACTS_SUB_DIR "lib/$<CONFIG>")
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
endif()
set(INSTALL_SUB_DIR "${ARTIFACTS_SUB_DIR}")

message("Artifacts Directory:${ARTIFACTS_OUTPUT_BASE_DIR}/${ARTIFACTS_SUB_DIR}")
message("Dist Directory:${INSTALL_OUTPUT_BASE_DIR}/${INSTALL_SUB_DIR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ARTIFACTS_OUTPUT_BASE_DIR}/${ARTIFACTS_SUB_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ARTIFACTS_OUTPUT_BASE_DIR}/${ARTIFACTS_SUB_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${ARTIFACTS_OUTPUT_BASE_DIR}/${ARTIFACTS_SUB_DIR}")

#universal binary for mac os, support both Intel cpu and Apple cpu
if(TARGET_PLATFORM STREQUAL "mac")
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
endif()

if(BUILD_TYPE STREQUAL "static_lib")
    # build static library
    add_library(BqLog STATIC ${SRC_LIST} ${THIRD_PARTY_LIST} ${INC_LIST})
else()
    # build dynamic library
    add_library(BqLog SHARED ${SRC_LIST} ${THIRD_PARTY_LIST} ${INC_LIST})
endif()


if(TARGET_PLATFORM STREQUAL "android")
    find_library(log-lib log)
    target_link_libraries(BqLog ${log-lib} )
    find_library(android-lib android)
    target_link_libraries(BqLog ${android-lib} )
elseif(TARGET_PLATFORM STREQUAL "linux")
    target_link_libraries(BqLog PRIVATE pthread)
elseif(TARGET_PLATFORM STREQUAL "unix")
    target_link_libraries(BqLog PRIVATE pthread)
    find_library(EXECINFO_LIBRARY execinfo)
    if(EXECINFO_LIBRARY)
        target_link_libraries(BqLog PRIVATE ${EXECINFO_LIBRARY})
    endif()
elseif(TARGET_PLATFORM STREQUAL "mac")
    if(APPLE_LIB_FORMAT STREQUAL "framework")
        set_target_properties(BqLog PROPERTIES
            FRAMEWORK TRUE
            MACOSX_FRAMEWORK_IDENTIFIER com.tencent.BqLog
            PRODUCT_BUNDLE_IDENTIFIER com.tencent.BqLog
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.tencent.BqLog
            VERSION ${BQ_LOG_VERSION}
            SOVERSION ${BQ_LOG_SOVERSION}
        )
    endif()
elseif(TARGET_PLATFORM STREQUAL "ios")
    find_library(SECURITY_FRAMEWORK Security)
    if(SECURITY_FRAMEWORK)
        target_link_libraries(BqLog PRIVATE ${SECURITY_FRAMEWORK})
    endif()
	
    find_library(UIKIT_FRAMEWORK UIKit)
    if(UIKIT_FRAMEWORK)
	target_link_libraries(BqLog PRIVATE ${UIKIT_FRAMEWORK})
    endif()
    set_xcode_property(BqLog GCC_GENERATE_DEBUGGING_SYMBOLS NO "ALL")
    if(APPLE_LIB_FORMAT STREQUAL "framework")
        set_target_properties(BqLog PROPERTIES
            FRAMEWORK TRUE
            MACOSX_FRAMEWORK_IDENTIFIER com.tencent.BqLog
            PRODUCT_BUNDLE_IDENTIFIER com.tencent.BqLog
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.tencent.BqLog
            VERSION ${BQ_LOG_VERSION}
            SOVERSION ${BQ_LOG_SOVERSION}
        )
    endif()
elseif(TARGET_PLATFORM STREQUAL "win64")
    if (MSVC)
    else()
        target_link_libraries(BqLog PRIVATE -ldbghelp)
    endif()
elseif(TARGET_PLATFORM STREQUAL "harmony")
    message("harmony----------------------------------------------")
    target_link_libraries(BqLog PUBLIC libace_napi.z.so libbundle_ndk.z.so libohenvironment.so libhilog_ndk.z.so libohfileuri.so)
endif()

add_custom_command(TARGET BqLog POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${ARTIFACTS_OUTPUT_BASE_DIR}/include"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../include" "${ARTIFACTS_OUTPUT_BASE_DIR}/include"
    COMMENT "Copying include headers to install folder"
)
if(BUILD_TYPE STREQUAL "dynamic_lib")
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
set_target_properties(BqLog PROPERTIES
        VERSION ${BQ_LOG_VERSION}
        SOVERSION ${BQ_LOG_SOVERSION}
)

set(CMAKE_INSTALL_PREFIX "${INSTALL_OUTPUT_BASE_DIR}")


install(TARGETS BqLog
    ARCHIVE DESTINATION "${INSTALL_SUB_DIR}"
    LIBRARY DESTINATION "${INSTALL_SUB_DIR}"
    RUNTIME DESTINATION "${INSTALL_SUB_DIR}"
    FRAMEWORK DESTINATION "${INSTALL_SUB_DIR}"
)

install(FILES "${ARTIFACTS_OUTPUT_BASE_DIR}/${INSTALL_SUB_DIR}/BqLog.pdb"
    DESTINATION "${INSTALL_SUB_DIR}"
    OPTIONAL
)

file(GLOB README_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../*.md"
)

install(FILES ${README_FILES}
    DESTINATION ".."
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../docs"
    DESTINATION ".."
)

install(DIRECTORY "${ARTIFACTS_OUTPUT_BASE_DIR}/include"
    DESTINATION "."
)



