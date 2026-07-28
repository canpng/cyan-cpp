#include "cyan/core/cyan_pipeline.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

#include "cyan/archive/archive_service.hpp"
#include "cyan/archive/cyan_archive.hpp"
#include "cyan/core/app_bundle.hpp"
#include "cyan/core/input_validator.hpp"
#include "cyan/core/temporary_workspace.hpp"
#include "cyan/image/icon_processor.hpp"
#include "cyan/ipapatch/ipapatch_service.hpp"
#include "cyan/macho/insert_dylib_engine.hpp"
#include "cyan/macho/lief_macho_backend.hpp"
#include "cyan/macho/macho_inspector.hpp"
#include "cyan/platform/utf.hpp"
#include "cyan/plist/plist_document.hpp"
#include "cyan/signing/signing_backend.hpp"

namespace cyan {
namespace {

struct NamedItem {
  std::wstring name;
  std::filesystem::path path;
};

void log(const PipelineLogger& logger, std::wstring message) {
  if (logger) {
    logger(message);
  }
}

std::wstring lower(std::wstring value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
  return value;
}

bool extension_equals(const std::filesystem::path& path, std::wstring_view extension) {
  return lower(path.extension().native()) == extension;
}

bool is_reparse_point(const std::filesystem::path& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
}

Result<void> copy_secure(const std::filesystem::path& source,
                         const std::filesystem::path& destination, bool replace) {
  std::error_code error;
  const auto source_status = std::filesystem::symlink_status(source, error);
  if (error || !std::filesystem::exists(source_status)) {
    return Result<void>::failure({ErrorCode::file_not_found, "copy source does not exist", source});
  }
  if (std::filesystem::is_symlink(source_status) || is_reparse_point(source)) {
    return Result<void>::failure(
        {ErrorCode::archive_unsafe_path, "refusing to copy a link or reparse point", source});
  }

  if (replace) {
    std::filesystem::remove_all(destination, error);
    if (error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not replace existing bundle item", destination});
    }
  }

  if (std::filesystem::is_regular_file(source_status)) {
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error ||
        !std::filesystem::copy_file(source, destination,
                                    std::filesystem::copy_options::overwrite_existing, error)) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not copy bundle file", source});
    }
    return Result<void>::success();
  }
  if (!std::filesystem::is_directory(source_status)) {
    return Result<void>::failure(
        {ErrorCode::archive_unsafe_path, "unsupported bundle item type", source});
  }

  std::filesystem::create_directories(destination, error);
  if (error) {
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not create bundle directory", destination});
  }
  std::filesystem::recursive_directory_iterator iterator(
      source, std::filesystem::directory_options::none, error);
  const std::filesystem::recursive_directory_iterator end;
  if (error) {
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not enumerate copy source", source});
  }
  for (; iterator != end; iterator.increment(error)) {
    if (error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not enumerate copy source", source});
    }
    const auto current = iterator->path();
    const auto status = iterator->symlink_status(error);
    if (error || std::filesystem::is_symlink(status) || is_reparse_point(current)) {
      return Result<void>::failure({ErrorCode::archive_unsafe_path,
                                    "copy source contains a link or reparse point", current});
    }
    const auto relative = std::filesystem::relative(current, source, error);
    if (error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not make copy path relative", current});
    }
    const auto target = destination / relative;
    if (std::filesystem::is_directory(status)) {
      std::filesystem::create_directories(target, error);
    } else if (std::filesystem::is_regular_file(status)) {
      std::filesystem::create_directories(target.parent_path(), error);
      if (!error) {
        std::filesystem::copy_file(current, target,
                                   std::filesystem::copy_options::overwrite_existing, error);
      }
    } else {
      return Result<void>::failure(
          {ErrorCode::archive_unsafe_path, "copy source contains a special file", current});
    }
    if (error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not copy bundle item", current});
    }
  }
  return Result<void>::success();
}

