#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cyan/core/result.hpp"

namespace cyan::signing_internal {

Result<void> run_tool(const std::filesystem::path& executable,
                      const std::vector<std::wstring>& arguments,
                      const std::optional<std::filesystem::path>& standard_output,
                      std::string_view tool_name);

}  // namespace cyan::signing_internal
