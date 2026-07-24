find_package(LIEF CONFIG REQUIRED)
find_package(LibArchive REQUIRED)
find_package(unofficial-libplist CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

if(CYAN_BUILD_TESTS)
  find_package(Catch2 3 CONFIG REQUIRED)
endif()
