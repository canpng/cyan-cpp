#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "cyan/core/result.hpp"
#include "cyan/signing/signing_backend.hpp"

namespace cyan {

enum class IpaPatchStage {
  validating,
  extracting,
  discovering,
  capturing_signatures,
  injecting,
  installing_payload,
  signing,
  packaging,
  completed
};

struct IpaPatchProgressEvent {
  IpaPatchStage stage{IpaPatchStage::validating};
  std::filesystem::path current_file;
  std::size_t completed{0};
  std::size_t total{0};
  double fraction{0.0};
};

struct IpaPatchCallbacks {
  std::function<void(const IpaPatchProgressEvent&)> progress;
};

struct IpaPatchOptions {
  std::optional<std::filesystem::path> dylib;
  std::filesystem::path default_dylib;
  bool plugins_only{false};
  int compression_level{6};
};

struct IpaPatchTarget {
  std::filesystem::path bundle;
  std::filesystem::path executable;
  std::string bundle_identifier;
  SigningProfile signing_profile;
};

struct IpaPatchPlan {
  std::filesystem::path package_root;
  std::filesystem::path app_bundle;
  std::filesystem::path payload_source;
  std::filesystem::path payload_destination;
  std::string load_command;
  std::vector<IpaPatchTarget> targets;
};

using IpaPatchApplyResult = IpaPatchPlan;

class IpaPatchService {
 public:
  explicit IpaPatchService(ISigningBackend& signing_backend);

  Result<IpaPatchPlan> prepare_open_package(
      const std::filesystem::path& package_root, const IpaPatchOptions& options,
      const IpaPatchCallbacks& callbacks = {}, std::stop_token stop_token = {},
      std::optional<SigningProfile> main_profile_override = std::nullopt) const;

  Result<IpaPatchApplyResult> apply_prepared(
      IpaPatchPlan plan, const IpaPatchCallbacks& callbacks = {},
      std::stop_token stop_token = {}) const;

  Result<IpaPatchApplyResult> apply_to_open_package(
      const std::filesystem::path& package_root, const IpaPatchOptions& options,
      const IpaPatchCallbacks& callbacks = {}, std::stop_token stop_token = {}) const;

  Result<void> finalize_signatures(
      const IpaPatchApplyResult& result, const IpaPatchCallbacks& callbacks = {},
      std::stop_token stop_token = {}) const;

  Result<void> run_standalone(
      const std::filesystem::path& input_ipa, const std::filesystem::path& output_ipa,
      const IpaPatchOptions& options, const IpaPatchCallbacks& callbacks = {},
      std::stop_token stop_token = {}) const;

  static std::filesystem::path bundled_payload_path();

 private:
  ISigningBackend& signing_backend_;
};

std::wstring_view ipapatch_stage_name(IpaPatchStage stage);

}  // namespace cyan
