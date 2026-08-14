#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "cyan/core/result.hpp"

namespace cyan {

struct AppMetadata {
  std::string app_name;
  std::string version;
  std::string bundle_identifier;
  std::string minimum_os;
  std::string bundle_path;
  std::string icon_name;
  std::vector<std::uint8_t> icon_data;
};

class AppMetadataReader {
 public:
  [[nodiscard]] Result<AppMetadata> read(const std::filesystem::path& input) const;
};

}  // namespace cyan
