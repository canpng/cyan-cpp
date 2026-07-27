#include "cyan/core/cli_parser.hpp"

#include <cerrno>
#include <cwchar>
#include <limits>
#include <optional>
#include <utility>

#include "cyan/platform/utf.hpp"

namespace cyan {
namespace {

constexpr std::wstring_view cyan_help =
    LR"(usage: cyan -i input [-o output] [options]

cyan, an azule-compatible tool for modifying iOS apps

required:
  -i, --input PATH             app to modify (.app/.ipa/.tipa)

output:
  -o, --output PATH            output path; if omitted, overwrite input
  -c, --compress LEVEL         IPA compression level 0-9 (default: 6)
      --overwrite              overwrite existing output without prompting

configuration and injection:
  -z, --cyan FILE [FILE ...]   apply one or more .cyan files in order
  -f FILE [FILE ...]           inject/copy files or directories
  -n NAME                      change app name
  -v VERSION                   change app version
  -b BUNDLE_ID                 change bundle identifier
  -m VERSION                   change minimum iOS version
  -k IMAGE                     change app icon
  -l PLIST                     merge plist into Info.plist
  -x PLIST                     merge executable entitlements

bundle operations:
  short flags may be grouped (for example, -uwdsq)
  -u, --remove-supported-devices
  -w, --no-watch
  -d, --enable-documents
  -s, --fakesign
  -q, --thin
  -e, --remove-extensions
  -g, --remove-encrypted
      --ignore-encrypted

native extensions:
      --compatibility-mode cyan
      --dependency-dir PATH
      --ldid PATH               override bundled signer with ldid.exe
      --ipapatch                apply ipapatch v2.1.3 behavior
      --ipapatch-dylib PATH     use a custom ipapatch payload
      --ipapatch-plugins-only   patch app extensions, not the main executable

other:
  -h, --help
      --version
)";

constexpr std::wstring_view cgen_help =
    LR"(usage: cgen -o output.cyan [options]

a tool to generate .cyan files

required:
  -o, --output PATH            output .cyan file

content:
  short flags may be grouped (for example, -uwdsq)
  -f FILE [FILE ...]           items placed below inject/
  -n NAME                      app name
  -v VERSION                   app version
  -b BUNDLE_ID                 bundle identifier
  -m VERSION                   minimum iOS version
  -k IMAGE                     icon.idk
  -l PLIST                     merge.plist
  -x PLIST                     new.entitlements
  -u, --remove-supported-devices
  -w, --no-watch
  -d, --enable-documents
  -s, --fakesign
  -q, --thin
  -e, --remove-extensions
  -g, --remove-encrypted
      --overwrite

other:
  -h, --help
      --version
)";

constexpr std::wstring_view cyan_version =
    L"cyan v1.4.4 (cyan-cpp " CYAN_VERSION_WIDE L")";

Error argument_error(ErrorCode code, std::wstring_view message) {
  auto encoded = platform::utf8_from_wide(message);
  if (encoded) {
    return {code, encoded.take_value(), {}};
  }
  return {code, "invalid command-line argument", {}};
}

bool is_option(std::wstring_view argument) {
  return argument.size() >= 2U && argument.front() == L'-';
}

Result<std::wstring> required_value(const std::vector<std::wstring>& arguments, std::size_t& index,
                                    std::wstring_view option) {
  if (index + 1U >= arguments.size()) {
    return Result<std::wstring>::failure(argument_error(
        ErrorCode::missing_argument, std::wstring(L"missing value for ") + std::wstring(option)));
  }
  ++index;
  return Result<std::wstring>::success(arguments[index]);
}

Result<std::vector<std::filesystem::path>> one_or_more_paths(
    const std::vector<std::wstring>& arguments, std::size_t& index, std::wstring_view option) {
  std::vector<std::filesystem::path> values;
  while (index + 1U < arguments.size() && !is_option(arguments[index + 1U])) {
    ++index;
    std::wstring value = arguments[index];
    if (value == L",") {
      continue;
    }
    if (!value.empty() && value.back() == L',') {
      value.pop_back();
    }
    if (!value.empty()) {
      values.emplace_back(std::move(value));
    }
  }
  if (values.empty()) {
    return Result<std::vector<std::filesystem::path>>::failure(
        argument_error(ErrorCode::missing_argument,
                       std::wstring(L"expected one or more paths after ") + std::wstring(option)));
  }
  return Result<std::vector<std::filesystem::path>>::success(std::move(values));
}

