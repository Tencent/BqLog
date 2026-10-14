vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Tencent/BqLog
    REF "Release_${VERSION}"
    SHA512 bf63b07647751f75cc6bf4545c62d2e01181ec10fa1018a2f11c67fe62df1be71ec3788c9c3799dff12d14b271b48b99bad3d607b50bcafff4a9c045d0ee9789
    HEAD_REF develop
)

# vcpkg decides static or shared through the triplet, so the port must follow it instead of
# asking the build for both forms.
if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    set(BQ_BUILD_LIB_TYPE static_lib)
else()
    set(BQ_BUILD_LIB_TYPE dynamic_lib)
endif()

set(BQ_CMAKE_OPTIONS
    -DBUILD_LIB_TYPE=${BQ_BUILD_LIB_TYPE}
    -DBQ_BUILD_TOOLS=OFF
    -DBQ_WERROR=OFF
)

# vcpkg, and the pkg-config file it rewrites, expect lib/libBqLog.a or lib/libBqLog.dylib. On
# Apple the build would otherwise produce BqLog.framework for a single-form build, which neither
# can consume (the Homebrew formula dodges this by asking for both forms at once).
if(VCPKG_TARGET_IS_OSX OR VCPKG_TARGET_IS_IOS)
    if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
        list(APPEND BQ_CMAKE_OPTIONS -DAPPLE_LIB_FORMAT=a)
    else()
        list(APPEND BQ_CMAKE_OPTIONS -DAPPLE_LIB_FORMAT=dylib)
    endif()
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS ${BQ_CMAKE_OPTIONS}
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/BqLog)
vcpkg_fixup_pkgconfig()
vcpkg_copy_pdbs()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
    "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig"
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