Result<std::filesystem::path> find_app(const std::filesystem::path& package_root) {
  const auto payload = package_root / L"Payload";
  std::error_code error;
  if (!std::filesystem::is_directory(payload, error) || error) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::invalid_input_type, "IPA has no Payload directory", payload});
  }
  std::vector<std::filesystem::path> apps;
  std::filesystem::directory_iterator iterator(payload, error);
  const std::filesystem::directory_iterator end;
  if (error) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::invalid_input_type, "could not enumerate IPA Payload", payload});
  }
  for (; iterator != end; iterator.increment(error)) {
    if (error) {
      return Result<std::filesystem::path>::failure(
          {ErrorCode::invalid_input_type, "could not enumerate IPA Payload", payload});
    }
    if (iterator->is_directory(error) && extension_equals(iterator->path(), L".app") &&
        std::filesystem::is_regular_file(iterator->path() / L"Info.plist", error)) {
      apps.push_back(iterator->path());
    }
    error.clear();
  }
  if (apps.empty()) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::invalid_input_type, "IPA Payload has no valid app bundle", payload});
  }
  std::sort(apps.begin(), apps.end());
  return Result<std::filesystem::path>::success(apps.front());
}

void add_or_replace(std::vector<NamedItem>& items, const std::filesystem::path& path) {
  const std::wstring name = path.filename().native();
  const std::wstring folded = lower(name);
  const auto found = std::find_if(items.begin(), items.end(), [&](const NamedItem& item) {
    return lower(item.name) == folded;
  });
  if (found == items.end()) {
    items.push_back({name, path});
  } else {
    found->path = path;
  }
}

std::size_t count_substring(std::wstring_view text, std::wstring_view needle) {
  std::size_t count = 0;
  std::size_t offset = 0;
  while ((offset = text.find(needle, offset)) != std::wstring_view::npos) {
    ++count;
    offset += needle.size();
  }
  return count;
}

Result<std::vector<std::filesystem::path>> extract_deb_items(const std::filesystem::path& deb,
                                                             const std::filesystem::path& root) {
  ArchiveService archive;
  const auto container_root = root / L"container";
  auto outer = archive.extract(deb, container_root);
  if (!outer) {
    return Result<std::vector<std::filesystem::path>>::failure(
        {ErrorCode::archive_invalid, "could not extract DEB container: " + outer.error().message,
         deb});
  }

  std::filesystem::path data_archive;
  std::error_code error;
  std::filesystem::directory_iterator outer_iterator(container_root, error);
  const std::filesystem::directory_iterator outer_end;
  if (error) {
    return Result<std::vector<std::filesystem::path>>::failure(
        {ErrorCode::archive_invalid, "could not enumerate DEB container", deb});
  }
  for (; outer_iterator != outer_end; outer_iterator.increment(error)) {
    if (error) {
      return Result<std::vector<std::filesystem::path>>::failure(
          {ErrorCode::archive_invalid, "could not enumerate DEB container", deb});
    }
    const std::wstring name = lower(outer_iterator->path().filename().native());
    if (outer_iterator->is_regular_file(error) &&
        (name.starts_with(L"data.tar") || name.starts_with(L"data."))) {
      data_archive = outer_iterator->path();
      break;
    }
  }
  if (data_archive.empty()) {
    return Result<std::vector<std::filesystem::path>>::failure(
        {ErrorCode::archive_invalid, "DEB has no data archive", deb});
  }

  const auto data_root = root / L"data";
  auto data = archive.extract(data_archive, data_root);
  if (!data) {
    return Result<std::vector<std::filesystem::path>>::failure(
        {ErrorCode::archive_invalid, "could not extract DEB data archive: " + data.error().message,
         deb});
  }

  std::vector<std::filesystem::path> discovered;
  std::filesystem::recursive_directory_iterator iterator(
      data_root, std::filesystem::directory_options::none, error);
  const std::filesystem::recursive_directory_iterator end;
  if (error) {
    return Result<std::vector<std::filesystem::path>>::failure(
        {ErrorCode::filesystem_error, "could not enumerate extracted DEB", deb});
  }
  for (; iterator != end; iterator.increment(error)) {
    if (error) {
      return Result<std::vector<std::filesystem::path>>::failure(
          {ErrorCode::filesystem_error, "could not enumerate extracted DEB", deb});
    }
    const auto candidate = iterator->path();
    const std::wstring extension = lower(candidate.extension().native());
    const bool file_match = iterator->is_regular_file(error) && extension == L".dylib";
    const bool directory_match =
        iterator->is_directory(error) &&
        (extension == L".appex" || extension == L".bundle" || extension == L".framework");
    if (!file_match && !directory_match) {
      continue;
    }
    const std::wstring relative =
        lower(std::filesystem::relative(candidate, data_root, error).generic_wstring());
    if (error) {
      return Result<std::vector<std::filesystem::path>>::failure(
          {ErrorCode::filesystem_error, "could not normalize extracted DEB path", candidate});
    }
    if (count_substring(relative, L".bundle") > 1U ||
        count_substring(relative, L".framework") > 1U) {
      continue;
    }
    discovered.push_back(candidate);
    if (directory_match) {
      iterator.disable_recursion_pending();
    }
  }
  return Result<std::vector<std::filesystem::path>>::success(std::move(discovered));
}

