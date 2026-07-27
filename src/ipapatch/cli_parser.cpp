#include "cyan/ipapatch/cli_parser.hpp"

#include <utility>

#include "cyan/platform/utf.hpp"

namespace cyan {
namespace {

constexpr std::wstring_view help_text =
    LR"(usage: ipapatch --input PATH [--output PATH | --inplace] [options]

patch an iOS app and its extensions with zxPluginsInject

input and output:
  -i, --input PATH        input .ipa or .tipa
  -o, --output PATH       output .ipa or .tipa
      --inplace           atomically replace the input after success
      --noconfirm         do not ask interactive questions

patching:
  -d, --dylib PATH        use a custom injection dylib
      --plugins-only      patch app extensions, not the main executable
      --ldid PATH         override bundled ldid.exe

other:
  -h, --help
      --version
)";

constexpr std::wstring_view version_text =
    L"ipapatch v2.1.3 (cyan-cpp " CYAN_VERSION_WIDE L")";

Error argument_error(ErrorCode code, std::wstring_view message) {
  auto encoded = platform::utf8_from_wide(message);
  return {code, encoded ? encoded.take_value() : "invalid ipapatch argument", {}};
}

Result<std::wstring> required_value(const std::vector<std::wstring>& arguments,
                                    std::size_t& index, std::wstring_view option) {
  if (index + 1U >= arguments.size()) {
    return Result<std::wstring>::failure(
        argument_error(ErrorCode::missing_argument,
                       std::wstring(L"missing value for ") + std::wstring(option)));
  }
  ++index;
  return Result<std::wstring>::success(arguments[index]);
}

}  // namespace

Result<IpaPatchCliOptions> parse_ipapatch_arguments(
    const std::vector<std::wstring>& arguments) {
  IpaPatchCliOptions options;
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const std::wstring_view argument = arguments[index];
    if (argument == L"-h" || argument == L"--help") {
      options.show_help = true;
    } else if (argument == L"--version") {
      options.show_version = true;
    } else if (argument == L"--inplace") {
      options.inplace = true;
    } else if (argument == L"--noconfirm") {
      options.noconfirm = true;
    } else if (argument == L"--plugins-only") {
      options.plugins_only = true;
    } else if (argument == L"-i" || argument == L"--input") {
      auto value = required_value(arguments, index, argument);
      if (!value) {
        return Result<IpaPatchCliOptions>::failure(value.error());
      }
      options.input = std::filesystem::path(value.take_value());
    } else if (argument == L"-o" || argument == L"--output") {
      auto value = required_value(arguments, index, argument);
      if (!value) {
        return Result<IpaPatchCliOptions>::failure(value.error());
      }
      options.output = std::filesystem::path(value.take_value());
    } else if (argument == L"-d" || argument == L"--dylib") {
      auto value = required_value(arguments, index, argument);
      if (!value) {
        return Result<IpaPatchCliOptions>::failure(value.error());
      }
      options.dylib = std::filesystem::path(value.take_value());
    } else if (argument == L"--ldid") {
      auto value = required_value(arguments, index, argument);
      if (!value) {
        return Result<IpaPatchCliOptions>::failure(value.error());
      }
      options.ldid_path = std::filesystem::path(value.take_value());
    } else {
      return Result<IpaPatchCliOptions>::failure(
          argument_error(ErrorCode::unknown_option,
                         std::wstring(L"unrecognised option: ") + std::wstring(argument)));
    }
  }

  if (options.show_help || options.show_version) {
    return Result<IpaPatchCliOptions>::success(std::move(options));
  }
  if (options.input.empty()) {
    return Result<IpaPatchCliOptions>::failure(
        argument_error(ErrorCode::missing_argument, L"--input is required"));
  }
  if (options.inplace) {
    options.output = options.input;
  } else if (options.output.empty() && options.noconfirm) {
    return Result<IpaPatchCliOptions>::failure(
        argument_error(ErrorCode::missing_argument,
                       L"--output or --inplace is required with --noconfirm"));
  }
  return Result<IpaPatchCliOptions>::success(std::move(options));
}

std::wstring_view ipapatch_help_text() { return help_text; }

std::wstring_view ipapatch_version_text() { return version_text; }

}  // namespace cyan
