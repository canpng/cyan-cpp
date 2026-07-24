#include "cyan/archive/cyan_archive.hpp"

#include <array>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <system_error>
#include <tuple>

#include "cyan/archive/archive_service.hpp"
#include "cyan/core/temporary_workspace.hpp"
#include "cyan/platform/utf.hpp"

namespace cyan {
namespace {

using Json = nlohmann::json;

Result<std::string> utf8_value(const std::wstring& value) {
  return platform::utf8_from_wide(value);
}

Result<void> add_optional_string(Json& configuration, std::string_view key,
                                 const std::optional<std::wstring>& value) {
  if (!value || value->empty()) {
    return Result<void>::success();
  }
  auto encoded = utf8_value(*value);
  if (!encoded) {
    return Result<void>::failure(encoded.error());
  }
  configuration[std::string(key)] = encoded.take_value();
  return Result<void>::success();
}

Result<void> copy_tree(const std::filesystem::path& source,
                       const std::filesystem::path& destination) {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(source, error);
  if (error || std::filesystem::is_symlink(status)) {
    return Result<void>::failure(
        {ErrorCode::archive_unsafe_path, "cannot add a symlink to a .cyan file", source});
  }

  if (std::filesystem::is_regular_file(status)) {
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error ||
        !std::filesystem::copy_file(source, destination,
                                    std::filesystem::copy_options::overwrite_existing, error)) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not stage .cyan input file", source});
    }
    return Result<void>::success();
  }
  if (!std::filesystem::is_directory(status)) {
    return Result<void>::failure(
        {ErrorCode::invalid_argument, "unsupported .cyan input type", source});
  }

  std::filesystem::create_directories(destination, error);
  if (error) {
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not stage .cyan input directory", source});
  }
  for (std::filesystem::recursive_directory_iterator
           iterator(source, std::filesystem::directory_options::none, error),
       end;
       iterator != end; iterator.increment(error)) {
    if (error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not enumerate .cyan input", source});
    }
    const auto relative = std::filesystem::relative(iterator->path(), source, error);
    if (error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not make .cyan input relative", iterator->path()});
    }
    const auto target = destination / relative;
    const auto child_status = iterator->symlink_status(error);
    if (error || std::filesystem::is_symlink(child_status)) {
      return Result<void>::failure({ErrorCode::archive_unsafe_path,
                                    "cannot add a symlink to a .cyan file", iterator->path()});
    }
    if (std::filesystem::is_directory(child_status)) {
      std::filesystem::create_directories(target, error);
    } else if (std::filesystem::is_regular_file(child_status)) {
      std::filesystem::create_directories(target.parent_path(), error);
      if (!error) {
        std::filesystem::copy_file(iterator->path(), target,
                                   std::filesystem::copy_options::overwrite_existing, error);
      }
    } else {
      return Result<void>::failure(
          {ErrorCode::invalid_argument, "unsupported .cyan input type", iterator->path()});
    }
    if (error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not stage .cyan input", iterator->path()});
    }
  }
  return Result<void>::success();
}

Result<void> stage_named_file(const std::optional<std::filesystem::path>& source,
                              const std::filesystem::path& staging, std::wstring_view name) {
  if (!source) {
    return Result<void>::success();
  }
  return copy_tree(*source, staging / name);
}

Result<Json> read_configuration(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    return Result<Json>::failure(
        {ErrorCode::cyan_invalid, ".cyan archive has no readable config.json", path});
  }
  const std::streamoff size = input.tellg();
  constexpr std::streamoff maximum_configuration_size = 1024 * 1024;
  if (size < 0 || size > maximum_configuration_size) {
    return Result<Json>::failure(
        {ErrorCode::cyan_invalid, ".cyan config.json exceeds the size limit", path});
  }
  input.seekg(0);
  try {
    Json configuration;
    input >> configuration;
    if (!configuration.is_object()) {
      return Result<Json>::failure(
          {ErrorCode::cyan_invalid, ".cyan config.json root must be an object", path});
    }
    return Result<Json>::success(std::move(configuration));
  } catch (const Json::exception&) {
    return Result<Json>::failure(
        {ErrorCode::cyan_invalid, ".cyan config.json is invalid JSON", path});
  }
}