Result<std::filesystem::path> nested_bundle_executable(const std::filesystem::path& bundle) {
  auto plist = PlistDocument::load(bundle / L"Info.plist");
  if (!plist) {
    return Result<std::filesystem::path>::failure(plist.error());
  }
  const auto name = plist.value().string("CFBundleExecutable");
  if (!name) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::invalid_input_type, "nested bundle has no CFBundleExecutable", bundle});
  }
  auto wide = platform::wide_from_utf8(*name);
  if (!wide) {
    return Result<std::filesystem::path>::failure(wide.error());
  }
  const std::filesystem::path relative(wide.value());
  if (relative.has_parent_path() || relative.is_absolute()) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::archive_unsafe_path, "unsafe nested CFBundleExecutable", bundle});
  }
  return Result<std::filesystem::path>::success(bundle / relative);
}

bool fallback_allowed(InjectionError error) {
  switch (error) {
    case InjectionError::InsufficientLoadCommandSpace:
    case InjectionError::CodeSignatureLayoutUnsupported:
    case InjectionError::LinkEditNotAtEnd:
    case InjectionError::InvalidSymtab:
    case InjectionError::UnsupportedArchitecture:
      return true;
    default:
      return false;
  }
}

Result<void> inject_dependency(const std::filesystem::path& executable, std::string_view dependency,
                               bool compatibility_mode, const PipelineLogger& logger) {
  InjectionOptions options;
  options.commandType = LoadCommandType::Weak;
  options.stripCodeSignature = true;
  options.allowUnsafeOverwrite = compatibility_mode;

  InsertDylibEngine native;
  const InjectionResult result = native.inject(executable, dependency, options);
  if (result.error == InjectionError::None) {
    log(logger, L"[*] native insert_dylib backend used");
    return Result<void>::success();
  }
  if (!fallback_allowed(result.error)) {
    return Result<void>::failure({ErrorCode::injection_failed, result.message, executable});
  }

  log(logger, L"[?] native injection unavailable; using LIEF fallback");
  LiefMachOBackend lief;
  const InjectionResult fallback = lief.inject(executable, dependency, options);
  if (fallback.error != InjectionError::None) {
    return Result<void>::failure({ErrorCode::injection_failed, fallback.message, executable});
  }
  return Result<void>::success();
}

std::filesystem::path executable_directory() {
  std::vector<wchar_t> buffer(1024U);
  while (true) {
    const DWORD length =
        GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0U) {
      return {};
    }
    if (length < buffer.size() - 1U) {
      return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
    }
    buffer.resize(buffer.size() * 2U);
  }
}

std::optional<std::filesystem::path> bundled_signer() {
  const auto program_directory = executable_directory();
  if (program_directory.empty()) {
    return std::nullopt;
  }
  const auto candidate = program_directory / L"ldid.exe";
  std::error_code error;
  if (std::filesystem::is_regular_file(candidate, error) && !error) {
    return candidate;
  }
  return std::nullopt;
}

Result<std::filesystem::path> locate_common_framework(const CommonDependency& dependency,
                                                      const CyanOptions& options) {
  std::vector<std::filesystem::path> candidates;
  if (options.dependency_directory) {
    candidates.push_back(*options.dependency_directory / dependency.framework_name);
  }
  const auto program_directory = executable_directory();
  if (!program_directory.empty()) {
    auto name = platform::wide_from_utf8(dependency.framework_name);
    if (name) {
      candidates.push_back(program_directory / L"dependencies" / name.value());
    }
  }
  for (const auto& candidate : candidates) {
    std::error_code error;
    if (std::filesystem::is_directory(candidate, error) && !error) {
      return Result<std::filesystem::path>::success(candidate);
    }
  }
  return Result<std::filesystem::path>::failure(
      {ErrorCode::feature_unavailable,
       "required tweak framework is unavailable; use --dependency-dir",
       {}});
}

