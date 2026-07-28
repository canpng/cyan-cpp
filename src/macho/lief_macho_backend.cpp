#include "cyan/macho/lief_macho_backend.hpp"

#include <Windows.h>

#include <LIEF/MachO/Binary.hpp>
#include <LIEF/MachO/DylibCommand.hpp>
#include <LIEF/MachO/FatBinary.hpp>
#include <LIEF/MachO/Header.hpp>
#include <LIEF/MachO/Parser.hpp>
#include <LIEF/MachO/RPathCommand.hpp>
#include <algorithm>
#include <chrono>
#include <exception>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "cyan/macho/macho_inspector.hpp"

namespace cyan {
namespace {

InjectionResult failure(InjectionError error, std::string message) {
  return {error, std::move(message), false};
}

std::optional<std::vector<std::uint8_t>> read_all(const std::filesystem::path& path,
                                                  InjectionResult& error) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    error = failure(InjectionError::FileNotFound, "could not open Mach-O file for LIEF");
    return std::nullopt;
  }
  const std::streamoff length = input.tellg();
  if (length < 0 ||
      static_cast<unsigned long long>(length) >
          static_cast<unsigned long long>((std::numeric_limits<std::size_t>::max)())) {
    error = failure(InjectionError::ReadFailure, "could not determine safe Mach-O size");
    return std::nullopt;
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
  input.seekg(0);
  if (!bytes.empty()) {
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  if (!input) {
    error = failure(InjectionError::ReadFailure, "could not read Mach-O for LIEF");
    return std::nullopt;
  }
  return bytes;
}

InjectionResult publish(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
  const auto ticks =
      static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
  const auto temporary =
      path.parent_path() / (path.filename().native() + L".cyan-lief-" + std::to_wstring(ticks));
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
      return failure(InjectionError::WriteFailure, "could not create LIEF output");
    }
    if (!bytes.empty()) {
      output.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }
    output.flush();
    if (!output) {
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
      return failure(InjectionError::WriteFailure, "could not write LIEF output");
    }
  }
  if (!MoveFileExW(temporary.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    return failure(InjectionError::WriteFailure, "could not publish LIEF output");
  }
  return {InjectionError::None, "injected dylib with LIEF backend", true};
}

Result<void> publish_result(const std::filesystem::path& path,
                            std::span<const std::uint8_t> bytes) {
  auto published = publish(path, bytes);
  if (published.error != InjectionError::None) {
    return Result<void>::failure({ErrorCode::filesystem_error, published.message, path});
  }
  return Result<void>::success();
}

bool path_component_matches(std::string_view dependency, std::string_view available) {
  const std::size_t separator = dependency.find_last_of('/');
  const std::string_view leaf =
      separator == std::string_view::npos ? dependency : dependency.substr(separator + 1U);
  if (available.ends_with(".framework")) {
    const std::string_view stem = available.substr(0U, available.size() - 10U);
    return leaf == stem;
  }
  return leaf == available;
}

}  // namespace

