#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

#include "cyan/archive/archive_service.hpp"
#include "cyan/core/result.hpp"

namespace cyan {

struct ZipEntryInfo {
  std::string archive_name;
  std::filesystem::path relative_path;
  std::uint64_t uncompressed_size{0};
  std::uint64_t compressed_size{0};
  bool is_directory{false};
};

struct ZipReplacement {
  std::filesystem::path archive_path;
  std::filesystem::path source_path;
  bool executable{true};
};

class ZipUpdateService {
 public:
  Result<std::vector<ZipEntryInfo>> list_entries(const std::filesystem::path& archive_path,
                                                 const ExtractionLimits& limits = {}) const;

  Result<void> extract_entries(const std::filesystem::path& archive_path,
                               const std::filesystem::path& destination,
                               std::span<const std::filesystem::path> entries,
                               const ExtractionLimits& limits = {}) const;

  Result<void> extract_entries(const std::filesystem::path& archive_path,
                               const std::filesystem::path& destination,
                               std::span<const std::filesystem::path> entries,
                               std::span<const ZipEntryInfo> catalog) const;

  Result<void> update(const std::filesystem::path& input, const std::filesystem::path& output,
                      std::span<const ZipReplacement> replacements, int compression_level,
                      std::stop_token stop_token = {}) const;
};

}  // namespace cyan
