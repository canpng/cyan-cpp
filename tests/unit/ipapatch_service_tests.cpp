#include <Windows.h>
#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cyan/archive/archive_service.hpp"
#include "cyan/archive/zip_update_service.hpp"
#include "cyan/ipapatch/ipapatch_service.hpp"
#include "cyan/macho/macho_inspector.hpp"
#include "cyan/platform/utf.hpp"
#include "cyan/plist/plist_document.hpp"

namespace {

class TemporaryDirectory {
 public:
  explicit TemporaryDirectory(std::wstring_view suffix = L"") {
    const auto ticks = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    path_ = std::filesystem::temp_directory_path() /
            (L"cyan-ipapatch-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
             std::to_wstring(ticks) + std::wstring(suffix));
    std::filesystem::create_directories(path_);
  }

  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value);
  bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
  bytes[offset + 2U] = static_cast<std::uint8_t>(value >> 16U);
  bytes[offset + 3U] = static_cast<std::uint8_t>(value >> 24U);
}

void put64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
  put32(bytes, offset, static_cast<std::uint32_t>(value));
  put32(bytes, offset + 4U, static_cast<std::uint32_t>(value >> 32U));
}

void put_name(std::vector<std::uint8_t>& bytes, std::size_t offset, std::string_view name) {
  std::copy(name.begin(), name.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

void put32be(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value >> 24U);
  bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 16U);
  bytes[offset + 2U] = static_cast<std::uint8_t>(value >> 8U);
  bytes[offset + 3U] = static_cast<std::uint8_t>(value);
}

std::vector<std::uint8_t> synthetic_macho(std::uint32_t cpu_type = 0x0100000cU) {
  std::vector<std::uint8_t> bytes(0x800U, 0U);
  put32(bytes, 0U, 0xfeedfacfU);
  put32(bytes, 4U, cpu_type);
  put32(bytes, 8U, 0U);
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

std::vector<std::uint8_t> synthetic_fat() {
  const auto arm64 = synthetic_macho(0x0100000cU);
  const auto x64 = synthetic_macho(0x01000007U);
  std::vector<std::uint8_t> bytes(0x2800U, 0U);
  put32be(bytes, 0U, 0xcafebabeU);
  put32be(bytes, 4U, 2U);
  put32be(bytes, 8U, 0x0100000cU);
  put32be(bytes, 12U, 0U);
  put32be(bytes, 16U, 0x1000U);
  put32be(bytes, 20U, static_cast<std::uint32_t>(arm64.size()));
  put32be(bytes, 24U, 12U);
  put32be(bytes, 28U, 0x01000007U);
  put32be(bytes, 32U, 3U);
  put32be(bytes, 36U, 0x2000U);
  put32be(bytes, 40U, static_cast<std::uint32_t>(x64.size()));
  put32be(bytes, 44U, 12U);
  std::copy(arm64.begin(), arm64.end(), bytes.begin() + 0x1000);
  std::copy(x64.begin(), x64.end(), bytes.begin() + 0x2000);
  return bytes;
}

void write_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  REQUIRE(output);
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  REQUIRE(output);
}

