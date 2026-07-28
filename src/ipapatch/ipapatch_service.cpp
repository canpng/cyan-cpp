#include "cyan/ipapatch/ipapatch_service.hpp"

#include <Windows.h>

#include <algorithm>
#include <cwctype>
#include <system_error>
#include <utility>

#include "cyan/archive/zip_update_service.hpp"
#include "cyan/core/app_bundle.hpp"
#include "cyan/core/temporary_workspace.hpp"
#include "cyan/macho/insert_dylib_engine.hpp"
#include "cyan/macho/lief_macho_backend.hpp"
#include "cyan/macho/macho_inspector.hpp"
#include "cyan/platform/utf.hpp"
#include "cyan/plist/plist_document.hpp"

namespace cyan {
namespace {

constexpr std::string_view fallback_identifier = "fyi.zxcvbn.ipapatch.app";

void report(const IpaPatchCallbacks& callbacks, IpaPatchStage stage,
            const std::filesystem::path& current, std::size_t completed, std::size_t total) {
  if (!callbacks.progress) {
    return;
  }
  callbacks.progress(
      {stage, current, completed, total,
       total == 0U ? 0.0 : static_cast<double>(completed) / static_cast<double>(total)});
}

Result<void> cancelled(std::stop_token token, const std::filesystem::path& path = {}) {
  if (!token.stop_requested()) {
    return Result<void>::success();
  }
  return Result<void>::failure(
      {ErrorCode::operation_cancelled, "ipapatch operation was cancelled", path});
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

Result<std::filesystem::path> find_main_app(const std::filesystem::path& package_root) {
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
        {ErrorCode::filesystem_error, "could not enumerate IPA Payload", payload});
  }
  for (; iterator != end; iterator.increment(error)) {
    if (error) {
      return Result<std::filesystem::path>::failure(
          {ErrorCode::filesystem_error, "could not enumerate IPA Payload", payload});
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

Result<std::string> bundle_identifier(const std::filesystem::path& bundle) {
  auto plist = PlistDocument::load(bundle / L"Info.plist");
  if (!plist) {
    return Result<std::string>::failure(plist.error());
  }
  return Result<std::string>::success(
      plist.value().string("CFBundleIdentifier").value_or(std::string(fallback_identifier)));
}

Result<std::filesystem::path> bundle_executable(const std::filesystem::path& bundle) {
  auto plist = PlistDocument::load(bundle / L"Info.plist");
  if (!plist) {
    return Result<std::filesystem::path>::failure(plist.error());
  }
  const auto name = plist.value().string("CFBundleExecutable");
  if (!name || name->empty()) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::invalid_input_type, "bundle has no CFBundleExecutable", bundle});
  }
  auto wide = platform::wide_from_utf8(*name);
  if (!wide) {
    return Result<std::filesystem::path>::failure(wide.error());
  }
  const std::filesystem::path relative(wide.value());
  if (relative.empty() || relative.is_absolute() || relative.has_parent_path()) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::archive_unsafe_path, "CFBundleExecutable must be a file name", bundle});
  }
  const auto executable = bundle / relative;
  std::error_code error;
  if (!std::filesystem::is_regular_file(executable, error) || error ||
      is_reparse_point(executable)) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::file_not_found, "bundle executable does not exist", executable});
  }
  return Result<std::filesystem::path>::success(executable);
}

Result<std::filesystem::path> declared_bundle_executable(const std::filesystem::path& bundle) {
  auto plist = PlistDocument::load(bundle / L"Info.plist");
  if (!plist) {
    return Result<std::filesystem::path>::failure(plist.error());
  }
  const auto name = plist.value().string("CFBundleExecutable");
  if (!name || name->empty()) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::invalid_input_type, "bundle has no CFBundleExecutable", bundle});
  }
  auto wide = platform::wide_from_utf8(*name);
  if (!wide) {
    return Result<std::filesystem::path>::failure(wide.error());
  }
  const std::filesystem::path relative(wide.value());
  if (relative.empty() || relative.is_absolute() || relative.has_parent_path()) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::archive_unsafe_path, "CFBundleExecutable must be a file name", bundle});
  }
  return Result<std::filesystem::path>::success(bundle / relative);
}

