#pragma once

#include <filesystem>

#include "cyan/core/result.hpp"
#include "cyan/plist/plist_document.hpp"

namespace cyan {

class IconProcessor {
 public:
  Result<void> replace_icon(const std::filesystem::path& source,
                            const std::filesystem::path& app_bundle,
                            PlistDocument& info_plist) const;
};

}  // namespace cyan
