#include "cyan/core/app_bundle.hpp"

#include <algorithm>
#include <array>
#include <cwctype>
#include <system_error>

#include "cyan/macho/macho_inspector.hpp"
#include "cyan/platform/utf.hpp"

namespace cyan {
namespace {

bool extension_equals(const std::filesystem::path& path, std::wstring_view expected) {
  std::wstring extension = path.extension().native();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
  return extension == expected;
}

Result<std::filesystem::path> bundle_executable(const std::filesystem::path& bundle) {
  auto plist = PlistDocument::load(bundle / L"Info.plist");
  if (!plist) {
    return Result<std::filesystem::path>::failure(plist.error());
  }
  const auto executable_name = plist.value().string("CFBundleExecutable");
  if (!executable_name || executable_name->empty()) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::invalid_input_type, "bundle has no CFBundleExecutable", bundle});
  }
  auto decoded = platform::wide_from_utf8(*executable_name);
  if (!decoded) {
    return Result<std::filesystem::path>::failure(decoded.error());
  }
  const std::filesystem::path relative(decoded.value());
  if (relative.has_parent_path() || relative.is_absolute()) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::invalid_input_type, "CFBundleExecutable must be a file name", bundle});
  }
  const auto executable = bundle / relative;
  std::error_code error;
  if (!std::filesystem::is_regular_file(executable, error) || error) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::file_not_found, "bundle executable does not exist", executable});
  }
  return Result<std::filesystem::path>::success(executable);
}

Result<bool> remove_if_present(const std::filesystem::path& path) {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (error == std::errc::no_such_file_or_directory ||
      (!error && !std::filesystem::exists(status))) {
    return Result<bool>::success(false);
  }
  if (error) {
    return Result<bool>::failure(
        {ErrorCode::filesystem_error, "could not inspect bundle path", path});
  }
  std::filesystem::remove_all(path, error);
  if (error) {
    return Result<bool>::failure(
        {ErrorCode::filesystem_error, "could not remove bundle path", path});
  }
  return Result<bool>::success(true);
}

std::string replace_all(std::string value, std::string_view original,
                        std::string_view replacement) {
  if (original.empty()) {
    return value;
  }
  std::size_t offset = 0;
  while ((offset = value.find(original, offset)) != std::string::npos) {
    value.replace(offset, original.size(), replacement);
    offset += replacement.size();
  }
  return value;
}

}  // namespace

AppBundle::AppBundle(std::filesystem::path path, std::filesystem::path executable,
                     PlistDocument plist)
    : path_(std::move(path)), executable_(std::move(executable)), plist_(std::move(plist)) {}

Result<AppBundle> AppBundle::open(const std::filesystem::path& path) {
  auto plist = PlistDocument::load(path / L"Info.plist");
  if (!plist) {
    return Result<AppBundle>::failure(plist.error());
  }
  auto executable = bundle_executable(path);
  if (!executable) {
    return Result<AppBundle>::failure(executable.error());
  }
  return Result<AppBundle>::success(AppBundle(path, executable.take_value(), plist.take_value()));
}

const std::filesystem::path& AppBundle::path() const noexcept { return path_; }

const std::filesystem::path& AppBundle::executable() const noexcept { return executable_; }

PlistDocument& AppBundle::info_plist() noexcept { return plist_; }

Result<void> AppBundle::save() { return plist_.save(path_ / L"Info.plist", PlistFormat::xml); }

Result<void> AppBundle::change_name(std::string_view value) {
  auto first = plist_.set_string("CFBundleName", value);
  if (!first) {
    return first;
  }
  auto second = plist_.set_string("CFBundleDisplayName", value);
  if (!second) {
    return second;
  }
  auto saved = save();
  if (!saved) {
    return saved;
  }

  std::error_code error;
  std::filesystem::directory_iterator iterator(path_, error);
  const std::filesystem::directory_iterator end;
  if (error) {
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not enumerate localized resources", path_});
  }
  for (; iterator != end; iterator.increment(error)) {
    if (error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not enumerate localized resources", path_});
    }
    if (!iterator->is_directory(error) || !extension_equals(iterator->path(), L".lproj")) {
      continue;
    }
    const auto localized_path = iterator->path() / L"InfoPlist.strings";
    if (!std::filesystem::is_regular_file(localized_path, error)) {
      error.clear();
      continue;
    }
    auto localized = PlistDocument::load(localized_path);
    if (!localized) {
      continue;
    }
    static_cast<void>(localized.value().set_string("CFBundleName", value));
    static_cast<void>(localized.value().set_string("CFBundleDisplayName", value));
    auto localized_saved = localized.value().save(localized_path);
    if (!localized_saved) {
      return localized_saved;
    }
  }
  return Result<void>::success();
}

Result<void> AppBundle::change_version(std::string_view value) {
  auto bundle = plist_.set_string("CFBundleVersion", value);
  if (!bundle) {
    return bundle;
  }
  auto short_version = plist_.set_string("CFBundleShortVersionString", value);
  if (!short_version) {
    return short_version;
  }
  return save();
}

