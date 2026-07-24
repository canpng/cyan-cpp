#include "cyan/archive/archive_service.hpp"

#include <Windows.h>
#include <archive.h>
#include <archive_entry.h>

#include <array>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "cyan/archive/archive_path_validator.hpp"
#include "cyan/platform/utf.hpp"

namespace cyan {
namespace {

struct ArchiveReadDeleter {
  void operator()(archive* handle) const noexcept {
    if (handle != nullptr) {
      archive_read_free(handle);
    }
  }
};

struct ArchiveWriteDeleter {
  void operator()(archive* handle) const noexcept {
    if (handle != nullptr) {
      archive_write_free(handle);
    }
  }
};

struct ArchiveEntryDeleter {
  void operator()(archive_entry* entry) const noexcept { archive_entry_free(entry); }
};

using ReadArchive = std::unique_ptr<archive, ArchiveReadDeleter>;
using WriteArchive = std::unique_ptr<archive, ArchiveWriteDeleter>;
using Entry = std::unique_ptr<archive_entry, ArchiveEntryDeleter>;

std::string archive_message(archive* handle, std::string_view prefix) {
  const char* detail = archive_error_string(handle);
  std::string message(prefix);
  if (detail != nullptr) {
    message += ": ";
    message += detail;
  }
  return message;
}

bool checked_add(std::uint64_t left, std::uint64_t right, std::uint64_t& output) {
  if (right > (std::numeric_limits<std::uint64_t>::max)() - left) {
    return false;
  }
  output = left + right;
  return true;
}

bool has_reparse_component(const std::filesystem::path& root,
                           const std::filesystem::path& relative) {
  std::filesystem::path current = root;
  for (const auto& component : relative.parent_path()) {
    current /= component;
    const DWORD attributes = GetFileAttributesW(current.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
      return true;
    }
  }
  return false;
}

bool is_hidden_relative(const std::filesystem::path& relative) {
  for (const auto& component_path : relative) {
    const std::wstring component = component_path.native();
    if (!component.empty() && component.front() == L'.') {
      return true;
    }
  }
  return false;
}

bool is_macho_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::array<unsigned char, 4> magic{};
  input.read(reinterpret_cast<char*>(magic.data()), static_cast<std::streamsize>(magic.size()));
  if (input.gcount() != static_cast<std::streamsize>(magic.size())) {
    return false;
  }
  const std::uint32_t little =
      static_cast<std::uint32_t>(magic[0]) | (static_cast<std::uint32_t>(magic[1]) << 8U) |
      (static_cast<std::uint32_t>(magic[2]) << 16U) | (static_cast<std::uint32_t>(magic[3]) << 24U);
  return little == 0xfeedfaceU || little == 0xcefaedfeU || little == 0xfeedfacfU ||
         little == 0xcffaedfeU || little == 0xcafebabeU || little == 0xbebafecaU ||
         little == 0xcafebabfU || little == 0xbfbafecaU;
}

Result<void> publish_file(const std::filesystem::path& temporary,
                          const std::filesystem::path& output) {
  if (!MoveFileExW(temporary.c_str(), output.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not atomically publish output", output});
  }
  return Result<void>::success();
}

std::filesystem::path temporary_sibling(const std::filesystem::path& output) {
  const auto ticks = GetTickCount64();
  return output.parent_path() /
         (output.filename().native() + L".cyan-tmp-" + std::to_wstring(ticks));
}

Result<void> write_entry_data(archive* writer, const std::filesystem::path& source,
                              std::uintmax_t file_size) {
  std::ifstream input(source, std::ios::binary);
  if (!input) {
    return Result<void>::failure(
        {ErrorCode::archive_write_failed, "could not open source file for archiving", source});
  }

  std::array<char, 64U * 1024U> buffer{};
  std::uintmax_t remaining = file_size;
  while (remaining > 0U) {
    const std::uintmax_t chunk = (std::min)(remaining, static_cast<std::uintmax_t>(buffer.size()));
    input.read(buffer.data(), static_cast<std::streamsize>(chunk));
    const auto received = input.gcount();
    if (received <= 0) {
      return Result<void>::failure(
          {ErrorCode::archive_write_failed, "source file ended while archiving", source});
    }
    const la_ssize_t written =
        archive_write_data(writer, buffer.data(), static_cast<std::size_t>(received));
    if (written != received) {
      return Result<void>::failure({ErrorCode::archive_write_failed,
                                    archive_message(writer, "could not write ZIP data"), source});
    }
    remaining -= static_cast<std::uintmax_t>(received);
  }
  return Result<void>::success();
}

}  // namespace

Result<void> ArchiveService::extract(const std::filesystem::path& archive_path,
                                     const std::filesystem::path& destination,
                                     const ExtractionLimits& limits) const {
  ReadArchive reader(archive_read_new());
  if (!reader) {
    return Result<void>::failure(
        {ErrorCode::archive_open_failed, "could not allocate archive reader", archive_path});
  }
  archive_read_support_filter_all(reader.get());
  archive_read_support_format_all(reader.get());
  if (archive_read_open_filename_w(reader.get(), archive_path.c_str(), 64U * 1024U) != ARCHIVE_OK) {
    return Result<void>::failure({ErrorCode::archive_open_failed,
                                  archive_message(reader.get(), "could not open archive"),
                                  archive_path});
  }

  std::error_code filesystem_error;
  std::filesystem::create_directories(destination, filesystem_error);
  if (filesystem_error) {
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not create extraction directory", destination});
  }
  const DWORD root_attributes = GetFileAttributesW(destination.c_str());
  if (root_attributes == INVALID_FILE_ATTRIBUTES ||
      (root_attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
    return Result<void>::failure(
        {ErrorCode::archive_unsafe_path, "extraction root is a reparse point", destination});
  }

  ArchivePathValidator validator;
  std::uint64_t entry_count = 0;
  std::uint64_t total_size = 0;
  archive_entry* raw_entry = nullptr;

  int status = ARCHIVE_OK;
  while ((status = archive_read_next_header(reader.get(), &raw_entry)) == ARCHIVE_OK) {
    ++entry_count;
    if (entry_count > limits.maximum_entries) {
      return Result<void>::failure(
          {ErrorCode::archive_limit_exceeded, "archive entry limit exceeded", archive_path});
    }

    const char* utf8_name = archive_entry_pathname_utf8(raw_entry);
    if (utf8_name == nullptr) {
      return Result<void>::failure(
          {ErrorCode::archive_unsafe_path, "archive entry has no UTF-8 path", archive_path});
    }
    auto relative_result = validator.validate_and_reserve(utf8_name);
    if (!relative_result) {
      return Result<void>::failure(relative_result.error());
    }
    const std::filesystem::path relative = relative_result.take_value();
    const std::filesystem::path target = destination / relative;

    if (archive_entry_symlink(raw_entry) != nullptr ||
        archive_entry_hardlink(raw_entry) != nullptr) {
      return Result<void>::failure(
          {ErrorCode::archive_unsafe_path, "archive links are not extracted", relative});
    }

    const auto file_type = archive_entry_filetype(raw_entry);
    if (relative.empty()) {
      if (file_type == AE_IFDIR) {
        continue;
      }
      return Result<void>::failure({ErrorCode::archive_unsafe_path,
                                    "archive file resolves to the extraction root", relative});
    }
    if (has_reparse_component(destination, relative)) {
      return Result<void>::failure(
          {ErrorCode::archive_unsafe_path, "archive path crosses a reparse point", relative});
    }

    if (file_type == AE_IFDIR) {
      std::filesystem::create_directories(target, filesystem_error);
      if (filesystem_error) {
        return Result<void>::failure(
            {ErrorCode::filesystem_error, "could not create archive directory", target});
      }
      continue;
    }
    if (file_type != AE_IFREG) {
      return Result<void>::failure({ErrorCode::archive_unsafe_path,
                                    "archive contains an unsupported special file", relative});
    }

    std::uint64_t declared_size = 0;
    if (archive_entry_size_is_set(raw_entry) != 0) {
      const la_int64_t signed_size = archive_entry_size(raw_entry);
      if (signed_size < 0) {
        return Result<void>::failure(
            {ErrorCode::archive_invalid, "archive entry has a negative size", relative});
      }
      declared_size = static_cast<std::uint64_t>(signed_size);
      if (declared_size > limits.maximum_file_size ||
          !checked_add(total_size, declared_size, total_size) ||
          total_size > limits.maximum_total_size) {
        return Result<void>::failure(
            {ErrorCode::archive_limit_exceeded, "archive expanded-size limit exceeded", relative});
      }
    }

    std::filesystem::create_directories(target.parent_path(), filesystem_error);
    if (filesystem_error || has_reparse_component(destination, relative)) {
      return Result<void>::failure(
          {ErrorCode::archive_unsafe_path, "could not create a safe archive parent path", target});
    }
    std::ofstream output(target, std::ios::binary | std::ios::trunc);
    if (!output) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not create extracted file", target});
    }

    std::uint64_t actual_size = 0;
    const void* block = nullptr;
    std::size_t block_size = 0;
    la_int64_t block_offset = 0;
    int data_status = ARCHIVE_OK;
    while ((data_status = archive_read_data_block(reader.get(), &block, &block_size,
                                                  &block_offset)) == ARCHIVE_OK) {
      if (block_offset < 0) {
        return Result<void>::failure(
            {ErrorCode::archive_invalid, "archive entry has a negative data offset", relative});
      }
      const auto unsigned_offset = static_cast<std::uint64_t>(block_offset);
      std::uint64_t block_end = 0;
      if (!checked_add(unsigned_offset, static_cast<std::uint64_t>(block_size), block_end) ||
          block_end > limits.maximum_file_size) {
        return Result<void>::failure(
            {ErrorCode::archive_limit_exceeded, "archive file-size limit exceeded", relative});
      }
      output.seekp(static_cast<std::streamoff>(block_offset));
      output.write(static_cast<const char*>(block), static_cast<std::streamsize>(block_size));
      if (!output) {
        return Result<void>::failure(
            {ErrorCode::filesystem_error, "could not write extracted file", target});
      }
      actual_size = (std::max)(actual_size, block_end);
    }
    if (data_status != ARCHIVE_EOF) {
      return Result<void>::failure({ErrorCode::archive_invalid,
                                    archive_message(reader.get(), "could not read archive entry"),
                                    relative});
    }
    if (archive_entry_size_is_set(raw_entry) == 0) {
      if (!checked_add(total_size, actual_size, total_size) ||
          total_size > limits.maximum_total_size) {
        return Result<void>::failure(
            {ErrorCode::archive_limit_exceeded, "archive expanded-size limit exceeded", relative});
      }
    } else if (actual_size != declared_size) {
      return Result<void>::failure(
          {ErrorCode::archive_invalid, "archive entry size does not match its data", relative});
    }

    const la_int64_t compressed = archive_filter_bytes(reader.get(), -1);
    if (compressed > 0 && total_size > 1024U * 1024U &&
        total_size / static_cast<std::uint64_t>(compressed) > limits.maximum_expansion_ratio) {
      return Result<void>::failure({ErrorCode::archive_limit_exceeded,
                                    "archive expansion-ratio limit exceeded", archive_path});
    }
  }

  if (status != ARCHIVE_EOF) {
    return Result<void>::failure({ErrorCode::archive_invalid,
                                  archive_message(reader.get(), "could not read archive headers"),
                                  archive_path});
  }
  return Result<void>::success();
}

Result<void> ArchiveService::create_zip(const std::filesystem::path& source_root,
                                        const std::filesystem::path& output, int compression_level,
                                        bool exclude_hidden) const {
  if (compression_level < 0 || compression_level > 9) {
    return Result<void>::failure(
        {ErrorCode::invalid_compression_level, "ZIP compression level must be 0-9", output});
  }

  std::error_code filesystem_error;
  if (!std::filesystem::is_directory(source_root, filesystem_error) || filesystem_error) {
    return Result<void>::failure(
        {ErrorCode::path_not_directory, "ZIP source root is not a directory", source_root});
  }
  if (!output.parent_path().empty()) {
    std::filesystem::create_directories(output.parent_path(), filesystem_error);
    if (filesystem_error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not create output directory", output.parent_path()});
    }
  }

  const std::filesystem::path temporary = temporary_sibling(output);
  WriteArchive writer(archive_write_new());
  if (!writer) {
    return Result<void>::failure(
        {ErrorCode::archive_write_failed, "could not allocate ZIP writer", output});
  }
  if (archive_write_set_format_zip(writer.get()) != ARCHIVE_OK) {
    return Result<void>::failure({ErrorCode::archive_write_failed,
                                  archive_message(writer.get(), "could not select ZIP format"),
                                  output});
  }
  if (archive_write_set_options(writer.get(), "hdrcharset=UTF-8") != ARCHIVE_OK) {
    return Result<void>::failure(
        {ErrorCode::archive_write_failed,
         archive_message(writer.get(), "could not select UTF-8 ZIP headers"), output});
  }
  const std::string level = std::to_string(compression_level);
  if (archive_write_set_format_option(writer.get(), "zip", "compression-level", level.c_str()) !=
      ARCHIVE_OK) {
    return Result<void>::failure(
        {ErrorCode::archive_write_failed,
         archive_message(writer.get(), "could not set ZIP compression level"), output});
  }
  if (archive_write_open_filename_w(writer.get(), temporary.c_str()) != ARCHIVE_OK) {
    return Result<void>::failure({ErrorCode::archive_write_failed,
                                  archive_message(writer.get(), "could not create ZIP"),
                                  temporary});
  }

  std::filesystem::recursive_directory_iterator iterator(
      source_root, std::filesystem::directory_options::none, filesystem_error);
  if (filesystem_error) {
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not enumerate ZIP source", source_root});
  }
  const std::filesystem::recursive_directory_iterator end;
  for (; iterator != end; iterator.increment(filesystem_error)) {
    if (filesystem_error) {
      archive_write_close(writer.get());
      std::filesystem::remove(temporary, filesystem_error);
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not enumerate ZIP source", source_root});
    }

    const auto source = iterator->path();
    const auto relative = std::filesystem::relative(source, source_root, filesystem_error);
    if (filesystem_error) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not make ZIP entry path relative", source});
    }
    if (exclude_hidden && is_hidden_relative(relative)) {
      if (iterator->is_directory(filesystem_error)) {
        iterator.disable_recursion_pending();
      }
      continue;
    }
    const DWORD source_attributes = GetFileAttributesW(source.c_str());
    if (iterator->is_symlink(filesystem_error) || source_attributes == INVALID_FILE_ATTRIBUTES ||
        (source_attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
      return Result<void>::failure(
          {ErrorCode::archive_unsafe_path, "refusing to archive a link or reparse point", source});
    }

    const bool is_directory = iterator->is_directory(filesystem_error);
    const bool is_regular = iterator->is_regular_file(filesystem_error);
    if (filesystem_error || (!is_directory && !is_regular)) {
      return Result<void>::failure(
          {ErrorCode::archive_unsafe_path, "unsupported file type in ZIP source", source});
    }

    std::wstring portable_wide = relative.generic_wstring();
    if (is_directory) {
      portable_wide.push_back(L'/');
    }
    auto portable = platform::utf8_from_wide(portable_wide);
    if (!portable) {
      return Result<void>::failure(portable.error());
    }

    Entry entry(archive_entry_new());
    if (!entry) {
      return Result<void>::failure(
          {ErrorCode::archive_write_failed, "could not allocate ZIP entry", source});
    }
    archive_entry_set_pathname_utf8(entry.get(), portable.value().c_str());
    archive_entry_set_filetype(entry.get(), is_directory ? AE_IFDIR : AE_IFREG);
    archive_entry_set_perm(entry.get(),
                           is_directory || (is_regular && is_macho_file(source)) ? 0755 : 0644);

    std::uintmax_t file_size = 0;
    if (is_regular) {
      file_size = iterator->file_size(filesystem_error);
      if (filesystem_error ||
          file_size > static_cast<std::uintmax_t>((std::numeric_limits<la_int64_t>::max)())) {
        return Result<void>::failure(
            {ErrorCode::archive_write_failed, "could not determine safe ZIP entry size", source});
      }
      archive_entry_set_size(entry.get(), static_cast<la_int64_t>(file_size));
    } else {
      archive_entry_set_size(entry.get(), 0);
    }

    if (archive_write_header(writer.get(), entry.get()) != ARCHIVE_OK) {
      return Result<void>::failure({ErrorCode::archive_write_failed,
                                    archive_message(writer.get(), "could not write ZIP header"),
                                    source});
    }
    if (is_regular) {
      auto data = write_entry_data(writer.get(), source, file_size);
      if (!data) {
        return data;
      }
    }
  }

  if (archive_write_close(writer.get()) != ARCHIVE_OK) {
    std::filesystem::remove(temporary, filesystem_error);
    return Result<void>::failure({ErrorCode::archive_write_failed,
                                  archive_message(writer.get(), "could not finish ZIP"),
                                  temporary});
  }
  writer.reset();

  auto published = publish_file(temporary, output);
  if (!published) {
    std::filesystem::remove(temporary, filesystem_error);
    return published;
  }
  return Result<void>::success();
}

}  // namespace cyan
