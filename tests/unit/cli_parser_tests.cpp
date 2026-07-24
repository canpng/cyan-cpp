#include <catch2/catch_test_macros.hpp>

#include "cyan/core/cli_parser.hpp"
#include "cyan/core/input_validator.hpp"

TEST_CASE("cyan parses the 1.4.4 option surface") {
  const std::vector<std::wstring> arguments{L"-i",          L"C:\\Apps\\Test.tipa",
                                            L"-o",          L"C:/Output/Test.tipa",
                                            L"-z",          L"one.cyan",
                                            L"two.cyan",    L"-f",
                                            L"one.dylib",   L"Two.framework",
                                            L"-n",          L"Test",
                                            L"-v",          L"2.0",
                                            L"-b",          L"dev.example.test",
                                            L"-m",          L"15.0",
                                            L"-u",          L"-w",
                                            L"-d",          L"-s",
                                            L"-q",          L"-e",
                                            L"-g",          L"-c",
                                            L"9",           L"--ignore-encrypted",
                                            L"--overwrite", L"--compatibility-mode",
                                            L"cyan"};

  auto result = cyan::parse_cyan_arguments(arguments);
  REQUIRE(result);
  CHECK(result.value().input == std::filesystem::path(L"C:\\Apps\\Test.tipa"));
  CHECK(result.value().cyan_files.size() == 2U);
  CHECK(result.value().injected_items.size() == 2U);
  CHECK(result.value().compression_level == 9);
  CHECK(result.value().remove_supported_devices);
  CHECK(result.value().no_watch);
  CHECK(result.value().enable_documents);
  CHECK(result.value().fakesign);
  CHECK(result.value().thin);
  CHECK(result.value().remove_extensions);
  CHECK(result.value().remove_encrypted);
  CHECK(result.value().ignore_encrypted);
  CHECK(result.value().overwrite);
  CHECK(result.value().compatibility_cyan);
}

TEST_CASE("cyan rejects missing input and invalid compression") {
  auto missing = cyan::parse_cyan_arguments({});
  REQUIRE_FALSE(missing);
  CHECK(missing.error().code == cyan::ErrorCode::missing_argument);

  auto invalid = cyan::parse_cyan_arguments({L"-i", L"Test.ipa", L"--compress", L"10"});
  REQUIRE_FALSE(invalid);
  CHECK(invalid.error().code == cyan::ErrorCode::invalid_compression_level);
}

TEST_CASE("cyan and cgen accept grouped short flags") {
  auto cyan_result = cyan::parse_cyan_arguments({L"-i", L"Test.ipa", L"-uwdsqeg"});
  REQUIRE(cyan_result);
  CHECK(cyan_result.value().remove_supported_devices);
  CHECK(cyan_result.value().no_watch);
  CHECK(cyan_result.value().enable_documents);
  CHECK(cyan_result.value().fakesign);
  CHECK(cyan_result.value().thin);
  CHECK(cyan_result.value().remove_extensions);
  CHECK(cyan_result.value().remove_encrypted);

  auto cgen_result = cyan::parse_cgen_arguments({L"-o", L"preset", L"-uwdsq"});
  REQUIRE(cgen_result);
  CHECK(cgen_result.value().remove_supported_devices);
  CHECK(cgen_result.value().no_watch);
  CHECK(cgen_result.value().enable_documents);
  CHECK(cgen_result.value().fakesign);
  CHECK(cgen_result.value().thin);
}

TEST_CASE("grouped short flags reject value-taking options") {
  auto result = cyan::parse_cyan_arguments({L"-i", L"Test.ipa", L"-uc"});
  REQUIRE_FALSE(result);
  CHECK(result.error().code == cyan::ErrorCode::unknown_option);
}

TEST_CASE("cgen appends the cyan extension") {
  auto parsed = cyan::parse_cgen_arguments({L"-o", L"preset"});
  REQUIRE(parsed);
  auto options = parsed.take_value();
  auto valid = cyan::validate_and_normalize(options);
  REQUIRE(valid);
  CHECK(options.output.extension() == L".cyan");
}