Result<void> AppBundle::change_bundle_id(std::string_view value) {
  const auto original = plist_.string("CFBundleIdentifier");
  auto changed = plist_.set_string("CFBundleIdentifier", value);
  if (!changed) {
    return changed;
  }
  auto saved = save();
  if (!saved || !original || original->empty()) {
    return saved;
  }

  for (const auto& directory_name : {L"PlugIns", L"Extensions"}) {
    const auto directory = path_ / directory_name;
    std::error_code error;
    std::filesystem::directory_iterator iterator(directory, error);
    const std::filesystem::directory_iterator end;
    if (error == std::errc::no_such_file_or_directory) {
      continue;
    }
    if (error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not enumerate app extensions", directory});
    }
    for (; iterator != end; iterator.increment(error)) {
      if (error) {
        return Result<void>::failure(
            {ErrorCode::filesystem_error, "could not enumerate app extensions", directory});
      }
      if (!iterator->is_directory(error) || !extension_equals(iterator->path(), L".appex")) {
        continue;
      }
      const auto extension_plist_path = iterator->path() / L"Info.plist";
      auto extension_plist = PlistDocument::load(extension_plist_path);
      if (!extension_plist) {
        continue;
      }
      const auto extension_identifier = extension_plist.value().string("CFBundleIdentifier");
      if (!extension_identifier) {
        continue;
      }
      static_cast<void>(extension_plist.value().set_string(
          "CFBundleIdentifier", replace_all(*extension_identifier, *original, value)));
      auto extension_saved = extension_plist.value().save(extension_plist_path);
      if (!extension_saved) {
        return extension_saved;
      }
    }
  }
  return Result<void>::success();
}

Result<void> AppBundle::change_minimum_version(std::string_view value) {
  auto changed = plist_.set_string("MinimumOSVersion", value);
  return changed ? save() : changed;
}

Result<void> AppBundle::merge_plist(const std::filesystem::path& overlay) {
  auto loaded = PlistDocument::load(overlay);
  if (!loaded) {
    return Result<void>::failure(loaded.error());
  }
  auto merged = plist_.merge(loaded.value());
  return merged ? save() : merged;
}

Result<void> AppBundle::remove_supported_devices() {
  auto removed = plist_.remove("UISupportedDevices");
  return removed ? save() : removed;
}

Result<void> AppBundle::enable_documents() {
  auto browser = plist_.set_boolean("UISupportsDocumentBrowser", true);
  if (!browser) {
    return browser;
  }
  auto sharing = plist_.set_boolean("UIFileSharingEnabled", true);
  return sharing ? save() : sharing;
}

Result<bool> AppBundle::remove_watch_content() {
  bool removed = false;
  for (const auto& name : {L"Watch", L"WatchKit", L"com.apple.WatchPlaceholder"}) {
    auto one = remove_if_present(path_ / name);
    if (!one) {
      return one;
    }
    removed = one.value() || removed;
  }
  return Result<bool>::success(removed);
}

Result<bool> AppBundle::remove_all_extensions() {
  bool removed = false;
  for (const auto& name : {L"Extensions", L"PlugIns"}) {
    auto one = remove_if_present(path_ / name);
    if (!one) {
      return one;
    }
    removed = one.value() || removed;
  }
  return Result<bool>::success(removed);
}

Result<std::vector<std::filesystem::path>> AppBundle::remove_encrypted_extensions(
    const MachOInspector& inspector) {
  std::vector<std::filesystem::path> removed;
  for (const auto& directory_name : {L"PlugIns", L"Extensions"}) {
    const auto directory = path_ / directory_name;
    std::error_code error;
    std::filesystem::directory_iterator iterator(directory, error);
    const std::filesystem::directory_iterator end;
    if (error == std::errc::no_such_file_or_directory) {
      continue;
    }
    if (error) {
      return Result<std::vector<std::filesystem::path>>::failure(
          {ErrorCode::filesystem_error, "could not enumerate app extensions", directory});
    }
    for (; iterator != end; iterator.increment(error)) {
      if (error) {
        return Result<std::vector<std::filesystem::path>>::failure(
            {ErrorCode::filesystem_error, "could not enumerate app extensions", directory});
      }
      if (!iterator->is_directory(error) || !extension_equals(iterator->path(), L".appex")) {
        continue;
      }
      auto executable = bundle_executable(iterator->path());
      if (!executable) {
        continue;
      }
      auto info = inspector.inspect(executable.value());
      if (!info) {
        continue;
      }
      const bool encrypted =
          std::any_of(info.value().slices.begin(), info.value().slices.end(),
                      [](const MachOSliceInfo& slice) { return slice.encrypted; });
      if (!encrypted) {
        continue;
      }
      const auto path = iterator->path();
      std::filesystem::remove_all(path, error);
      if (error) {
        return Result<std::vector<std::filesystem::path>>::failure(
            {ErrorCode::filesystem_error, "could not remove encrypted app extension", path});
      }
      removed.push_back(path.filename());
    }
  }
  return Result<std::vector<std::filesystem::path>>::success(std::move(removed));
}

Result<std::vector<std::filesystem::path>> AppBundle::discover_executables() const {
  std::vector<std::filesystem::path> executables{executable_};
  std::error_code error;
  std::filesystem::recursive_directory_iterator iterator(
      path_, std::filesystem::directory_options::none, error);
  const std::filesystem::recursive_directory_iterator end;
  if (error) {
    return Result<std::vector<std::filesystem::path>>::failure(
        {ErrorCode::filesystem_error, "could not enumerate bundle executables", path_});
  }
  for (; iterator != end; iterator.increment(error)) {
    if (error) {
      return Result<std::vector<std::filesystem::path>>::failure(
          {ErrorCode::filesystem_error, "could not enumerate bundle executables", path_});
    }
    const auto candidate = iterator->path();
    if (iterator->is_regular_file(error) && extension_equals(candidate, L".dylib")) {
      executables.push_back(candidate);
      continue;
    }
    if (!iterator->is_directory(error) ||
        (!extension_equals(candidate, L".appex") && !extension_equals(candidate, L".framework"))) {
      continue;
    }
    auto nested = bundle_executable(candidate);
    if (nested) {
      executables.push_back(nested.take_value());
    }
  }
  return Result<std::vector<std::filesystem::path>>::success(std::move(executables));
}

}  // namespace cyan
