#include <Windows.h>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cyan/core/temporary_workspace.hpp"
#include "cyan/macho/macho_inspector.hpp"
#include "cyan/signing/signing_backend.hpp"

namespace {

void put_little_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value & 0xffU);
  bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
  bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
  bytes[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
}

void put_little_u64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
  put_little_u32(bytes, offset, static_cast<std::uint32_t>(value));
  put_little_u32(bytes, offset + 4U, static_cast<std::uint32_t>(value >> 32U));
}

void put_big_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
  bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
  bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
  bytes[offset + 3U] = static_cast<std::uint8_t>(value & 0xffU);
}

void put_name(std::vector<std::uint8_t>& bytes, std::size_t offset, std::string_view name) {
  for (std::size_t index = 0; index < name.size() && index < 16U; ++index) {
    bytes[offset + index] = static_cast<std::uint8_t>(name[index]);
  }
}

void write_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  REQUIRE(output);
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  REQUIRE(output);
}

std::vector<std::uint8_t> unsigned_macho() {
  std::vector<std::uint8_t> bytes(0x800U, 0U);
  put_little_u32(bytes, 0U, 0xfeedfacfU);
  put_little_u32(bytes, 4U, 0x0100000cU);
  put_little_u32(bytes, 8U, 0U);
  put_little_u32(bytes, 12U, 2U);
  put_little_u32(bytes, 16U, 2U);
  put_little_u32(bytes, 20U, 224U);

  constexpr std::size_t text = 32U;
  put_little_u32(bytes, text, 0x19U);
  put_little_u32(bytes, text + 4U, 152U);
  put_name(bytes, text + 8U, "__TEXT");
  put_little_u64(bytes, text + 24U, 0x100000000ULL);
  put_little_u64(bytes, text + 32U, 0x1000U);
  put_little_u64(bytes, text + 40U, 0U);
  put_little_u64(bytes, text + 48U, 0x400U);
  put_little_u32(bytes, text + 56U, 7U);
  put_little_u32(bytes, text + 60U, 5U);
  put_little_u32(bytes, text + 64U, 1U);

  constexpr std::size_t section = text + 72U;
  put_name(bytes, section, "__text");
  put_name(bytes, section + 16U, "__TEXT");
  put_little_u64(bytes, section + 32U, 0x100000200ULL);
  put_little_u64(bytes, section + 40U, 4U);
  put_little_u32(bytes, section + 48U, 0x200U);

  constexpr std::size_t linkedit = text + 152U;
  put_little_u32(bytes, linkedit, 0x19U);
  put_little_u32(bytes, linkedit + 4U, 72U);
  put_name(bytes, linkedit + 8U, "__LINKEDIT");
  put_little_u64(bytes, linkedit + 24U, 0x100001000ULL);
  put_little_u64(bytes, linkedit + 32U, 0x1000U);
  put_little_u64(bytes, linkedit + 40U, 0x400U);
  put_little_u64(bytes, linkedit + 48U, 0x400U);
  put_little_u32(bytes, linkedit + 56U, 1U);
  put_little_u32(bytes, linkedit + 60U, 1U);
  return bytes;
}

std::vector<std::uint8_t> macho_with_entitlements() {
  const std::string xml =
      R"(<?xml version="1.0" encoding="UTF-8"?>
<plist version="1.0"><dict><key>application-identifier</key><string>TEAM.test</string></dict></plist>)";
  constexpr std::size_t signature_offset = 0x200U;
  const std::size_t blob_length = 8U + xml.size();
  const std::size_t signature_length = 20U + blob_length;
  std::vector<std::uint8_t> bytes(signature_offset + signature_length, 0U);

  put_little_u32(bytes, 0U, 0xfeedfacfU);
  put_little_u32(bytes, 4U, 0x0100000cU);
  put_little_u32(bytes, 12U, 2U);
  put_little_u32(bytes, 16U, 1U);
  put_little_u32(bytes, 20U, 16U);
  put_little_u32(bytes, 32U, 0x1dU);
  put_little_u32(bytes, 36U, 16U);
  put_little_u32(bytes, 40U, static_cast<std::uint32_t>(signature_offset));
  put_little_u32(bytes, 44U, static_cast<std::uint32_t>(signature_length));

  put_big_u32(bytes, signature_offset, 0xfade0cc0U);
  put_big_u32(bytes, signature_offset + 4U, static_cast<std::uint32_t>(signature_length));
  put_big_u32(bytes, signature_offset + 8U, 1U);
  put_big_u32(bytes, signature_offset + 12U, 5U);
  put_big_u32(bytes, signature_offset + 16U, 20U);
  put_big_u32(bytes, signature_offset + 20U, 0xfade7171U);
  put_big_u32(bytes, signature_offset + 24U, static_cast<std::uint32_t>(blob_length));
  std::copy(xml.begin(), xml.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(signature_offset + 28U));
  return bytes;
}

