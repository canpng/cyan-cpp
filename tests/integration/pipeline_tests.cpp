#include <Windows.h>
#include <archive.h>
#include <archive_entry.h>

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "cyan/archive/archive_service.hpp"
#include "cyan/archive/cyan_archive.hpp"
#include "cyan/core/cyan_pipeline.hpp"
#include "cyan/macho/macho_inspector.hpp"
#include "cyan/plist/plist_document.hpp"

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto ticks = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    path_ = std::filesystem::temp_directory_path() /
            (L"cyan-pipeline-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
             std::to_wstring(ticks));
    std::filesystem::create_directories(path_);
  }

  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value & 0xffU);
  bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
  bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
  bytes[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
}

void put64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
  put32(bytes, offset, static_cast<std::uint32_t>(value));
  put32(bytes, offset + 4U, static_cast<std::uint32_t>(value >> 32U));
}

void put_name(std::vector<std::uint8_t>& bytes, std::size_t offset, std::string_view name) {
  for (std::size_t index = 0; index < name.size() && index < 16U; ++index) {
    bytes[offset + index] = static_cast<std::uint8_t>(name[index]);
  }
}

std::vector<std::uint8_t> synthetic_executable_bytes() {
  std::vector<std::uint8_t> bytes(0x800U, 0U);
  put32(bytes, 0U, 0xfeedfacfU);
  put32(bytes, 4U, 0x0100000cU);
  put32(bytes, 12U, 2U);
  put32(bytes, 16U, 1U);
  put32(bytes, 20U, 152U);

  constexpr std::size_t segment = 32U;
  put32(bytes, segment, 0x19U);
  put32(bytes, segment + 4U, 152U);
  put_name(bytes, segment + 8U, "__TEXT");
  put64(bytes, segment + 24U, 0x100000000ULL);
  put64(bytes, segment + 32U, 0x1000U);
  put64(bytes, segment + 48U, bytes.size());
  put32(bytes, segment + 64U, 1U);

  constexpr std::size_t section = segment + 72U;
  put_name(bytes, section, "__text");
  put_name(bytes, section + 16U, "__TEXT");
  put64(bytes, section + 32U, 0x100000400ULL);
  put64(bytes, section + 40U, 4U);
  put32(bytes, section + 48U, 0x400U);
  return bytes;
}