std::vector<std::wstring> path_components(const std::filesystem::path& path) {
  std::vector<std::wstring> components;
  for (const auto& component : path) {
    components.push_back(component.native());
  }
  return components;
}

bool is_main_info_plist(const std::filesystem::path& relative) {
  const auto components = path_components(relative);
  return components.size() == 3U && components[0] == L"Payload" &&
         extension_equals(std::filesystem::path(components[1]), L".app") &&
         components[2] == L"Info.plist";
}

bool is_extension_info_plist(const std::filesystem::path& relative,
                             const std::filesystem::path& main_bundle) {
  const auto components = path_components(relative);
  const auto main = path_components(main_bundle);
  return components.size() == 5U && main.size() == 2U && components[0] == main[0] &&
         components[1] == main[1] &&
         (components[2] == L"PlugIns" || components[2] == L"Extensions") &&
         extension_equals(std::filesystem::path(components[3]), L".appex") &&
         components[4] == L"Info.plist";
}

Result<std::filesystem::path> relative_to_package(const std::filesystem::path& path,
                                                  const std::filesystem::path& package_root) {
  const auto relative = path.lexically_normal().lexically_relative(package_root.lexically_normal());
  if (relative.empty() || relative.is_absolute()) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::archive_unsafe_path, "path is outside the package root", path});
  }
  const auto components = path_components(relative);
  if (components.empty() || components.front() == L"..") {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::archive_unsafe_path, "path is outside the package root", path});
  }
  return Result<std::filesystem::path>::success(relative);
}

Result<std::vector<std::filesystem::path>> find_extensions(const std::filesystem::path& app) {
  std::vector<std::filesystem::path> result;
  for (const auto* directory_name : {L"PlugIns", L"Extensions"}) {
    const auto directory = app / directory_name;
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
      const auto bundle = iterator->path();
      if (!extension_equals(bundle, L".appex")) {
        continue;
      }
      const auto bundle_status = iterator->symlink_status(error);
      if (error) {
        return Result<std::vector<std::filesystem::path>>::failure(
            {ErrorCode::filesystem_error, "could not inspect app extension", bundle});
      }
      if (!std::filesystem::is_directory(bundle_status)) {
        continue;
      }
      if (std::filesystem::is_symlink(bundle_status) || is_reparse_point(bundle)) {
        return Result<std::vector<std::filesystem::path>>::failure(
            {ErrorCode::archive_unsafe_path, "app extension is a link or reparse point", bundle});
      }
      const auto info_plist = bundle / L"Info.plist";
      const auto info_status = std::filesystem::symlink_status(info_plist, error);
      if (error == std::errc::no_such_file_or_directory) {
        error.clear();
        continue;
      }
      if (error) {
        return Result<std::vector<std::filesystem::path>>::failure(
            {ErrorCode::filesystem_error, "could not inspect app-extension Info.plist",
             info_plist});
      }
      if (!std::filesystem::is_regular_file(info_status) ||
          std::filesystem::is_symlink(info_status) || is_reparse_point(info_plist)) {
        continue;
      }
      result.push_back(bundle);
    }
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return Result<std::vector<std::filesystem::path>>::success(std::move(result));
}

Result<void> validate_payload(const std::filesystem::path& path) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error) || error || is_reparse_point(path)) {
    return Result<void>::failure(
        {ErrorCode::file_not_found, "ipapatch payload dylib is unavailable", path});
  }
  if (path.filename().empty()) {
    return Result<void>::failure(
        {ErrorCode::invalid_argument, "ipapatch payload has no file name", path});
  }
  return Result<void>::success();
}

