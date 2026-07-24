#include "cyan/archive/archive_path_validator.hpp"

#include <algorithm>
#include <array>
#include <cwctype>
#include <vector>

#include "cyan/platform/utf.hpp"

namespace cyan {
namespace {

bool is_ascii_letter(wchar_t character) {
  return (character >= L'A' && character <= L'Z') || (character >= L'a' && character <= L'z');
}

std::wstring upper_ascii(std::wstring_view value) {
  std::wstring output(value);
  std::transform(output.begin(), output.end(), output.begin(), [](wchar_t character) {
    if (character >= L'a' && character <= L'z') {
      return static_cast<wchar_t>(character - (L'a' - L'A'));
    }
    return character;
  });
  return output;
}

bool is_reserved_device(std::wstring_view component) {
  const std::size_t dot = component.find(L'.');
  const std::wstring stem = upper_ascii(component.substr(0, dot));
  constexpr std::array<std::wstring_view, 6> fixed{L"CON", L"PRN",    L"AUX",
                                                   L"NUL", L"CONIN$", L"CONOUT$"};
  if (std::find(fixed.begin(), fixed.end(), stem) != fixed.end()) {
    return true;
  }
  if (stem.size() == 4U && (stem.rfind(L"COM", 0) == 0 || stem.rfind(L"LPT", 0) == 0) &&
      stem[3] >= L'1' && stem[3] <= L'9') {
    return true;
  }
  return false;
}

Result<std::filesystem::path> unsafe(std::string message) {
  return Result<std::filesystem::path>::failure(
      {ErrorCode::archive_unsafe_path, std::move(message), {}});
}

}  // namespace

Result<std::filesystem::path> ArchivePathValidator::validate_and_reserve(
    std::string_view archive_name) {
  if (archive_name.empty() || archive_name.find('\0') != std::string_view::npos) {
    return unsafe("archive entry has an empty name or embedded NUL");
  }

  auto decoded = platform::wide_from_utf8(archive_name);
  if (!decoded) {
    return Result<std::filesystem::path>::failure(decoded.error());
  }

  std::wstring name = decoded.take_value();
  std::replace(name.begin(), name.end(), L'\\', L'/');
  while (!name.empty() && name.back() == L'/') {
    name.pop_back();
  }
  if (name.empty()) {
    return unsafe("archive entry resolves to an empty path");
  }
  if (name.front() == L'/' || (name.size() >= 2U && is_ascii_letter(name[0]) && name[1] == L':')) {
    return unsafe("absolute, UNC, and drive-qualified archive paths are forbidden");
  }

  std::vector<std::wstring> components;
  std::size_t position = 0;
  while (position <= name.size()) {
    const std::size_t separator = name.find(L'/', position);
    const std::size_t end = separator == std::wstring::npos ? name.size() : separator;
    const std::wstring component = name.substr(position, end - position);

    if (component.empty()) {
      return unsafe("archive entry contains an empty component");
    }
    if (component == L"..") {
      return unsafe("archive entry contains a parent component");
    }
    if (component == L".") {
      if (separator == std::wstring::npos) {
        break;
      }
      position = separator + 1U;
      continue;
    }
    if (component.find(L':') != std::wstring::npos) {
      return unsafe("archive entry contains an NTFS alternate-data-stream separator");
    }
    if (component.back() == L'.' || component.back() == L' ') {
      return unsafe("archive entry contains a component ending in a dot or space");
    }
    if (is_reserved_device(component)) {
      return unsafe("archive entry uses a reserved Windows device name");
    }
    components.push_back(component);

    if (separator == std::wstring::npos) {
      break;
    }
    position = separator + 1U;
  }

  // POSIX tar producers commonly emit "." or "./" as the archive root. The
  // extraction service permits the resulting empty path only for a directory
  // entry and ignores it; regular files may never target the extraction root.
  if (components.empty()) {
    return Result<std::filesystem::path>::success({});
  }

  std::filesystem::path relative;
  std::wstring normalised;
  for (const auto& component : components) {
    relative /= component;
    if (!normalised.empty()) {
      normalised.push_back(L'/');
    }
    normalised += component;
  }

  const std::wstring folded = platform::invariant_lower(normalised);
  if (!reserved_paths_.insert(folded).second) {
    return Result<std::filesystem::path>::failure(
        {ErrorCode::archive_duplicate_path,
         "archive contains duplicate paths after Windows normalisation", relative});
  }
  return Result<std::filesystem::path>::success(std::move(relative));
}

void ArchivePathValidator::clear() { reserved_paths_.clear(); }

}  // namespace cyan
