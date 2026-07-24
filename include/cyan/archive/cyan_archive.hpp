#pragma once

#include "cyan/core/options.hpp"
#include "cyan/core/result.hpp"

namespace cyan {

class CyanArchiveWriter {
 public:
  Result<void> write(const CgenOptions& options) const;
};

class CyanArchiveReader {
 public:
  Result<void> apply(const std::filesystem::path& archive,
                     const std::filesystem::path& extraction_root, CyanOptions& options) const;
};

}  // namespace cyan
