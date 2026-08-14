#include <Windows.h>
#include <archive.h>
#include <archive_entry.h>

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "cyan/metadata/app_metadata_reader.hpp"
#if defined(CYAN_TEST_GUI_ICONS)
#include <QCoreApplication>
#include <QTemporaryDir>

#include "input_icon_cache.hpp"
#endif

namespace {

class TemporaryIpa {
 public:
  TemporaryIpa() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            (L"cyan-metadata-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
             std::to_wstring(nonce) + L".ipa");
  }

  ~TemporaryIpa() {
    std::error_code error;
    std::filesystem::remove(path_, error);
  }

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

void add_entry(archive* writer, std::string_view name, std::string_view data) {
  archive_entry* entry = archive_entry_new();
  REQUIRE(entry != nullptr);
  archive_entry_set_pathname(entry, std::string(name).c_str());
  archive_entry_set_filetype(entry, AE_IFREG);
  archive_entry_set_perm(entry, 0644);
  archive_entry_set_size(entry, static_cast<la_int64_t>(data.size()));
  REQUIRE(archive_write_header(writer, entry) == ARCHIVE_OK);
  REQUIRE(archive_write_data(writer, data.data(), data.size()) ==
          static_cast<la_ssize_t>(data.size()));
  archive_entry_free(entry);
}

std::string plist(std::string_view name, std::string_view identifier, std::string_view version,
                  std::string_view executable, std::string_view package_type = "APPL") {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
         "<plist version=\"1.0\"><dict>"
         "<key>CFBundleDisplayName</key><string>" +
         std::string(name) +
         "</string>"
         "<key>CFBundleIdentifier</key><string>" +
         std::string(identifier) +
         "</string>"
         "<key>CFBundleShortVersionString</key><string>" +
         std::string(version) +
         "</string>"
         "<key>MinimumOSVersion</key><string>15.0</string>"
         "<key>CFBundleExecutable</key><string>" +
         std::string(executable) +
         "</string>"
         "<key>CFBundlePackageType</key><string>" +
         std::string(package_type) +
         "</string>"
         "<key>CFBundleIconFiles</key><array>"
         "<string>AppIcon60x60</string></array>"
         "</dict></plist>";
}

std::vector<std::filesystem::path> real_ipa_paths() {
  const DWORD required = GetEnvironmentVariableW(L"CYAN_TEST_REAL_IPAS", nullptr, 0);
  if (required == 0) {
    return {};
  }
  std::wstring values(required, L'\0');
  const DWORD written = GetEnvironmentVariableW(L"CYAN_TEST_REAL_IPAS", values.data(), required);
  if (written == 0 || written >= required) {
    return {};
  }
  values.resize(written);
  std::vector<std::filesystem::path> paths;
  std::size_t start = 0;
  while (start <= values.size()) {
    const std::size_t separator = values.find(L'|', start);
    const std::size_t end = separator == std::wstring::npos ? values.size() : separator;
    if (end > start) {
      paths.emplace_back(values.substr(start, end - start));
    }
    if (separator == std::wstring::npos) {
      break;
    }
    start = separator + 1;
  }
  return paths;
}

}  // namespace

TEST_CASE("IPA metadata reader selects the top-level application plist and icon") {
  TemporaryIpa fixture;
  archive* writer = archive_write_new();
  REQUIRE(writer != nullptr);
  REQUIRE(archive_write_set_format_zip(writer) == ARCHIVE_OK);
  REQUIRE(archive_write_open_filename_w(writer, fixture.path().c_str()) == ARCHIVE_OK);
  add_entry(writer, "Payload/Main.app/PlugIns/Share.appex/Info.plist",
            plist("Wrong Extension", "com.example.main.share", "99", "Share", "XPC!"));
  add_entry(writer, "Payload/Main.app/Watch/Watch.app/Info.plist",
            plist("Wrong Watch App", "com.example.main.watch", "88", "Watch"));
  add_entry(writer, "Payload/Main.app/Info.plist",
            plist("Main Application", "com.example.main", "2.4.1", "Main"));
  add_entry(writer, "Payload/Main.app/Main", "executable");
  add_entry(writer, "Payload/Main.app/AppIcon60x60@3x.png", "icon-data");
  REQUIRE(archive_write_close(writer) == ARCHIVE_OK);
  REQUIRE(archive_write_free(writer) == ARCHIVE_OK);

  const cyan::AppMetadataReader reader;
  auto result = reader.read(fixture.path());
  REQUIRE(result);
  CHECK(result.value().app_name == "Main Application");
  CHECK(result.value().bundle_identifier == "com.example.main");
  CHECK(result.value().version == "2.4.1");
  CHECK(result.value().minimum_os == "15.0");
  CHECK(result.value().bundle_path == "Payload/Main.app");
  CHECK(result.value().icon_name == "Payload/Main.app/AppIcon60x60@3x.png");
  CHECK_FALSE(result.value().icon_data.empty());
}

TEST_CASE("real IPA metadata samples have distinct primary applications", "[real-ipa]") {
#if defined(CYAN_TEST_GUI_ICONS)
  int qt_argument_count = 1;
  char qt_application_name[] = "cyan-metadata-tests";
  char* qt_arguments[] = {qt_application_name, nullptr};
  QCoreApplication qt_application(qt_argument_count, qt_arguments);
#endif
  const auto paths = real_ipa_paths();
  if (paths.empty()) {
    SUCCEED("CYAN_TEST_REAL_IPAS is not set; optional local corpus skipped");
    return;
  }
  REQUIRE(paths.size() >= 4);

  const cyan::AppMetadataReader reader;
  std::set<std::string> identifiers;
  for (const auto& path : paths) {
    INFO("IPA: " << path.string());
    auto result = reader.read(path);
    REQUIRE(result);
    INFO("name=" << result.value().app_name << ", version=" << result.value().version << ", bundle="
                 << result.value().bundle_identifier << ", icon=" << result.value().icon_name
                 << ", icon bytes=" << result.value().icon_data.size());
    CHECK_FALSE(result.value().app_name.empty());
    CHECK_FALSE(result.value().version.empty());
    CHECK_FALSE(result.value().bundle_identifier.empty());
    CHECK_FALSE(result.value().icon_data.empty());
#if defined(CYAN_TEST_GUI_ICONS)
    const QImage decoded_icon = cyan::gui::decode_application_icon(result.value().icon_data);
    INFO("decoded icon=" << decoded_icon.width() << "x" << decoded_icon.height());
    CHECK_FALSE(decoded_icon.isNull());
    QTemporaryDir icon_cache;
    CHECK_FALSE(cyan::gui::cache_application_icon(result.value().icon_data, icon_cache).isEmpty());
#endif
    identifiers.insert(result.value().bundle_identifier);
  }
  CHECK(identifiers.size() == paths.size());
}