InjectionResult LiefMachOBackend::inject(const std::filesystem::path& binary,
                                         std::string_view dylib_path,
                                         const InjectionOptions& options) const {
  if (dylib_path.empty() || dylib_path.find('\0') != std::string_view::npos) {
    return failure(InjectionError::InvalidMachO, "dylib path is empty or contains NUL");
  }

  InjectionResult read_error;
  auto bytes = read_all(binary, read_error);
  if (!bytes) {
    return read_error;
  }

  try {
    auto fat = LIEF::MachO::Parser::parse(*bytes);
    if (!fat || fat->empty()) {
      return failure(InjectionError::InvalidMachO, "LIEF could not parse Mach-O");
    }

    const std::string requested(dylib_path);
    if (!options.allowDuplicate) {
      for (const LIEF::MachO::Binary& slice : *fat) {
        const auto libraries = slice.libraries();
        if (std::any_of(libraries.begin(), libraries.end(),
                        [&](const LIEF::MachO::DylibCommand& command) {
                          return command.name() == requested;
                        })) {
          return failure(InjectionError::DuplicateLoadCommand,
                         "the requested dylib load command already exists");
        }
      }
    }

    for (LIEF::MachO::Binary& slice : *fat) {
      if (options.stripCodeSignature) {
        static_cast<void>(slice.remove_signature());
      }
      const auto command = options.commandType == LoadCommandType::Weak
                               ? LIEF::MachO::DylibCommand::weak_dylib(requested, 0U, 0U, 0U)
                               : LIEF::MachO::DylibCommand::load_dylib(requested, 0U, 0U, 0U);
      if (slice.add(command) == nullptr) {
        return failure(InjectionError::WriteFailure, "LIEF could not add dylib load command");
      }
    }

    std::vector<std::uint8_t> output = fat->raw();
    MachOInspector inspector;
    auto verified = inspector.verify_dependency(output, requested, options.stripCodeSignature);
    if (!verified) {
      return failure(InjectionError::VerificationFailure, verified.error().message);
    }
    return publish(binary, output);
  } catch (const std::exception& exception) {
    return failure(InjectionError::WriteFailure,
                   std::string("LIEF injection failed: ") + exception.what());
  }
}

Result<std::vector<CommonDependency>> LiefMachOBackend::repair_dependencies(
    const std::filesystem::path& binary, const std::vector<std::string>& available_items,
    bool add_framework_rpath) const {
  InjectionResult read_error;
  auto bytes = read_all(binary, read_error);
  if (!bytes) {
    return Result<std::vector<CommonDependency>>::failure(
        {ErrorCode::file_not_found, read_error.message, binary});
  }

  try {
    auto fat = LIEF::MachO::Parser::parse(*bytes);
    if (!fat || fat->empty()) {
      return Result<std::vector<CommonDependency>>::failure(
          {ErrorCode::macho_invalid, "LIEF could not parse Mach-O dependencies", binary});
    }

    DependencyResolver resolver;
    std::vector<CommonDependency> required;
    std::unordered_set<std::string> required_keys;
    bool modified = false;
    for (LIEF::MachO::Binary& slice : *fat) {
      for (LIEF::MachO::DylibCommand& command : slice.libraries()) {
        const std::string original = command.name();
        if (auto common = resolver.resolve_common(original)) {
          if (original != common->canonical_path) {
            command.name(common->canonical_path);
            modified = true;
          }
          if (required_keys.insert(common->key).second) {
            required.push_back(std::move(*common));
          }
          continue;
        }

        for (const auto& available : available_items) {
          if (!path_component_matches(original, available)) {
            continue;
          }
          const std::string replacement = resolver.canonical_user_dependency(available);
          if (original != replacement) {
            command.name(replacement);
            modified = true;
          }
          break;
        }
      }

      if (add_framework_rpath) {
        constexpr std::string_view requested_rpath = "@executable_path/Frameworks";
        bool found = false;
        for (const LIEF::MachO::RPathCommand& command : slice.rpaths()) {
          if (command.path() == requested_rpath) {
            found = true;
            break;
          }
        }
        if (!found) {
          auto rpath = LIEF::MachO::RPathCommand::create(std::string(requested_rpath));
          if (!rpath || slice.add(std::move(rpath)) == nullptr) {
            return Result<std::vector<CommonDependency>>::failure(
                {ErrorCode::injection_failed, "LIEF could not add LC_RPATH", binary});
          }
          modified = true;
        }
      }
    }

    if (required_keys.contains("orion.") && required_keys.insert("substrate.").second) {
      if (auto substrate = resolver.resolve_common("/usr/lib/libsubstrate.dylib")) {
        required.push_back(std::move(*substrate));
      }
    }

    if (modified) {
      const std::vector<std::uint8_t> output = fat->raw();
      auto published = publish_result(binary, output);
      if (!published) {
        return Result<std::vector<CommonDependency>>::failure(published.error());
      }
      MachOInspector inspector;
      auto verified = inspector.inspect(binary);
      if (!verified) {
        return Result<std::vector<CommonDependency>>::failure(
            {ErrorCode::verification_failed,
             "dependency edit produced an invalid Mach-O: " + verified.error().message, binary});
      }
    }
    return Result<std::vector<CommonDependency>>::success(std::move(required));
  } catch (const std::exception& exception) {
    return Result<std::vector<CommonDependency>>::failure(
        {ErrorCode::injection_failed,
         std::string("LIEF dependency edit failed: ") + exception.what(), binary});
  }
}

