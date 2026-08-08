#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cyan {

struct CyanOptions {
  std::filesystem::path input;
  std::filesystem::path output;
  std::vector<std::filesystem::path> cyan_files;
  std::vector<std::filesystem::path> injected_items;
  // Items copied beside the application bundle at <package>/Payload/<name>.
  // This is intentionally distinct from injected_items, whose generic items
  // are installed inside the .app bundle.
  std::vector<std::filesystem::path> payload_root_items;
  std::optional<std::wstring> name;
  std::optional<std::wstring> version;
  std::optional<std::wstring> bundle_id;
  std::optional<std::wstring> minimum_os;
  std::optional<std::filesystem::path> icon;
  std::optional<std::filesystem::path> merge_plist;
  std::optional<std::filesystem::path> entitlements;
  std::optional<std::filesystem::path> dependency_directory;
  std::optional<std::filesystem::path> ldid_path;
  std::optional<std::filesystem::path> ipapatch_dylib;
  bool remove_supported_devices{false};
  bool no_watch{false};
  bool enable_documents{false};
  bool fakesign{false};
  bool thin{false};
  bool remove_extensions{false};
  bool remove_encrypted{false};
  bool ignore_encrypted{false};
  bool overwrite{false};
  bool compatibility_cyan{false};
  bool ipapatch{false};
  bool ipapatch_plugins_only{false};
  bool show_help{false};
  bool show_version{false};
  int compression_level{6};
};

struct CgenOptions {
  std::filesystem::path output;
  std::vector<std::filesystem::path> injected_items;
  std::optional<std::wstring> name;
  std::optional<std::wstring> version;
  std::optional<std::wstring> bundle_id;
  std::optional<std::wstring> minimum_os;
  std::optional<std::filesystem::path> icon;
  std::optional<std::filesystem::path> merge_plist;
  std::optional<std::filesystem::path> entitlements;
  bool remove_supported_devices{false};
  bool no_watch{false};
  bool enable_documents{false};
  bool fakesign{false};
  bool thin{false};
  bool remove_extensions{false};
  bool remove_encrypted{false};
  bool overwrite{false};
  bool show_help{false};
  bool show_version{false};
};

}  // namespace cyan
