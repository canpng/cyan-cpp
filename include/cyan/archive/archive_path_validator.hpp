#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>

#include "cyan/core/result.hpp"

namespace cyan {

class ArchivePathValidator {
 public:
  Result<std::filesystem::path> validate_and_reserve(std::string_view archive_name);
  void clear();

 private:
  std::unordered_set<std::wstring> reserved_paths_;
};

}  // namespace cyan
