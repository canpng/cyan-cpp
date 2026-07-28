#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cyan/core/temporary_workspace.hpp"
#include "cyan/macho/code_signature_inspector.hpp"
#include "cyan/macho/macho_inspector.hpp"
#include "cyan/platform/utf.hpp"
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

constexpr std::uint32_t signature_host = 0x0001U;
constexpr std::uint32_t signature_adhoc = 0x0002U;
constexpr std::uint32_t signature_hard = 0x0100U;
constexpr std::uint32_t signature_kill = 0x0200U;
constexpr std::uint32_t signature_expires = 0x0400U;
constexpr std::uint32_t signature_restrict = 0x0800U;
constexpr std::uint32_t signature_enforcement = 0x1000U;
constexpr std::uint32_t signature_library_validation = 0x2000U;
constexpr std::uint32_t signature_runtime = 0x10000U;
constexpr std::uint32_t signature_linker_signed = 0x20000U;
constexpr std::uint32_t supported_signature_flags =
    signature_host | signature_adhoc | signature_hard | signature_kill | signature_expires |
    signature_restrict | signature_enforcement | signature_library_validation |
    signature_runtime | signature_linker_signed;

std::wstring signature_flag_argument(std::uint32_t flags) {
  struct NamedFlag {
    std::uint32_t value;
    std::wstring_view name;
  };
  static constexpr NamedFlag known[] = {
      {signature_host, L"host"},
      {signature_adhoc, L"adhoc"},
      {signature_hard, L"hard"},
      {signature_kill, L"kill"},
      {signature_expires, L"expires"},
      {signature_restrict, L"restrict"},
      {signature_enforcement, L"enforcement"},
      {signature_library_validation, L"library-validation"},
      {signature_runtime, L"runtime"},
      {signature_linker_signed, L"linker-signed"},
  };
  std::wstring result = L"-C";
  bool first = true;
  for (const auto& flag : known) {
    if ((flags & flag.value) == 0U) {
      continue;
    }
    if (!first) {
      result.push_back(L',');
    }
    result += flag.name;
    first = false;
  }
  return result;
}

