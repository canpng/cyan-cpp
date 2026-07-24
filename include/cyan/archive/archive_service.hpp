#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

#include "cyan/core/result.hpp"

namespace cyan {

struct ExtractionLimits {
  std::uint64_t maximum_entries{100'000};
  std::uint64_t maximum_file_size{2ULL * 1024ULL * 1024ULL * 1024ULL};
  std::uint64_t maximum_total_size{8ULL * 1024ULL * 1024ULL * 1024ULL};
  std::uint64_t maximum_expansion_ratio{1'000};
};

class ArchiveService {
 public:
  Result<void> extract(const std::filesystem::path& archive_path,
                       const std::filesystem::path& destination,
                       const ExtractionLimits& limits = {}) const;

  Result<void> create_zip(const std::filesystem::path& source_root,
                          const std::filesystem::path& output, int compression_level,
                          bool exclude_hidden) const;
};

}  // namespace cyan