Result<void> LiefMachOBackend::thin_to_arm64(const std::filesystem::path& binary) const {
  MachOInspector inspector;
  auto inspected = inspector.inspect(binary);
  if (!inspected) {
    return Result<void>::failure(inspected.error());
  }
  const bool has_arm64 =
      std::any_of(inspected.value().slices.begin(), inspected.value().slices.end(),
                  [](const MachOSliceInfo& slice) { return slice.cpu_type == 0x0100000c; });
  if (!has_arm64) {
    return Result<void>::failure(
        {ErrorCode::macho_unsupported, "Mach-O has no arm64 slice", binary});
  }
  if (!inspected.value().is_fat && inspected.value().slices.size() == 1U) {
    return Result<void>::success();
  }

  InjectionResult read_error;
  auto bytes = read_all(binary, read_error);
  if (!bytes) {
    return Result<void>::failure({ErrorCode::file_not_found, read_error.message, binary});
  }
  try {
    auto fat = LIEF::MachO::Parser::parse(*bytes);
    if (!fat || fat->empty()) {
      return Result<void>::failure(
          {ErrorCode::macho_invalid, "LIEF could not parse Mach-O for thinning", binary});
    }
    LIEF::MachO::Binary* arm64 = nullptr;
    for (LIEF::MachO::Binary& slice : *fat) {
      if (slice.header().cpu_type() == LIEF::MachO::Header::CPU_TYPE::ARM64) {
        arm64 = &slice;
        break;
      }
    }
    if (arm64 == nullptr) {
      return Result<void>::failure(
          {ErrorCode::macho_unsupported, "Mach-O has no arm64 slice", binary});
    }
    if (fat->size() == 1U) {
      return Result<void>::success();
    }

    const std::vector<std::uint8_t> output = arm64->raw();
    auto published = publish_result(binary, output);
    if (!published) {
      return published;
    }
    auto verified = inspector.inspect(binary);
    if (!verified || verified.value().slices.size() != 1U ||
        verified.value().slices.front().cpu_type !=
            static_cast<std::int32_t>(LIEF::MachO::Header::CPU_TYPE::ARM64)) {
      return Result<void>::failure(
          {ErrorCode::verification_failed, "arm64 thinning verification failed", binary});
    }
    return Result<void>::success();
  } catch (const std::exception& exception) {
    return Result<void>::failure({ErrorCode::injection_failed,
                                  std::string("LIEF arm64 thinning failed: ") + exception.what(),
                                  binary});
  }
}

Result<void> LiefMachOBackend::remove_signature(const std::filesystem::path& binary) const {
  InjectionResult read_error;
  auto bytes = read_all(binary, read_error);
  if (!bytes) {
    return Result<void>::failure({ErrorCode::file_not_found, read_error.message, binary});
  }
  try {
    auto fat = LIEF::MachO::Parser::parse(*bytes);
    if (!fat || fat->empty()) {
      return Result<void>::failure(
          {ErrorCode::macho_invalid, "LIEF could not parse Mach-O signature", binary});
    }
    bool modified = false;
    for (LIEF::MachO::Binary& slice : *fat) {
      modified = slice.remove_signature() || modified;
    }
    if (!modified) {
      return Result<void>::success();
    }
    const std::vector<std::uint8_t> output = fat->raw();
    return publish_result(binary, output);
  } catch (const std::exception& exception) {
    return Result<void>::failure({ErrorCode::injection_failed,
                                  std::string("LIEF signature removal failed: ") + exception.what(),
                                  binary});
  }
}

}  // namespace cyan
