#include "cyan/platform/utf.hpp"

#include <Windows.h>

#include <limits>

namespace cyan::platform {
namespace {

template <typename T>
bool fits_int(T value) {
  return value <= static_cast<T>((std::numeric_limits<int>::max)());
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

}  // namespace cyan::platform
