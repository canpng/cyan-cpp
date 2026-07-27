#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cyan/core/result.hpp"
#include "cyan/plist/plist_document.hpp"

namespace cyan {

struct SigningProfile {
  bool had_signature{false};
  std::optional<PlistDocument> entitlements;
  std::string identifier;
  std::string team_identifier;
  std::uint32_t flags{0};
  std::uint8_t platform{0};
  bool had_der_entitlements{false};
};

class ISigningBackend {
 public:
  virtual ~ISigningBackend() = default;

  virtual Result<SigningProfile> captureProfile(
      const std::filesystem::path& executable) = 0;

  virtual Result<void> extractEntitlements(const std::filesystem::path& executable,
                                           PlistDocument& output) = 0;

  virtual Result<void> removeSignature(const std::filesystem::path& executable) = 0;

  virtual Result<void> signAdHoc(const std::filesystem::path& executable,
                                 const std::optional<PlistDocument>& entitlements) = 0;

  virtual Result<void> signAdHoc(const std::filesystem::path& executable,
                                 const SigningProfile& profile) = 0;
};

class ExternalLdidSigningBackend final : public ISigningBackend {
 public:
  explicit ExternalLdidSigningBackend(std::filesystem::path executable);

  Result<SigningProfile> captureProfile(
      const std::filesystem::path& executable) override;
  Result<void> extractEntitlements(const std::filesystem::path& executable,
                                   PlistDocument& output) override;
  Result<void> removeSignature(const std::filesystem::path& executable) override;
  Result<void> signAdHoc(const std::filesystem::path& executable,
                         const std::optional<PlistDocument>& entitlements) override;
  Result<void> signAdHoc(const std::filesystem::path& executable,
                         const SigningProfile& profile) override;

 private:
  Result<void> run(const std::vector<std::wstring>& arguments,
                   const std::optional<std::filesystem::path>& standard_output) const;
  Result<void> sign(const std::filesystem::path& executable,
                    const std::optional<PlistDocument>& entitlements,
                    std::string_view identifier, std::string_view team_identifier,
                    std::uint32_t flags, std::uint8_t code_directory_platform) const;

  std::filesystem::path executable_;
};

}  // namespace cyan
