#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace cyan {

enum class LoadCommandType { Strong, Weak };

enum class FatArchitecturePolicy { PreserveAll, IpaPatchV213 };

enum class InjectionError {
  None,
  FileNotFound,
  InvalidMachO,
  UnsupportedMagic,
  UnsupportedArchitecture,
  InvalidFatHeader,
  DuplicateLoadCommand,
  InsufficientLoadCommandSpace,
  CodeSignatureLayoutUnsupported,
  LinkEditNotAtEnd,
  InvalidSymtab,
  ArithmeticOverflow,
  ReadFailure,
  WriteFailure,
  VerificationFailure
};

struct InjectionOptions {
  LoadCommandType commandType{LoadCommandType::Weak};
  bool stripCodeSignature{true};
  bool allowDuplicate{false};
  bool allowUnsafeOverwrite{false};
  FatArchitecturePolicy fatArchitecturePolicy{FatArchitecturePolicy::PreserveAll};
};

struct InjectionResult {
  InjectionError error{InjectionError::None};
  std::string message;
  bool modified{false};
};

class InsertDylibEngine {
 public:
  InjectionResult inject(const std::filesystem::path& binary, std::string_view dylibPath,
                         const InjectionOptions& options);
};

}  // namespace cyan
