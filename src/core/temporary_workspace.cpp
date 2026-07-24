#include "cyan/core/temporary_workspace.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <system_error>

namespace cyan {
namespace {

std::wstring make_workspace_name() {
  std::array<unsigned int, 4> random_words{};
  std::random_device source;
  for (auto& word : random_words) {
    word = source();
  }

  const auto ticks =
      static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
  std::wostringstream stream;
  stream << L"cyan-" << std::hex << std::setfill(L'0') << std::setw(16) << ticks;
  for (const auto word : random_words) {
    stream << std::setw(8) << word;
  }
  return stream.str();
}

bool is_safe_workspace(const std::filesystem::path& path) {
  const auto filename = path.filename().native();
  return !path.empty() && filename.rfind(L"cyan-", 0) == 0 &&
         path.parent_path() == std::filesystem::temp_directory_path();
}

}  // namespace

TemporaryWorkspace::TemporaryWorkspace(std::filesystem::path path) : path_(std::move(path)) {}

TemporaryWorkspace::TemporaryWorkspace(TemporaryWorkspace&& other) noexcept
    : path_(std::move(other.path_)), preserve_(other.preserve_) {
  other.path_.clear();
  other.preserve_ = true;
}

TemporaryWorkspace& TemporaryWorkspace::operator=(TemporaryWorkspace&& other) noexcept {
  if (this != &other) {
    cleanup();
    path_ = std::move(other.path_);
    preserve_ = other.preserve_;
    other.path_.clear();
    other.preserve_ = true;
  }
  return *this;
}

TemporaryWorkspace::~TemporaryWorkspace() { cleanup(); }

Result<TemporaryWorkspace> TemporaryWorkspace::create() {
  std::error_code error;
  const auto temporary_root = std::filesystem::temp_directory_path(error);
  if (error) {
    return Result<TemporaryWorkspace>::failure(
        {ErrorCode::filesystem_error, "could not locate the temporary directory", {}});
  }

  for (int attempt = 0; attempt < 32; ++attempt) {
    const auto candidate = temporary_root / make_workspace_name();
    if (std::filesystem::create_directory(candidate, error)) {
      return Result<TemporaryWorkspace>::success(TemporaryWorkspace(candidate));
    }
    if (error && error != std::errc::file_exists) {
      return Result<TemporaryWorkspace>::failure(
          {ErrorCode::filesystem_error, "could not create a temporary workspace", candidate});
    }
    error.clear();
  }

  return Result<TemporaryWorkspace>::failure(
      {ErrorCode::filesystem_error, "could not allocate a unique temporary workspace", {}});
}

const std::filesystem::path& TemporaryWorkspace::path() const noexcept { return path_; }

void TemporaryWorkspace::preserve() noexcept { preserve_ = true; }

void TemporaryWorkspace::cleanup() noexcept {
  if (preserve_ || path_.empty()) {
    return;
  }

  try {
    if (is_safe_workspace(path_)) {
      std::error_code ignored;
      std::filesystem::remove_all(path_, ignored);
    }
  } catch (...) {
    // Destructors must not throw. The workspace remains for manual cleanup.
  }
  path_.clear();
}

}  // namespace cyan
