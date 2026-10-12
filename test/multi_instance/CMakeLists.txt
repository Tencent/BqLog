cmake_minimum_required(VERSION 3.22)
project(BqLogMultiInstanceProbe LANGUAGES C CXX)
if(WIN32)
    set(TARGET_PLATFORM win64 CACHE STRING "BqLog platform")
elseif(APPLE)
    set(TARGET_PLATFORM mac CACHE STRING "BqLog platform")
    set(APPLE_LIB_FORMAT a CACHE STRING "Apple library format")
else()
    set(TARGET_PLATFORM linux CACHE STRING "BqLog platform")
endif()
set(BUILD_LIB_TYPE static_lib CACHE STRING "Library type")
if(NOT DEFINED CPP_VER)
    set(CPP_VER 17)
endif()

add_subdirectory(../../src bqlog)

add_executable(bq_multi_instance_worker worker.cpp)
target_compile_features(bq_multi_instance_worker PRIVATE cxx_std_17)
target_include_directories(bq_multi_instance_worker PRIVATE ../../include)
target_link_libraries(bq_multi_instance_worker PRIVATE BqLog)
# statically link the C++ runtime so the worker never loads a mismatched
# libstdc++-6.dll from another toolchain earlier in PATH (e.g. Git's)
if(MINGW)
    target_link_options(bq_multi_instance_worker PRIVATE -static-libstdc++ -static-libgcc)
endif()

# test-only C API in the library, see T04: __api_test_get_storage_report
target_compile_definitions(BqLog PUBLIC BQ_MULTI_INSTANCE_TEST)

add_executable(bq_instance_lock_probe lock_probe.cpp)
target_compile_features(bq_instance_lock_probe PRIVATE cxx_std_11)
target_include_directories(bq_instance_lock_probe PRIVATE ../../src ../../include)
target_link_libraries(bq_instance_lock_probe PRIVATE BqLog)
if(MSVC)
    target_compile_options(bq_instance_lock_probe PRIVATE /EHsc /D_HAS_EXCEPTIONS=0 /GR-)
else()
    target_compile_options(bq_instance_lock_probe PRIVATE -fno-exceptions -fno-rtti)
endif()
if(MINGW)
    target_link_options(bq_instance_lock_probe PRIVATE -static-libstdc++ -static-libgcc)
endif()

# M6: a worker built from the 2.6.0 sources to prove the new runtime coexists
# with (and cleans up around) the legacy mmap_<log> directory layout.
set(BQ_LEGACY_SRC "" CACHE PATH "path to a 2.6.0 checkout (must contain src/ and include/)")
if(BQ_LEGACY_SRC)
    add_executable(bq_multi_instance_worker_legacy worker_legacy.cpp)
    target_compile_features(bq_multi_instance_worker_legacy PRIVATE cxx_std_17)
    target_include_directories(bq_multi_instance_worker_legacy PRIVATE
        ${BQ_LEGACY_SRC}/include ${BQ_LEGACY_SRC}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/thirdparty)
    if(WIN32)
        target_link_libraries(bq_multi_instance_worker_legacy PRIVATE dbghelp)
    endif()
    if(MINGW)
        target_link_options(bq_multi_instance_worker_legacy PRIVATE -static-libstdc++ -static-libgcc)
    endif()
    target_sources(bq_multi_instance_worker_legacy PRIVATE
        ${BQ_LEGACY_SRC}/src/bq_common/encryption/aes.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/encryption/rsa.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/encryption/vernam.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/global/common_vars.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/android_misc.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/io/memory_map_posix.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/io/memory_map_win.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/java_misc.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/linux_misc.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/napi_misc.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/no_lib_cpp_impl.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/ohos_misc.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/platform_misc.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/posix_misc.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/ps_misc.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/python_misc.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/simd.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/thread/condition_variable_posix.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/thread/condition_variable_win64.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/thread/mutex_posix.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/thread/mutex_win64.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/thread/spin_lock.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/thread/thread.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/thread/thread_posix.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/thread/thread_win64.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/unix_misc.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/platform/win64_misc.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/utils/aligned_allocator.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/utils/file_manager.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/utils/property.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/utils/property_ex.cpp
        ${BQ_LEGACY_SRC}/src/bq_common/utils/util.cpp
    )
    file(GLOB BQ_LEGACY_LOG_SRCS ${BQ_LEGACY_SRC}/src/bq_log/*.cpp
        ${BQ_LEGACY_SRC}/src/bq_log/*/*.cpp
        ${BQ_LEGACY_SRC}/src/bq_log/*/*/*.cpp)
    target_sources(bq_multi_instance_worker_legacy PRIVATE ${BQ_LEGACY_LOG_SRCS})
