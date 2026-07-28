#include "cyan/archive/zip_update_service.hpp"

#include <Windows.h>
#include <zip.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "cyan/archive/archive_path_validator.hpp"
#include "cyan/platform/utf.hpp"

namespace cyan {
namespace {

struct ZipArchiveDeleter {
  void operator()(zip_t* archive) const noexcept {
    if (archive != nullptr) {
      zip_discard(archive);
    }
  }
};

struct ZipFileDeleter {
  void operator()(zip_file_t* file) const noexcept {
    if (file != nullptr) {
      static_cast<void>(zip_fclose(file));
    }
  }
};

using ZipArchive = std::unique_ptr<zip_t, ZipArchiveDeleter>;
using ZipFile = std::unique_ptr<zip_file_t, ZipFileDeleter>;

class TemporaryFileCleanup {
 public:
  explicit TemporaryFileCleanup(std::filesystem::path path) : path_(std::move(path)) {}
  ~TemporaryFileCleanup() {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
  }

 private:
  std::filesystem::path path_;
};

std::string zip_error_message(zip_error_t& error, std::string_view prefix) {
  std::string message(prefix);
  const char* detail = zip_error_strerror(&error);
  if (detail != nullptr) {
    message += ": ";
    message += detail;
  }
  return message;
}

std::string zip_archive_error(zip_t* archive, std::string_view prefix) {
  std::string message(prefix);
  const char* detail = zip_strerror(archive);
  if (detail != nullptr) {
    message += ": ";
    message += detail;
  }
  return message;
}

Result<ZipArchive> open_archive(const std::filesystem::path& path, int flags) {
  zip_error_t error;
  zip_error_init(&error);
  zip_source_t* source = zip_source_win32w_create(path.c_str(), 0U, ZIP_LENGTH_TO_END, &error);
  if (source == nullptr) {
    const std::string message = zip_error_message(error, "could not open ZIP source");
    zip_error_fini(&error);
    return Result<ZipArchive>::failure({ErrorCode::archive_open_failed, message, path});
  }

  zip_t* archive = zip_open_from_source(source, flags, &error);
  if (archive == nullptr) {
    zip_source_free(source);
    const std::string message = zip_error_message(error, "could not open ZIP archive");
    zip_error_fini(&error);
    return Result<ZipArchive>::failure({ErrorCode::archive_open_failed, message, path});
  }
  zip_error_fini(&error);
  return Result<ZipArchive>::success(ZipArchive(archive));
}

bool checked_add(std::uint64_t left, std::uint64_t right, std::uint64_t& output) {
  if (right > (std::numeric_limits<std::uint64_t>::max)() - left) {
    return false;
  }
  output = left + right;
  return true;
}

bool is_reparse_point(const std::filesystem::path& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
}

bool has_reparse_component(const std::filesystem::path& root,
                           const std::filesystem::path& relative) {
  std::filesystem::path current = root;
  for (const auto& component : relative.parent_path()) {
    current /= component;
    if (is_reparse_point(current)) {
      return true;
    }
  }
  return false;
}

std::wstring path_key(const std::filesystem::path& path) {
  return platform::invariant_lower(path.generic_wstring());
}

std::filesystem::path temporary_sibling(const std::filesystem::path& output) {
  return output.parent_path() /
         (output.filename().native() + L".cyan-update-" + std::to_wstring(GetCurrentProcessId()) +
          L"-" + std::to_wstring(GetTickCount64()));
}

Result<void> publish_file(const std::filesystem::path& temporary,
                          const std::filesystem::path& output) {
  if (!MoveFileExW(temporary.c_str(), output.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not atomically publish updated ZIP", output});
  }
  return Result<void>::success();
}

int cancel_update(zip_t*, void* state) {
  return static_cast<std::stop_token*>(state)->stop_requested() ? 1 : 0;
}

}  // namespace

Result<std::vector<ZipEntryInfo>> ZipUpdateService::list_entries(
    const std::filesystem::path& archive_path, const ExtractionLimits& limits) const {
  auto opened = open_archive(archive_path, ZIP_RDONLY);
  if (!opened) {
    return Result<std::vector<ZipEntryInfo>>::failure(opened.error());
  }
  auto archive = opened.take_value();
  const zip_int64_t count = zip_get_num_entries(archive.get(), ZIP_FL_UNCHANGED);
  if (count < 0) {
    return Result<std::vector<ZipEntryInfo>>::failure(
        {ErrorCode::archive_invalid,
         zip_archive_error(archive.get(), "could not count ZIP entries"), archive_path});
  }
  if (static_cast<std::uint64_t>(count) > limits.maximum_entries) {
    return Result<std::vector<ZipEntryInfo>>::failure(
        {ErrorCode::archive_limit_exceeded, "archive entry limit exceeded", archive_path});
  }

  ArchivePathValidator validator;
  std::vector<ZipEntryInfo> entries;
  entries.reserve(static_cast<std::size_t>(count));
  std::uint64_t total_size = 0U;
  for (zip_uint64_t index = 0U; index < static_cast<zip_uint64_t>(count); ++index) {
    zip_stat_t stat{};
    zip_stat_init(&stat);
    if (zip_stat_index(archive.get(), index, ZIP_FL_UNCHANGED, &stat) != 0 ||
        stat.name == nullptr) {
      return Result<std::vector<ZipEntryInfo>>::failure(
          {ErrorCode::archive_invalid,
           zip_archive_error(archive.get(), "could not inspect ZIP entry"), archive_path});
    }
    const std::string name(stat.name);
    auto relative = validator.validate_and_reserve(name);
    if (!relative) {
      return Result<std::vector<ZipEntryInfo>>::failure(relative.error());
    }
    const bool directory = name.ends_with('/');
    if (relative.value().empty()) {
      if (directory) {
        continue;
      }
      return Result<std::vector<ZipEntryInfo>>::failure(
          {ErrorCode::archive_unsafe_path, "archive file resolves to the extraction root",
           archive_path});
    }

    const std::uint64_t size = (stat.valid & ZIP_STAT_SIZE) != 0U ? stat.size : 0U;
    const std::uint64_t compressed = (stat.valid & ZIP_STAT_COMP_SIZE) != 0U ? stat.comp_size : 0U;
    if (!directory &&
        (size > limits.maximum_file_size || !checked_add(total_size, size, total_size) ||
         total_size > limits.maximum_total_size)) {
      return Result<std::vector<ZipEntryInfo>>::failure({ErrorCode::archive_limit_exceeded,
                                                         "archive expanded-size limit exceeded",
                                                         relative.value()});
    }
    if (!directory && compressed > 0U && size > 1024U * 1024U &&
        size / compressed > limits.maximum_expansion_ratio) {
      return Result<std::vector<ZipEntryInfo>>::failure({ErrorCode::archive_limit_exceeded,
                                                         "archive expansion-ratio limit exceeded",
                                                         relative.value()});
    }
    entries.push_back({name, relative.take_value(), size, compressed, directory});
  }
  return Result<std::vector<ZipEntryInfo>>::success(std::move(entries));
}

Result<void> ZipUpdateService::extract_entries(const std::filesystem::path& archive_path,
                                               const std::filesystem::path& destination,
                                               std::span<const std::filesystem::path> requested,
                                               const ExtractionLimits& limits) const {
  auto listed = list_entries(archive_path, limits);
  if (!listed) {
    return Result<void>::failure(listed.error());
  }
  return extract_entries(archive_path, destination, requested, listed.value());
}

Result<void> ZipUpdateService::extract_entries(const std::filesystem::path& archive_path,
                                               const std::filesystem::path& destination,
                                               std::span<const std::filesystem::path> requested,
                                               std::span<const ZipEntryInfo> catalog) const {
  std::unordered_map<std::wstring, const ZipEntryInfo*> available;
  for (const auto& entry : catalog) {
    available.emplace(path_key(entry.relative_path), &entry);
  }

  std::error_code filesystem_error;
  std::filesystem::create_directories(destination, filesystem_error);
  if (filesystem_error || is_reparse_point(destination)) {
    return Result<void>::failure({ErrorCode::archive_unsafe_path,
                                  "could not create a safe selective-extraction root",
                                  destination});
  }

  auto opened = open_archive(archive_path, ZIP_RDONLY);
  if (!opened) {
    return Result<void>::failure(opened.error());
  }
  auto archive = opened.take_value();
  std::unordered_set<std::wstring> extracted;
  std::array<char, 128U * 1024U> buffer{};
  for (const auto& requested_path : requested) {
    const std::wstring key = path_key(requested_path);
    if (!extracted.insert(key).second) {
      continue;
    }
    const auto found = available.find(key);
    if (found == available.end() || found->second->is_directory) {
      return Result<void>::failure(
          {ErrorCode::file_not_found, "required ZIP entry does not exist", requested_path});
    }
    const auto& info = *found->second;
    const auto target = destination / info.relative_path;
    std::filesystem::create_directories(target.parent_path(), filesystem_error);
    if (filesystem_error || has_reparse_component(destination, info.relative_path)) {
      return Result<void>::failure(
          {ErrorCode::archive_unsafe_path, "could not create a safe ZIP entry parent", target});
    }
    if (is_reparse_point(target)) {
      return Result<void>::failure({ErrorCode::archive_unsafe_path,
                                    "selective extraction target is a reparse point", target});
    }

    const zip_int64_t archive_index =
        zip_name_locate(archive.get(), info.archive_name.c_str(), ZIP_FL_ENC_GUESS);
    if (archive_index < 0) {
      return Result<void>::failure({ErrorCode::archive_invalid,
                                    zip_archive_error(archive.get(), "could not locate ZIP entry"),
                                    info.relative_path});
    }
    ZipFile file(
        zip_fopen_index(archive.get(), static_cast<zip_uint64_t>(archive_index), ZIP_FL_UNCHANGED));
    if (!file) {
      return Result<void>::failure({ErrorCode::archive_invalid,
                                    zip_archive_error(archive.get(), "could not open ZIP entry"),
                                    info.relative_path});
    }
    std::ofstream output(target, std::ios::binary | std::ios::trunc);
    if (!output) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not create extracted ZIP entry", target});
    }
    std::uint64_t written = 0U;
    while (true) {
      const zip_int64_t received =
          zip_fread(file.get(), buffer.data(), static_cast<zip_uint64_t>(buffer.size()));
      if (received < 0) {
        return Result<void>::failure(
            {ErrorCode::archive_invalid, "could not read ZIP entry data", info.relative_path});
      }
      if (received == 0) {
        break;
      }
      if (!checked_add(written, static_cast<std::uint64_t>(received), written) ||
          written > info.uncompressed_size) {
        return Result<void>::failure({ErrorCode::archive_invalid,
                                      "ZIP entry exceeded its declared size", info.relative_path});
      }
      output.write(buffer.data(), static_cast<std::streamsize>(received));
      if (!output) {
        return Result<void>::failure(
            {ErrorCode::filesystem_error, "could not write extracted ZIP entry", target});
      }
    }
    if (written != info.uncompressed_size) {
      return Result<void>::failure({ErrorCode::archive_invalid,
                                    "ZIP entry size does not match its data", info.relative_path});
    }
  }
  return Result<void>::success();
}

Result<void> ZipUpdateService::update(const std::filesystem::path& input,
                                      const std::filesystem::path& output,
                                      std::span<const ZipReplacement> replacements,
                                      int compression_level, std::stop_token stop_token) const {
  if (compression_level < 0 || compression_level > 9) {
    return Result<void>::failure(
        {ErrorCode::invalid_compression_level, "ZIP compression level must be 0-9", output});
  }
  if (stop_token.stop_requested()) {
    return Result<void>::failure(
        {ErrorCode::operation_cancelled, "ZIP update was cancelled", output});
  }
  auto listed = list_entries(input);
  if (!listed) {
    return Result<void>::failure(listed.error());
  }

  std::unordered_map<std::wstring, const ZipEntryInfo*> existing;
  for (const auto& entry : listed.value()) {
    existing.emplace(path_key(entry.relative_path), &entry);
  }
  ArchivePathValidator replacement_validator;
  std::unordered_set<std::wstring> replacement_keys;
  for (const auto& replacement : replacements) {
    auto encoded = platform::utf8_from_wide(replacement.archive_path.generic_wstring());
    if (!encoded) {
      return Result<void>::failure(encoded.error());
    }
    auto validated = replacement_validator.validate_and_reserve(encoded.value());
    if (!validated) {
      return Result<void>::failure(validated.error());
    }
    const std::wstring key = path_key(validated.value());
    if (!replacement_keys.insert(key).second) {
      return Result<void>::failure({ErrorCode::archive_duplicate_path,
                                    "ZIP update contains duplicate replacement paths",
                                    replacement.archive_path});
    }
    std::error_code error;
    if (!std::filesystem::is_regular_file(replacement.source_path, error) || error ||
        is_reparse_point(replacement.source_path)) {
      return Result<void>::failure({ErrorCode::file_not_found,
                                    "ZIP replacement source does not exist",
                                    replacement.source_path});
    }
  }

  std::error_code filesystem_error;
  if (!output.parent_path().empty()) {
    std::filesystem::create_directories(output.parent_path(), filesystem_error);
    if (filesystem_error) {
      return Result<void>::failure({ErrorCode::filesystem_error,
                                    "could not create ZIP output directory", output.parent_path()});
    }
  }
  const auto temporary = temporary_sibling(output);
  if (!CopyFileW(input.c_str(), temporary.c_str(), TRUE)) {
    return Result<void>::failure({ErrorCode::filesystem_error,
                                  "could not stage input IPA for atomic ZIP update", temporary});
  }
  TemporaryFileCleanup temporary_cleanup(temporary);

  auto cleanup = [&]() {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
  };
  auto opened = open_archive(temporary, 0);
  if (!opened) {
    cleanup();
    return Result<void>::failure(opened.error());
  }
  auto archive = opened.take_value();
  zip_register_cancel_callback_with_state(archive.get(), cancel_update, nullptr, &stop_token);

  for (const auto& replacement : replacements) {
    const std::wstring key = path_key(replacement.archive_path);
    const auto found = existing.find(key);
    std::string archive_name;
    if (found != existing.end()) {
      archive_name = found->second->archive_name;
    } else {
      auto encoded = platform::utf8_from_wide(replacement.archive_path.generic_wstring());
      if (!encoded) {
        cleanup();
        return Result<void>::failure(encoded.error());
      }
      archive_name = encoded.take_value();
    }

    zip_uint8_t operating_system = ZIP_OPSYS_UNIX;
    zip_uint32_t attributes =
        static_cast<zip_uint32_t>((replacement.executable ? 0100755U : 0100644U) << 16U);
    zip_int64_t entry_index =
        zip_name_locate(archive.get(), archive_name.c_str(), ZIP_FL_ENC_GUESS);
    if (entry_index >= 0) {
      static_cast<void>(
          zip_file_get_external_attributes(archive.get(), static_cast<zip_uint64_t>(entry_index),
                                           ZIP_FL_UNCHANGED, &operating_system, &attributes));
    }

    zip_source_t* source =
        zip_source_win32w(archive.get(), replacement.source_path.c_str(), 0U, ZIP_LENGTH_TO_END);
    if (source == nullptr) {
      const Error error{ErrorCode::archive_write_failed,
                        zip_archive_error(archive.get(), "could not open replacement source"),
                        replacement.source_path};
      cleanup();
      return Result<void>::failure(error);
    }
    if (entry_index >= 0) {
      if (zip_file_replace(archive.get(), static_cast<zip_uint64_t>(entry_index), source, 0) != 0) {
        zip_source_free(source);
        const Error error{ErrorCode::archive_write_failed,
                          zip_archive_error(archive.get(), "could not replace ZIP entry"),
                          replacement.archive_path};
        cleanup();
        return Result<void>::failure(error);
      }
    } else {
      entry_index = zip_file_add(archive.get(), archive_name.c_str(), source, ZIP_FL_ENC_UTF_8);
      if (entry_index < 0) {
        zip_source_free(source);
        const Error error{ErrorCode::archive_write_failed,
                          zip_archive_error(archive.get(), "could not add ZIP entry"),
                          replacement.archive_path};
        cleanup();
        return Result<void>::failure(error);
      }
    }
    if (zip_set_file_compression(archive.get(), static_cast<zip_uint64_t>(entry_index),
                                 ZIP_CM_DEFLATE,
                                 static_cast<zip_uint32_t>(compression_level)) != 0 ||
        zip_file_set_external_attributes(archive.get(), static_cast<zip_uint64_t>(entry_index), 0,
                                         operating_system, attributes) != 0) {
      const Error error{ErrorCode::archive_write_failed,
                        zip_archive_error(archive.get(), "could not configure ZIP entry"),
                        replacement.archive_path};
      cleanup();
      return Result<void>::failure(error);
    }
  }

  zip_t* raw_archive = archive.release();
  if (zip_close(raw_archive) != 0) {
    const std::string message =
        zip_archive_error(raw_archive, "could not finish atomic ZIP update");
    zip_discard(raw_archive);
    cleanup();
    const ErrorCode code = stop_token.stop_requested() ? ErrorCode::operation_cancelled
                                                       : ErrorCode::archive_write_failed;
    return Result<void>::failure({code, message, temporary});
  }
  if (stop_token.stop_requested()) {
    cleanup();
    return Result<void>::failure(
        {ErrorCode::operation_cancelled, "ZIP update was cancelled", output});
  }

  auto verified = list_entries(temporary);
  if (!verified) {
    cleanup();
    return Result<void>::failure(
        {ErrorCode::verification_failed,
         "updated ZIP failed structural validation: " + verified.error().message, temporary});
  }
  std::unordered_set<std::wstring> published_entries;
  for (const auto& entry : verified.value()) {
    published_entries.insert(path_key(entry.relative_path));
  }
  for (const auto& replacement : replacements) {
    if (!published_entries.contains(path_key(replacement.archive_path))) {
      cleanup();
      return Result<void>::failure({ErrorCode::verification_failed,
                                    "updated ZIP is missing a replacement entry",
                                    replacement.archive_path});
    }
  }

  auto published = publish_file(temporary, output);
  if (!published) {
    cleanup();
    return published;
  }
  return Result<void>::success();
}

}  // namespace cyan
