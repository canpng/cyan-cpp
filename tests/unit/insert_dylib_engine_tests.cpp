#include <Windows.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

#include "cyan/macho/insert_dylib_engine.hpp"
#include "cyan/macho/macho_inspector.hpp"

namespace {

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value & 0xffU);
  bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
  bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
  bytes[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
}

void put64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
  put32(bytes, offset, static_cast<std::uint32_t>(value & 0xffffffffULL));
  put32(bytes, offset + 4U, static_cast<std::uint32_t>(value >> 32U));
}

void put32be(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
  bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
  bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
  bytes[offset + 3U] = static_cast<std::uint8_t>(value & 0xffU);
}

void name16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::string_view name) {
  for (std::size_t index = 0; index < name.size() && index < 16U; ++index) {
    bytes[offset + index] = static_cast<std::uint8_t>(name[index]);
  }
}

std::vector<std::uint8_t> thin64(std::uint32_t cpu_type = 0x0100000cU,
                                 std::uint32_t section_offset = 0x400U) {
  std::vector<std::uint8_t> bytes(0x800U, 0U);
  put32(bytes, 0U, 0xfeedfacfU);
  put32(bytes, 4U, cpu_type);
  put32(bytes, 8U, 0U);
  put32(bytes, 12U, 2U);
  put32(bytes, 16U, 1U);
  put32(bytes, 20U, 152U);

  const std::size_t segment = 32U;
  put32(bytes, segment, 0x19U);
  put32(bytes, segment + 4U, 152U);
  name16(bytes, segment + 8U, "__TEXT");
  put64(bytes, segment + 24U, 0x100000000ULL);
  put64(bytes, segment + 32U, 0x1000U);
  put64(bytes, segment + 40U, 0U);
  put64(bytes, segment + 48U, bytes.size());
  put32(bytes, segment + 64U, 1U);

  const std::size_t section = segment + 72U;
  name16(bytes, section, "__text");
  name16(bytes, section + 16U, "__TEXT");
  put64(bytes, section + 32U, 0x100000000ULL + section_offset);
  put64(bytes, section + 40U, 4U);
  put32(bytes, section + 48U, section_offset);
  bytes[section_offset] = 0xc3U;
  return bytes;
}

std::vector<std::uint8_t> signed_thin64() {
  std::vector<std::uint8_t> bytes(0x400U, 0U);
  put32(bytes, 0U, 0xfeedfacfU);
  put32(bytes, 4U, 0x0100000cU);
  put32(bytes, 12U, 2U);
  put32(bytes, 16U, 3U);
  put32(bytes, 20U, 112U);

  const std::size_t linkedit = 32U;
  put32(bytes, linkedit, 0x19U);
  put32(bytes, linkedit + 4U, 72U);
  name16(bytes, linkedit + 8U, "__LINKEDIT");
  put64(bytes, linkedit + 24U, 0x100002000ULL);
  put64(bytes, linkedit + 32U, 0x1000U);
  put64(bytes, linkedit + 40U, 0x200U);
  put64(bytes, linkedit + 48U, 0x200U);

  const std::size_t symtab = linkedit + 72U;
  put32(bytes, symtab, 0x2U);
  put32(bytes, symtab + 4U, 24U);
  put32(bytes, symtab + 8U, 0x200U);
  put32(bytes, symtab + 12U, 1U);
  put32(bytes, symtab + 16U, 0x300U);
  put32(bytes, symtab + 20U, 0x20U);

  const std::size_t signature = symtab + 24U;
  put32(bytes, signature, 0x1dU);
  put32(bytes, signature + 4U, 16U);
  put32(bytes, signature + 8U, 0x3f0U);
  put32(bytes, signature + 12U, 0x10U);
  for (std::size_t index = 0x3f0U; index < bytes.size(); ++index) {
    bytes[index] = 0xa5U;
  }
  return bytes;
}

std::vector<std::uint8_t> fat_binary(std::uint32_t arm_subtype = 0U,
                                     std::uint32_t x64_subtype = 0U) {
  const auto arm64 = thin64(0x0100000cU);
  const auto x64 = thin64(0x01000007U);
  std::vector<std::uint8_t> bytes(0x2800U, 0U);
  put32be(bytes, 0U, 0xcafebabeU);
  put32be(bytes, 4U, 2U);
  put32be(bytes, 8U, 0x0100000cU);
  put32be(bytes, 12U, arm_subtype);
  put32be(bytes, 16U, 0x1000U);
  put32be(bytes, 20U, static_cast<std::uint32_t>(arm64.size()));
  put32be(bytes, 24U, 12U);
  put32be(bytes, 28U, 0x01000007U);
  put32be(bytes, 32U, x64_subtype);
  put32be(bytes, 36U, 0x2000U);
  put32be(bytes, 40U, static_cast<std::uint32_t>(x64.size()));
  put32be(bytes, 44U, 12U);
  std::copy(arm64.begin(), arm64.end(), bytes.begin() + 0x1000);
  std::copy(x64.begin(), x64.end(), bytes.begin() + 0x2000);
  return bytes;
}

