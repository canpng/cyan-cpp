#include "cyan/macho/code_signature_inspector.hpp"

#include <fstream>
#include <limits>
#include <span>

#include "macho_internal.hpp"

namespace cyan {
namespace {

constexpr std::uint32_t embedded_signature_magic = 0xfade0cc0U;
constexpr std::uint32_t code_directory_magic = 0xfade0c02U;
constexpr std::uint32_t xml_entitlements_slot = 5U;
constexpr std::uint32_t der_entitlements_slot = 7U;

std::uint32_t read_be32(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
         (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
         (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
         static_cast<std::uint32_t>(bytes[offset + 3U]);
}

bool range_valid(std::size_t offset, std::size_t length, std::size_t limit) {
  return offset <= limit && length <= limit - offset;
}

Result<std::vector<std::uint8_t>> read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::file_not_found, "could not open Mach-O for signature inspection", path});
  }
  const std::streamoff length = input.tellg();
  if (length < 0 ||
      static_cast<unsigned long long>(length) >
          static_cast<unsigned long long>((std::numeric_limits<std::size_t>::max)())) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::filesystem_error, "could not determine Mach-O size", path});
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
  input.seekg(0);
  if (!bytes.empty()) {
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  if (!input) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::filesystem_error, "could not read Mach-O signature", path});
  }
  return Result<std::vector<std::uint8_t>>::success(std::move(bytes));
}

Result<std::string> read_c_string(std::span<const std::uint8_t> blob, std::size_t offset,
                                  const std::filesystem::path& path) {
  if (offset >= blob.size()) {
    return Result<std::string>::failure(
        {ErrorCode::macho_invalid, "code-signature string offset is out of range", path});
  }
  std::size_t end = offset;
  while (end < blob.size() && blob[end] != 0U) {
    ++end;
  }
  if (end == blob.size()) {
    return Result<std::string>::failure(
        {ErrorCode::macho_invalid, "code-signature string is not terminated", path});
  }
  return Result<std::string>::success(
      std::string(reinterpret_cast<const char*>(blob.data() + offset), end - offset));
}

Result<CodeSignatureMetadata> inspect_signature(
    std::span<const std::uint8_t> file, const macho_internal::Slice& slice,
    const std::filesystem::path& path) {
  CodeSignatureMetadata metadata;
  if (!slice.signature) {
    return Result<CodeSignatureMetadata>::success(std::move(metadata));
  }
  metadata.present = true;

  const std::size_t signature_offset =
      slice.file_offset + static_cast<std::size_t>(slice.signature->data_offset);
  const std::size_t signature_size = static_cast<std::size_t>(slice.signature->data_size);
  if (!range_valid(signature_offset, signature_size, file.size()) || signature_size < 12U) {
    return Result<CodeSignatureMetadata>::failure(
        {ErrorCode::macho_invalid, "embedded code signature is out of range", path});
  }
  const auto signature = file.subspan(signature_offset, signature_size);
  if (read_be32(signature, 0U) != embedded_signature_magic) {
    return Result<CodeSignatureMetadata>::failure(
        {ErrorCode::macho_invalid, "unsupported embedded code-signature container", path});
  }
  const std::size_t declared_length = read_be32(signature, 4U);
  const std::size_t count = read_be32(signature, 8U);
  if (declared_length > signature.size() || declared_length < 12U ||
      count > (declared_length - 12U) / 8U) {
    return Result<CodeSignatureMetadata>::failure(
        {ErrorCode::macho_invalid, "invalid embedded code-signature index", path});
  }
  const auto container = signature.first(declared_length);

  bool found_directory = false;
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t entry = 12U + index * 8U;
    const std::uint32_t type = read_be32(container, entry);
    const std::size_t offset = read_be32(container, entry + 4U);
    if (!range_valid(offset, 8U, container.size())) {
      return Result<CodeSignatureMetadata>::failure(
          {ErrorCode::macho_invalid, "code-signature blob offset is out of range", path});
    }
    const std::size_t length = read_be32(container, offset + 4U);
    if (length < 8U || !range_valid(offset, length, container.size())) {
      return Result<CodeSignatureMetadata>::failure(
          {ErrorCode::macho_invalid, "code-signature blob length is out of range", path});
    }
    const auto blob = container.subspan(offset, length);

    if (type == 0U && !found_directory) {
      if (read_be32(blob, 0U) != code_directory_magic || blob.size() < 44U) {
        return Result<CodeSignatureMetadata>::failure(
            {ErrorCode::macho_invalid, "invalid CodeDirectory blob", path});
      }
      const std::uint32_t version = read_be32(blob, 8U);
      metadata.flags = read_be32(blob, 12U);
      const std::size_t identifier_offset = read_be32(blob, 20U);
      auto identifier = read_c_string(blob, identifier_offset, path);
      if (!identifier) {
        return Result<CodeSignatureMetadata>::failure(identifier.error());
      }
      metadata.identifier = identifier.take_value();
      metadata.platform = blob[38U];
      if (version >= 0x20200U) {
        if (blob.size() < 52U) {
          return Result<CodeSignatureMetadata>::failure(
              {ErrorCode::macho_invalid, "truncated CodeDirectory team field", path});
        }
        const std::size_t team_offset = read_be32(blob, 48U);
        if (team_offset != 0U) {
          auto team = read_c_string(blob, team_offset, path);
          if (!team) {
            return Result<CodeSignatureMetadata>::failure(team.error());
          }
          metadata.team_identifier = team.take_value();
        }
      }
      found_directory = true;
    } else if (type == xml_entitlements_slot) {
      metadata.xml_entitlements.assign(blob.begin() + 8, blob.end());
    } else if (type == der_entitlements_slot) {
      metadata.der_entitlements.assign(blob.begin() + 8, blob.end());
    }
  }

  if (!found_directory) {
    return Result<CodeSignatureMetadata>::failure(
        {ErrorCode::macho_unsupported, "signed Mach-O has no primary CodeDirectory", path});
  }
  return Result<CodeSignatureMetadata>::success(std::move(metadata));
}

}  // namespace

Result<std::vector<CodeSignatureMetadata>> CodeSignatureInspector::inspect(
    const std::filesystem::path& executable) const {
  auto bytes = read_file(executable);
  if (!bytes) {
    return Result<std::vector<CodeSignatureMetadata>>::failure(bytes.error());
  }
  auto container = macho_internal::parse_container(bytes.value());
  if (!container) {
    return Result<std::vector<CodeSignatureMetadata>>::failure(
        {ErrorCode::macho_invalid, container.failure.message, executable});
  }

  std::vector<CodeSignatureMetadata> result;
  result.reserve(container.value->slices.size());
  for (const auto& slice : container.value->slices) {
    auto metadata = inspect_signature(bytes.value(), slice, executable);
    if (!metadata) {
      return Result<std::vector<CodeSignatureMetadata>>::failure(metadata.error());
    }
    result.push_back(metadata.take_value());
  }
  return Result<std::vector<CodeSignatureMetadata>>::success(std::move(result));
}

}  // namespace cyan