std::filesystem::path ldid_from_environment() {
  std::vector<wchar_t> buffer(32768U);
  const DWORD length = GetEnvironmentVariableW(L"CYAN_TEST_LDID", buffer.data(),
                                               static_cast<DWORD>(buffer.size()));
  if (length == 0U || static_cast<std::size_t>(length) >= buffer.size()) {
    return {};
  }
  return std::filesystem::path(std::wstring(buffer.data(), length));
}

}  // namespace

TEST_CASE("ldid extracts embedded XML entitlements") {
  const auto signer = ldid_from_environment();
  if (signer.empty()) {
    SKIP("CYAN_TEST_LDID is not configured");
  }

  auto workspace = cyan::TemporaryWorkspace::create();
  REQUIRE(workspace);
  const auto executable = workspace.value().path() / L"signed";
  write_bytes(executable, macho_with_entitlements());

  cyan::ExternalLdidSigningBackend backend(signer);
  cyan::PlistDocument entitlements;
  auto extracted = backend.extractEntitlements(executable, entitlements);
  if (!extracted) {
    workspace.value().preserve();
  }
  const std::string extraction_error =
      extracted ? std::string{} : extracted.error().message;
  INFO(extraction_error);
  INFO(executable.string());
  REQUIRE(extracted);
  CHECK(entitlements.string("application-identifier") == "TEAM.test");
}

TEST_CASE("Procursus ldid creates an ad-hoc Mach-O signature") {
  const auto signer = ldid_from_environment();
  if (signer.empty()) {
    SKIP("CYAN_TEST_LDID is not configured");
  }

  auto workspace = cyan::TemporaryWorkspace::create();
  REQUIRE(workspace);
  const auto executable = workspace.value().path() / L"signing-fixture";
  write_bytes(executable, unsigned_macho());

  auto document = cyan::PlistDocument::create_dictionary();
  REQUIRE(document);
  REQUIRE(document.value().set_string("application-identifier", "TEAM.fixture"));
  std::optional<cyan::PlistDocument> entitlements;
  entitlements.emplace(document.take_value());

  cyan::ExternalLdidSigningBackend backend(signer);
  auto signed_result = backend.signAdHoc(executable, entitlements);
  if (!signed_result) {
    workspace.value().preserve();
  }
  const std::string signing_error = signed_result ? std::string{} : signed_result.error().message;
  INFO(signing_error);
  INFO(executable.string());
  REQUIRE(signed_result);

  cyan::MachOInspector inspector;
  auto inspected = inspector.inspect(executable);
  REQUIRE(inspected);
  REQUIRE(inspected.value().slices.size() == 1U);
  CHECK(inspected.value().slices.front().has_code_signature);

  cyan::PlistDocument extracted;
  auto extracted_result = backend.extractEntitlements(executable, extracted);
  if (!extracted_result) {
    workspace.value().preserve();
  }
  const std::string extraction_error =
      extracted_result ? std::string{} : extracted_result.error().message;
  INFO(extraction_error);
  INFO(executable.string());
  REQUIRE(extracted_result);
  CHECK(extracted.string("application-identifier") == "TEAM.fixture");
}
