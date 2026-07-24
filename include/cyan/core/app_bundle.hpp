#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "cyan/core/result.hpp"
#include "cyan/plist/plist_document.hpp"

namespace cyan {

class MachOInspector;

class AppBundle {
 public:
  static Result<AppBundle> open(const std::filesystem::path& path);

  [[nodiscard]] const std::filesystem::path& path() const noexcept;
  [[nodiscard]] const std::filesystem::path& executable() const noexcept;
  [[nodiscard]] PlistDocument& info_plist() noexcept;

  Result<void> change_name(std::string_view value);
  Result<void> change_version(std::string_view value);
  Result<void> change_bundle_id(std::string_view value);
  Result<void> change_minimum_version(std::string_view value);
  Result<void> merge_plist(const std::filesystem::path& overlay);
  Result<void> remove_supported_devices();
  Result<void> enable_documents();

  Result<bool> remove_watch_content();
  Result<bool> remove_all_extensions();
  Result<std::vector<std::filesystem::path>> remove_encrypted_extensions(
      const MachOInspector& inspector);

  Result<std::vector<std::filesystem::path>> discover_executables() const;

 private:
  AppBundle(std::filesystem::path path, std::filesystem::path executable, PlistDocument plist);

  Result<void> save();

  std::filesystem::path path_;
  std::filesystem::path executable_;
  PlistDocument plist_;
};

}  // namespace cyan