Result<void> publish_app_directory(const std::filesystem::path& staged_app,
                                   const std::filesystem::path& output) {
  std::error_code error;
  if (!output.parent_path().empty()) {
    std::filesystem::create_directories(output.parent_path(), error);
    if (error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not create output directory", output.parent_path()});
    }
  }
  const std::wstring nonce = std::to_wstring(GetTickCount64());
  const auto temporary =
      output.parent_path() / (output.filename().native() + L".cyan-new-" + nonce);
  const auto backup = output.parent_path() / (output.filename().native() + L".cyan-old-" + nonce);
  auto copied = copy_secure(staged_app, temporary, true);
  if (!copied) {
    return copied;
  }

  const bool existed = std::filesystem::exists(output, error);
  if (error) {
    std::filesystem::remove_all(temporary, error);
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not inspect app output", output});
  }
  if (existed) {
    std::filesystem::rename(output, backup, error);
    if (error) {
      std::filesystem::remove_all(temporary, error);
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not stage old app output", output});
    }
  }
  std::filesystem::rename(temporary, output, error);
  if (error) {
    if (existed) {
      std::error_code rollback_error;
      std::filesystem::rename(backup, output, rollback_error);
    }
    std::filesystem::remove_all(temporary, error);
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not publish app output", output});
  }
  if (existed) {
    std::filesystem::remove_all(backup, error);
    if (error) {
      return Result<void>::failure({ErrorCode::filesystem_error,
                                    "new app was published but old backup could not be removed",
                                    backup});
    }
  }
  return Result<void>::success();
}

Result<std::string> utf8_option(const std::wstring& value) {
  return platform::utf8_from_wide(value);
}

}  // namespace

