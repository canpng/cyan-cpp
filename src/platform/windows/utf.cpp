#include "cyan/platform/utf.hpp"

#include <Windows.h>

#include <algorithm>
#include <limits>

namespace cyan::platform {
namespace {

template <typename T>
bool fits_int(T value) {
  return value <= static_cast<T>((std::numeric_limits<int>::max)());
}

void write_handle(DWORD standard_handle, std::wstring_view value) {
  if (value.empty()) {
    return;
  }
  const HANDLE handle = GetStdHandle(standard_handle);
  if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
    return;
  }

  DWORD console_mode = 0;
  if (GetConsoleMode(handle, &console_mode) != 0) {
    std::size_t offset = 0;
    while (offset < value.size()) {
      const auto remaining = value.size() - offset;
      const DWORD chunk =
          static_cast<DWORD>((std::min)(remaining, static_cast<std::size_t>(32767U)));
      DWORD written = 0;
      if (WriteConsoleW(handle, value.data() + offset, chunk, &written, nullptr) == 0 ||
          written == 0U) {
        return;
      }
      offset += written;
    }
    return;
  }

  auto encoded = utf8_from_wide(value);
  if (!encoded) {
    return;
  }
  const std::string& bytes = encoded.value();
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const auto remaining = bytes.size() - offset;
    const DWORD chunk =
        static_cast<DWORD>((std::min)(remaining, static_cast<std::size_t>(32767U)));
    DWORD written = 0;
    if (WriteFile(handle, bytes.data() + offset, chunk, &written, nullptr) == 0 ||
        written == 0U) {
      return;
    }
    offset += written;
  }
}

}  // namespace

Result<std::string> utf8_from_wide(std::wstring_view value) {
  if (value.empty()) {
    return Result<std::string>::success({});
  }
  if (!fits_int(value.size())) {
    return Result<std::string>::failure(
        {ErrorCode::invalid_argument, "wide string is too large", {}});
  }

  const int source_size = static_cast<int>(value.size());
  const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), source_size,
                                           nullptr, 0, nullptr, nullptr);
  if (required <= 0) {
    return Result<std::string>::failure(
        {ErrorCode::invalid_utf8, "could not encode UTF-16 as UTF-8", {}});
  }

  std::string output(static_cast<std::size_t>(required), '\0');
  const int written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), source_size,
                                          output.data(), required, nullptr, nullptr);
  if (written != required) {
    return Result<std::string>::failure(
        {ErrorCode::invalid_utf8, "could not encode UTF-16 as UTF-8", {}});
  }
  return Result<std::string>::success(std::move(output));
}

Result<std::wstring> wide_from_utf8(std::string_view value) {
  if (value.empty()) {
    return Result<std::wstring>::success({});
  }
  if (!fits_int(value.size())) {
    return Result<std::wstring>::failure(
        {ErrorCode::invalid_argument, "UTF-8 string is too large", {}});
  }

  const int source_size = static_cast<int>(value.size());
  const int required =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), source_size, nullptr, 0);
  if (required <= 0) {
    return Result<std::wstring>::failure(
        {ErrorCode::invalid_utf8, "archive path is not valid UTF-8", {}});
  }

  std::wstring output(static_cast<std::size_t>(required), L'\0');
  const int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), source_size,
                                          output.data(), required);
  if (written != required) {
    return Result<std::wstring>::failure(
        {ErrorCode::invalid_utf8, "archive path is not valid UTF-8", {}});
  }
  return Result<std::wstring>::success(std::move(output));
}

std::wstring invariant_lower(std::wstring_view value) {
  if (value.empty() || !fits_int(value.size())) {
    return std::wstring(value);
  }

  const int source_size = static_cast<int>(value.size());
  const int required = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, value.data(),
                                     source_size, nullptr, 0, nullptr, nullptr, 0);
  if (required <= 0) {
    return std::wstring(value);
  }

  std::wstring output(static_cast<std::size_t>(required), L'\0');
  const int written = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, value.data(),
                                    source_size, output.data(), required, nullptr, nullptr, 0);
  if (written != required) {
    return std::wstring(value);
  }
  return output;
}

void write_stdout(std::wstring_view value) {
  write_handle(STD_OUTPUT_HANDLE, value);
}

void write_stderr(std::wstring_view value) {
  write_handle(STD_ERROR_HANDLE, value);
}

}  // namespace cyan::platform
