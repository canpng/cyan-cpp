#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cyan/core/result.hpp"

namespace cyan {

struct IpaPatchCliOptions {
  std::filesystem::path input;
  std::filesystem::path output;
  std::optional<std::filesystem::path> dylib;
  std::optional<std::filesystem::path> ldid_path;
  bool inplace{false};
  bool noconfirm{false};
  bool plugins_only{false};
  bool show_help{false};
  bool show_version{false};
};

Result<IpaPatchCliOptions> parse_ipapatch_arguments(
    const std::vector<std::wstring>& arguments);

std::wstring_view ipapatch_help_text();
std::wstring_view ipapatch_version_text();

}  // namespace cyan