endif()

# M7: two plugins, each statically linking its own BqLog runtime, loaded into
# one host process. The BqLog sources are compiled directly into each plugin
# with hidden visibility so the two copies never share symbols.
if(NOT WIN32 OR TARGET_PLATFORM STREQUAL "win64")
    # COLLECT_SRC_FILES / COLLECT_INCLUDE_DIRS come from CMake_utils.txt,
    # included by src/CMakeLists.txt above (functions are global in CMake).
    set(DUAL_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/src)
    set(DUAL_PUB_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/include)
    set(DUAL_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty ${CMAKE_CURRENT_SOURCE_DIR}/../../../../submodules/BqCommon/thirdparty)
    COLLECT_SRC_FILES(DUAL_LOG_SRCS "${DUAL_SRC_DIR}")
    COLLECT_INCLUDE_DIRS(DUAL_INC_DIRS "${DUAL_SRC_DIR};${DUAL_PUB_INC_DIR};${DUAL_THIRD_PARTY_DIR}")
    foreach(plugin_name plugin_a plugin_b)
        add_library(${plugin_name} SHARED dual_runtime/plugin.cpp ${DUAL_LOG_SRCS})
        target_compile_features(${plugin_name} PRIVATE cxx_std_17)
        target_include_directories(${plugin_name} PRIVATE ${DUAL_INC_DIRS})
        target_compile_definitions(${plugin_name} PRIVATE BQ_MULTI_INSTANCE_TEST)
        set_target_properties(${plugin_name} PROPERTIES
            PREFIX ""
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON)
        if(MSVC)
            target_compile_options(${plugin_name} PRIVATE /EHsc /D_HAS_EXCEPTIONS=0 /GR-)
        else()
            target_compile_options(${plugin_name} PRIVATE -fno-exceptions -fno-rtti)
            if(NOT APPLE)
                target_link_options(${plugin_name} PRIVATE -static-libstdc++ -static-libgcc)
            endif()
        endif()
        if(WIN32)
            target_link_libraries(${plugin_name} PRIVATE dbghelp)
        endif()
    endforeach()
    # two file copies of each plugin so the OS loader treats them as distinct
    # modules; host loads plugin_a and plugin_b_copy
    add_custom_command(TARGET plugin_a POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:plugin_a> $<TARGET_FILE_DIR:plugin_a>/plugin_a_copy$<TARGET_FILE_SUFFIX:plugin_a>)
    add_custom_command(TARGET plugin_b POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:plugin_b> $<TARGET_FILE_DIR:plugin_b>/plugin_b_copy$<TARGET_FILE_SUFFIX:plugin_b>)
    add_executable(bq_dual_runtime_host dual_runtime/host.cpp)
    target_compile_features(bq_dual_runtime_host PRIVATE cxx_std_17)
    target_include_directories(bq_dual_runtime_host PRIVATE ../../include)
    if(MINGW)
        target_link_options(bq_dual_runtime_host PRIVATE -static-libstdc++ -static-libgcc)
    endif()
endif()

enable_testing()
add_test(NAME multi_instance_lock COMMAND bq_instance_lock_probe)
set_tests_properties(multi_instance_lock PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
foreach(case M1 M2 M3 M4 M5 M6 M7 M8 M9)
    add_test(NAME multi_instance_${case}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run.py --case ${case}
                --worker $<TARGET_FILE:bq_multi_instance_worker>
                --out ${CMAKE_BINARY_DIR}/mi_out)
    set_tests_properties(multi_instance_${case} PROPERTIES SKIP_RETURN_CODE 77)
endforeach()
