#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cyan/core/options.hpp"
#include "cyan/core/result.hpp"

namespace cyan {

Result<CyanOptions> parse_cyan_arguments(const std::vector<std::wstring>& arguments);
Result<CgenOptions> parse_cgen_arguments(const std::vector<std::wstring>& arguments);

std::wstring_view cyan_help_text();
std::wstring_view cgen_help_text();
std::wstring_view cyan_version_text();

}  // namespace cyan
