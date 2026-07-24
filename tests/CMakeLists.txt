add_executable(
  cyan_tests
  unit/archive_path_validator_tests.cpp
  unit/cli_parser_tests.cpp
  unit/dependency_resolver_tests.cpp
  unit/insert_dylib_engine_tests.cpp
  unit/plist_document_tests.cpp
  integration/pipeline_tests.cpp
)
target_link_libraries(
  cyan_tests
  PRIVATE
    cyan_core
    cyan_archive
    cyan_macho
    cyan_pipeline
    cyan_plist
    Catch2::Catch2WithMain
)
cyan_set_project_warnings(cyan_tests)

add_test(NAME cyan_unit COMMAND cyan_tests)
