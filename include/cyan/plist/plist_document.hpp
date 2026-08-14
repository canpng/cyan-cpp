#pragma once

#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cyan/core/result.hpp"

namespace cyan {

enum class PlistFormat { xml, binary };

class PlistDocument {
 public:
  PlistDocument();
  ~PlistDocument();
  PlistDocument(PlistDocument&&) noexcept;
  PlistDocument& operator=(PlistDocument&&) noexcept;

  PlistDocument(const PlistDocument&) = delete;
  PlistDocument& operator=(const PlistDocument&) = delete;

  static Result<PlistDocument> create_dictionary();
  static Result<PlistDocument> load(const std::filesystem::path& path);
  static Result<PlistDocument> load_memory(std::span<const std::byte> bytes);

  [[nodiscard]] PlistFormat source_format() const noexcept;
  [[nodiscard]] bool contains(std::string_view key) const;
  [[nodiscard]] std::optional<std::string> string(std::string_view key) const;
  [[nodiscard]] std::optional<std::string> string_path(
      std::initializer_list<std::string_view> keys) const;
  [[nodiscard]] std::vector<std::string> string_array(std::string_view key) const;
  [[nodiscard]] std::vector<std::string> string_array_path(
      std::initializer_list<std::string_view> keys) const;
  [[nodiscard]] std::optional<bool> boolean(std::string_view key) const;

  Result<void> set_string(std::string_view key, std::string_view value);
  Result<void> set_boolean(std::string_view key, bool value);
  Result<void> set_string_array(std::string_view key, const std::vector<std::string>& values);
  Result<void> remove(std::string_view key);
  Result<void> merge(const PlistDocument& overlay);
  Result<void> set_icon_configuration(std::string_view icon_name, std::string_view phone_base_name,
                                      std::string_view tablet_base_name);

  Result<void> save(const std::filesystem::path& path,
                    std::optional<PlistFormat> format = std::nullopt) const;

 private:
  struct Impl;
  explicit PlistDocument(std::unique_ptr<Impl> implementation);

  std::unique_ptr<Impl> implementation_;
};

}  // namespace cyan
