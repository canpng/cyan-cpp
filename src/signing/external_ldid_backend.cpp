#include <string>
#include <utility>
#include <vector>

#include "cyan/core/temporary_workspace.hpp"
#include "cyan/macho/macho_inspector.hpp"
#include "cyan/signing/signing_backend.hpp"
#include "process_runner.hpp"

namespace cyan {
namespace {

Result<void> verify_signed(const std::filesystem::path& executable) {
  MachOInspector inspector;
  auto inspected = inspector.inspect(executable);
  if (!inspected) {
    return Result<void>::failure(inspected.error());
  }
  for (const auto& slice : inspected.value().slices) {
    if (!slice.has_code_signature) {
      return Result<void>::failure(
          {ErrorCode::signing_failed, "ldid did not sign every Mach-O slice", executable});
    }
  }
  return Result<void>::success();
}

}  // namespace

ExternalLdidSigningBackend::ExternalLdidSigningBackend(std::filesystem::path executable)
    : executable_(std::move(executable)) {}

Result<void> ExternalLdidSigningBackend::run(
    const std::vector<std::wstring>& arguments,
    const std::optional<std::filesystem::path>& standard_output) const {
  return signing_internal::run_tool(executable_, arguments, standard_output, "ldid");
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
  return run({L"-r", executable.native()}, std::nullopt);
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
    auto signed_result = run(arguments, std::nullopt);
    if (!signed_result) {
      return signed_result;
    }
    return verify_signed(executable);
  } else {
    arguments.push_back(L"-S");
  }
  arguments.push_back(L"-Cadhoc");
  arguments.push_back(executable.native());
  auto signed_result = run(arguments, std::nullopt);
  if (!signed_result) {
    return signed_result;
  }
  return verify_signed(executable);
}

}  // namespace cyan
