#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "cyan/core/result.hpp"

namespace cyan {

struct CodeSignatureMetadata {
  bool present{false};
  std::string identifier;
  std::string team_identifier;
  std::uint32_t flags{0};
  std::uint8_t platform{0};
  std::vector<std::uint8_t> xml_entitlements;
  std::vector<std::uint8_t> der_entitlements;

  bool operator==(const CodeSignatureMetadata&) const = default;
};

class CodeSignatureInspector {
 public:
  Result<std::vector<CodeSignatureMetadata>> inspect(
      const std::filesystem::path& executable) const;
};

}  // namespace cyan
