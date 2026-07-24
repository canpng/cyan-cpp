cmake_minimum_required(VERSION 3.28)

cmake_policy(SET CMP0091 NEW)

project(
  cyan-cpp
  VERSION 0.1.0
  DESCRIPTION "Native Windows C++20 rewrite of cyan"
  LANGUAGES CXX
)

if(NOT WIN32)
  message(FATAL_ERROR "The first supported cyan-cpp target is Windows x64")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "cyan-cpp requires a 64-bit toolchain")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

option(CYAN_BUILD_TESTS "Build the test suite" ON)
option(CYAN_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
option(CYAN_STATIC_CORE "Build project libraries statically" ON)
set(
  CYAN_LDID_EXECUTABLE
  ""
  CACHE FILEPATH
  "Pinned Windows x64 Procursus ldid executable to package beside cyan.exe"
)

if(CYAN_LDID_EXECUTABLE AND NOT EXISTS "${CYAN_LDID_EXECUTABLE}")
  message(FATAL_ERROR "CYAN_LDID_EXECUTABLE does not exist: ${CYAN_LDID_EXECUTABLE}")
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Dependencies)

add_library(
  cyan_core STATIC
  src/core/cli_parser.cpp
  src/core/dependency_resolver.cpp
  src/core/input_validator.cpp
  src/core/temporary_workspace.cpp
  src/platform/windows/utf.cpp
)
target_include_directories(
  cyan_core
  PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_compile_definitions(
  cyan_core
  PUBLIC
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
  PRIVATE
    CYAN_VERSION="${PROJECT_VERSION}"
)
cyan_set_project_warnings(cyan_core)

add_library(
  cyan_archive STATIC
  src/archive/archive_path_validator.cpp
  src/archive/archive_service.cpp
  src/archive/cyan_archive.cpp
)
target_link_libraries(
  cyan_archive
  PUBLIC cyan_core
  PRIVATE LibArchive::LibArchive nlohmann_json::nlohmann_json
)
cyan_set_project_warnings(cyan_archive)

add_library(
  cyan_macho STATIC
  src/macho/insert_dylib_engine.cpp
  src/macho/macho_inspector.cpp
)
target_link_libraries(cyan_macho PUBLIC cyan_core)
cyan_set_project_warnings(cyan_macho)

add_library(
  cyan_plist STATIC
  src/plist/plist_document.cpp
)
target_link_libraries(
  cyan_plist
  PUBLIC cyan_core
  PRIVATE unofficial::libplist::libplist
)
cyan_set_project_warnings(cyan_plist)

add_library(
  cyan_image STATIC
  src/image/icon_processor.cpp
)
target_link_libraries(
  cyan_image
  PUBLIC cyan_core cyan_plist
  PRIVATE windowscodecs ole32
)
cyan_set_project_warnings(cyan_image)

add_library(
  cyan_bundle STATIC
  src/core/app_bundle.cpp
)
target_link_libraries(
  cyan_bundle
  PUBLIC cyan_core cyan_plist cyan_macho
)
cyan_set_project_warnings(cyan_bundle)

add_library(
  cyan_signing STATIC
  src/signing/external_ldid_backend.cpp
  src/signing/windows_process_runner.cpp
)
target_link_libraries(
  cyan_signing
  PUBLIC cyan_core cyan_plist
  PRIVATE cyan_macho
)
cyan_set_project_warnings(cyan_signing)

add_library(
  cyan_pipeline STATIC
  src/core/cyan_pipeline.cpp
)
target_link_libraries(
  cyan_pipeline
  PUBLIC cyan_core
  PRIVATE
    cyan_archive
    cyan_bundle
    cyan_image
    cyan_lief
    cyan_macho
    cyan_plist
    cyan_signing
)
cyan_set_project_warnings(cyan_pipeline)

add_library(
  cyan_lief STATIC
  src/macho/lief_macho_backend.cpp
)
target_link_libraries(cyan_lief PUBLIC cyan_core PRIVATE LIEF::LIEF)
cyan_set_project_warnings(cyan_lief)

add_executable(cyan src/cli/main.cpp)
target_link_libraries(
  cyan
  PRIVATE
    cyan_core
    cyan_archive
    cyan_plist
    cyan_image
    cyan_bundle
    cyan_macho
    cyan_lief
    cyan_signing
    cyan_pipeline
)
cyan_set_project_warnings(cyan)

add_executable(cgen src/cgen/main.cpp)
target_link_libraries(cgen PRIVATE cyan_core cyan_archive)
cyan_set_project_warnings(cgen)

if(CYAN_LDID_EXECUTABLE)
  add_custom_command(
    TARGET cyan
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different
      "${CYAN_LDID_EXECUTABLE}"
      "$<TARGET_FILE_DIR:cyan>/ldid.exe"
    VERBATIM
  )
endif()

if(MINGW)
  target_link_options(cyan PRIVATE -municode)
  target_link_options(cgen PRIVATE -municode)
endif()

if(CYAN_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

include(Packaging)
