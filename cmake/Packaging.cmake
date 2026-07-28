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
  TARGETS cyan cgen ipapatch
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
  FILES "${PROJECT_SOURCE_DIR}/assets/ipapatch/v2.1.3/zxPluginsInject.dylib"
  DESTINATION "."
)
install(
  FILES
    "${PROJECT_SOURCE_DIR}/LICENSE"
    "${PROJECT_SOURCE_DIR}/README.md"
  DESTINATION "."
)
install(
  DIRECTORY "${PROJECT_SOURCE_DIR}/assets/dependencies/"
  DESTINATION "dependencies"
)
if(CYAN_LDID_EXECUTABLE)
  install(
    PROGRAMS "${CYAN_LDID_EXECUTABLE}"
    DESTINATION "."
    RENAME "ldid.exe"
  )
  get_filename_component(cyan_ldid_directory "${CYAN_LDID_EXECUTABLE}" DIRECTORY)
  set(cyan_ldid_notice "")
  foreach(cyan_ldid_notice_name IN ITEMS "ldid.COPYING" "COPYING")
    if(EXISTS "${cyan_ldid_directory}/${cyan_ldid_notice_name}")
      set(cyan_ldid_notice "${cyan_ldid_directory}/${cyan_ldid_notice_name}")
      break()
    endif()
  endforeach()
  if(cyan_ldid_notice)
    install(
      FILES "${cyan_ldid_notice}"
      DESTINATION "."
      RENAME "ldid.COPYING"
    )
  endif()
endif()

set(CPACK_PACKAGE_NAME "cyan-cpp")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "cyan-cpp-windows-x64")
include(CPack)
