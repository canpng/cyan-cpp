#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "cyan/core/dependency_resolver.hpp"
#include "cyan/core/result.hpp"
#include "cyan/macho/insert_dylib_engine.hpp"

namespace cyan {

class LiefMachOBackend {
 public:
  InjectionResult inject(const std::filesystem::path& binary, std::string_view dylib_path,
                         const InjectionOptions& options) const;

  Result<std::vector<CommonDependency>> repair_dependencies(
      const std::filesystem::path& binary, const std::vector<std::string>& available_items,
      bool add_framework_rpath) const;

  Result<void> thin_to_arm64(const std::filesystem::path& binary) const;
  Result<void> remove_signature(const std::filesystem::path& binary) const;
};

}  // namespace cyan
