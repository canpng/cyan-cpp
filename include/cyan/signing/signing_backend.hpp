#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "cyan/core/result.hpp"
#include "cyan/plist/plist_document.hpp"

namespace cyan {

class ISigningBackend {
 public:
  virtual ~ISigningBackend() = default;

  virtual Result<void> extractEntitlements(const std::filesystem::path& executable,
                                           PlistDocument& output) = 0;

  virtual Result<void> removeSignature(const std::filesystem::path& executable) = 0;

  virtual Result<void> signAdHoc(const std::filesystem::path& executable,
                                 const std::optional<PlistDocument>& entitlements) = 0;
};

class ExternalLdidSigningBackend final : public ISigningBackend {
 public:
  explicit ExternalLdidSigningBackend(std::filesystem::path executable);

  Result<void> extractEntitlements(const std::filesystem::path& executable,
                                   PlistDocument& output) override;
  Result<void> removeSignature(const std::filesystem::path& executable) override;
  Result<void> signAdHoc(const std::filesystem::path& executable,
                         const std::optional<PlistDocument>& entitlements) override;

 private:
  Result<void> run(const std::vector<std::wstring>& arguments,
                   const std::optional<std::filesystem::path>& standard_output) const;

  std::filesystem::path executable_;
};

}  // namespace cyan
