#pragma once

#include <functional>
#include <string_view>

#include "cyan/core/options.hpp"
#include "cyan/core/result.hpp"

namespace cyan {

using PipelineLogger = std::function<void(std::wstring_view)>;

class CyanPipeline {
 public:
  Result<void> run(CyanOptions options, const PipelineLogger& logger = {}) const;
};

}  // namespace cyan