Result<int> compression_level(std::wstring_view text) {
  if (text.empty()) {
    return Result<int>::failure(
        argument_error(ErrorCode::invalid_compression_level, L"compression level must be 0-9"));
  }

  wchar_t* end = nullptr;
  errno = 0;
  const std::wstring owned(text);
  const long parsed = std::wcstol(owned.c_str(), &end, 10);
  if (errno != 0 || end == nullptr || *end != L'\0' || parsed < 0L || parsed > 9L) {
    return Result<int>::failure(
        argument_error(ErrorCode::invalid_compression_level, L"compression level must be 0-9"));
  }
  return Result<int>::success(static_cast<int>(parsed));
}

template <typename Options>
void apply_short_flag(wchar_t flag, Options& options) {
  switch (flag) {
    case L'u':
      options.remove_supported_devices = true;
      break;
    case L'w':
      options.no_watch = true;
      break;
    case L'd':
      options.enable_documents = true;
      break;
    case L's':
      options.fakesign = true;
      break;
    case L'q':
      options.thin = true;
      break;
    case L'e':
      options.remove_extensions = true;
      break;
    case L'g':
      options.remove_encrypted = true;
      break;
    case L'h':
      options.show_help = true;
      break;
  }
}

bool is_short_flag(wchar_t flag) {
  return flag == L'u' || flag == L'w' || flag == L'd' || flag == L's' || flag == L'q' ||
         flag == L'e' || flag == L'g' || flag == L'h';
}

template <typename Options>
void apply_common_flag(std::wstring_view argument, Options& options, bool& matched) {
  matched = true;
  if (argument.size() >= 2U && argument.front() == L'-' && argument[1] != L'-') {
    for (std::size_t index = 1U; index < argument.size(); ++index) {
      if (!is_short_flag(argument[index])) {
        matched = false;
        return;
      }
    }
    for (std::size_t index = 1U; index < argument.size(); ++index) {
      apply_short_flag(argument[index], options);
    }
  } else if (argument == L"--remove-supported-devices") {
    options.remove_supported_devices = true;
  } else if (argument == L"--no-watch") {
    options.no_watch = true;
  } else if (argument == L"--enable-documents") {
    options.enable_documents = true;
  } else if (argument == L"--fakesign") {
    options.fakesign = true;
  } else if (argument == L"--thin") {
    options.thin = true;
  } else if (argument == L"--remove-extensions") {
    options.remove_extensions = true;
  } else if (argument == L"--remove-encrypted") {
    options.remove_encrypted = true;
  } else if (argument == L"--overwrite") {
    options.overwrite = true;
  } else if (argument == L"--help") {
    options.show_help = true;
  } else if (argument == L"--version") {
    options.show_version = true;
  } else {
    matched = false;
  }
}

template <typename Options>
Result<void> apply_common_value(std::wstring_view argument,
                                const std::vector<std::wstring>& arguments, std::size_t& index,
                                Options& options, bool& matched) {
  matched = true;
  auto read = [&](std::wstring_view option) { return required_value(arguments, index, option); };

  Result<std::wstring> value =
      Result<std::wstring>::failure({ErrorCode::internal_error, "uninitialised option", {}});
  if (argument == L"-n") {
    value = read(argument);
    if (value) {
      options.name = value.take_value();
    }
  } else if (argument == L"-v") {
    value = read(argument);
    if (value) {
      options.version = value.take_value();
    }
  } else if (argument == L"-b") {
    value = read(argument);
    if (value) {
      options.bundle_id = value.take_value();
    }
  } else if (argument == L"-m") {
    value = read(argument);
    if (value) {
      options.minimum_os = value.take_value();
    }
  } else if (argument == L"-k") {
    value = read(argument);
    if (value) {
      options.icon = std::filesystem::path(value.take_value());
    }
  } else if (argument == L"-l") {
    value = read(argument);
    if (value) {
      options.merge_plist = std::filesystem::path(value.take_value());
    }
  } else if (argument == L"-x") {
    value = read(argument);
    if (value) {
      options.entitlements = std::filesystem::path(value.take_value());
    }
  } else {
    matched = false;
    return Result<void>::success();
  }

  if (!value) {
    return Result<void>::failure(value.error());
  }
  return Result<void>::success();
}

}  // namespace