Result<void> install_payload(const std::filesystem::path& source,
                             const std::filesystem::path& destination) {
  auto valid = validate_payload(source);
  if (!valid) {
    return valid;
  }
  std::error_code error;
  std::filesystem::create_directories(destination.parent_path(), error);
  if (error || is_reparse_point(destination.parent_path())) {
    return Result<void>::failure({ErrorCode::filesystem_error,
                                  "could not create app Frameworks directory",
                                  destination.parent_path()});
  }
  if (!std::filesystem::copy_file(source, destination,
                                  std::filesystem::copy_options::overwrite_existing, error) ||
      error) {
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not install ipapatch payload", destination});
  }
  return Result<void>::success();
}

bool has_unsupported_fat_slice(const MachOInfo& info) {
  return info.is_fat &&
         std::any_of(info.slices.begin(), info.slices.end(), [](const MachOSliceInfo& slice) {
           return static_cast<std::uint32_t>(slice.cpu_subtype) > 2U;
         });
}

Result<void> inject_one(const std::filesystem::path& executable, std::string_view load_command,
                        bool has_skipped_fat_slices) {
  InjectionOptions options;
  options.commandType = LoadCommandType::Weak;
  options.stripCodeSignature = true;
  options.fatArchitecturePolicy = FatArchitecturePolicy::IpaPatchV213;

  InsertDylibEngine native;
  const auto injected = native.inject(executable, load_command, options);
  if (injected.error == InjectionError::None) {
    return Result<void>::success();
  }
  if (injected.error == InjectionError::DuplicateLoadCommand) {
    return Result<void>::failure(
        {ErrorCode::injection_failed,
         "load command '" + std::string(load_command) + "' already exists (already patched)",
         executable});
  }

  const bool can_fallback = !has_skipped_fat_slices &&
                            (injected.error == InjectionError::InsufficientLoadCommandSpace ||
                             injected.error == InjectionError::CodeSignatureLayoutUnsupported ||
                             injected.error == InjectionError::LinkEditNotAtEnd ||
                             injected.error == InjectionError::InvalidSymtab ||
                             injected.error == InjectionError::UnsupportedArchitecture);
  if (!can_fallback) {
    return Result<void>::failure({ErrorCode::injection_failed, injected.message, executable});
  }
  LiefMachOBackend lief;
  const auto fallback = lief.inject(executable, load_command, options);
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

}  // namespace

IpaPatchService::IpaPatchService(ISigningBackend& signing_backend)
    : signing_backend_(signing_backend) {}

