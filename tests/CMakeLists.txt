add_executable(
  cyan_tests
  unit/archive_path_validator_tests.cpp
  unit/app_metadata_reader_tests.cpp
  unit/cli_parser_tests.cpp
  unit/dependency_resolver_tests.cpp
  unit/insert_dylib_engine_tests.cpp
  unit/ipapatch_service_tests.cpp
  unit/plist_document_tests.cpp
  unit/signing_backend_tests.cpp
  integration/pipeline_tests.cpp
)
target_link_libraries(
  cyan_tests
  PRIVATE
    cyan_core
    cyan_archive
    cyan_macho
    cyan_metadata
    cyan_ipapatch
    cyan_pipeline
    cyan_plist
    cyan_signing
    Catch2::Catch2WithMain
)
target_compile_definitions(
  cyan_tests
  PRIVATE
    CYAN_TEST_IPAPATCH_PAYLOAD="${PROJECT_SOURCE_DIR}/assets/ipapatch/v2.1.3/zxPluginsInject.dylib"
)
cyan_set_project_warnings(cyan_tests)

if(CYAN_BUILD_GUI)
  target_sources(cyan_tests PRIVATE ../src/gui/input_icon_cache.cpp)
  target_include_directories(cyan_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/gui")
  target_link_libraries(cyan_tests PRIVATE Qt6::Gui ZLIB::ZLIB)
  target_compile_definitions(cyan_tests PRIVATE CYAN_TEST_GUI_ICONS=1)
endif()

add_test(NAME cyan_unit COMMAND cyan_tests)
set(
  cyan_test_environment
  "CYAN_TEST_IPAPATCH_CLI=$<TARGET_FILE:ipapatch>"
)
if(CYAN_LDID_EXECUTABLE)
  list(APPEND cyan_test_environment "CYAN_TEST_LDID=${CYAN_LDID_EXECUTABLE}")
endif()
set_tests_properties(cyan_unit PROPERTIES ENVIRONMENT "${cyan_test_environment}")
