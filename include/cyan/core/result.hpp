#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace cyan {

enum class ErrorCode {
  none = 0,
  invalid_argument,
  missing_argument,
  unknown_option,
  file_not_found,
  path_not_directory,
  output_exists,
  invalid_input_type,
  invalid_output_type,
  invalid_compression_level,
  invalid_version,
  invalid_utf8,
  filesystem_error,
  archive_open_failed,
  archive_invalid,
  archive_unsafe_path,
  archive_duplicate_path,
  archive_limit_exceeded,
  archive_write_failed,
  json_invalid,
  cyan_invalid,
  macho_invalid,
  macho_unsupported,
  macho_encrypted,
  injection_failed,
  signing_backend_unavailable,
  signing_failed,
  feature_unavailable,
  verification_failed,
  internal_error
};

struct Error {
  ErrorCode code{ErrorCode::none};
  std::string message;
  std::filesystem::path path;
};

template <typename T>
class Result {
 public:
  static Result success(T value) { return Result(std::move(value)); }

  static Result failure(Error error) { return Result(std::move(error)); }

  [[nodiscard]] bool has_value() const noexcept { return std::holds_alternative<T>(storage_); }

  explicit operator bool() const noexcept { return has_value(); }

  T& value() {
    if (!has_value()) {
      throw std::logic_error("attempted to access a failed cyan::Result");
    }
    return std::get<T>(storage_);
  }

  const T& value() const {
    if (!has_value()) {
      throw std::logic_error("attempted to access a failed cyan::Result");
    }
    return std::get<T>(storage_);
  }

  T&& take_value() {
    if (!has_value()) {
      throw std::logic_error("attempted to access a failed cyan::Result");
    }
    return std::move(std::get<T>(storage_));
  }

  Error& error() {
    if (has_value()) {
      throw std::logic_error("attempted to access an error from a successful cyan::Result");
    }
    return std::get<Error>(storage_);
  }

  const Error& error() const {
    if (has_value()) {
      throw std::logic_error("attempted to access an error from a successful cyan::Result");
    }
    return std::get<Error>(storage_);
  }

 private:
  explicit Result(T value) : storage_(std::move(value)) {}
  explicit Result(Error error) : storage_(std::move(error)) {}

  std::variant<T, Error> storage_;
};

template <>
class Result<void> {
 public:
  static Result success() { return Result(); }

  static Result failure(Error error) { return Result(std::move(error)); }

  [[nodiscard]] bool has_value() const noexcept { return !error_.has_value(); }

  explicit operator bool() const noexcept { return has_value(); }

  Error& error() {
    if (!error_) {
      throw std::logic_error("attempted to access an error from a successful cyan::Result");
    }
    return *error_;
  }

  const Error& error() const {
    if (!error_) {
      throw std::logic_error("attempted to access an error from a successful cyan::Result");
    }
    return *error_;
  }

 private:
  Result() = default;
  explicit Result(Error error) : error_(std::move(error)) {}

  std::optional<Error> error_;
};

}  // namespace cyan