Result<void> set_optional_wide(const Json& configuration, std::string_view key,
                               std::optional<std::wstring>& target) {
  const auto found = configuration.find(key);
  if (found == configuration.end()) {
    return Result<void>::success();
  }
  if (!found->is_string()) {
    return Result<void>::failure(
        {ErrorCode::cyan_invalid, ".cyan string option has the wrong JSON type", {}});
  }
  auto decoded = platform::wide_from_utf8(found->get<std::string>());
  if (!decoded) {
    return Result<void>::failure(decoded.error());
  }
  target = decoded.take_value();
  return Result<void>::success();
}

Result<void> apply_boolean(const Json& configuration, std::string_view long_key,
                           std::string_view short_key, bool& target) {
  auto found = configuration.find(long_key);
  if (found == configuration.end() && !short_key.empty()) {
    found = configuration.find(short_key);
  }
  if (found == configuration.end()) {
    return Result<void>::success();
  }
  if (!found->is_boolean()) {
    return Result<void>::failure(
        {ErrorCode::cyan_invalid, ".cyan boolean option has the wrong JSON type", {}});
  }
  target = found->get<bool>();
  return Result<void>::success();
}

Result<void> apply_embedded_file(const Json& configuration, std::string_view key,
                                 const std::filesystem::path& extracted, std::wstring_view filename,
                                 std::optional<std::filesystem::path>& target) {
  const auto found = configuration.find(key);
  if (found == configuration.end()) {
    return Result<void>::success();
  }
  if (!found->is_boolean() || !found->get<bool>()) {
    return Result<void>::failure(
        {ErrorCode::cyan_invalid, ".cyan embedded-file marker must be true", extracted});
  }
  const auto file = extracted / filename;
  std::error_code error;
  if (!std::filesystem::is_regular_file(file, error) || error) {
    return Result<void>::failure({ErrorCode::cyan_invalid, ".cyan embedded file is missing", file});
  }
  target = file;
  return Result<void>::success();
}

}  // namespace

Result<void> CyanArchiveWriter::write(const CgenOptions& options) const {
  auto workspace_result = TemporaryWorkspace::create();
  if (!workspace_result) {
    return Result<void>::failure(workspace_result.error());
  }
  auto workspace = workspace_result.take_value();
  const auto staging = workspace.path() / L"cyan";

  std::error_code filesystem_error;
  std::filesystem::create_directories(staging, filesystem_error);
  if (filesystem_error) {
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not create .cyan staging directory", staging});
  }

  Json configuration = Json::object();
  for (const auto& item : std::array<std::pair<std::string_view, std::optional<std::wstring>>, 4>{
           std::pair<std::string_view, std::optional<std::wstring>>{"n", options.name},
           {"v", options.version},
           {"b", options.bundle_id},
           {"m", options.minimum_os}}) {
    auto added = add_optional_string(configuration, item.first, item.second);
    if (!added) {
      return added;
    }
  }

  if (!options.injected_items.empty()) {
    configuration["f"] = true;
    const auto inject_root = staging / L"inject";
    std::filesystem::create_directories(inject_root, filesystem_error);
    if (filesystem_error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not create .cyan inject directory", inject_root});
    }
    for (const auto& source : options.injected_items) {
      auto copied = copy_tree(source, inject_root / source.filename());
      if (!copied) {
        return copied;
      }
    }
  }

  if (options.icon) {
    configuration["k"] = true;
  }
  if (options.merge_plist) {
    configuration["l"] = true;
  }
  if (options.entitlements) {
    configuration["x"] = true;
  }

  if (options.remove_supported_devices) {
    configuration["remove_supported_devices"] = true;
  }
  if (options.no_watch) {
    configuration["no_watch"] = true;
  }
  if (options.enable_documents) {
    configuration["enable_documents"] = true;
  }
  if (options.fakesign) {
    configuration["fakesign"] = true;
  }
  if (options.thin) {
    configuration["thin"] = true;
  }
  if (options.remove_extensions) {
    configuration["remove_extensions"] = true;
  }
  if (options.remove_encrypted) {
    configuration["remove_encrypted"] = true;
  }

  const auto configuration_path = staging / L"config.json";
  {
    std::ofstream output(configuration_path, std::ios::binary | std::ios::trunc);
    if (!output) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not create .cyan config.json", configuration_path});
    }
    output << configuration.dump();
    if (!output) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not write .cyan config.json", configuration_path});
    }
  }

  auto icon = stage_named_file(options.icon, staging, L"icon.idk");
  if (!icon) {
    return icon;
  }
  auto plist = stage_named_file(options.merge_plist, staging, L"merge.plist");
  if (!plist) {
    return plist;
  }
  auto entitlements = stage_named_file(options.entitlements, staging, L"new.entitlements");
  if (!entitlements) {
    return entitlements;
  }

  ArchiveService archives;
  return archives.create_zip(staging, options.output, 1, false);
}

