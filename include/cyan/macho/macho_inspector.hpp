#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "cyan/core/result.hpp"

namespace cyan {

enum class ByteOrder { little, big };

struct MachOSliceInfo {
  std::int32_t cpu_type{0};
  std::int32_t cpu_subtype{0};
  bool is_64_bit{false};
  ByteOrder byte_order{ByteOrder::little};
  bool encrypted{false};
  bool has_code_signature{false};
  std::vector<std::string> dependencies;
  std::vector<std::string> rpaths;
};

struct MachOInfo {
  bool is_fat{false};
  bool is_fat64{false};
  std::vector<MachOSliceInfo> slices;
};

class MachOInspector {
 public:
  Result<MachOInfo> inspect(const std::filesystem::path& binary) const;
  Result<MachOInfo> inspect(std::span<const std::uint8_t> bytes) const;

  Result<void> verify_dependency(std::span<const std::uint8_t> bytes, std::string_view dependency,
                                 bool require_signature_removed) const;
};

}  // namespace cyan
