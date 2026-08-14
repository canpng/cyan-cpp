cmake_minimum_required(VERSION 3.28)

cmake_policy(SET CMP0091 NEW)

project(
  cyan-cpp
  VERSION 2.0.0
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
option(CYAN_BUILD_GUI "Build the Qt 6 desktop application" ON)
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
    CYAN_VERSION="${PROJECT_VERSION}"
    CYAN_VERSION_WIDE=L"${PROJECT_VERSION}"
)
cyan_set_project_warnings(cyan_core)

add_library(
  cyan_archive STATIC
  src/archive/archive_path_validator.cpp
  src/archive/archive_service.cpp
  src/archive/cyan_archive.cpp
  src/archive/zip_update_service.cpp
)
target_link_libraries(
  cyan_archive
  PUBLIC cyan_core
  PRIVATE LibArchive::LibArchive libzip::zip nlohmann_json::nlohmann_json
)
cyan_set_project_warnings(cyan_archive)

add_library(
  cyan_macho STATIC
  src/macho/code_signature_inspector.cpp
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
  cyan_metadata STATIC
  src/metadata/app_metadata_reader.cpp
)
target_link_libraries(
  cyan_metadata
  PUBLIC cyan_core
  PRIVATE cyan_plist LibArchive::LibArchive
)
cyan_set_project_warnings(cyan_metadata)

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
  cyan_ipapatch STATIC
  src/ipapatch/cli_parser.cpp
  src/ipapatch/ipapatch_service.cpp
)
target_link_libraries(
  cyan_ipapatch
  PUBLIC cyan_core cyan_signing
  PRIVATE cyan_archive cyan_bundle cyan_lief cyan_macho cyan_plist
)
cyan_set_project_warnings(cyan_ipapatch)

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
    cyan_ipapatch
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
    cyan_ipapatch
    cyan_bundle
    cyan_macho
    cyan_lief
    cyan_signing
    cyan_pipeline
)
cyan_set_project_warnings(cyan)

add_executable(ipapatch src/ipapatch/main.cpp)
target_link_libraries(
  ipapatch
  PRIVATE cyan_core cyan_ipapatch cyan_signing
)
cyan_set_project_warnings(ipapatch)

add_executable(cgen src/cgen/main.cpp)
target_link_libraries(cgen PRIVATE cyan_core cyan_archive)
cyan_set_project_warnings(cgen)