Result<IpaPatchPlan> IpaPatchService::prepare_open_package(
    const std::filesystem::path& package_root, const IpaPatchOptions& options,
    const IpaPatchCallbacks& callbacks, std::stop_token stop_token,
    std::optional<SigningProfile> main_profile_override) const {
  report(callbacks, IpaPatchStage::validating, package_root, 0U, 1U);
  auto not_cancelled = cancelled(stop_token, package_root);
  if (!not_cancelled) {
    return Result<IpaPatchPlan>::failure(not_cancelled.error());
  }

  const auto payload_source = options.dylib.value_or(
      options.default_dylib.empty() ? bundled_payload_path() : options.default_dylib);
  auto payload_valid = validate_payload(payload_source);
  if (!payload_valid) {
    return Result<IpaPatchPlan>::failure(payload_valid.error());
  }
  auto payload_name = platform::utf8_from_wide(payload_source.filename().native());
  if (!payload_name) {
    return Result<IpaPatchPlan>::failure(payload_name.error());
  }

  auto app_path = find_main_app(package_root);
  if (!app_path) {
    return Result<IpaPatchPlan>::failure(app_path.error());
  }
  auto app = AppBundle::open(app_path.value());
  if (!app) {
    return Result<IpaPatchPlan>::failure(app.error());
  }

  IpaPatchPlan plan;
  plan.package_root = package_root;
  plan.app_bundle = app.value().path();
  plan.payload_source = payload_source;
  plan.payload_destination = app.value().path() / L"Frameworks" / payload_source.filename();
  plan.load_command = "@rpath/" + payload_name.value();

  if (!options.plugins_only) {
    auto identifier = bundle_identifier(app.value().path());
    if (!identifier) {
      return Result<IpaPatchPlan>::failure(identifier.error());
    }
    plan.targets.push_back(
        {app.value().path(), app.value().executable(), identifier.take_value(), {}});
  }
  auto extensions = find_extensions(app.value().path());
  if (!extensions) {
    return Result<IpaPatchPlan>::failure(extensions.error());
  }
  for (const auto& extension : extensions.value()) {
    auto executable = bundle_executable(extension);
    if (!executable) {
      return Result<IpaPatchPlan>::failure(executable.error());
    }
    auto identifier = bundle_identifier(extension);
    if (!identifier) {
      return Result<IpaPatchPlan>::failure(identifier.error());
    }
    plan.targets.push_back({extension, executable.take_value(), identifier.take_value(), {}});
  }
  if (plan.targets.empty()) {
    return Result<IpaPatchPlan>::failure({ErrorCode::invalid_argument,
                                          "no eligible app or extension executable was found",
                                          app.value().path()});
  }

  report(callbacks, IpaPatchStage::discovering, app.value().path(), plan.targets.size(),
         plan.targets.size());
  MachOInspector inspector;
  for (std::size_t index = 0; index < plan.targets.size(); ++index) {
    not_cancelled = cancelled(stop_token, plan.targets[index].executable);
    if (!not_cancelled) {
      return Result<IpaPatchPlan>::failure(not_cancelled.error());
    }
    auto info = inspector.inspect(plan.targets[index].executable);
    if (!info) {
      return Result<IpaPatchPlan>::failure(info.error());
    }
    const bool duplicate = std::any_of(
        info.value().slices.begin(), info.value().slices.end(), [&](const MachOSliceInfo& slice) {
          return std::find(slice.dependencies.begin(), slice.dependencies.end(),
                           plan.load_command) != slice.dependencies.end();
        });
    if (duplicate) {
      return Result<IpaPatchPlan>::failure(
          {ErrorCode::injection_failed,
           "load command '" + plan.load_command + "' already exists (already patched)",
           plan.targets[index].executable});
    }
    if (info.value().is_fat &&
        std::none_of(info.value().slices.begin(), info.value().slices.end(),
                     [](const MachOSliceInfo& slice) {
                       return static_cast<std::uint32_t>(slice.cpu_subtype) <= 2U;
                     })) {
      return Result<IpaPatchPlan>::failure({ErrorCode::macho_unsupported,
                                            "FAT Mach-O has no slice supported by ipapatch v2.1.3",
                                            plan.targets[index].executable});
    }

    const bool is_main =
        plan.targets[index].bundle.lexically_normal() == app.value().path().lexically_normal();
    if (is_main && main_profile_override) {
      plan.targets[index].signing_profile = std::move(*main_profile_override);
      main_profile_override.reset();
    } else {
      auto profile = signing_backend_.captureProfile(plan.targets[index].executable);
      if (!profile) {
        return Result<IpaPatchPlan>::failure(profile.error());
      }
      plan.targets[index].signing_profile = profile.take_value();
    }
    if (plan.targets[index].signing_profile.identifier.empty()) {
      plan.targets[index].signing_profile.identifier = plan.targets[index].bundle_identifier.empty()
                                                           ? std::string(fallback_identifier)
                                                           : plan.targets[index].bundle_identifier;
    }
    report(callbacks, IpaPatchStage::capturing_signatures, plan.targets[index].executable,
           index + 1U, plan.targets.size());
  }
  return Result<IpaPatchPlan>::success(std::move(plan));
}

Result<IpaPatchApplyResult> IpaPatchService::apply_prepared(IpaPatchPlan plan,
                                                            const IpaPatchCallbacks& callbacks,
                                                            std::stop_token stop_token) const {
  MachOInspector inspector;
  for (std::size_t index = 0; index < plan.targets.size(); ++index) {
    auto not_cancelled = cancelled(stop_token, plan.targets[index].executable);
    if (!not_cancelled) {
      return Result<IpaPatchApplyResult>::failure(not_cancelled.error());
    }
    auto info = inspector.inspect(plan.targets[index].executable);
    if (!info) {
      return Result<IpaPatchApplyResult>::failure(info.error());
    }
    auto injected = inject_one(plan.targets[index].executable, plan.load_command,
                               has_unsupported_fat_slice(info.value()));
    if (!injected) {
      return Result<IpaPatchApplyResult>::failure(injected.error());
    }
    report(callbacks, IpaPatchStage::injecting, plan.targets[index].executable, index + 1U,
           plan.targets.size());
  }

  auto installed = install_payload(plan.payload_source, plan.payload_destination);
  if (!installed) {
    return Result<IpaPatchApplyResult>::failure(installed.error());
  }
  report(callbacks, IpaPatchStage::installing_payload, plan.payload_destination, 1U, 1U);
  return Result<IpaPatchApplyResult>::success(std::move(plan));
}