class TemporaryFile {
 public:
  explicit TemporaryFile(std::vector<std::uint8_t> bytes) {
    const auto ticks = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    path_ = std::filesystem::temp_directory_path() /
            (L"cyan-macho-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
             std::to_wstring(ticks));
    std::ofstream output(path_, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
  }

  ~TemporaryFile() {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

}  // namespace

TEST_CASE("native engine injects and detects duplicate weak dylibs") {
  TemporaryFile file(thin64());
  cyan::InsertDylibEngine engine;
  const auto first = engine.inject(file.path(), "@rpath/Test.dylib", cyan::InjectionOptions{});
  REQUIRE(first.error == cyan::InjectionError::None);
  REQUIRE(first.modified);

  cyan::MachOInspector inspector;
  auto info = inspector.inspect(file.path());
  REQUIRE(info);
  REQUIRE(info.value().slices.size() == 1U);
  CHECK(info.value().slices[0].dependencies == std::vector<std::string>{"@rpath/Test.dylib"});

  const auto duplicate = engine.inject(file.path(), "@rpath/Test.dylib", cyan::InjectionOptions{});
  CHECK(duplicate.error == cyan::InjectionError::DuplicateLoadCommand);
  CHECK_FALSE(duplicate.modified);
}

TEST_CASE("native engine rejects insufficient load-command space") {
  TemporaryFile file(thin64(0x0100000cU, 184U));
  cyan::InsertDylibEngine engine;
  const auto result = engine.inject(file.path(), "@rpath/NoRoom.dylib", cyan::InjectionOptions{});
  CHECK(result.error == cyan::InjectionError::InsufficientLoadCommandSpace);
  CHECK_FALSE(result.modified);
}

TEST_CASE("native engine removes a terminal code signature and repairs linkedit") {
  TemporaryFile file(signed_thin64());
  cyan::InsertDylibEngine engine;
  const auto result = engine.inject(file.path(), "@rpath/Signed.dylib", cyan::InjectionOptions{});
  REQUIRE(result.error == cyan::InjectionError::None);

  cyan::MachOInspector inspector;
  auto info = inspector.inspect(file.path());
  REQUIRE(info);
  CHECK_FALSE(info.value().slices.front().has_code_signature);
  CHECK(info.value().slices.front().dependencies ==
        std::vector<std::string>{"@rpath/Signed.dylib"});
  CHECK(std::filesystem::file_size(file.path()) == 0x3f0U);
}

TEST_CASE("native engine rebuilds every FAT architecture") {
  TemporaryFile file(fat_binary());
  cyan::InsertDylibEngine engine;
  const auto result = engine.inject(file.path(), "@rpath/Fat.dylib", cyan::InjectionOptions{});
  REQUIRE(result.error == cyan::InjectionError::None);

  cyan::MachOInspector inspector;
  auto info = inspector.inspect(file.path());
  REQUIRE(info);
  REQUIRE(info.value().is_fat);
  REQUIRE(info.value().slices.size() == 2U);
  for (const auto& slice : info.value().slices) {
    CHECK(slice.dependencies == std::vector<std::string>{"@rpath/Fat.dylib"});
  }
}

TEST_CASE("native engine reproduces ipapatch v2.1.3 FAT subtype filtering") {
  TemporaryFile file(fat_binary(0U, 3U));
  cyan::InsertDylibEngine engine;
  cyan::InjectionOptions options;
  options.fatArchitecturePolicy = cyan::FatArchitecturePolicy::IpaPatchV213;
  const auto result = engine.inject(file.path(), "@rpath/Fat.dylib", options);
  REQUIRE(result.error == cyan::InjectionError::None);

  cyan::MachOInspector inspector;
  auto info = inspector.inspect(file.path());
  REQUIRE(info);
  REQUIRE(info.value().is_fat);
  REQUIRE(info.value().slices.size() == 1U);
  CHECK(info.value().slices.front().cpu_type == 0x0100000c);
  CHECK(info.value().slices.front().dependencies ==
        std::vector<std::string>{"@rpath/Fat.dylib"});

  TemporaryFile unsupported(fat_binary(3U, 3U));
  const auto rejected =
      engine.inject(unsupported.path(), "@rpath/Fat.dylib", options);
  CHECK(rejected.error == cyan::InjectionError::UnsupportedArchitecture);
}