void write_zip_entry(const std::filesystem::path& path, std::string_view name,
                     const std::vector<std::uint8_t>& bytes) {
  archive* writer = archive_write_new();
  REQUIRE(writer != nullptr);
  REQUIRE(archive_write_set_format_zip(writer) == ARCHIVE_OK);
  REQUIRE(archive_write_open_filename_w(writer, path.c_str()) == ARCHIVE_OK);
  archive_entry* entry = archive_entry_new();
  REQUIRE(entry != nullptr);
  archive_entry_set_pathname(entry, std::string(name).c_str());
  archive_entry_set_filetype(entry, AE_IFREG);
  archive_entry_set_perm(entry, 0644);
  archive_entry_set_size(entry, static_cast<la_int64_t>(bytes.size()));
  REQUIRE(archive_write_header(writer, entry) == ARCHIVE_OK);
  REQUIRE(archive_write_data(writer, bytes.data(), bytes.size()) ==
          static_cast<la_ssize_t>(bytes.size()));
  archive_entry_free(entry);
  REQUIRE(archive_write_close(writer) == ARCHIVE_OK);
  REQUIRE(archive_write_free(writer) == ARCHIVE_OK);
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  REQUIRE(input);
  const auto length = input.tellg();
  REQUIRE(length >= 0);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
  input.seekg(0);
  if (!bytes.empty()) {
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  REQUIRE(input);
  return bytes;
}

void make_bundle(const std::filesystem::path& bundle, std::string_view executable,
                 std::string_view identifier) {
  std::filesystem::create_directories(bundle);
  auto plist = cyan::PlistDocument::create_dictionary();
  REQUIRE(plist);
  REQUIRE(plist.value().set_string("CFBundleExecutable", executable));
  REQUIRE(plist.value().set_string("CFBundleIdentifier", identifier));
  REQUIRE(plist.value().save(bundle / L"Info.plist", cyan::PlistFormat::binary));
  write_bytes(bundle / std::filesystem::path(executable), synthetic_macho());
}

class RecordingSigner final : public cyan::ISigningBackend {
 public:
  cyan::Result<cyan::SigningProfile> captureProfile(
      const std::filesystem::path& executable) override {
    auto document = cyan::PlistDocument::create_dictionary();
    if (!document) {
      return cyan::Result<cyan::SigningProfile>::failure(document.error());
    }
    auto encoded = cyan::platform::utf8_from_wide(executable.parent_path().filename().native());
    if (!encoded) {
      return cyan::Result<cyan::SigningProfile>::failure(encoded.error());
    }
    const std::string value = encoded.take_value();
    auto set = document.value().set_string("profile", value);
    if (!set) {
      return cyan::Result<cyan::SigningProfile>::failure(set.error());
    }
    cyan::SigningProfile profile;
    profile.had_signature = true;
    profile.entitlements.emplace(document.take_value());
    profile.identifier = "identifier." + value;
    profile.had_der_entitlements = true;
    return cyan::Result<cyan::SigningProfile>::success(std::move(profile));
  }

  cyan::Result<void> extractEntitlements(const std::filesystem::path&,
                                         cyan::PlistDocument&) override {
    return cyan::Result<void>::failure(
        {cyan::ErrorCode::internal_error, "unexpected legacy entitlement extraction", {}});
  }

  cyan::Result<void> removeSignature(const std::filesystem::path&) override {
    return cyan::Result<void>::success();
  }

  cyan::Result<void> signAdHoc(const std::filesystem::path& executable,
                               const std::optional<cyan::PlistDocument>&) override {
    signed_paths.push_back(executable);
    return cyan::Result<void>::success();
  }

  cyan::Result<void> signAdHoc(const std::filesystem::path& executable,
                               const cyan::SigningProfile& profile) override {
    signed_paths.push_back(executable);
    signed_profiles.push_back(
        profile.entitlements ? profile.entitlements->string("profile").value_or("") : "");
    return cyan::Result<void>::success();
  }

  std::vector<std::filesystem::path> signed_paths;
  std::vector<std::string> signed_profiles;
};

std::filesystem::path test_payload() { return std::filesystem::path(CYAN_TEST_IPAPATCH_PAYLOAD); }

std::filesystem::path tool_from_environment(std::wstring_view name) {
  std::vector<wchar_t> buffer(32768U);
  const std::wstring owned(name);
  const DWORD length =
      GetEnvironmentVariableW(owned.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length == 0U || static_cast<std::size_t>(length) >= buffer.size()) {
    return {};
  }
  return std::filesystem::path(std::wstring(buffer.data(), length));
}

std::wstring quote_argument(std::wstring_view argument) {
  if (argument.find_first_of(L" \t\"") == std::wstring_view::npos) {
    return std::wstring(argument);
  }
  std::wstring result = L"\"";
  std::size_t slashes = 0U;
  for (const wchar_t character : argument) {
    if (character == L'\\') {
      ++slashes;
    } else if (character == L'"') {
      result.append(slashes * 2U + 1U, L'\\');
      result.push_back(character);
      slashes = 0U;
    } else {
      result.append(slashes, L'\\');
      result.push_back(character);
      slashes = 0U;
    }
  }
  result.append(slashes * 2U, L'\\');
  result.push_back(L'"');
  return result;
}

bool run_process(const std::filesystem::path& executable,
                 const std::vector<std::wstring>& arguments) {
  std::wstring command = quote_argument(executable.native());
  for (const auto& argument : arguments) {
    command.push_back(L' ');
    command += quote_argument(argument);
  }
  std::vector<wchar_t> mutable_command(command.begin(), command.end());
  mutable_command.push_back(L'\0');
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(executable.c_str(), mutable_command.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
    return false;
  }
  const DWORD waited = WaitForSingleObject(process.hProcess, 30'000U);
  DWORD exit_code = 1U;
  if (waited == WAIT_OBJECT_0) {
    static_cast<void>(GetExitCodeProcess(process.hProcess, &exit_code));
  } else {
    static_cast<void>(TerminateProcess(process.hProcess, 124U));
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return waited == WAIT_OBJECT_0 && exit_code == 0U;
}

std::vector<std::filesystem::path> relative_files(const std::filesystem::path& root) {
  std::vector<std::filesystem::path> result;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
    if (entry.is_regular_file()) {
      result.push_back(std::filesystem::relative(entry.path(), root));
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

int zip_permissions(const std::filesystem::path& zip, std::string_view entry_name) {
  archive* reader = archive_read_new();
  REQUIRE(reader != nullptr);
  REQUIRE(archive_read_support_format_zip(reader) == ARCHIVE_OK);
  REQUIRE(archive_read_open_filename_w(reader, zip.c_str(), 10240U) == ARCHIVE_OK);

  int permissions = -1;
  archive_entry* entry = nullptr;
  while (archive_read_next_header(reader, &entry) == ARCHIVE_OK) {
    const char* name = archive_entry_pathname_utf8(entry);
    if (name != nullptr && entry_name == name) {
      permissions = archive_entry_perm(entry);
      break;
    }
    REQUIRE(archive_read_data_skip(reader) == ARCHIVE_OK);
  }
  CHECK(archive_read_close(reader) == ARCHIVE_OK);
  CHECK(archive_read_free(reader) == ARCHIVE_OK);
  return permissions;
}

void require_dependency(const std::filesystem::path& executable, std::string_view dependency,
                        bool expected) {
  cyan::MachOInspector inspector;
  auto inspected = inspector.inspect(executable);
  REQUIRE(inspected);
  REQUIRE(inspected.value().slices.size() == 1U);
  const auto& dependencies = inspected.value().slices.front().dependencies;
  CHECK((std::find(dependencies.begin(), dependencies.end(), dependency) != dependencies.end()) ==
        expected);
}

}  // namespace

TEST_CASE("ipapatch patches the main executable and multiple app extensions") {
  TemporaryDirectory fixture(L"-Unicode Boşluk-測試");
  const auto package = fixture.path() / L"package";
  const auto app = package / L"Payload" / L"Örnek Uygulama.app";
  const auto first = app / L"PlugIns" / L"Bir.appex";
  const auto second = app / L"Extensions" / L"İki.appex";
  const auto watch = app / L"Watch" / L"Saat.app";
  make_bundle(app, "Main", "example.main");
  make_bundle(first, "First", "example.first");
  make_bundle(second, "Second", "example.second");
  make_bundle(watch, "Watch", "example.watch");

  const auto custom_payload = fixture.path() / L"Özel Payload.dylib";
  write_bytes(custom_payload, {1U, 2U, 3U, 4U});
  RecordingSigner signer;
  cyan::IpaPatchService service(signer);
  cyan::IpaPatchOptions options;
  options.dylib = custom_payload;

  auto applied = service.apply_to_open_package(package, options);
  REQUIRE(applied);
  REQUIRE(applied.value().targets.size() == 3U);
  CHECK(applied.value().load_command == "@rpath/Özel Payload.dylib");
  require_dependency(app / L"Main", applied.value().load_command, true);
  require_dependency(first / L"First", applied.value().load_command, true);
  require_dependency(second / L"Second", applied.value().load_command, true);
  require_dependency(watch / L"Watch", applied.value().load_command, false);
  CHECK(std::filesystem::is_regular_file(app / L"Frameworks" / custom_payload.filename()));

  auto finalized = service.finalize_signatures(applied.value());
  REQUIRE(finalized);
  CHECK(signer.signed_paths.size() == 3U);
  CHECK(signer.signed_profiles ==
        std::vector<std::string>{"Örnek Uygulama.app", "İki.appex", "Bir.appex"});
}

TEST_CASE("ipapatch plugins-only leaves the main executable untouched") {
  TemporaryDirectory fixture;
  const auto package = fixture.path() / L"package";
  const auto app = package / L"Payload" / L"Test.app";
  const auto plugin = app / L"PlugIns" / L"Plugin.appex";
  make_bundle(app, "Main", "example.main");
  make_bundle(plugin, "Plugin", "example.plugin");

  RecordingSigner signer;
  cyan::IpaPatchService service(signer);
  cyan::IpaPatchOptions options;
  options.default_dylib = test_payload();
  options.plugins_only = true;
  auto applied = service.apply_to_open_package(package, options);
  REQUIRE(applied);
  REQUIRE(applied.value().targets.size() == 1U);
  require_dependency(app / L"Main", "@rpath/zxPluginsInject.dylib", false);
  require_dependency(plugin / L"Plugin", "@rpath/zxPluginsInject.dylib", true);
}

TEST_CASE("ipapatch detects an existing load command before publishing output") {
  TemporaryDirectory fixture;
  const auto package = fixture.path() / L"package";
  const auto app = package / L"Payload" / L"Test.app";
  make_bundle(app, "Main", "example.main");

  RecordingSigner signer;
  cyan::IpaPatchService service(signer);
  cyan::IpaPatchOptions options;
  options.default_dylib = test_payload();
  auto first = service.apply_to_open_package(package, options);
  REQUIRE(first);
  auto duplicate = service.prepare_open_package(package, options);
  REQUIRE_FALSE(duplicate);
  CHECK(duplicate.error().code == cyan::ErrorCode::injection_failed);
  CHECK(duplicate.error().message.find("already patched") != std::string::npos);
}

TEST_CASE("standalone ipapatch selectively updates once and preserves input on failure") {
  TemporaryDirectory fixture(L"-Yol Boşluk-ğ");
  const auto source_package = fixture.path() / L"source";
  const auto app = source_package / L"Payload" / L"Test.app";
  make_bundle(app, "Main", "example.main");
  write_bytes(app / L".preserved-hidden-file", {9U, 8U, 7U});
  const auto input = fixture.path() / L"Girdi Uygulama.ipa";
  const auto output = fixture.path() / L"Çıktı Uygulama.ipa";
  cyan::ArchiveService archive;
  REQUIRE(archive.create_zip(source_package, input, 1, false));
  const auto original = read_bytes(input);
  const int original_executable_permissions = zip_permissions(input, "Payload/Test.app/Main");
  const int original_plist_permissions = zip_permissions(input, "Payload/Test.app/Info.plist");

  RecordingSigner signer;
  cyan::IpaPatchService service(signer);
  cyan::IpaPatchOptions options;
  options.default_dylib = test_payload();
  std::size_t extracts = 0U;
  std::size_t packages = 0U;
  cyan::IpaPatchCallbacks callbacks;
  callbacks.progress = [&](const cyan::IpaPatchProgressEvent& event) {
    if (event.completed == 1U && event.total == 1U) {
      extracts += event.stage == cyan::IpaPatchStage::extracting ? 1U : 0U;
      packages += event.stage == cyan::IpaPatchStage::packaging ? 1U : 0U;
    }
  };
  auto completed = service.run_standalone(input, output, options, callbacks);
  REQUIRE(completed);
  CHECK(extracts == 1U);
  CHECK(packages == 1U);
  CHECK(read_bytes(input) == original);
  CHECK(zip_permissions(output, "Payload/Test.app/Main") == original_executable_permissions);
  CHECK(zip_permissions(output, "Payload/Test.app/Info.plist") == original_plist_permissions);
  CHECK(zip_permissions(output, "Payload/Test.app/Frameworks/zxPluginsInject.dylib") == 0755);

  const auto extracted = fixture.path() / L"output-tree";
  REQUIRE(archive.extract(output, extracted));
  require_dependency(extracted / L"Payload" / L"Test.app" / L"Main", "@rpath/zxPluginsInject.dylib",
                     true);
  CHECK(std::filesystem::is_regular_file(extracted / L"Payload" / L"Test.app" /
                                         L".preserved-hidden-file"));

  const auto patched_input = fixture.path() / L"Patched.ipa";
  REQUIRE(std::filesystem::copy_file(output, patched_input));
  const auto before_failure = read_bytes(patched_input);
  auto failed_inplace = service.run_standalone(patched_input, patched_input, options);
  REQUIRE_FALSE(failed_inplace);
  CHECK(read_bytes(patched_input) == before_failure);
}

TEST_CASE("selective ZIP updates reject unsafe archive paths before mutation") {
  TemporaryDirectory fixture;
  const auto input = fixture.path() / L"Unsafe.ipa";
  write_zip_entry(input, "../outside", {1U, 2U, 3U});

  cyan::ZipUpdateService updater;
  auto listed = updater.list_entries(input);
  REQUIRE_FALSE(listed);
  CHECK(listed.error().code == cyan::ErrorCode::archive_unsafe_path);
  CHECK_FALSE(std::filesystem::exists(fixture.path().parent_path() / L"outside"));
}

TEST_CASE("standalone ipapatch executable uses the shared backend and bundled payload") {
  const auto cli = tool_from_environment(L"CYAN_TEST_IPAPATCH_CLI");
  const auto ldid = tool_from_environment(L"CYAN_TEST_LDID");
  if (cli.empty() || ldid.empty()) {
    SKIP("standalone CLI or ldid is not configured");
  }

  TemporaryDirectory fixture;
  const auto package = fixture.path() / L"source";
  const auto app = package / L"Payload" / L"Standalone.app";
  make_bundle(app, "Main", "example.standalone");
  cyan::ArchiveService archive;
  const auto input = fixture.path() / L"Standalone Input.ipa";
  const auto output = fixture.path() / L"Standalone Output.ipa";
  REQUIRE(archive.create_zip(package, input, 1, false));
  const auto original = read_bytes(input);

  REQUIRE(
      run_process(cli, {L"--input", input.native(), L"--output", output.native(), L"--noconfirm"}));
  CHECK(read_bytes(input) == original);

  const auto extracted = fixture.path() / L"output";
  REQUIRE(archive.extract(output, extracted));
  const auto executable = extracted / L"Payload" / L"Standalone.app" / L"Main";
  require_dependency(executable, "@rpath/zxPluginsInject.dylib", true);
  cyan::MachOInspector inspector;
  auto inspected = inspector.inspect(executable);
  REQUIRE(inspected);
  CHECK(inspected.value().slices.front().has_code_signature);
}

TEST_CASE("C++ output is structurally equivalent to ipapatch v2.1.3") {
  const auto reference = tool_from_environment(L"CYAN_TEST_IPAPATCH_REFERENCE");
  if (reference.empty()) {
    SKIP("CYAN_TEST_IPAPATCH_REFERENCE is not configured");
  }

  TemporaryDirectory fixture;
  const auto package = fixture.path() / L"source";
  const auto app = package / L"Payload" / L"Diff.app";
  const auto plugin = app / L"PlugIns" / L"DiffPlugin.appex";
  make_bundle(app, "Main", "example.main");
  make_bundle(plugin, "Plugin", "example.plugin");
  write_bytes(app / L"Main", synthetic_fat());
  cyan::ArchiveService archive;
  const auto input = fixture.path() / L"Input.ipa";
  const auto cpp_output = fixture.path() / L"Cpp.ipa";
  const auto reference_output = fixture.path() / L"Reference.ipa";
  REQUIRE(archive.create_zip(package, input, 1, false));

  RecordingSigner signer;
  cyan::IpaPatchService service(signer);
  cyan::IpaPatchOptions options;
  options.dylib = test_payload();
  REQUIRE(service.run_standalone(input, cpp_output, options));
  REQUIRE(
      run_process(reference, {L"--input", input.native(), L"--output", reference_output.native(),
                              L"--dylib", test_payload().native(), L"--noconfirm"}));

  const auto cpp_tree = fixture.path() / L"cpp-tree";
  const auto reference_tree = fixture.path() / L"reference-tree";
  REQUIRE(archive.extract(cpp_output, cpp_tree));
  REQUIRE(archive.extract(reference_output, reference_tree));
  CHECK(relative_files(cpp_tree) == relative_files(reference_tree));

  for (const auto& relative :
       {std::filesystem::path(L"Payload/Diff.app/Main"),
        std::filesystem::path(L"Payload/Diff.app/PlugIns/DiffPlugin.appex/Plugin")}) {
    cyan::MachOInspector inspector;
    auto cpp_info = inspector.inspect(cpp_tree / relative);
    auto reference_info = inspector.inspect(reference_tree / relative);
    REQUIRE(cpp_info);
    REQUIRE(reference_info);
    REQUIRE(cpp_info.value().slices.size() == reference_info.value().slices.size());
    CHECK(cpp_info.value().slices.front().cpu_type ==
          reference_info.value().slices.front().cpu_type);
    CHECK(cpp_info.value().slices.front().cpu_subtype ==
          reference_info.value().slices.front().cpu_subtype);
    CHECK(cpp_info.value().slices.front().dependencies ==
          reference_info.value().slices.front().dependencies);
  }
  const auto payload_relative =
      std::filesystem::path(L"Payload/Diff.app/Frameworks/zxPluginsInject.dylib");
  const bool payload_equal =
      read_bytes(cpp_tree / payload_relative) == read_bytes(reference_tree / payload_relative);
  CHECK(payload_equal);
}