Result<void> CyanArchiveReader::apply(const std::filesystem::path& archive,
                                      const std::filesystem::path& extraction_root,
                                      CyanOptions& options) const {
  ArchiveService archives;
  ExtractionLimits limits;
  limits.maximum_entries = 20'000;
  limits.maximum_file_size = 512ULL * 1024ULL * 1024ULL;
  limits.maximum_total_size = 2ULL * 1024ULL * 1024ULL * 1024ULL;
  auto extracted = archives.extract(archive, extraction_root, limits);
  if (!extracted) {
    return Result<void>::failure({ErrorCode::cyan_invalid, extracted.error().message, archive});
  }

  auto configuration_result = read_configuration(extraction_root / L"config.json");
  if (!configuration_result) {
    return Result<void>::failure(configuration_result.error());
  }
  const Json& configuration = configuration_result.value();

  for (auto item : std::array<std::pair<std::string_view, std::optional<std::wstring>*>, 4>{
           std::pair<std::string_view, std::optional<std::wstring>*>{"n", &options.name},
           {"v", &options.version},
           {"b", &options.bundle_id},
           {"m", &options.minimum_os}}) {
    auto applied = set_optional_wide(configuration, item.first, *item.second);
    if (!applied) {
      return applied;
    }
  }

  const auto inject_marker = configuration.find("f");
  if (inject_marker != configuration.end()) {
    if (!inject_marker->is_boolean() || !inject_marker->get<bool>()) {
      return Result<void>::failure(
          {ErrorCode::cyan_invalid, ".cyan inject marker must be true", archive});
    }
    const auto inject_root = extraction_root / L"inject";
    std::error_code error;
    if (!std::filesystem::is_directory(inject_root, error) || error) {
      return Result<void>::failure(
          {ErrorCode::cyan_invalid, ".cyan inject directory is missing", archive});
    }
    std::filesystem::directory_iterator iterator(inject_root, error);
    const std::filesystem::directory_iterator end;
    if (error) {
      return Result<void>::failure(
          {ErrorCode::cyan_invalid, "could not enumerate .cyan inject directory", inject_root});
    }
    for (; iterator != end; iterator.increment(error)) {
      if (error) {
        return Result<void>::failure(
            {ErrorCode::cyan_invalid, "could not enumerate .cyan inject directory", inject_root});
      }
      options.injected_items.push_back(iterator->path());
    }
  }

  for (auto file : std::array<
           std::tuple<std::string_view, std::wstring_view, std::optional<std::filesystem::path>*>,
           3>{std::tuple{std::string_view{"k"}, std::wstring_view{L"icon.idk"}, &options.icon},
              {std::string_view{"l"}, std::wstring_view{L"merge.plist"}, &options.merge_plist},
              {std::string_view{"x"}, std::wstring_view{L"new.entitlements"},
               &options.entitlements}}) {
    auto applied = apply_embedded_file(configuration, std::get<0>(file), extraction_root,
                                       std::get<1>(file), *std::get<2>(file));
    if (!applied) {
      return applied;
    }
  }

  for (auto flag : std::array<std::tuple<std::string_view, std::string_view, bool*>, 7>{
           std::tuple{std::string_view{"remove_supported_devices"}, std::string_view{"u"},
                      &options.remove_supported_devices},
           {std::string_view{"no_watch"}, std::string_view{"w"}, &options.no_watch},
           {std::string_view{"enable_documents"}, std::string_view{"d"}, &options.enable_documents},
           {std::string_view{"fakesign"}, std::string_view{"s"}, &options.fakesign},
           {std::string_view{"thin"}, std::string_view{"q"}, &options.thin},
           {std::string_view{"remove_extensions"}, std::string_view{"e"},
            &options.remove_extensions},
           {std::string_view{"remove_encrypted"}, std::string_view{"g"},
            &options.remove_encrypted}}) {
    auto applied =
        apply_boolean(configuration, std::get<0>(flag), std::get<1>(flag), *std::get<2>(flag));
    if (!applied) {
      return applied;
    }
  }
  return Result<void>::success();
}

}  // namespace cyan
