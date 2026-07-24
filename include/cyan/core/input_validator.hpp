#pragma once

#include "cyan/core/options.hpp"
#include "cyan/core/result.hpp"

namespace cyan {

Result<void> validate_and_normalize(CyanOptions& options);
Result<void> validate_and_normalize(CgenOptions& options);

}  // namespace cyan
