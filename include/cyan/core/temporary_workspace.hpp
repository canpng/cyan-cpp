#pragma once

#include <filesystem>

#include "cyan/core/result.hpp"

namespace cyan {

class TemporaryWorkspace {
 public:
  TemporaryWorkspace(const TemporaryWorkspace&) = delete;
  TemporaryWorkspace& operator=(const TemporaryWorkspace&) = delete;
  TemporaryWorkspace(TemporaryWorkspace&& other) noexcept;
  TemporaryWorkspace& operator=(TemporaryWorkspace&& other) noexcept;
  ~TemporaryWorkspace();

  static Result<TemporaryWorkspace> create();

  [[nodiscard]] const std::filesystem::path& path() const noexcept;
  void preserve() noexcept;

 private:
  explicit TemporaryWorkspace(std::filesystem::path path);
  void cleanup() noexcept;

  std::filesystem::path path_;
  bool preserve_{false};
};

}  // namespace cyan
