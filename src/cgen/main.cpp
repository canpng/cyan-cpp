#include <cwctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "cyan/archive/cyan_archive.hpp"
#include "cyan/core/cli_parser.hpp"
#include "cyan/core/input_validator.hpp"
#include "cyan/platform/utf.hpp"

namespace {

std::vector<std::wstring> collect_arguments(int argc, wchar_t* argv[]) {
  std::vector<std::wstring> arguments;
  if (argc > 1) {
    arguments.reserve(static_cast<std::size_t>(argc - 1));
  }
  for (int index = 1; index < argc; ++index) {
    arguments.emplace_back(argv[index]);
  }
  return arguments;
}

void print_error(const cyan::Error& error) {
  auto message = cyan::platform::wide_from_utf8(error.message);
  std::wstring output = L"[!] ";
  output += message ? message.value() : L"operation failed";
  if (!error.path.empty()) {
    output += L": ";
    output += error.path.native();
  }
  output += L'\n';
  cyan::platform::write_stderr(output);
}

bool confirm_overwrite(const std::filesystem::path& output) {
  cyan::platform::write_stdout(L"[<] " + output.native() +
                               L" already exists. overwrite? [Y/n] ");
  std::wstring answer;
  if (!std::getline(std::wcin, answer)) {
    return false;
  }
  for (auto& character : answer) {
    character = static_cast<wchar_t>(std::towlower(character));
  }
  return answer.empty() || answer == L"y" || answer == L"yes";
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
  auto parsed = cyan::parse_cgen_arguments(collect_arguments(argc, argv));
  if (!parsed) {
    print_error(parsed.error());
    cyan::platform::write_stderr(L"Try 'cgen --help' for usage.\n");
    return 2;
  }

  auto options = parsed.take_value();
  if (options.show_help) {
    cyan::platform::write_stdout(cyan::cgen_help_text());
    return 0;
  }
  if (options.show_version) {
    cyan::platform::write_stdout(L"cgen (cyan-cpp " CYAN_VERSION_WIDE L")\n");
    return 0;
  }

  auto valid = cyan::validate_and_normalize(options);
  if (!valid) {
    print_error(valid.error());
    return 2;
  }

  std::error_code error;
  const bool output_exists = std::filesystem::exists(options.output, error);
  if (error) {
    print_error(
        {cyan::ErrorCode::filesystem_error, "could not inspect output path", options.output});
    return 2;
  }
  if (output_exists && !options.overwrite && !confirm_overwrite(options.output)) {
    cyan::platform::write_stdout(L"[>] quitting.\n");
    return 0;
  }

  cyan::platform::write_stdout(L"[*] generating..\n");
  cyan::CyanArchiveWriter writer;
  auto generated = writer.write(options);
  if (!generated) {
    print_error(generated.error());
    return 1;
  }
  cyan::platform::write_stdout(L"[*] generated " + options.output.native() + L"\n");
  return 0;
}
