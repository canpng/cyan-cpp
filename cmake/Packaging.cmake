include(GNUInstallDirs)

set(cyan_runtime_dependency_directories)
if(MINGW)
  get_filename_component(cyan_compiler_runtime_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(
    cyan_runtime_dependency_directories
    DIRECTORIES "${cyan_compiler_runtime_directory}"
  )
endif()

install(
  TARGETS cyan cgen
  RUNTIME_DEPENDENCIES
    ${cyan_runtime_dependency_directories}
    PRE_EXCLUDE_REGEXES
      "api-ms-.*"
      "ext-ms-.*"
    POST_EXCLUDE_REGEXES
      ".*[\\\\/][Ss][Yy][Ss][Tt][Ee][Mm]32[\\\\/].*"
      ".*[\\\\/][Ss][Yy][Ss][Ww][Oo][Ww]64[\\\\/].*"
  RUNTIME DESTINATION "."
)
install(
  FILES
    "${PROJECT_SOURCE_DIR}/LICENSE"
    "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    "${PROJECT_SOURCE_DIR}/README.md"
  DESTINATION "."
)
install(
  DIRECTORY "${PROJECT_SOURCE_DIR}/assets/dependencies/"
  DESTINATION "dependencies"
)

set(CPACK_PACKAGE_NAME "cyan-cpp")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "cyan-cpp-windows-x64")
include(CPack)
