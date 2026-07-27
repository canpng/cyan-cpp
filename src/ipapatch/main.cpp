#include <Windows.h>

#include <cwctype>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "cyan/ipapatch/cli_parser.hpp"
#include "cyan/ipapatch/ipapatch_service.hpp"
#include "cyan/platform/utf.hpp"
#include "cyan/signing/signing_backend.hpp"

namespace {

std::vector<std::wstring> collect_arguments(int argc, wchar_t* argv[]) {
  std::vector<std::wstring> arguments;
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
  cyan::platform::write_stderr(output + L"\n");
}

bool confirm(std::wstring prompt) {
  cyan::platform::write_stdout(std::move(prompt));
  std::wstring answer;
  if (!std::getline(std::wcin, answer)) {
    return false;
  }
  for (auto& character : answer) {
    character = static_cast<wchar_t>(std::towlower(character));
  }
  return answer.empty() || answer == L"y" || answer == L"yes";
}

std::filesystem::path executable_directory() {
  std::vector<wchar_t> buffer(1024U);
  while (true) {
    const DWORD length =
        GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0U) {
      return {};
    }
    if (length < buffer.size() - 1U) {
      return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
    }
    buffer.resize(buffer.size() * 2U);
  }
}

std::optional<std::filesystem::path> bundled_ldid() {
  const auto candidate = executable_directory() / L"ldid.exe";
  std::error_code error;
  if (std::filesystem::is_regular_file(candidate, error) && !error) {
    return candidate;
  }
  return std::nullopt;
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
  auto parsed = cyan::parse_ipapatch_arguments(collect_arguments(argc, argv));
  if (!parsed) {
    print_error(parsed.error());
    cyan::platform::write_stderr(L"Try 'ipapatch --help' for usage.\n");
    return 2;
  }
  auto cli = parsed.take_value();
  if (cli.show_help) {
    cyan::platform::write_stdout(cyan::ipapatch_help_text());
    return 0;
  }
  if (cli.show_version) {
    cyan::platform::write_stdout(std::wstring(cyan::ipapatch_version_text()) + L"\n");
    return 0;
  }

  if (cli.output.empty()) {
    if (!confirm(L"[<] --inplace not specified; overwrite the input? [Y/n] ")) {
      cyan::platform::write_stdout(L"[>] quitting.\n");
      return 0;
    }
    cli.output = cli.input;
    cli.inplace = true;
  }
  std::error_code error;
  if (!cli.inplace && std::filesystem::exists(cli.output, error) && !error &&
      !cli.noconfirm &&
      !confirm(L"[<] " + cli.output.native() + L" already exists; overwrite? [Y/n] ")) {
    cyan::platform::write_stdout(L"[>] quitting.\n");
    return 0;
  }
  if (error) {
    print_error({cyan::ErrorCode::filesystem_error, "could not inspect output path", cli.output});
    return 2;
  }

  const auto signer_path = cli.ldid_path ? cli.ldid_path : bundled_ldid();
  if (!signer_path) {
    print_error({cyan::ErrorCode::signing_backend_unavailable,
                 "ldid.exe is unavailable; install the packaged runtime or use --ldid PATH", {}});
    return 2;
  }
  cyan::ExternalLdidSigningBackend signing(*signer_path);
  cyan::IpaPatchService service(signing);
  cyan::IpaPatchOptions options;
  options.dylib = cli.dylib;
  options.plugins_only = cli.plugins_only;

  std::optional<cyan::IpaPatchStage> last_stage;
  cyan::IpaPatchCallbacks callbacks;
  callbacks.progress = [&](const cyan::IpaPatchProgressEvent& event) {
    if (last_stage == event.stage) {
      return;
    }
    last_stage = event.stage;
    std::wstring message = L"[*] ";
    message += cyan::ipapatch_stage_name(event.stage);
    if (!event.current_file.empty()) {
      message += L": ";
      message += event.current_file.filename().native();
    }
    cyan::platform::write_stdout(message + L"\n");
  };

  auto completed =
      service.run_standalone(cli.input, cli.output, options, callbacks);
  if (!completed) {
    print_error(completed.error());
    return 1;
  }
  return 0;
}
