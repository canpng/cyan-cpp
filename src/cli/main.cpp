#include <cwctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "cyan/core/cli_parser.hpp"
#include "cyan/core/cyan_pipeline.hpp"
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
  std::wcerr << L"[!] " << (message ? message.value() : L"operation failed");
  if (!error.path.empty()) {
    std::wcerr << L": " << error.path.native();
  }
  std::wcerr << L'\n';
}

bool confirm_overwrite(const std::filesystem::path& output) {
  std::wcout << L"[<] " << output.native() << L" already exists, overwrite it? [Y/n] ";
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
  auto parsed = cyan::parse_cyan_arguments(collect_arguments(argc, argv));
  if (!parsed) {
    print_error(parsed.error());
    std::wcerr << L"Try 'cyan --help' for usage.\n";
    return 2;
  }

  auto options = parsed.take_value();
  if (options.show_help) {
    std::wcout << cyan::cyan_help_text();
    return 0;
  }
  if (options.show_version) {
    std::wcout << cyan::cyan_version_text() << L'\n';
    return 0;
  }

  auto valid = cyan::validate_and_normalize(options);
  if (!valid) {
    print_error(valid.error());
    return 2;
  }

  std::error_code filesystem_error;
  const bool output_exists = std::filesystem::exists(options.output, filesystem_error);
  if (filesystem_error) {
    print_error(
        {cyan::ErrorCode::filesystem_error, "could not inspect output path", options.output});
    return 2;
  }
  if (output_exists) {
    if (!options.overwrite && !confirm_overwrite(options.output)) {
      std::wcout << L"[>] quitting.\n";
      return 0;
    }
    options.overwrite = true;
  }

  cyan::CyanPipeline pipeline;
  auto completed = pipeline.run(std::move(options),
                                [](std::wstring_view message) { std::wcout << message << L'\n'; });
  if (!completed) {
    print_error(completed.error());
    return 1;
  }
  return 0;
}