Result<void> CyanPipeline::run(CyanOptions options, const PipelineLogger& logger) const {
  auto workspace_result = TemporaryWorkspace::create();
  if (!workspace_result) {
    return Result<void>::failure(workspace_result.error());
  }
  auto workspace = workspace_result.take_value();
  const auto package_root = workspace.path() / L"package";

  ArchiveService archives;
  if (extension_equals(options.input, L".app")) {
    log(logger, L"[*] copying app..");
    const auto payload = package_root / L"Payload";
    std::error_code error;
    std::filesystem::create_directories(payload, error);
    if (error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not create Payload directory", payload});
    }
    auto copied = copy_secure(options.input, payload / options.input.filename(), false);
    if (!copied) {
      return copied;
    }
  } else {
    log(logger, L"[*] extracting ipa..");
    auto extracted = archives.extract(options.input, package_root);
    if (!extracted) {
      return extracted;
    }
  }

  auto app_path = find_app(package_root);
  if (!app_path) {
    return Result<void>::failure(app_path.error());
  }
  auto app_result = AppBundle::open(app_path.value());
  if (!app_result) {
    return Result<void>::failure(app_result.error());
  }
  auto app = app_result.take_value();

  MachOInspector inspector;
  auto main_info = inspector.inspect(app.executable());
  if (!main_info) {
    return Result<void>::failure(main_info.error());
  }
  const bool main_encrypted =
      std::any_of(main_info.value().slices.begin(), main_info.value().slices.end(),
                  [](const MachOSliceInfo& slice) { return slice.encrypted; });
  if (main_encrypted && !options.ignore_encrypted) {
    return Result<void>::failure(
        {ErrorCode::macho_encrypted, "main binary is encrypted; exiting", app.executable()});
  }
  if (main_encrypted) {
    log(logger, L"[?] main binary is encrypted, ignoring");
  }

  CyanArchiveReader cyan_reader;
  for (std::size_t index = 0; index < options.cyan_files.size(); ++index) {
    log(logger, L"[*] parsing " + options.cyan_files[index].filename().native() + L" ..");
    auto applied = cyan_reader.apply(
        options.cyan_files[index], workspace.path() / (L"cyan-" + std::to_wstring(index)), options);
    if (!applied) {
      return applied;
    }
  }
  auto revalidated = validate_and_normalize(options);
  if (!revalidated) {
    return revalidated;
  }

  if (options.remove_extensions) {
    auto removed = app.remove_all_extensions();
    if (!removed) {
      return Result<void>::failure(removed.error());
    }
    log(logger, removed.value() ? L"[*] removed app extensions" : L"[?] no app extensions");
  } else if (options.remove_encrypted) {
    auto removed = app.remove_encrypted_extensions(inspector);
    if (!removed) {
      return Result<void>::failure(removed.error());
    }
    log(logger,
        removed.value().empty() ? L"[?] no encrypted plugins" : L"[*] removed encrypted plugins");
  }

  std::unique_ptr<ISigningBackend> signing;
  if (options.ldid_path) {
    signing = std::make_unique<ExternalLdidSigningBackend>(*options.ldid_path);
  } else if (auto signer = bundled_signer()) {
    signing = std::make_unique<ExternalLdidSigningBackend>(std::move(*signer));
  }
  if (options.ipapatch && !signing) {
    return Result<void>::failure({ErrorCode::signing_backend_unavailable,
                                  "--ipapatch requires bundled ldid.exe or --ldid PATH",
                                  {}});
  }

  std::optional<PlistDocument> saved_entitlements;
  std::optional<SigningProfile> ipapatch_main_profile;
  const bool main_had_signature =
      std::any_of(main_info.value().slices.begin(), main_info.value().slices.end(),
                  [](const MachOSliceInfo& slice) { return slice.has_code_signature; });
  const bool ipapatch_patches_main = options.ipapatch && !options.ipapatch_plugins_only;
  if (signing && ipapatch_patches_main) {
    auto captured = signing->captureProfile(app.executable());
    if (!captured) {
      return Result<void>::failure(captured.error());
    }
    ipapatch_main_profile.emplace(captured.take_value());
    if (ipapatch_main_profile->identifier.empty()) {
      ipapatch_main_profile->identifier =
          app.info_plist().string("CFBundleIdentifier").value_or("fyi.zxcvbn.ipapatch.app");
    }
  } else if (signing && !options.injected_items.empty() && main_had_signature) {
    auto empty = PlistDocument::create_dictionary();
    if (!empty) {
      return Result<void>::failure(empty.error());
    }
    saved_entitlements.emplace(empty.take_value());
    auto extracted = signing->extractEntitlements(app.executable(), *saved_entitlements);
    if (!extracted) {
      return extracted;
    }
  }

  std::vector<std::pair<std::filesystem::path, SigningProfile>> ipapatch_additional_profiles;
  auto preserve_additional_profile = [&](const std::filesystem::path& source,
                                         const std::filesystem::path& final_path) -> Result<void> {
    if (!options.ipapatch) {
      return Result<void>::success();
    }
    auto profile = signing->captureProfile(source);
    if (!profile) {
      return Result<void>::failure(profile.error());
    }
    if (profile.value().identifier.empty()) {
      auto identifier = platform::utf8_from_wide(final_path.filename().native());
      if (!identifier) {
        return Result<void>::failure(identifier.error());
      }
      profile.value().identifier = identifier.take_value();
    }
    ipapatch_additional_profiles.emplace_back(final_path, profile.take_value());
    return Result<void>::success();
  };

  std::vector<NamedItem> items;
  for (const auto& injected : options.injected_items) {
    add_or_replace(items, injected);
  }
  for (std::size_t index = 0; index < items.size();) {
    if (!extension_equals(items[index].path, L".deb")) {
      ++index;
      continue;
    }
    const auto deb = items[index].path;
    auto extracted = extract_deb_items(deb, workspace.path() / (L"deb-" + std::to_wstring(index)));
    if (!extracted) {
      return Result<void>::failure(extracted.error());
    }
    log(logger, L"[*] extracted " + deb.filename().native());
    items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
    for (const auto& item : extracted.value()) {
      add_or_replace(items, item);
    }
  }

  std::vector<std::string> available_names;
  for (const auto& item : items) {
    if (!extension_equals(item.path, L".dylib") && !extension_equals(item.path, L".framework")) {
      continue;
    }
    auto encoded = platform::utf8_from_wide(item.name);
    if (!encoded) {
      return Result<void>::failure(encoded.error());
    }
    available_names.push_back(encoded.take_value());
  }

  LiefMachOBackend lief;
  std::vector<CommonDependency> required_common;
  std::unordered_set<std::string> required_keys;
  auto remember_dependencies = [&](std::vector<CommonDependency> dependencies) {
    for (auto& dependency : dependencies) {
      if (required_keys.insert(dependency.key).second) {
        required_common.push_back(std::move(dependency));
      }
    }
  };

  if (!items.empty()) {
    std::error_code error;
    std::filesystem::create_directories(app.path() / L"Frameworks", error);
    if (error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not create Frameworks directory", app.path()});
    }
    auto main_repaired = lief.repair_dependencies(app.executable(), available_names, true);
    if (!main_repaired) {
      return Result<void>::failure(main_repaired.error());
    }
    remember_dependencies(main_repaired.take_value());
  }

  for (std::size_t index = 0; index < items.size(); ++index) {
    const NamedItem& item = items[index];
    const auto extension = lower(item.path.extension().native());
    std::filesystem::path destination;

    if (extension == L".appex") {
      destination = app.path() / L"PlugIns" / item.name;
      auto copied = copy_secure(item.path, destination, true);
      if (!copied) {
        return copied;
      }
    } else if (extension == L".dylib") {
      destination = app.path() / L"Frameworks" / item.name;
      const auto working =
          workspace.path() / L"injection-working" / (std::to_wstring(index) + L"-" + item.name);
      auto copied = copy_secure(item.path, working, true);
      if (!copied) {
        return copied;
      }
      auto preserved = preserve_additional_profile(working, destination);
      if (!preserved) {
        return preserved;
      }
      auto repaired = lief.repair_dependencies(working, available_names, false);
      if (!repaired) {
        return Result<void>::failure(repaired.error());
      }
      remember_dependencies(repaired.take_value());
      auto name = platform::utf8_from_wide(item.name);
      if (!name) {
        return Result<void>::failure(name.error());
      }
      const std::string command = "@rpath/" + name.value();
      auto injected =
          inject_dependency(app.executable(), command, options.compatibility_cyan, logger);
      if (!injected) {
        return injected;
      }
      auto installed = copy_secure(working, destination, true);
      if (!installed) {
        return installed;
      }
    } else if (extension == L".framework") {
      destination = app.path() / L"Frameworks" / item.name;
      auto copied = copy_secure(item.path, destination, true);
      if (!copied) {
        return copied;
      }
      auto framework_executable = nested_bundle_executable(destination);
      if (framework_executable) {
        auto preserved =
            preserve_additional_profile(framework_executable.value(), framework_executable.value());
        if (!preserved) {
          return preserved;
        }
        auto repaired =
            lief.repair_dependencies(framework_executable.value(), available_names, false);
        if (!repaired) {
          return Result<void>::failure(repaired.error());
        }
        remember_dependencies(repaired.take_value());
      }
      auto encoded_name = platform::utf8_from_wide(item.name);
      if (!encoded_name) {
        return Result<void>::failure(encoded_name.error());
      }
      const std::string framework = encoded_name.value();
      const std::string stem = framework.substr(0U, framework.size() - 10U);
      auto injected = inject_dependency(app.executable(), "@rpath/" + framework + "/" + stem,
                                        options.compatibility_cyan, logger);
      if (!injected) {
        return injected;
      }
    } else {
      destination = app.path() / item.name;
      auto copied = copy_secure(item.path, destination, true);
      if (!copied) {
        return copied;
      }
    }
    log(logger, L"[*] injected " + item.name);
  }

  for (const auto& dependency : required_common) {
    auto source = locate_common_framework(dependency, options);
    if (!source) {
      return Result<void>::failure(source.error());
    }
    auto wide_name = platform::wide_from_utf8(dependency.framework_name);
    if (!wide_name) {
      return Result<void>::failure(wide_name.error());
    }
    const auto destination = app.path() / L"Frameworks" / wide_name.value();
    auto copied = copy_secure(source.value(), destination, true);
    if (!copied) {
      return copied;
    }
    log(logger, L"[*] auto-injected " + wide_name.value());
  }

  if (saved_entitlements && !options.fakesign && !options.thin) {
    auto restored = signing->signAdHoc(app.executable(), saved_entitlements);
    if (!restored) {
      return restored;
    }
    log(logger, L"[*] restored entitlements");
  }

  if (options.name) {
    auto value = utf8_option(*options.name);
    if (!value) {
      return Result<void>::failure(value.error());
    }
    auto changed = app.change_name(value.value());
    if (!changed) {
      return changed;
    }
    log(logger, L"[*] changed app name");
  }
  if (options.version) {
    auto value = utf8_option(*options.version);
    if (!value) {
      return Result<void>::failure(value.error());
    }
    auto changed = app.change_version(value.value());
    if (!changed) {
      return changed;
    }
    log(logger, L"[*] changed app version");
  }
  if (options.bundle_id) {
    auto value = utf8_option(*options.bundle_id);
    if (!value) {
      return Result<void>::failure(value.error());
    }
    auto changed = app.change_bundle_id(value.value());
    if (!changed) {
      return changed;
    }
    log(logger, L"[*] changed bundle id");
  }
  if (options.minimum_os) {
    auto value = utf8_option(*options.minimum_os);
    if (!value) {
      return Result<void>::failure(value.error());
    }
    auto changed = app.change_minimum_version(value.value());
    if (!changed) {
      return changed;
    }
    log(logger, L"[*] changed minimum OS version");
  }
  if (options.icon) {
    IconProcessor icons;
    auto changed = icons.replace_icon(*options.icon, app.path(), app.info_plist());
    if (!changed) {
      return changed;
    }
    auto saved = app.info_plist().save(app.path() / L"Info.plist", PlistFormat::xml);
    if (!saved) {
      return saved;
    }
    log(logger, L"[*] updated app icon");
  }
  if (options.merge_plist) {
    auto merged = app.merge_plist(*options.merge_plist);
    if (!merged) {
      return merged;
    }
    log(logger, L"[*] merged Info.plist");
  }
  if (options.entitlements) {
    if (!signing) {
      return Result<void>::failure({ErrorCode::signing_backend_unavailable,
                                    "--entitlements requires bundled ldid.exe or --ldid PATH",
                                    *options.entitlements});
    }
    auto entitlements = PlistDocument::load(*options.entitlements);
    if (!entitlements) {
      return Result<void>::failure(entitlements.error());
    }
    if (ipapatch_main_profile) {
      if (ipapatch_main_profile->entitlements) {
        auto merged = ipapatch_main_profile->entitlements->merge(entitlements.value());
        if (!merged) {
          return merged;
        }
      } else {
        ipapatch_main_profile->entitlements.emplace(entitlements.take_value());
      }
      ipapatch_main_profile->had_der_entitlements = true;
    } else {
      std::optional<PlistDocument> document;
      document.emplace(entitlements.take_value());
      auto signed_result = signing->signAdHoc(app.executable(), document);
      if (!signed_result) {
        return signed_result;
      }
      auto combined = PlistDocument::create_dictionary();
      if (!combined) {
        return Result<void>::failure(combined.error());
      }
      auto extracted = signing->extractEntitlements(app.executable(), combined.value());
      if (!extracted) {
        return extracted;
      }
      saved_entitlements.emplace(combined.take_value());
    }
    log(logger, L"[*] merged new entitlements");
  }

  if (options.remove_supported_devices) {
    auto removed = app.remove_supported_devices();
    if (!removed) {
      return removed;
    }
    log(logger, L"[*] removed UISupportedDevices");
  }
  if (options.no_watch) {
    auto removed = app.remove_watch_content();
    if (!removed) {
      return Result<void>::failure(removed.error());
    }
    log(logger, removed.value() ? L"[*] removed watch app" : L"[?] watch app not present");
  }
  if (options.enable_documents) {
    auto enabled = app.enable_documents();
    if (!enabled) {
      return enabled;
    }
    log(logger, L"[*] enabled documents support");
  }

  if (!options.ipapatch && (options.fakesign || options.thin)) {
    auto executables = app.discover_executables();
    if (!executables) {
      return Result<void>::failure(executables.error());
    }
    if (options.thin) {
      for (const auto& executable : executables.value()) {
        auto thinned = lief.thin_to_arm64(executable);
        if (!thinned) {
          return thinned;
        }
      }
      log(logger, L"[*] thinned " + std::to_wstring(executables.value().size()) + L" item(s)");
    }
    if (options.fakesign) {
      if (!signing) {
        return Result<void>::failure({ErrorCode::signing_backend_unavailable,
                                      "--fakesign requires bundled ldid.exe or --ldid PATH",
                                      {}});
      }
      const std::optional<PlistDocument> no_entitlements;
      for (const auto& executable : executables.value()) {
        const bool is_main = executable.lexically_normal() == app.executable().lexically_normal();
        const auto& signing_entitlements = is_main ? saved_entitlements : no_entitlements;
        auto signed_result = signing->signAdHoc(executable, signing_entitlements);
        if (!signed_result) {
          return signed_result;
        }
      }
      log(logger, L"[*] fakesigned " + std::to_wstring(executables.value().size()) + L" item(s)");
    } else if (saved_entitlements) {
      auto restored = signing->signAdHoc(app.executable(), saved_entitlements);
      if (!restored) {
        return restored;
      }
      log(logger, L"[*] restored entitlements");
    }
  } else if (options.ipapatch) {
    IpaPatchService ipapatch_service(*signing);
    IpaPatchCallbacks ipapatch_callbacks;
    ipapatch_callbacks.progress = [&](const IpaPatchProgressEvent& event) {
      if (event.completed == event.total) {
        log(logger, L"[*] ipapatch " + std::wstring(ipapatch_stage_name(event.stage)));
      }
    };
    IpaPatchOptions patch_options;
    patch_options.dylib = options.ipapatch_dylib;
    patch_options.plugins_only = options.ipapatch_plugins_only;
    patch_options.compression_level = options.compression_level;
    auto prepared = ipapatch_service.prepare_open_package(
        package_root, patch_options, ipapatch_callbacks, {}, std::move(ipapatch_main_profile));
    if (!prepared) {
      return Result<void>::failure(prepared.error());
    }
    auto plan = prepared.take_value();

    auto additional_profiles = std::move(ipapatch_additional_profiles);
    std::vector<std::filesystem::path> final_executables;
    if (options.fakesign || options.thin) {
      auto discovered = app.discover_executables();
      if (!discovered) {
        return Result<void>::failure(discovered.error());
      }
      final_executables = discovered.take_value();
      for (const auto& executable : final_executables) {
        const auto normalized = executable.lexically_normal();
        const bool is_target = std::any_of(
            plan.targets.begin(), plan.targets.end(), [&](const IpaPatchTarget& target) {
              return target.executable.lexically_normal() == normalized;
            });
        if (is_target) {
          continue;
        }
        const bool already_preserved = std::any_of(
            additional_profiles.begin(), additional_profiles.end(), [&](const auto& preserved) {
              return preserved.first.lexically_normal() == normalized;
            });
        if (already_preserved) {
          continue;
        }
        auto profile = signing->captureProfile(executable);
        if (!profile) {
          return Result<void>::failure(profile.error());
        }
        if (profile.value().identifier.empty()) {
          auto identifier = platform::utf8_from_wide(executable.filename().native());
          if (!identifier) {
            return Result<void>::failure(identifier.error());
          }
          profile.value().identifier = identifier.take_value();
        }
        additional_profiles.emplace_back(executable, profile.take_value());
      }
    }
    if (options.thin) {
      for (const auto& executable : final_executables) {
        auto thinned = lief.thin_to_arm64(executable);
        if (!thinned) {
          return thinned;
        }
      }
      log(logger, L"[*] thinned " + std::to_wstring(final_executables.size()) + L" item(s)");
    }

    auto applied = ipapatch_service.apply_prepared(std::move(plan), ipapatch_callbacks);
    if (!applied) {
      return Result<void>::failure(applied.error());
    }
    auto ipapatch_result = applied.take_value();
    auto finalized = ipapatch_service.finalize_signatures(ipapatch_result, ipapatch_callbacks);
    if (!finalized) {
      return finalized;
    }
    for (const auto& [executable, profile] : additional_profiles) {
      auto signed_result = signing->signAdHoc(executable, profile);
      if (!signed_result) {
        return signed_result;
      }
    }
    if (!additional_profiles.empty()) {
      log(logger, L"[*] final-signed " + std::to_wstring(additional_profiles.size()) +
                      L" additional item(s)");
    }
  }

  if (extension_equals(options.output, L".app")) {
    auto published = publish_app_directory(app.path(), options.output);
    if (!published) {
      return published;
    }
    log(logger, L"[*] generated app at " + options.output.native());
  } else {
    log(logger, L"[*] generating ipa with compression level " +
                    std::to_wstring(options.compression_level) + L"..");
    auto packaged =
        archives.create_zip(package_root, options.output, options.compression_level, false);
    if (!packaged) {
      return packaged;
    }
    log(logger, L"[*] generated ipa at " + options.output.native());
  }
  return Result<void>::success();
}

}  // namespace cyan