Result<CyanOptions> parse_cyan_arguments(const std::vector<std::wstring>& arguments) {
  CyanOptions options;

  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const std::wstring_view argument = arguments[index];
    bool matched = false;
    apply_common_flag(argument, options, matched);
    if (matched) {
      continue;
    }

    auto common = apply_common_value(argument, arguments, index, options, matched);
    if (!common) {
      return Result<CyanOptions>::failure(common.error());
    }
    if (matched) {
      continue;
    }

    if (argument == L"-i" || argument == L"--input") {
      auto value = required_value(arguments, index, argument);
      if (!value) {
        return Result<CyanOptions>::failure(value.error());
      }
      options.input = std::filesystem::path(value.take_value());
    } else if (argument == L"-o" || argument == L"--output") {
      auto value = required_value(arguments, index, argument);
      if (!value) {
        return Result<CyanOptions>::failure(value.error());
      }
      options.output = std::filesystem::path(value.take_value());
    } else if (argument == L"-z" || argument == L"--cyan") {
      auto values = one_or_more_paths(arguments, index, argument);
      if (!values) {
        return Result<CyanOptions>::failure(values.error());
      }
      options.cyan_files = values.take_value();
    } else if (argument == L"-f") {
      auto values = one_or_more_paths(arguments, index, argument);
      if (!values) {
        return Result<CyanOptions>::failure(values.error());
      }
      options.injected_items = values.take_value();
    } else if (argument == L"-c" || argument == L"--compress") {
      auto value = required_value(arguments, index, argument);
      if (!value) {
        return Result<CyanOptions>::failure(value.error());
      }
      auto level = compression_level(value.value());
      if (!level) {
        return Result<CyanOptions>::failure(level.error());
      }
      options.compression_level = level.take_value();
    } else if (argument == L"--ignore-encrypted") {
      options.ignore_encrypted = true;
    } else if (argument == L"--compatibility-mode") {
      auto value = required_value(arguments, index, argument);
      if (!value) {
        return Result<CyanOptions>::failure(value.error());
      }
      if (value.value() != L"cyan") {
        return Result<CyanOptions>::failure(
            argument_error(ErrorCode::invalid_argument, L"compatibility mode must be 'cyan'"));
      }
      options.compatibility_cyan = true;
    } else if (argument == L"--dependency-dir") {
      auto value = required_value(arguments, index, argument);
      if (!value) {
        return Result<CyanOptions>::failure(value.error());
      }
      options.dependency_directory = std::filesystem::path(value.take_value());
    } else if (argument == L"--ldid") {
      auto value = required_value(arguments, index, argument);
      if (!value) {
        return Result<CyanOptions>::failure(value.error());
      }
      options.ldid_path = std::filesystem::path(value.take_value());
    } else if (argument == L"--ipapatch") {
      options.ipapatch = true;
    } else if (argument == L"--ipapatch-dylib") {
      auto value = required_value(arguments, index, argument);
      if (!value) {
        return Result<CyanOptions>::failure(value.error());
      }
      options.ipapatch_dylib = std::filesystem::path(value.take_value());
      options.ipapatch = true;
    } else if (argument == L"--ipapatch-plugins-only") {
      options.ipapatch_plugins_only = true;
      options.ipapatch = true;
    } else {
      return Result<CyanOptions>::failure(
          argument_error(ErrorCode::unknown_option,
                         std::wstring(L"unrecognised option: ") + std::wstring(argument)));
    }
  }

  if (!options.show_help && !options.show_version && options.input.empty()) {
    return Result<CyanOptions>::failure(
        argument_error(ErrorCode::missing_argument, L"-i/--input is required"));
  }
  return Result<CyanOptions>::success(std::move(options));
}

Result<CgenOptions> parse_cgen_arguments(const std::vector<std::wstring>& arguments) {
  CgenOptions options;

  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const std::wstring_view argument = arguments[index];
    bool matched = false;
    apply_common_flag(argument, options, matched);
    if (matched) {
      continue;
    }

    auto common = apply_common_value(argument, arguments, index, options, matched);
    if (!common) {
      return Result<CgenOptions>::failure(common.error());
    }
    if (matched) {
      continue;
    }

    if (argument == L"-o" || argument == L"--output") {
      auto value = required_value(arguments, index, argument);
      if (!value) {
        return Result<CgenOptions>::failure(value.error());
      }
      options.output = std::filesystem::path(value.take_value());
    } else if (argument == L"-f") {
      auto values = one_or_more_paths(arguments, index, argument);
      if (!values) {
        return Result<CgenOptions>::failure(values.error());
      }
      options.injected_items = values.take_value();
    } else {
      return Result<CgenOptions>::failure(
          argument_error(ErrorCode::unknown_option,
                         std::wstring(L"unrecognised option: ") + std::wstring(argument)));
    }
  }

  if (!options.show_help && !options.show_version && options.output.empty()) {
    return Result<CgenOptions>::failure(
        argument_error(ErrorCode::missing_argument, L"-o/--output is required"));
  }
  return Result<CgenOptions>::success(std::move(options));
}

std::wstring_view cyan_help_text() { return cyan_help; }

std::wstring_view cgen_help_text() { return cgen_help; }

std::wstring_view cyan_version_text() { return cyan_version; }

}  // namespace cyan
