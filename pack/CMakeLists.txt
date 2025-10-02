cmake_minimum_required(VERSION 3.22)

#Parse version from version.cpp
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../CMake_utils.txt)
endif()
bq_parse_version_from_cpp("${CMAKE_CURRENT_SOURCE_DIR}/../src/bq_log/global/version.cpp")
message(STATUS "BqLog Version:${BQ_LOG_VERSION}")

project("${PACKAGE_NAME}Pack" VERSION ${BQ_LOG_VERSION} LANGUAGES CXX)
BQ_DETECT_ARCH(DETECTED_ARCH)

if(TARGET_PLATFORM STREQUAL "linux")
    set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-linux-${DETECTED_ARCH}-${PROJECT_VERSION}")
elseif(TARGET_PLATFORM STREQUAL "win64")
    set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-windows-${DETECTED_ARCH}-${PROJECT_VERSION}")
elseif(TARGET_PLATFORM STREQUAL "unix")
    if(DEFINED ENV{BQ_UNIX_VERSION} AND NOT "$ENV{BQ_UNIX_VERSION}" STREQUAL "")
        set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-$ENV{BQ_UNIX_VERSION}-${DETECTED_ARCH}-${PROJECT_VERSION}")
    else()
        set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-unix-${DETECTED_ARCH}-${PROJECT_VERSION}")
    endif()
elseif(TARGET_PLATFORM STREQUAL "mac")
    set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-macos-universal-${PROJECT_VERSION}")
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



