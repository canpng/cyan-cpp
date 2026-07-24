#include <Windows.h>

#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cyan/core/temporary_workspace.hpp"
#include "cyan/signing/signing_backend.hpp"

namespace cyan {
namespace {

class UniqueHandle {
 public:
  UniqueHandle() = default;
  explicit UniqueHandle(HANDLE handle) : handle_(handle) {}
  ~UniqueHandle() {
    if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
      CloseHandle(handle_);
    }
  }
  UniqueHandle(const UniqueHandle&) = delete;
  UniqueHandle& operator=(const UniqueHandle&) = delete;
  UniqueHandle(UniqueHandle&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  UniqueHandle& operator=(UniqueHandle&& other) noexcept {
    if (this != &other) {
      if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
      }
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }
  [[nodiscard]] HANDLE get() const noexcept { return handle_; }
  [[nodiscard]] bool valid() const noexcept {
    return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
  }

 private:
  HANDLE handle_{nullptr};
};

std::wstring quote_argument(std::wstring_view argument) {
  if (argument.empty()) {
    return L"\"\"";
  }
  if (argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos) {
    return std::wstring(argument);
  }

  std::wstring quoted(1U, L'"');
  std::size_t backslashes = 0;
  for (const wchar_t character : argument) {
    if (character == L'\\') {
      ++backslashes;
      continue;
    }
    if (character == L'"') {
      quoted.append(backslashes * 2U + 1U, L'\\');
      quoted.push_back(L'"');
      backslashes = 0;
      continue;
    }
    quoted.append(backslashes, L'\\');
    backslashes = 0;
    quoted.push_back(character);
  }
  quoted.append(backslashes * 2U, L'\\');
  quoted.push_back(L'"');
  return quoted;
}

}  // namespace

ExternalLdidSigningBackend::ExternalLdidSigningBackend(std::filesystem::path executable)
    : executable_(std::move(executable)) {}

Result<void> ExternalLdidSigningBackend::run(
    const std::vector<std::wstring>& arguments,
    const std::optional<std::filesystem::path>& standard_output) const {
  std::error_code error;
  if (!std::filesystem::is_regular_file(executable_, error) || error) {
    return Result<void>::failure(
        {ErrorCode::signing_backend_unavailable, "ldid executable does not exist", executable_});
  }

  SECURITY_ATTRIBUTES security{};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  UniqueHandle null_input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  UniqueHandle null_output(CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  UniqueHandle captured;
  if (standard_output) {
    captured = UniqueHandle(CreateFileW(standard_output->c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                        &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  }
  if (!null_input.valid() || !null_output.valid() || (standard_output && !captured.valid())) {
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not prepare ldid process handles", executable_});
  }

  std::wstring command_line = quote_argument(executable_.native());
  for (const auto& argument : arguments) {
    command_line.push_back(L' ');
    command_line += quote_argument(argument);
  }
  std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
  mutable_command.push_back(L'\0');

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = null_input.get();
  startup.hStdOutput = standard_output ? captured.get() : null_output.get();
  startup.hStdError = null_output.get();
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(executable_.c_str(), mutable_command.data(), nullptr, nullptr, TRUE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
    return Result<void>::failure(
        {ErrorCode::signing_backend_unavailable, "could not start ldid", executable_});
  }
  UniqueHandle process_handle(process.hProcess);
  UniqueHandle thread_handle(process.hThread);

  constexpr DWORD timeout_milliseconds = 120'000U;
  const DWORD waited = WaitForSingleObject(process_handle.get(), timeout_milliseconds);
  if (waited == WAIT_TIMEOUT) {
    static_cast<void>(TerminateProcess(process_handle.get(), 124U));
    return Result<void>::failure({ErrorCode::signing_failed, "ldid timed out", executable_});
  }
  if (waited != WAIT_OBJECT_0) {
    return Result<void>::failure(
        {ErrorCode::signing_failed, "could not wait for ldid", executable_});
  }
  DWORD exit_code = 0;
  if (!GetExitCodeProcess(process_handle.get(), &exit_code) || exit_code != 0U) {
    return Result<void>::failure({ErrorCode::signing_failed,
                                  "ldid failed with exit code " + std::to_string(exit_code),
                                  executable_});
  }
  return Result<void>::success();
}

Result<void> ExternalLdidSigningBackend::extractEntitlements(
    const std::filesystem::path& executable, PlistDocument& output) {
  auto workspace = TemporaryWorkspace::create();
  if (!workspace) {
    return Result<void>::failure(workspace.error());
  }
  const auto extracted = workspace.value().path() / L"entitlements.plist";
  auto executed = run({L"-e", executable.native()}, extracted);
  if (!executed) {
    return executed;
  }
  auto loaded = PlistDocument::load(extracted);
  if (!loaded) {
    return Result<void>::failure(
        {ErrorCode::signing_failed, "ldid returned no valid entitlements", executable});
  }
  output = loaded.take_value();
  return Result<void>::success();
}

Result<void> ExternalLdidSigningBackend::removeSignature(const std::filesystem::path& executable) {
  return run({L"-R", executable.native()}, std::nullopt);
}

Result<void> ExternalLdidSigningBackend::signAdHoc(
    const std::filesystem::path& executable, const std::optional<PlistDocument>& entitlements) {
  std::vector<std::wstring> arguments;
  if (entitlements) {
    auto workspace = TemporaryWorkspace::create();
    if (!workspace) {
      return Result<void>::failure(workspace.error());
    }
    const auto entitlement_path = workspace.value().path() / L"signing-entitlements.plist";
    auto saved = entitlements->save(entitlement_path, PlistFormat::xml);
    if (!saved) {
      return saved;
    }
    arguments.push_back(L"-S" + entitlement_path.native());
    arguments.push_back(L"-M");
    arguments.push_back(L"-Cadhoc");
    arguments.push_back(executable.native());
    return run(arguments, std::nullopt);
  } else {
    arguments.push_back(L"-S");
  }
  arguments.push_back(L"-Cadhoc");
  arguments.push_back(executable.native());
  return run(arguments, std::nullopt);
}

}  // namespace cyan