Result<IpaPatchApplyResult> IpaPatchService::apply_to_open_package(
    const std::filesystem::path& package_root, const IpaPatchOptions& options,
    const IpaPatchCallbacks& callbacks, std::stop_token stop_token) const {
  auto prepared = prepare_open_package(package_root, options, callbacks, stop_token);
  if (!prepared) {
    return Result<IpaPatchApplyResult>::failure(prepared.error());
  }
  return apply_prepared(prepared.take_value(), callbacks, stop_token);
}

Result<void> IpaPatchService::finalize_signatures(const IpaPatchApplyResult& result,
                                                  const IpaPatchCallbacks& callbacks,
                                                  std::stop_token stop_token) const {
  for (std::size_t index = 0; index < result.targets.size(); ++index) {
    auto not_cancelled = cancelled(stop_token, result.targets[index].executable);
    if (!not_cancelled) {
      return not_cancelled;
    }
    auto signed_result = signing_backend_.signAdHoc(result.targets[index].executable,
                                                    result.targets[index].signing_profile);
    if (!signed_result) {
      return signed_result;
    }
    report(callbacks, IpaPatchStage::signing, result.targets[index].executable, index + 1U,
           result.targets.size());
  }
  return Result<void>::success();
}

Result<void> IpaPatchService::run_standalone(const std::filesystem::path& input_ipa,
                                             const std::filesystem::path& output_ipa,
                                             const IpaPatchOptions& options,
                                             const IpaPatchCallbacks& callbacks,
                                             std::stop_token stop_token) const {
  report(callbacks, IpaPatchStage::validating, input_ipa, 0U, 1U);
  if (!extension_equals(input_ipa, L".ipa") && !extension_equals(input_ipa, L".tipa")) {
    return Result<void>::failure(
        {ErrorCode::invalid_input_type, "ipapatch input must be an IPA or TIPA", input_ipa});
  }
  if (!extension_equals(output_ipa, L".ipa") && !extension_equals(output_ipa, L".tipa")) {
    return Result<void>::failure(
        {ErrorCode::invalid_output_type, "ipapatch output must be an IPA or TIPA", output_ipa});
  }
  std::error_code error;
  if (!std::filesystem::is_regular_file(input_ipa, error) || error || is_reparse_point(input_ipa)) {
    return Result<void>::failure(
        {ErrorCode::file_not_found, "input IPA does not exist", input_ipa});
  }
  if (options.compression_level < 0 || options.compression_level > 9) {
    return Result<void>::failure(
        {ErrorCode::invalid_compression_level, "ZIP compression level must be 0-9", output_ipa});
  }

  auto workspace_result = TemporaryWorkspace::create();
  if (!workspace_result) {
    return Result<void>::failure(workspace_result.error());
  }
  auto workspace = workspace_result.take_value();
  const auto package_root = workspace.path() / L"package";
  ZipUpdateService archive;
  report(callbacks, IpaPatchStage::extracting, input_ipa, 0U, 1U);

  auto listed = archive.list_entries(input_ipa);
  if (!listed) {
    return Result<void>::failure(listed.error());
  }
  std::vector<std::filesystem::path> main_plists;
  for (const auto& entry : listed.value()) {
    if (!entry.is_directory && is_main_info_plist(entry.relative_path)) {
      main_plists.push_back(entry.relative_path);
    }
  }
  std::sort(main_plists.begin(), main_plists.end());
  if (main_plists.empty()) {
    return Result<void>::failure(
        {ErrorCode::invalid_input_type, "IPA Payload has no valid app bundle", input_ipa});
  }

  const std::filesystem::path main_bundle = main_plists.front().parent_path();
  std::vector<std::filesystem::path> plist_entries{main_plists.front()};
  for (const auto& entry : listed.value()) {
    if (!entry.is_directory && is_extension_info_plist(entry.relative_path, main_bundle)) {
      plist_entries.push_back(entry.relative_path);
    }
  }
  std::sort(plist_entries.begin(), plist_entries.end());
  auto extracted_plists =
      archive.extract_entries(input_ipa, package_root, plist_entries, listed.value());
  if (!extracted_plists) {
    return extracted_plists;
  }

  std::vector<std::filesystem::path> executable_entries;
  executable_entries.reserve(plist_entries.size());
  for (const auto& plist_entry : plist_entries) {
    auto executable = declared_bundle_executable((package_root / plist_entry).parent_path());
    if (!executable) {
      return Result<void>::failure(executable.error());
    }
    auto relative = relative_to_package(executable.value(), package_root);
    if (!relative) {
      return Result<void>::failure(relative.error());
    }
    executable_entries.push_back(relative.take_value());
  }
  auto extracted_executables =
      archive.extract_entries(input_ipa, package_root, executable_entries, listed.value());
  if (!extracted_executables) {
    return extracted_executables;
  }
  report(callbacks, IpaPatchStage::extracting, input_ipa, 1U, 1U);

  auto applied = apply_to_open_package(package_root, options, callbacks, stop_token);
  if (!applied) {
    return Result<void>::failure(applied.error());
  }
  auto signed_result = finalize_signatures(applied.value(), callbacks, stop_token);
  if (!signed_result) {
    return signed_result;
  }
  auto not_cancelled = cancelled(stop_token, output_ipa);
  if (!not_cancelled) {
    return not_cancelled;
  }

  report(callbacks, IpaPatchStage::packaging, output_ipa, 0U, 1U);
  std::vector<ZipReplacement> replacements;
  replacements.reserve(applied.value().targets.size() + 1U);
  for (const auto& target : applied.value().targets) {
    auto relative = relative_to_package(target.executable, package_root);
    if (!relative) {
      return Result<void>::failure(relative.error());
    }
    replacements.push_back({relative.take_value(), target.executable, true});
  }
  auto payload_relative = relative_to_package(applied.value().payload_destination, package_root);
  if (!payload_relative) {
    return Result<void>::failure(payload_relative.error());
  }
  replacements.push_back(
      {payload_relative.take_value(), applied.value().payload_destination, true});
  auto packaged =
      archive.update(input_ipa, output_ipa, replacements, options.compression_level, stop_token);
  if (!packaged) {
    return packaged;
  }
  report(callbacks, IpaPatchStage::packaging, output_ipa, 1U, 1U);
  report(callbacks, IpaPatchStage::completed, output_ipa, 1U, 1U);
  return Result<void>::success();
}

std::filesystem::path IpaPatchService::bundled_payload_path() {
  const auto directory = executable_directory();
  return directory.empty() ? std::filesystem::path(L"zxPluginsInject.dylib")
                           : directory / L"zxPluginsInject.dylib";
}

std::wstring_view ipapatch_stage_name(IpaPatchStage stage) {
  switch (stage) {
    case IpaPatchStage::validating:
      return L"validating";
    case IpaPatchStage::extracting:
      return L"extracting";
    case IpaPatchStage::discovering:
      return L"discovering";
    case IpaPatchStage::capturing_signatures:
      return L"capturing signatures";
    case IpaPatchStage::injecting:
      return L"injecting";
    case IpaPatchStage::installing_payload:
      return L"installing payload";
    case IpaPatchStage::signing:
      return L"signing";
    case IpaPatchStage::packaging:
      return L"packaging";
    case IpaPatchStage::completed:
      return L"completed";
  }
  return L"working";
}

}  // namespace cyan
