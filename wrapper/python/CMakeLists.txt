cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../submodules/BqCommon/CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../submodules/BqCommon/CMake_utils.txt)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../CMake_utils.txt)
endif()
bq_parse_version_from_cpp("${CMAKE_CURRENT_SOURCE_DIR}/../../src/bq_log/global/version.cpp")

project(BqLogPythonWrapper VERSION ${BQ_VERSION})

set(ARTIFACTS_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/wrapper/python")
set(INSTALL_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../install/wrapper_python")

set(PYPROJECT_TOML "${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml")
file(READ "${PYPROJECT_TOML}" _toml_raw)
string(REGEX REPLACE "version[ \t]*=[ \t]*\"[^\"]+\"" "version = \"${CMAKE_PROJECT_VERSION}\"" _toml_new "${_toml_raw}")

if(NOT _toml_raw STREQUAL _toml_new)
  file(WRITE "${PYPROJECT_TOML}" "${_toml_new}")
  message(STATUS "Updated pyproject.toml version -> ${CMAKE_PROJECT_VERSION}")
else()
  message(STATUS "pyproject.toml version already ${CMAKE_PROJECT_VERSION}")
endif()

# Stage Python source files to artifacts (wheel build source)
add_custom_target(python_wrapper ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ARTIFACTS_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/src" "${ARTIFACTS_OUTPUT_DIR}/src"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml" "${ARTIFACTS_OUTPUT_DIR}/pyproject.toml"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/setup.py" "${ARTIFACTS_OUTPUT_DIR}/setup.py"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/README.md" "${ARTIFACTS_OUTPUT_DIR}/README.md"
    COMMENT "Preparing Python wrapper artifacts"
)

if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()

install(DIRECTORY "${ARTIFACTS_OUTPUT_DIR}/wheel/"
    DESTINATION "${INSTALL_OUTPUT_DIR}"
    FILES_MATCHING PATTERN "*.whl"
)

file(GLOB MD_FILES "${CMAKE_CURRENT_SOURCE_DIR}/../../*.md")
install(FILES ${MD_FILES}
    DESTINATION "${INSTALL_OUTPUT_DIR}/.."
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../docs"
    DESTINATION "${INSTALL_OUTPUT_DIR}/.."
    OPTIONAL
)
