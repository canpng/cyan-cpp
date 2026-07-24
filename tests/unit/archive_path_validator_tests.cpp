#include <catch2/catch_test_macros.hpp>

#include "cyan/archive/archive_path_validator.hpp"

TEST_CASE("archive paths are normalised and reserved") {
  cyan::ArchivePathValidator validator;
  auto archive_root = validator.validate_and_reserve("./");
  REQUIRE(archive_root);
  CHECK(archive_root.value().empty());

  auto accepted = validator.validate_and_reserve("./Payload/./Test.app/Info.plist");
  REQUIRE(accepted);
  CHECK(accepted.value().generic_wstring() == L"Payload/Test.app/Info.plist");

  auto duplicate = validator.validate_and_reserve("payload/test.app/info.plist");
  REQUIRE_FALSE(duplicate);
  CHECK(duplicate.error().code == cyan::ErrorCode::archive_duplicate_path);
}

TEST_CASE("archive traversal and Windows hazards are rejected") {
  for (const auto* path : {"../outside", "./../outside", "Payload/../../outside", "/absolute",
                           "C:/absolute", "\\\\server\\share\\file", "Payload/file:stream",
                           "Payload/CON.txt", "Payload/name.", "Payload/name ", "Payload//name"}) {
    cyan::ArchivePathValidator validator;
    INFO(path);
    auto result = validator.validate_and_reserve(path);
    REQUIRE_FALSE(result);
    CHECK((result.error().code == cyan::ErrorCode::archive_unsafe_path ||
           result.error().code == cyan::ErrorCode::invalid_utf8));
  }
}

TEST_CASE("archive entry names must be valid UTF-8") {
  cyan::ArchivePathValidator validator;
  const std::string invalid{"bad\xffname", 8U};
  auto result = validator.validate_and_reserve(invalid);
  REQUIRE_FALSE(result);
  CHECK(result.error().code == cyan::ErrorCode::invalid_utf8);
}
