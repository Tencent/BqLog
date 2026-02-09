cmake_minimum_required(VERSION 3.22)

#Parse version from version.cpp
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../CMake_utils.txt)
endif()
bq_parse_version_from_cpp("${CMAKE_CURRENT_SOURCE_DIR}/../src/bq_log/global/version.cpp")
message(STATUS "BqLog Version:${BQ_VERSION}")

project("${PACKAGE_NAME}Pack" VERSION ${BQ_VERSION} LANGUAGES CXX)
BQ_DETECT_ARCH(DETECTED_ARCH)

# ========================================================
# 1. Always clean the dist directory first
# ========================================================
set(BQ_DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../dist")
message(STATUS "Cleaning dist directory: ${BQ_DIST_DIR}")
file(REMOVE_RECURSE "${BQ_DIST_DIR}")
file(MAKE_DIRECTORY "${BQ_DIST_DIR}")


# Platform specific naming logic
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
elseif(TARGET_PLATFORM STREQUAL "ohos")
    set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-harmonyOS-${PROJECT_VERSION}")
else()
    set(BQ_PACK_FILE_NAME "${PACKAGE_NAME}-${PROJECT_VERSION}")
endif()

# ========================================================
# 2. Branch Logic: GitHub Actions vs Normal Packaging
# ========================================================
if(DEFINED ENV{GITHUB_ACTIONS} AND "$ENV{GITHUB_ACTIONS}" STREQUAL "true")
    # --- Branch A: GitHub Actions (Copy only) ---
    message(STATUS "Detected: running inside GitHub Actions")
    message(STATUS "Skipping CPack generation. Copying install directory to dist...")

    # Manually define a 'package' target that copies the files, 
    # satisfying the 'cmake --build . --target package' command in external scripts.
    add_custom_target(package
        COMMAND ${CMAKE_COMMAND} -E echo "GitHub Actions: Copying install directory to dist..."
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BQ_DIST_DIR}/${BQ_PACK_FILE_NAME}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../install" "${BQ_DIST_DIR}/${BQ_PACK_FILE_NAME}"
        COMMENT "Copying files to dist folder instead of packing"
        VERBATIM
    )

else()
    # --- Branch B: Normal Packaging (CPack ZIP/TGZ) ---

    set(CPACK_INSTALLED_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../install/;.")
    set(CPACK_PACKAGE_BASE_NAME "${PACKAGE_NAME}")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "${BQ_PACK_FILE_NAME}")
    # Note: CPACK_OUTPUT_FILE_PREFIX points to the dist folder we cleaned above
    set(CPACK_OUTPUT_FILE_PREFIX "${BQ_DIST_DIR}")
    set(CPACK_PACKAGE_CHECKSUM "SHA256")

    # choose binary generator per target platform
    if(TARGET_PLATFORM STREQUAL "win64" OR TARGET_PLATFORM STREQUAL "android")
        set(CPACK_GENERATOR "ZIP")
    else()
        # linux/unix/mac -> tar.gz (TGZ)
        set(CPACK_GENERATOR "TGZ")
    endif()

    include(CPack)
endif()