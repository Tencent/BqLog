cmake_minimum_required(VERSION 3.22)


if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../CMake_utils.txt)
endif()

#Parse version from version.cpp
bq_parse_version_from_cpp("${CMAKE_CURRENT_SOURCE_DIR}/../src/bq_log/global/version.cpp")






project("${PACKAGE_NAME}Pack" VERSION ${BQ_LOG_VERSION} LANGUAGES CXX)






# Prefer the system processor variable
set(PROC_TYPE "${CMAKE_GENERATOR_PLATFORM}")
if("${PROC_TYPE}" STREQUAL "")
    set(PROC_TYPE "${CMAKE_SYSTEM_PROCESSOR}")
endif()
if("${PROC_TYPE}" STREQUAL "")
    set(PROC_TYPE "${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()
# If cross-compiling, the compiler target triple may be set
if(DEFINED CMAKE_C_COMPILER_TARGET AND NOT CMAKE_C_COMPILER_TARGET STREQUAL "")
    set(_compiler_target "${CMAKE_C_COMPILER_TARGET}")
else()
    set(_compiler_target "")
endif()
# Helper to test known patterns
if(PROC_TYPE MATCHES "^(x86_64|X86_64|amd64|AMD64|x86-64|i386|i686)$" OR _compiler_target MATCHES "x86_64|amd64|i386|i686")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(DETECTED_ARCH "x86_64")
    else()
        set(DETECTED_ARCH "x86")
    endif()
elseif(PROC_TYPE MATCHES "^(aarch64|arm64|AARCH64|ARM64|armv8|armv7l|arm)$" OR _compiler_target MATCHES "aarch64|armv7|arm")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(DETECTED_ARCH "arm64")
    else()
        set(DETECTED_ARCH "arm32")
    endif()
endif()


if(TARGET_PLATFORM STREQUAL "linux")
    set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-linux_${DETECTED_ARCH}-${PROJECT_VERSION}")
elseif(TARGET_PLATFORM STREQUAL "win64")
    set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-windows_${DETECTED_ARCH}-${PROJECT_VERSION}")
elseif(TARGET_PLATFORM STREQUAL "unix")
    if("${BQ_UNIX_VERSION}" STREQUAL "")
        set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-unix_${DETECTED_ARCH}-${PROJECT_VERSION}")
    else()
        set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-${BQ_UNIX_VERSION}_${DETECTED_ARCH}-${PROJECT_VERSION}")
    endif()
elseif(TARGET_PLATFORM STREQUAL "mac")
    set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-macos_universal-${PROJECT_VERSION}")
elseif(TARGET_PLATFORM STREQUAL "ios")
    set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-ios-${PROJECT_VERSION}")
elseif(TARGET_PLATFORM STREQUAL "android")
    set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-android-${PROJECT_VERSION}")
else()
    set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-${PROJECT_VERSION}")
endif()


set(CPACK_INSTALLED_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../install/;.")
set(CPACK_PACKAGE_BASE_NAME "${PACKAGE_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${BQ_PACK_FILE_NAME}")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/../dist")

# 生成包的同时生成 SHA256 校验文件（同名加 .sha256 后缀）
set(CPACK_PACKAGE_CHECKSUM "SHA256")

# choose binary generator per target platform
if(TARGET_PLATFORM STREQUAL "win64" OR TARGET_PLATFORM STREQUAL "android")
    set(CPACK_GENERATOR "ZIP")
else()
    # linux/unix/mac -> tar.gz (TGZ)
    set(CPACK_GENERATOR "TGZ")
endif()


include(CPack)



