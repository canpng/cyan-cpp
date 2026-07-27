#include <Windows.h>

#include <fstream>
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

class TemporaryFile {
 public:
  TemporaryFile() {
    std::vector<wchar_t> directory(MAX_PATH + 1U);
    const DWORD directory_length =
        GetTempPathW(static_cast<DWORD>(directory.size()), directory.data());
    if (directory_length == 0U || directory_length >= directory.size()) {
      return;
    }
    std::vector<wchar_t> path(MAX_PATH + 1U);
    if (GetTempFileNameW(directory.data(), L"cyn", 0U, path.data()) == 0U) {
      return;
    }
    path_ = path.data();
  }

  ~TemporaryFile() {
    if (!path_.empty()) {
      static_cast<void>(DeleteFileW(path_.c_str()));
    }
  }

  TemporaryFile(const TemporaryFile&) = delete;
  TemporaryFile& operator=(const TemporaryFile&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
  [[nodiscard]] bool valid() const noexcept { return !path_.empty(); }

 private:
  std::filesystem::path path_;
};

std::string read_diagnostic(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {};
  }
  constexpr std::size_t maximum_length = 16U * 1024U;
  std::string message(maximum_length, '\0');
  input.read(message.data(), static_cast<std::streamsize>(message.size()));
  message.resize(static_cast<std::size_t>(input.gcount()));
  while (!message.empty() &&
         (message.back() == '\0' || message.back() == '\r' || message.back() == '\n' ||
          message.back() == ' ' || message.back() == '\t')) {
    message.pop_back();
  }
  return message;
}

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
  TemporaryFile diagnostic_file;
  UniqueHandle diagnostic_output;
  if (standard_output) {
    captured = UniqueHandle(CreateFileW(standard_output->c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                        &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  }
  if (diagnostic_file.valid()) {
    diagnostic_output =
        UniqueHandle(CreateFileW(diagnostic_file.path().c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                 &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  }
  if (!null_input.valid() || !null_output.valid() || (standard_output && !captured.valid()) ||
      !diagnostic_file.valid() || !diagnostic_output.valid()) {
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
  startup.hStdError = diagnostic_output.get();
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
    diagnostic_output = UniqueHandle{};
    const auto diagnostic = read_diagnostic(diagnostic_file.path());
    return Result<void>::failure(
        {ErrorCode::signing_failed,
         std::string(tool_name) + " failed with exit code " + std::to_string(exit_code) +
             (diagnostic.empty() ? std::string{} : ": " + diagnostic),
         executable});
  }
  return Result<void>::success();
}

}  // namespace cyan::signing_internal
