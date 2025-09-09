cmake_minimum_required(VERSION 3.22)


if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../CMake_utils.txt)
endif()

#Parse version from version.cpp
bq_parse_version_from_cpp("${CMAKE_CURRENT_SOURCE_DIR}/../src/bq_log/global/version.cpp")

project("${PACKAGE_NAME}Pack" VERSION ${BQ_LOG_VERSION} LANGUAGES CXX)


set(CPACK_INSTALLED_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../install/;.")
set(CPACK_PACKAGE_BASE_NAME "${PACKAGE_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PACKAGE_NAME}-${PACKAGE_ARCH_NAME}-${PROJECT_VERSION}")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/../dist")

# choose binary generator per target platform
if(TARGET_PLATFORM STREQUAL "win64" OR TARGET_PLATFORM STREQUAL "android")
    set(CPACK_GENERATOR "ZIP")
else()
    # linux/unix/mac -> tar.gz (TGZ)
    set(CPACK_GENERATOR "TGZ")
endif()


include(CPack)



