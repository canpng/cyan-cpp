#pragma once

#include <string>
#include <string_view>

#include "cyan/core/result.hpp"

namespace cyan::platform {

Result<std::string> utf8_from_wide(std::wstring_view value);
Result<std::wstring> wide_from_utf8(std::string_view value);
std::wstring invariant_lower(std::wstring_view value);

}  // namespace cyan::platform