void write_synthetic_executable(const std::filesystem::path& path) {
  const auto bytes = synthetic_executable_bytes();
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

void add_archive_entry(archive* writer, std::string_view name,
                       const std::vector<std::uint8_t>& bytes) {
  archive_entry* entry = archive_entry_new();
  REQUIRE(entry != nullptr);
  archive_entry_set_pathname(entry, std::string(name).c_str());
  archive_entry_set_filetype(entry, AE_IFREG);
  archive_entry_set_perm(entry, 0755);
  archive_entry_set_size(entry, static_cast<la_int64_t>(bytes.size()));
  REQUIRE(archive_write_header(writer, entry) == ARCHIVE_OK);
  REQUIRE(archive_write_data(writer, bytes.data(), bytes.size()) ==
          static_cast<la_ssize_t>(bytes.size()));
  archive_entry_free(entry);
}

void add_archive_directory(archive* writer, std::string_view name) {
  archive_entry* entry = archive_entry_new();
  REQUIRE(entry != nullptr);
  archive_entry_set_pathname(entry, std::string(name).c_str());
  archive_entry_set_filetype(entry, AE_IFDIR);
  archive_entry_set_perm(entry, 0755);
  archive_entry_set_size(entry, 0);
  REQUIRE(archive_write_header(writer, entry) == ARCHIVE_OK);
  archive_entry_free(entry);
}

void write_test_deb(const std::filesystem::path& deb, const std::filesystem::path& temporary_tar) {
  archive* tar = archive_write_new();
  REQUIRE(tar != nullptr);
  REQUIRE(archive_write_set_format_pax_restricted(tar) == ARCHIVE_OK);
  REQUIRE(archive_write_open_filename_w(tar, temporary_tar.c_str()) == ARCHIVE_OK);
  add_archive_directory(tar, "./");
  add_archive_entry(tar, "./Library/MobileSubstrate/DynamicLibraries/DebExample.dylib",
                    synthetic_executable_bytes());
  REQUIRE(archive_write_close(tar) == ARCHIVE_OK);
  REQUIRE(archive_write_free(tar) == ARCHIVE_OK);

  std::ifstream input(temporary_tar, std::ios::binary | std::ios::ate);
  REQUIRE(input);
  const auto length = input.tellg();
  REQUIRE(length > 0);
  std::vector<std::uint8_t> tar_bytes(static_cast<std::size_t>(length));
  input.seekg(0);
  input.read(reinterpret_cast<char*>(tar_bytes.data()),
             static_cast<std::streamsize>(tar_bytes.size()));
  REQUIRE(input);

  archive* ar = archive_write_new();
  REQUIRE(ar != nullptr);
  REQUIRE(archive_write_set_format_ar_svr4(ar) == ARCHIVE_OK);
  REQUIRE(archive_write_open_filename_w(ar, deb.c_str()) == ARCHIVE_OK);
  add_archive_entry(ar, "debian-binary", {'2', '.', '0', '\n'});
  add_archive_entry(ar, "data.tar", tar_bytes);
  REQUIRE(archive_write_close(ar) == ARCHIVE_OK);
  REQUIRE(archive_write_free(ar) == ARCHIVE_OK);
}

void write_test_bitmap(const std::filesystem::path& path) {
  std::vector<std::uint8_t> bytes(58U, 0U);
  bytes[0] = 'B';
  bytes[1] = 'M';
  put32(bytes, 2U, static_cast<std::uint32_t>(bytes.size()));
  put32(bytes, 10U, 54U);
  put32(bytes, 14U, 40U);
  put32(bytes, 18U, 1U);
  put32(bytes, 22U, 1U);
  bytes[26U] = 1U;
  bytes[28U] = 24U;
  put32(bytes, 34U, 4U);
  bytes[54U] = 0x20U;
  bytes[55U] = 0x80U;
  bytes[56U] = 0xe0U;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

void make_app(const std::filesystem::path& app) {
  std::filesystem::create_directories(app);
  auto plist = cyan::PlistDocument::create_dictionary();
  REQUIRE(plist);
  REQUIRE(plist.value().set_string("CFBundleExecutable", "Synthetic"));
  REQUIRE(plist.value().set_string("CFBundleIdentifier", "example.original"));
  REQUIRE(plist.value().set_string("CFBundleName", "Original"));
  REQUIRE(plist.value().save(app / L"Info.plist", cyan::PlistFormat::binary));
  write_synthetic_executable(app / L"Synthetic");
}

std::filesystem::path ldid_from_environment() {
  std::vector<wchar_t> buffer(32768U);
  const DWORD length =
      GetEnvironmentVariableW(L"CYAN_TEST_LDID", buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length == 0U || static_cast<std::size_t>(length) >= buffer.size()) {
    return {};
  }
  return std::filesystem::path(std::wstring(buffer.data(), length));
}

}  // namespace

TEST_CASE("pipeline transforms a synthetic app bundle end to end") {
  TemporaryDirectory fixture;
  const auto input = fixture.path() / L"Input.app";
  const auto output = fixture.path() / L"Output.app";
  const auto icon = fixture.path() / L"icon.bmp";
  make_app(input);
  write_test_bitmap(icon);
  std::filesystem::create_directories(input / L"Watch");
  std::filesystem::create_directories(input / L"PlugIns");

  cyan::CyanOptions options;
  options.input = input;
  options.output = output;
  options.name = L"Yeni Ad";
  options.version = L"2.5";
  options.bundle_id = L"example.changed";
  options.minimum_os = L"15.0";
  options.icon = icon;
  options.no_watch = true;
  options.remove_extensions = true;
  options.enable_documents = true;
  options.overwrite = true;

  cyan::CyanPipeline pipeline;
  auto transformed = pipeline.run(std::move(options));
  REQUIRE(transformed);
  REQUIRE(std::filesystem::is_regular_file(output / L"Synthetic"));
  REQUIRE_FALSE(std::filesystem::exists(output / L"Watch"));
  REQUIRE_FALSE(std::filesystem::exists(output / L"PlugIns"));

  auto plist = cyan::PlistDocument::load(output / L"Info.plist");
  REQUIRE(plist);
  CHECK(plist.value().string("CFBundleName") == "Yeni Ad");
  CHECK(plist.value().string("CFBundleDisplayName") == "Yeni Ad");
  CHECK(plist.value().string("CFBundleVersion") == "2.5");
  CHECK(plist.value().string("CFBundleShortVersionString") == "2.5");
  CHECK(plist.value().string("CFBundleIdentifier") == "example.changed");
  CHECK(plist.value().string("MinimumOSVersion") == "15.0");
  CHECK(plist.value().boolean("UISupportsDocumentBrowser") == true);
  CHECK(plist.value().boolean("UIFileSharingEnabled") == true);
  REQUIRE(plist.value().string("CFBundleIconName"));

  std::size_t generated_icons = 0;
  for (const auto& entry : std::filesystem::directory_iterator(output)) {
    if (entry.is_regular_file() && entry.path().extension() == L".png" &&
        entry.path().filename().native().starts_with(L"cyan_")) {
      ++generated_icons;
    }
  }
  CHECK(generated_icons == 2U);
}

TEST_CASE("cgen output can be read back with ordered cyan semantics") {
  TemporaryDirectory fixture;
  cyan::CgenOptions generated;
  generated.output = fixture.path() / L"settings.cyan";
  generated.name = L"Configured";
  generated.version = L"7";
  generated.enable_documents = true;

  cyan::CyanArchiveWriter writer;
  REQUIRE(writer.write(generated));

  cyan::CyanOptions applied;
  cyan::CyanArchiveReader reader;
  REQUIRE(reader.apply(generated.output, fixture.path() / L"extracted", applied));
  CHECK(applied.name == L"Configured");
  CHECK(applied.version == L"7");
  CHECK(applied.enable_documents);
}

TEST_CASE("pipeline injects a dylib through the native load-command engine") {
  TemporaryDirectory fixture;
  const auto input = fixture.path() / L"Inject.app";
  const auto output = fixture.path() / L"Injected.app";
  const auto tweak = fixture.path() / L"Example.dylib";
  make_app(input);
  write_synthetic_executable(tweak);

  cyan::CyanOptions options;
  options.input = input;
  options.output = output;
  options.injected_items.push_back(tweak);
  options.overwrite = true;

  cyan::CyanPipeline pipeline;
  auto transformed = pipeline.run(std::move(options));
  REQUIRE(transformed);
  REQUIRE(std::filesystem::is_regular_file(output / L"Frameworks" / L"Example.dylib"));

  cyan::MachOInspector inspector;
  auto inspected = inspector.inspect(output / L"Synthetic");
  REQUIRE(inspected);
  REQUIRE(inspected.value().slices.size() == 1U);
  CHECK(inspected.value().slices.front().dependencies ==
        std::vector<std::string>{"@rpath/Example.dylib"});
  CHECK(inspected.value().slices.front().rpaths ==
        std::vector<std::string>{"@executable_path/Frameworks"});
}

TEST_CASE("pipeline extracts DEB data archives and injects discovered dylibs") {
  TemporaryDirectory fixture;
  const auto input = fixture.path() / L"DebInput.app";
  const auto output = fixture.path() / L"DebOutput.app";
  const auto deb = fixture.path() / L"example.deb";
  make_app(input);
  write_test_deb(deb, fixture.path() / L"data.tar");

  cyan::CyanOptions options;
  options.input = input;
  options.output = output;
  options.injected_items.push_back(deb);
  options.overwrite = true;

  cyan::CyanPipeline pipeline;
  auto transformed = pipeline.run(std::move(options));
  REQUIRE(transformed);
  REQUIRE(std::filesystem::is_regular_file(output / L"Frameworks" / L"DebExample.dylib"));

  cyan::MachOInspector inspector;
  auto inspected = inspector.inspect(output / L"Synthetic");
  REQUIRE(inspected);
  CHECK(inspected.value().slices.front().dependencies ==
        std::vector<std::string>{"@rpath/DebExample.dylib"});
}

TEST_CASE("pipeline accepts TIPA and atomically publishes IPA output") {
  TemporaryDirectory fixture;
  const auto unicode_root = fixture.path() / L"Türkçe 測試";
  const auto package_root = unicode_root / L"package";
  make_app(package_root / L"Payload" / L"Arşiv 測試.app");

  cyan::ArchiveService archives;
  const auto input = unicode_root / L"Girdi.tipa";
  auto created = archives.create_zip(package_root, input, 1, false);
  const std::string create_message = created ? std::string{} : created.error().message;
  INFO(create_message);
  REQUIRE(created);

  cyan::CyanOptions options;
  options.input = input;
  options.output = unicode_root / L"Çıktı.ipa";
  options.name = L"Archive Result";
  options.compression_level = 9;
  options.overwrite = true;

  cyan::CyanPipeline pipeline;
  REQUIRE(pipeline.run(std::move(options)));

  const auto extracted = unicode_root / L"published";
  REQUIRE(archives.extract(unicode_root / L"Çıktı.ipa", extracted));
  const auto published_app = extracted / L"Payload" / L"Arşiv 測試.app";
  REQUIRE(std::filesystem::is_regular_file(published_app / L"Synthetic"));

  auto plist = cyan::PlistDocument::load(published_app / L"Info.plist");
  REQUIRE(plist);
  CHECK(plist.value().string("CFBundleName") == "Archive Result");
}

TEST_CASE("pipeline copies payload root items beside and never inside the app bundle") {
  TemporaryDirectory fixture;
  const auto source = fixture.path() / L"source";
  const auto app = source / L"Payload" / L"PayloadRoot.app";
  make_app(app);

  cyan::ArchiveService archives;
  const auto input = fixture.path() / L"Input.ipa";
  REQUIRE(archives.create_zip(source, input, 1, false));

  const auto payload_file = fixture.path() / L"custom.file";
  const auto payload_directory = fixture.path() / L"ExtraAssets";
  std::ofstream(payload_file, std::ios::binary) << "payload-root";
  std::filesystem::create_directories(payload_directory);
  std::ofstream(payload_directory / L"nested.txt", std::ios::binary) << "nested";

  cyan::CyanOptions options;
  options.input = input;
  options.output = fixture.path() / L"Output.ipa";
  options.payload_root_items = {payload_file, payload_directory};
  options.overwrite = true;

  cyan::CyanPipeline pipeline;
  REQUIRE(pipeline.run(std::move(options)));

  const auto extracted = fixture.path() / L"published";
  REQUIRE(archives.extract(fixture.path() / L"Output.ipa", extracted));
  CHECK(std::filesystem::is_regular_file(extracted / L"Payload" / L"custom.file"));
  CHECK(std::filesystem::is_regular_file(extracted / L"Payload" / L"ExtraAssets" / L"nested.txt"));
  CHECK_FALSE(
      std::filesystem::exists(extracted / L"Payload" / L"PayloadRoot.app" / L"custom.file"));
  CHECK_FALSE(
      std::filesystem::exists(extracted / L"Payload" / L"PayloadRoot.app" / L"ExtraAssets"));
}

TEST_CASE("payload root items cannot replace the application bundle") {
  TemporaryDirectory fixture;
  const auto source = fixture.path() / L"source";
  make_app(source / L"Payload" / L"Collision.app");

  cyan::ArchiveService archives;
  const auto input = fixture.path() / L"Input.ipa";
  REQUIRE(archives.create_zip(source, input, 1, false));

  const auto colliding_item = fixture.path() / L"Collision.app";
  std::filesystem::create_directories(colliding_item);
  std::ofstream(colliding_item / L"unexpected.txt", std::ios::binary) << "collision";

  cyan::CyanOptions options;
  options.input = input;
  options.output = fixture.path() / L"Output.ipa";
  options.payload_root_items = {colliding_item};
  options.overwrite = true;

  cyan::CyanPipeline pipeline;
  const auto transformed = pipeline.run(std::move(options));
  REQUIRE_FALSE(transformed);
  CHECK(transformed.error().code == cyan::ErrorCode::archive_unsafe_path);
  CHECK_FALSE(std::filesystem::exists(fixture.path() / L"Output.ipa"));
}

TEST_CASE("integrated ipapatch uses one extract and one package pass") {
  const auto signer = ldid_from_environment();
  if (signer.empty()) {
    SKIP("CYAN_TEST_LDID is not configured");
  }

  TemporaryDirectory fixture;
  const auto source = fixture.path() / L"source";
  make_app(source / L"Payload" / L"Integrated.app");
  cyan::ArchiveService archive;
  const auto input = fixture.path() / L"Input.ipa";
  const auto output = fixture.path() / L"Output.ipa";
  REQUIRE(archive.create_zip(source, input, 1, false));
  const auto injected_dylib = fixture.path() / L"Integrated Tweak.dylib";
  write_synthetic_executable(injected_dylib);

  cyan::CyanOptions options;
  options.input = input;
  options.output = output;
  options.injected_items.push_back(injected_dylib);
  options.ipapatch = true;
  options.ipapatch_dylib = std::filesystem::path(CYAN_TEST_IPAPATCH_PAYLOAD);
  options.ldid_path = signer;
  options.overwrite = true;

  std::vector<std::wstring> messages;
  cyan::CyanPipeline pipeline;
  auto completed = pipeline.run(std::move(options),
                                [&](std::wstring_view message) { messages.emplace_back(message); });
  const std::string pipeline_error = completed ? std::string{} : completed.error().message;
  INFO(pipeline_error);
  REQUIRE(completed);
  CHECK(std::count_if(messages.begin(), messages.end(), [](const std::wstring& message) {
          return message.find(L"extracting ipa") != std::wstring::npos;
        }) == 1);
  CHECK(std::count_if(messages.begin(), messages.end(), [](const std::wstring& message) {
          return message.find(L"generating ipa") != std::wstring::npos;
        }) == 1);

  const auto extracted = fixture.path() / L"result";
  REQUIRE(archive.extract(output, extracted));
  const auto executable = extracted / L"Payload" / L"Integrated.app" / L"Synthetic";
  cyan::MachOInspector inspector;
  auto inspected = inspector.inspect(executable);
  REQUIRE(inspected);
  REQUIRE(inspected.value().slices.size() == 1U);
  CHECK(std::find(inspected.value().slices.front().dependencies.begin(),
                  inspected.value().slices.front().dependencies.end(),
                  "@rpath/zxPluginsInject.dylib") !=
        inspected.value().slices.front().dependencies.end());
  CHECK(inspected.value().slices.front().has_code_signature);

  const auto installed_dylib =
      extracted / L"Payload" / L"Integrated.app" / L"Frameworks" / injected_dylib.filename();
  auto dylib_info = inspector.inspect(installed_dylib);
  REQUIRE(dylib_info);
  REQUIRE(dylib_info.value().slices.size() == 1U);
  CHECK(dylib_info.value().slices.front().has_code_signature);
}
