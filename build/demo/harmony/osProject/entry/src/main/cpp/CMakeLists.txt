# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(TempLib)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include
                    ${NATIVERENDER_ROOT_PATH}/../../../../../../../../install/dynamic_lib/include)
#add_library(BqLog SHARED IMPORTED)
#set_target_properties(BqLog
#    PROPERTIES
#    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/third_party/BqLog/libs/${OHOS_ARCH}/libBqLog.so)
#
#add_library(entry SHARED napi_init.cpp)
#
#target_link_libraries(entry PUBLIC libace_napi.z.so BqLog)

set(LIBRARY_DIR "${NATIVERENDER_ROOT_PATH}/../../../../BqLog/build/default/intermediates/libs/default/${OHOS_ARCH}/")
add_library(BqLog SHARED IMPORTED)
set_target_properties(BqLog
    PROPERTIES
    IMPORTED_LOCATION ${LIBRARY_DIR}/libBqLog.so)
add_library(entry SHARED napi_init.cpp)
target_link_libraries(entry PUBLIC libace_napi.z.so BqLog)