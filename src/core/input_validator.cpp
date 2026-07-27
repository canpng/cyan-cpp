#include "cyan/core/input_validator.hpp"

#include <algorithm>
#include <cwctype>
#include <system_error>

namespace cyan {
namespace {

std::wstring lower_extension(const std::filesystem::path& path) {
  std::wstring extension = path.extension().native();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
  return extension;
}

Result<void> require_exists(const std::filesystem::path& path, bool regular_file_only) {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (error || !std::filesystem::exists(status)) {
    return Result<void>::failure({ErrorCode::file_not_found, "path does not exist", path});
  }
  if (regular_file_only && !std::filesystem::is_regular_file(status)) {
    return Result<void>::failure(
        {ErrorCode::invalid_argument, "path must be a regular file", path});
  }
  return Result<void>::success();
}

Result<void> validate_minimum_os(const std::optional<std::wstring>& minimum) {
  if (!minimum) {
    return Result<void>::success();
  }
  for (const wchar_t character : *minimum) {
    if ((character < L'0' || character > L'9') && character != L'.') {
      return Result<void>::failure(
          {ErrorCode::invalid_version, "minimum OS version may contain only digits and dots", {}});
    }
  }
  return Result<void>::success();
}

Result<void> validate_optional_file(const std::optional<std::filesystem::path>& path) {
  if (!path) {
    return Result<void>::success();
  }
  return require_exists(*path, true);
}

}  // namespace

Result<void> validate_and_normalize(CyanOptions& options) {
  const std::wstring input_extension = lower_extension(options.input);
  if (input_extension != L".ipa" && input_extension != L".tipa" && input_extension != L".app") {
    return Result<void>::failure(
        {ErrorCode::invalid_input_type, "the input must be an ipa, tipa, or app", options.input});
  }

  auto input_exists = require_exists(options.input, input_extension != L".app");
  if (!input_exists) {
    return input_exists;
  }

  if (input_extension == L".app") {
    std::error_code error;
    if (!std::filesystem::is_directory(options.input, error) || error) {
      return Result<void>::failure(
          {ErrorCode::path_not_directory, "an .app input must be a directory", options.input});
    }
    const auto info_plist = options.input / L"Info.plist";
    auto plist_exists = require_exists(info_plist, true);
    if (!plist_exists) {
      return Result<void>::failure(
          {ErrorCode::invalid_input_type, "no Info.plist; invalid app", options.input});
    }
  }

  if (options.output.empty()) {
    options.output = options.input;
  } else {
    const std::wstring output_extension = lower_extension(options.output);
    if (output_extension != L".ipa" && output_extension != L".tipa" &&
        output_extension != L".app") {
      options.output += L".ipa";
    }
  }

  auto minimum = validate_minimum_os(options.minimum_os);
  if (!minimum) {
    return minimum;
  }

  for (const auto& path : options.injected_items) {
    auto exists = require_exists(path, false);
    if (!exists) {
      return exists;
    }
  }
  for (const auto& path : options.cyan_files) {
    auto exists = require_exists(path, true);
    if (!exists) {
      return exists;
    }
  }

  for (const auto& optional :
       {options.icon, options.merge_plist, options.entitlements, options.ldid_path,
        options.ipapatch_dylib}) {
    auto valid = validate_optional_file(optional);
    if (!valid) {
      return valid;
    }
  }

  if (options.dependency_directory) {
    std::error_code error;
    if (!std::filesystem::is_directory(*options.dependency_directory, error) || error) {
      return Result<void>::failure({ErrorCode::path_not_directory,
                                    "dependency directory does not exist or is not a directory",
                                    *options.dependency_directory});
    }
  }

  return Result<void>::success();
}

Result<void> validate_and_normalize(CgenOptions& options) {
  auto minimum = validate_minimum_os(options.minimum_os);
  if (!minimum) {
    return minimum;
  }

  for (const auto& path : options.injected_items) {
    auto exists = require_exists(path, false);
    if (!exists) {
      return exists;
    }
  }
  for (const auto& optional : {options.icon, options.merge_plist, options.entitlements}) {
    auto valid = validate_optional_file(optional);
    if (!valid) {
      return valid;
    }
  }

  if (lower_extension(options.output) != L".cyan") {
    options.output += L".cyan";
  }
  return Result<void>::success();
}

}  // namespace cyan
