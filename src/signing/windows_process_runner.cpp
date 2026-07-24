#include <Windows.h>

#include <string>
#include <utility>
#include <vector>

#include "process_runner.hpp"

namespace cyan::signing_internal {
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

Result<void> run_tool(const std::filesystem::path& executable,
                      const std::vector<std::wstring>& arguments,
                      const std::optional<std::filesystem::path>& standard_output,
                      std::string_view tool_name) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(executable, error) || error) {
    return Result<void>::failure({ErrorCode::signing_backend_unavailable,
                                  std::string(tool_name) + " executable does not exist",
                                  executable});
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
    return Result<void>::failure({ErrorCode::filesystem_error,
                                  "could not prepare signing-tool process handles", executable});
  }

  std::wstring command_line = quote_argument(executable.native());
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
  if (!CreateProcessW(executable.c_str(), mutable_command.data(), nullptr, nullptr, TRUE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
    return Result<void>::failure({ErrorCode::signing_backend_unavailable,
                                  "could not start " + std::string(tool_name), executable});
  }
  UniqueHandle process_handle(process.hProcess);
  UniqueHandle thread_handle(process.hThread);

  constexpr DWORD timeout_milliseconds = 120'000U;
  const DWORD waited = WaitForSingleObject(process_handle.get(), timeout_milliseconds);
  if (waited == WAIT_TIMEOUT) {
    static_cast<void>(TerminateProcess(process_handle.get(), 124U));
    return Result<void>::failure(
        {ErrorCode::signing_failed, std::string(tool_name) + " timed out", executable});
  }
  if (waited != WAIT_OBJECT_0) {
    return Result<void>::failure(
        {ErrorCode::signing_failed, "could not wait for " + std::string(tool_name), executable});
  }

  DWORD exit_code = 0;
  if (!GetExitCodeProcess(process_handle.get(), &exit_code)) {
    return Result<void>::failure({ErrorCode::signing_failed,
                                  "could not read " + std::string(tool_name) + " exit code",
                                  executable});
  }
  if (exit_code != 0U) {
    return Result<void>::failure(
        {ErrorCode::signing_failed,
         std::string(tool_name) + " failed with exit code " + std::to_string(exit_code),
         executable});
  }
  return Result<void>::success();
}

}  // namespace cyan::signing_internal