if(CYAN_BUILD_GUI)
  # Keep the Windows executables and their Qt runtime DLLs together at the
  # package root so the GUI can be launched directly without modifying PATH.
  set(CMAKE_INSTALL_BINDIR ".")
  find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2)
  find_package(ZLIB REQUIRED)
  qt_standard_project_setup(REQUIRES 6.5)
  if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    qt_policy(SET QTP0004 NEW)
  endif()

  qt_add_executable(
    cyan_gui
    WIN32
    src/gui/main.cpp
    src/gui/app_controller.cpp
    src/gui/app_controller.hpp
    src/gui/app_settings.cpp
    src/gui/app_settings.hpp
    src/gui/composer_controller.cpp
    src/gui/composer_controller.hpp
    src/gui/file_list_model.cpp
    src/gui/file_list_model.hpp
    src/gui/job_definition.cpp
    src/gui/job_definition.hpp
    src/gui/job_queue_model.cpp
    src/gui/job_queue_model.hpp
    src/gui/input_icon_cache.cpp
    src/gui/input_icon_cache.hpp
    src/gui/preset_model.cpp
    src/gui/preset_model.hpp
  )
  set_target_properties(cyan_gui PROPERTIES OUTPUT_NAME "cyan-gui")
  target_include_directories(cyan_gui PRIVATE "${PROJECT_SOURCE_DIR}/src/gui")
  target_compile_definitions(cyan_gui PRIVATE CYAN_GUI_VERSION="${PROJECT_VERSION}")
  target_link_libraries(
    cyan_gui
    PRIVATE
      Qt6::Quick
      Qt6::QuickControls2
      ZLIB::ZLIB
      cyan_core
      cyan_metadata
      cyan_pipeline
  )
  cyan_set_project_warnings(cyan_gui)
  if(MSVC)
    # Qt 6.11's generated QML code instantiates constexpr helpers that trigger
    # C4702 inside Qt headers. Keep /WX for all other project warnings.
    target_compile_options(cyan_gui PRIVATE /wd4702)
  endif()

  set_source_files_properties(
    src/gui/qml/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
  )
  qt_add_qml_module(
    cyan_gui
    URI Cyan.Gui
    VERSION 1.0
    QML_FILES
      src/gui/qml/Main.qml
      src/gui/qml/Theme.qml
      src/gui/qml/components/AppButton.qml
      src/gui/qml/components/AppScrollBar.qml
      src/gui/qml/components/AppSlider.qml
      src/gui/qml/components/AppSwitch.qml
      src/gui/qml/components/CollapsibleCard.qml
      src/gui/qml/components/CompactCheckBox.qml
      src/gui/qml/components/CompactComboBox.qml
      src/gui/qml/components/DropZone.qml
      src/gui/qml/components/FileItemRow.qml
      src/gui/qml/components/FormField.qml
      src/gui/qml/components/IconButton.qml
      src/gui/qml/components/InlineMessage.qml
      src/gui/qml/components/PathPicker.qml
      src/gui/qml/components/Panel.qml
      src/gui/qml/components/NavButton.qml
      src/gui/qml/components/SectionCard.qml
      src/gui/qml/components/SegmentedControl.qml
      src/gui/qml/components/SidebarItem.qml
      src/gui/qml/components/StatusBadge.qml
      src/gui/qml/components/ToggleRow.qml
      src/gui/qml/pages/NewJobPage.qml
      src/gui/qml/pages/PresetsPage.qml
      src/gui/qml/pages/QueuePage.qml
      src/gui/qml/pages/SettingsPage.qml
  )

  add_custom_command(
    TARGET cyan_gui
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different
      "${PROJECT_SOURCE_DIR}/assets/ipapatch/v2.1.3/zxPluginsInject.dylib"
      "$<TARGET_FILE_DIR:cyan_gui>/zxPluginsInject.dylib"
    COMMAND
      "${CMAKE_COMMAND}" -E copy_directory
      "${PROJECT_SOURCE_DIR}/assets/dependencies"
      "$<TARGET_FILE_DIR:cyan_gui>/dependencies"
    VERBATIM
  )
  if(CYAN_LDID_EXECUTABLE)
    add_custom_command(
      TARGET cyan_gui
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy_if_different
        "${CYAN_LDID_EXECUTABLE}"
        "$<TARGET_FILE_DIR:cyan_gui>/ldid.exe"
      VERBATIM
    )
  endif()

  install(
    TARGETS cyan_gui
    RUNTIME_DEPENDENCIES
      PRE_EXCLUDE_REGEXES
        "api-ms-.*"
        "ext-ms-.*"
      POST_EXCLUDE_REGEXES
        ".*[\\\\/][Ss][Yy][Ss][Tt][Ee][Mm]32[\\\\/].*"
        ".*[\\\\/][Ss][Yy][Ss][Ww][Oo][Ww]64[\\\\/].*"
    RUNTIME DESTINATION "."
  )
  qt_generate_deploy_qml_app_script(
    TARGET cyan_gui
    OUTPUT_SCRIPT cyan_gui_deploy_script
    NO_TRANSLATIONS
    NO_COMPILER_RUNTIME
    NO_UNSUPPORTED_PLATFORM_ERROR
  )
  install(SCRIPT "${cyan_gui_deploy_script}")
endif()

set(
  CYAN_IPAPATCH_PAYLOAD
  "${PROJECT_SOURCE_DIR}/assets/ipapatch/v2.1.3/zxPluginsInject.dylib"
)
if(NOT EXISTS "${CYAN_IPAPATCH_PAYLOAD}")
  message(FATAL_ERROR "Pinned ipapatch v2.1.3 payload is missing")
endif()

foreach(cyan_runtime_target IN ITEMS cyan ipapatch)
  add_custom_command(
    TARGET ${cyan_runtime_target}
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different
      "${CYAN_IPAPATCH_PAYLOAD}"
      "$<TARGET_FILE_DIR:${cyan_runtime_target}>/zxPluginsInject.dylib"
    VERBATIM
  )
endforeach()

if(CYAN_LDID_EXECUTABLE)
  foreach(cyan_runtime_target IN ITEMS cyan ipapatch)
  add_custom_command(
    TARGET ${cyan_runtime_target}
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different
      "${CYAN_LDID_EXECUTABLE}"
      "$<TARGET_FILE_DIR:${cyan_runtime_target}>/ldid.exe"
    VERBATIM
  )
  endforeach()
endif()

if(MINGW)
  target_link_options(cyan PRIVATE -municode)
  target_link_options(ipapatch PRIVATE -municode)
  target_link_options(cgen PRIVATE -municode)
endif()

if(CYAN_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

include(Packaging)
