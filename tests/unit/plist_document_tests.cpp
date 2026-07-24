#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "cyan/plist/plist_document.hpp"

namespace {

class PlistFixture {
 public:
  PlistFixture() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() / (L"cyan-plist-test-" + std::to_wstring(nonce));
    std::filesystem::create_directories(root_);
  }

  ~PlistFixture() {
    std::error_code error;
    std::filesystem::remove_all(root_, error);
  }

  [[nodiscard]] std::filesystem::path path(std::wstring_view name) const { return root_ / name; }

 private:
  std::filesystem::path root_;
};

constexpr std::string_view kXmlPlist =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
    "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
    "<plist version=\"1.0\"><dict>"
    "<key>CFBundleIdentifier</key><string>example.old</string>"
    "<key>Flag</key><true/>"
    "</dict></plist>";

}  // namespace

TEST_CASE("XML property lists retain typed values and accept edits") {
  PlistFixture fixture;
  const auto source = fixture.path(L"Info.plist");
  {
    std::ofstream output(source, std::ios::binary);
    output << kXmlPlist;
  }

  auto loaded = cyan::PlistDocument::load(source);
  REQUIRE(loaded);
  REQUIRE(loaded.value().source_format() == cyan::PlistFormat::xml);
  REQUIRE(loaded.value().string("CFBundleIdentifier") == "example.old");
  REQUIRE(loaded.value().boolean("Flag") == true);

  REQUIRE(loaded.value().set_string("CFBundleIdentifier", "example.new"));
  REQUIRE(loaded.value().remove("Flag"));
  REQUIRE(loaded.value().save(source));

  auto reparsed = cyan::PlistDocument::load(source);
  REQUIRE(reparsed);
  REQUIRE(reparsed.value().string("CFBundleIdentifier") == "example.new");
  REQUIRE_FALSE(reparsed.value().contains("Flag"));
}

TEST_CASE("binary property lists round trip and shallow merge preserves types") {
  PlistFixture fixture;
  const auto binary = fixture.path(L"Info.binary.plist");

  auto base = cyan::PlistDocument::create_dictionary();
  auto overlay = cyan::PlistDocument::create_dictionary();
  REQUIRE(base);
  REQUIRE(overlay);
  REQUIRE(base.value().set_string("Name", "Before"));
  REQUIRE(overlay.value().set_string("Name", "After"));
  REQUIRE(overlay.value().set_boolean("Documents", true));
  REQUIRE(base.value().merge(overlay.value()));
  REQUIRE(base.value().save(binary, cyan::PlistFormat::binary));

  auto reparsed = cyan::PlistDocument::load(binary);
  REQUIRE(reparsed);
  REQUIRE(reparsed.value().source_format() == cyan::PlistFormat::binary);
  REQUIRE(reparsed.value().string("Name") == "After");
  REQUIRE(reparsed.value().boolean("Documents") == true);
}