Result<void> write_bytes(const std::filesystem::path& path,
                         const std::vector<std::uint8_t>& bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not stage embedded entitlements", path});
  }
  if (!bytes.empty()) {
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
  }
  if (!output) {
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not write embedded entitlements", path});
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

Result<SigningProfile> ExternalLdidSigningBackend::captureProfile(
    const std::filesystem::path& executable) {
  CodeSignatureInspector inspector;
  auto inspected = inspector.inspect(executable);
  if (!inspected) {
    return Result<SigningProfile>::failure(inspected.error());
  }
  if (inspected.value().empty()) {
    return Result<SigningProfile>::failure(
        {ErrorCode::macho_invalid, "Mach-O has no architecture slices", executable});
  }
  const auto& first = inspected.value().front();
  for (const auto& slice : inspected.value()) {
    if (slice != first) {
      return Result<SigningProfile>::failure(
          {ErrorCode::feature_unavailable,
           "ldid cannot preserve different code-signature profiles across FAT slices",
           executable});
    }
  }

  SigningProfile profile;
  profile.had_signature = first.present;
  if (!first.present) {
    return Result<SigningProfile>::success(std::move(profile));
  }
  profile.identifier = first.identifier;
  profile.team_identifier = first.team_identifier;
  if (!profile.team_identifier.empty()) {
    if (!team_id_supported_.has_value()) {
      team_id_supported_ = static_cast<bool>(run({L"-t"}, std::nullopt));
    }
    if (!*team_id_supported_) {
      return Result<SigningProfile>::failure(
          {ErrorCode::feature_unavailable,
           "the configured ldid does not support -tTeamID; use the bundled Team ID-capable "
           "ldid build",
           executable});
    }
  }
  profile.flags = first.flags;
  if ((profile.flags & ~supported_signature_flags) != 0U) {
    return Result<SigningProfile>::failure(
        {ErrorCode::feature_unavailable,
         "code-signature flags contain values unsupported by ldid", executable});
  }
  profile.platform = first.platform;
  profile.had_der_entitlements = !first.der_entitlements.empty();

  if (!first.der_entitlements.empty() && first.xml_entitlements.empty()) {
    return Result<SigningProfile>::failure(
        {ErrorCode::feature_unavailable,
         "DER-only entitlements cannot be preserved by the configured ldid backend",
         executable});
  }
  if (!first.xml_entitlements.empty()) {
    auto workspace = TemporaryWorkspace::create();
    if (!workspace) {
      return Result<SigningProfile>::failure(workspace.error());
    }
    const auto entitlement_path = workspace.value().path() / L"embedded-entitlements.plist";
    auto written = write_bytes(entitlement_path, first.xml_entitlements);
    if (!written) {
      return Result<SigningProfile>::failure(written.error());
    }
    auto loaded = PlistDocument::load(entitlement_path);
    if (!loaded) {
      return Result<SigningProfile>::failure(
          {ErrorCode::signing_failed, "embedded XML entitlements are invalid", executable});
    }
    profile.entitlements.emplace(loaded.take_value());
  }
  return Result<SigningProfile>::success(std::move(profile));
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
  return sign(executable, entitlements, {}, {}, signature_adhoc, 0U, true);
}

Result<void> ExternalLdidSigningBackend::signAdHoc(
    const std::filesystem::path& executable, const SigningProfile& profile) {
  const std::uint32_t flags = profile.flags | signature_adhoc;
  if ((flags & ~supported_signature_flags) != 0U) {
    return Result<void>::failure(
        {ErrorCode::feature_unavailable,
         "code-signature flags contain values unsupported by ldid", executable});
  }
  auto signed_result =
      sign(executable, profile.entitlements, profile.identifier, profile.team_identifier, flags,
           profile.platform, false);
  if (!signed_result) {
    return signed_result;
  }

  CodeSignatureInspector inspector;
  auto verified = inspector.inspect(executable);
  if (!verified) {
    return Result<void>::failure(verified.error());
  }
  for (const auto& slice : verified.value()) {
    if (!slice.present || slice.identifier != profile.identifier ||
        slice.team_identifier != profile.team_identifier || slice.flags != flags ||
        slice.platform != profile.platform ||
        (!profile.entitlements.has_value() && !slice.xml_entitlements.empty()) ||
        (profile.entitlements.has_value() && slice.xml_entitlements.empty()) ||
        (profile.had_der_entitlements && slice.der_entitlements.empty())) {
      return Result<void>::failure(
          {ErrorCode::verification_failed,
           "ldid did not preserve the requested code-signature profile", executable});
    }
  }
  return Result<void>::success();
}

Result<void> ExternalLdidSigningBackend::sign(
    const std::filesystem::path& executable,
    const std::optional<PlistDocument>& entitlements, std::string_view identifier,
    std::string_view team_identifier, std::uint32_t flags,
    std::uint8_t code_directory_platform, bool verify_signature) const {
  std::vector<std::wstring> arguments;
  std::optional<TemporaryWorkspace> workspace;
  if (entitlements) {
    auto created = TemporaryWorkspace::create();
    if (!created) {
      return Result<void>::failure(created.error());
    }
    workspace.emplace(created.take_value());
    const auto entitlement_path = workspace->path() / L"signing-entitlements.plist";
    auto saved = entitlements->save(entitlement_path, PlistFormat::xml);
    if (!saved) {
      return saved;
    }
    arguments.push_back(L"-S" + entitlement_path.native());
  } else {
    arguments.push_back(L"-S");
  }
  if (!identifier.empty()) {
    auto wide = platform::wide_from_utf8(identifier);
    if (!wide) {
      return Result<void>::failure(wide.error());
    }
    arguments.push_back(L"-I" + wide.value());
  }
  if (!team_identifier.empty()) {
    auto wide = platform::wide_from_utf8(team_identifier);
    if (!wide) {
      return Result<void>::failure(wide.error());
    }
    arguments.push_back(L"-t" + wide.value());
  }
  if (code_directory_platform != 0U) {
    arguments.push_back(L"-P" + std::to_wstring(code_directory_platform));
  }
  arguments.push_back(signature_flag_argument(flags | signature_adhoc));
  arguments.push_back(executable.native());
  auto signed_result = run(arguments, std::nullopt);
  if (!signed_result) {
    return signed_result;
  }
  return verify_signature ? verify_signed(executable) : Result<void>::success();
}

}  // namespace cyan
