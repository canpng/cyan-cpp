#include "cyan/macho/insert_dylib_engine.hpp"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "cyan/macho/macho_inspector.hpp"
#include "macho_internal.hpp"

namespace cyan {
namespace {

struct ModifiedSlice {
  std::vector<std::uint8_t> bytes;
  bool removed_signature{false};
};

struct SliceModification {
  std::optional<ModifiedSlice> value;
  InjectionResult failure;

  explicit operator bool() const noexcept { return value.has_value(); }
};

InjectionResult failed(InjectionError error, std::string message) {
  return {error, std::move(message), false};
}

SliceModification slice_failed(InjectionError error, std::string message) {
  return {std::nullopt, failed(error, std::move(message))};
}

bool checked_add(std::size_t left, std::size_t right, std::size_t& output) {
  if (right > (std::numeric_limits<std::size_t>::max)() - left) {
    return false;
  }
  output = left + right;
  return true;
}

bool checked_add_u64(std::uint64_t left, std::uint64_t right, std::uint64_t& output) {
  if (right > (std::numeric_limits<std::uint64_t>::max)() - left) {
    return false;
  }
  output = left + right;
  return true;
}

bool align_up(std::size_t value, std::size_t alignment, std::size_t& output) {
  if (alignment == 0U || (alignment & (alignment - 1U)) != 0U) {
    return false;
  }
  std::size_t with_padding = 0;
  if (!checked_add(value, alignment - 1U, with_padding)) {
    return false;
  }
  output = with_padding & ~(alignment - 1U);
  return true;
}

bool align_up_u64(std::uint64_t value, std::uint64_t alignment, std::uint64_t& output) {
  if (alignment == 0U || (alignment & (alignment - 1U)) != 0U) {
    return false;
  }
  std::uint64_t with_padding = 0;
  if (!checked_add_u64(value, alignment - 1U, with_padding)) {
    return false;
  }
  output = with_padding & ~(alignment - 1U);
  return true;
}

SliceModification modify_slice(std::span<const std::uint8_t> original,
                               const macho_internal::Slice& slice, std::string_view dylib_path,
                               const InjectionOptions& options) {
  if (!options.allowDuplicate && std::find(slice.dependencies.begin(), slice.dependencies.end(),
                                           dylib_path) != slice.dependencies.end()) {
    return slice_failed(InjectionError::DuplicateLoadCommand,
                        "the requested dylib load command already exists");
  }

  std::size_t raw_command_size = 0;
  if (!checked_add(24U, dylib_path.size(), raw_command_size) ||
      !checked_add(raw_command_size, 1U, raw_command_size)) {
    return slice_failed(InjectionError::ArithmeticOverflow, "dylib load-command size overflow");
  }
  std::size_t command_size = 0;
  if (!align_up(raw_command_size, 8U, command_size) ||
      command_size > (std::numeric_limits<std::uint32_t>::max)()) {
    return slice_failed(InjectionError::ArithmeticOverflow, "dylib load command is too large");
  }

  std::vector<std::uint8_t> bytes(original.begin(), original.end());
  std::uint32_t command_count = slice.command_count;
  std::uint32_t commands_size = slice.commands_size;
  bool removed_signature = false;

  if (options.stripCodeSignature && slice.signature) {
    const auto& signature = *slice.signature;
    if (signature.command_index + 1U != static_cast<std::size_t>(slice.command_count)) {
      return slice_failed(InjectionError::CodeSignatureLayoutUnsupported,
                          "LC_CODE_SIGNATURE is not the final load command");
    }

    std::uint64_t signature_end = 0;
    if (!checked_add_u64(signature.data_offset, signature.data_size, signature_end) ||
        signature_end != bytes.size()) {
      return slice_failed(InjectionError::CodeSignatureLayoutUnsupported,
                          "code-signature blob is not the final slice region");
    }
    if (!slice.linkedit) {
      return slice_failed(InjectionError::LinkEditNotAtEnd,
                          "signed Mach-O has no __LINKEDIT segment");
    }

    const auto& linkedit = *slice.linkedit;
    std::uint64_t linkedit_end = 0;
    if (!checked_add_u64(linkedit.file_offset, linkedit.file_size, linkedit_end) ||
        linkedit_end != bytes.size() || signature.data_offset < linkedit.file_offset) {
      return slice_failed(InjectionError::LinkEditNotAtEnd,
                          "__LINKEDIT does not end at the code-signature boundary");
    }

    const std::uint64_t new_linkedit_size =
        static_cast<std::uint64_t>(signature.data_offset) - linkedit.file_offset;
    const std::uint64_t page_size =
        (linkedit.vm_address % 0x4000U == 0U && linkedit.vm_size % 0x4000U == 0U) ? 0x4000U
                                                                                  : 0x1000U;
    std::uint64_t new_vm_size = 0;
    if (!align_up_u64(new_linkedit_size, page_size, new_vm_size)) {
      return slice_failed(InjectionError::ArithmeticOverflow,
                          "__LINKEDIT VM-size alignment overflow");
    }

    auto mutable_bytes = std::span<std::uint8_t>(bytes);
    if (linkedit.is_64_bit) {
      macho_internal::write_u64(mutable_bytes, linkedit.command_offset + 32U, new_vm_size,
                                slice.byte_order);
      macho_internal::write_u64(mutable_bytes, linkedit.command_offset + 48U, new_linkedit_size,
                                slice.byte_order);
    } else {
      if (new_linkedit_size > (std::numeric_limits<std::uint32_t>::max)() ||
          new_vm_size > (std::numeric_limits<std::uint32_t>::max)()) {
        return slice_failed(InjectionError::ArithmeticOverflow, "32-bit __LINKEDIT size overflow");
      }
      macho_internal::write_u32(mutable_bytes, linkedit.command_offset + 28U,
                                static_cast<std::uint32_t>(new_vm_size), slice.byte_order);
      macho_internal::write_u32(mutable_bytes, linkedit.command_offset + 36U,
                                static_cast<std::uint32_t>(new_linkedit_size), slice.byte_order);
    }

    if (slice.symtab) {
      const auto& symtab = *slice.symtab;
      std::uint64_t string_end = 0;
      if (symtab.string_offset == 0U ||
          !checked_add_u64(symtab.string_offset, symtab.string_size, string_end) ||
          string_end > signature.data_offset || symtab.string_offset > signature.data_offset) {
        return slice_failed(InjectionError::InvalidSymtab,
                            "LC_SYMTAB string table is incompatible with signature removal");
      }
      const std::uint64_t expanded =
          static_cast<std::uint64_t>(signature.data_offset) - symtab.string_offset;
      if (expanded > (std::numeric_limits<std::uint32_t>::max)()) {
        return slice_failed(InjectionError::ArithmeticOverflow, "LC_SYMTAB string size overflow");
      }
      macho_internal::write_u32(mutable_bytes, symtab.command_offset + 20U,
                                static_cast<std::uint32_t>(expanded), slice.byte_order);
    }

    if (command_count == 0U || commands_size < signature.command_size ||
        signature.command_offset > bytes.size() ||
        signature.command_size > bytes.size() - signature.command_offset) {
      return slice_failed(InjectionError::InvalidMachO,
                          "invalid code-signature load-command range");
    }
    std::fill(bytes.begin() + static_cast<std::ptrdiff_t>(signature.command_offset),
              bytes.begin() +
                  static_cast<std::ptrdiff_t>(signature.command_offset + signature.command_size),
              static_cast<std::uint8_t>(0));
    --command_count;
    commands_size -= signature.command_size;
    removed_signature = true;
  }

  std::size_t insertion_offset = 0;
  if (!checked_add(slice.header_size, static_cast<std::size_t>(commands_size), insertion_offset)) {
    return slice_failed(InjectionError::ArithmeticOverflow,
                        "load-command insertion offset overflow");
  }
  std::size_t insertion_end = 0;
  if (!checked_add(insertion_offset, command_size, insertion_end)) {
    return slice_failed(InjectionError::ArithmeticOverflow,
                        "load-command insertion range overflow");
  }
  if (insertion_end > slice.first_data_offset || insertion_end > bytes.size()) {
    return slice_failed(InjectionError::InsufficientLoadCommandSpace,
                        "not enough load-command padding for the requested dylib path");
  }
  if (!options.allowUnsafeOverwrite &&
      std::any_of(bytes.begin() + static_cast<std::ptrdiff_t>(insertion_offset),
                  bytes.begin() + static_cast<std::ptrdiff_t>(insertion_end),
                  [](std::uint8_t byte) { return byte != 0U; })) {
    return slice_failed(InjectionError::InsufficientLoadCommandSpace,
                        "load-command padding contains non-zero data");
  }
  if (command_count == (std::numeric_limits<std::uint32_t>::max)() ||
      command_size >
          static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)() - commands_size)) {
    return slice_failed(InjectionError::ArithmeticOverflow,
                        "Mach-O load-command counters overflow");
  }

  auto mutable_bytes = std::span<std::uint8_t>(bytes);
  std::fill(bytes.begin() + static_cast<std::ptrdiff_t>(insertion_offset),
            bytes.begin() + static_cast<std::ptrdiff_t>(insertion_end),
            static_cast<std::uint8_t>(0));
  macho_internal::write_u32(mutable_bytes, insertion_offset,
                            options.commandType == LoadCommandType::Weak
                                ? macho_internal::lc_load_weak_dylib
                                : macho_internal::lc_load_dylib,
                            slice.byte_order);
  macho_internal::write_u32(mutable_bytes, insertion_offset + 4U,
                            static_cast<std::uint32_t>(command_size), slice.byte_order);
  macho_internal::write_u32(mutable_bytes, insertion_offset + 8U, 24U, slice.byte_order);
  std::memcpy(bytes.data() + insertion_offset + 24U, dylib_path.data(), dylib_path.size());

  ++command_count;
  commands_size += static_cast<std::uint32_t>(command_size);
  macho_internal::write_u32(mutable_bytes, 16U, command_count, slice.byte_order);
  macho_internal::write_u32(mutable_bytes, 20U, commands_size, slice.byte_order);

  if (removed_signature) {
    bytes.resize(slice.signature->data_offset);
  }

  auto reparsed = macho_internal::parse_container(bytes);
  if (!reparsed || reparsed.value->slices.size() != 1U) {
    return slice_failed(InjectionError::VerificationFailure,
                        reparsed ? "modified slice did not reparse as one architecture"
                                 : "modified slice failed to reparse: " + reparsed.failure.message);
  }
  const auto& checked = reparsed.value->slices.front();
  if (std::find(checked.dependencies.begin(), checked.dependencies.end(), dylib_path) ==
      checked.dependencies.end()) {
    return slice_failed(InjectionError::VerificationFailure,
                        "modified slice does not contain the requested dependency");
  }
  if (removed_signature && checked.signature) {
    return slice_failed(InjectionError::VerificationFailure,
                        "modified slice still contains LC_CODE_SIGNATURE");
  }

  return {ModifiedSlice{std::move(bytes), removed_signature}, {}};
}

Result<std::vector<std::uint8_t>> read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::file_not_found, "could not open binary", path});
  }
  const std::streamoff length = input.tellg();
  if (length < 0 ||
      static_cast<unsigned long long>(length) >
          static_cast<unsigned long long>((std::numeric_limits<std::size_t>::max)())) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::filesystem_error, "could not determine a safe binary size", path});
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
  input.seekg(0);
  if (!bytes.empty()) {
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  if (!input) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::filesystem_error, "could not read binary", path});
  }
  return Result<std::vector<std::uint8_t>>::success(std::move(bytes));
}

std::filesystem::path temporary_sibling(const std::filesystem::path& path) {
  const auto ticks =
      static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
  return path.parent_path() / (path.filename().native() + L".cyan-macho-" + std::to_wstring(ticks));
}

InjectionResult write_atomically(const std::filesystem::path& path,
                                 std::span<const std::uint8_t> bytes) {
  const auto temporary = temporary_sibling(path);
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
      return failed(InjectionError::WriteFailure, "could not create temporary Mach-O output");
    }
    if (!bytes.empty()) {
      output.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }
    output.flush();
    if (!output) {
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
      return failed(InjectionError::WriteFailure, "could not write temporary Mach-O output");
    }
  }

  if (!MoveFileExW(temporary.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    return failed(InjectionError::WriteFailure, "could not atomically replace Mach-O file");
  }
  return {InjectionError::None, "injected dylib with native backend", true};
}

std::optional<std::vector<std::uint8_t>> rebuild_fat(std::span<const std::uint8_t> original,
                                                     const macho_internal::Container& container,
                                                     const std::vector<ModifiedSlice>& slices,
                                                     InjectionResult& failure) {
  std::vector<std::uint8_t> output(
      original.begin(), original.begin() + static_cast<std::ptrdiff_t>(container.fat_table_end));

  for (std::size_t index = 0; index < slices.size(); ++index) {
    const auto& architecture = container.fat_arches[index];
    const std::uint64_t alignment64 = 1ULL << architecture.alignment_exponent;
    if (alignment64 > (std::numeric_limits<std::size_t>::max)()) {
      failure = failed(InjectionError::ArithmeticOverflow, "FAT alignment exceeds host size");
      return std::nullopt;
    }
    std::size_t offset = 0;
    if (!align_up(output.size(), static_cast<std::size_t>(alignment64), offset)) {
      failure = failed(InjectionError::ArithmeticOverflow, "FAT slice alignment overflow");
      return std::nullopt;
    }
    output.resize(offset, 0U);
    if (slices[index].bytes.size() > (std::numeric_limits<std::size_t>::max)() - output.size()) {
      failure = failed(InjectionError::ArithmeticOverflow, "FAT output size overflow");
      return std::nullopt;
    }
    output.insert(output.end(), slices[index].bytes.begin(), slices[index].bytes.end());

    auto mutable_output = std::span<std::uint8_t>(output);
    if (container.is_fat64) {
      macho_internal::write_u64(mutable_output, architecture.table_offset + 8U,
                                static_cast<std::uint64_t>(offset), container.fat_byte_order);
      macho_internal::write_u64(mutable_output, architecture.table_offset + 16U,
                                static_cast<std::uint64_t>(slices[index].bytes.size()),
                                container.fat_byte_order);
    } else {
      if (offset > (std::numeric_limits<std::uint32_t>::max)() ||
          slices[index].bytes.size() > (std::numeric_limits<std::uint32_t>::max)()) {
        failure = failed(InjectionError::ArithmeticOverflow, "FAT32 slice offset or size overflow");
        return std::nullopt;
      }
      macho_internal::write_u32(mutable_output, architecture.table_offset + 8U,
                                static_cast<std::uint32_t>(offset), container.fat_byte_order);
      macho_internal::write_u32(mutable_output, architecture.table_offset + 12U,
                                static_cast<std::uint32_t>(slices[index].bytes.size()),
                                container.fat_byte_order);
    }
  }
  return output;
}

}  // namespace

InjectionResult InsertDylibEngine::inject(const std::filesystem::path& binary,
                                          std::string_view dylibPath,
                                          const InjectionOptions& options) {
  if (dylibPath.empty() || dylibPath.find('\0') != std::string_view::npos) {
    return failed(InjectionError::InvalidMachO, "dylib path is empty or contains NUL");
  }

  auto input = read_file(binary);
  if (!input) {
    return failed(input.error().code == ErrorCode::file_not_found ? InjectionError::FileNotFound
                                                                  : InjectionError::ReadFailure,
                  input.error().message);
  }

  auto parsed = macho_internal::parse_container(input.value());
  if (!parsed) {
    return failed(parsed.failure.error, parsed.failure.message);
  }

  std::vector<ModifiedSlice> modified_slices;
  modified_slices.reserve(parsed.value->slices.size());
  bool removed_any_signature = false;
  for (const auto& slice : parsed.value->slices) {
    const auto original =
        std::span<const std::uint8_t>(input.value()).subspan(slice.file_offset, slice.file_size);
    auto modified = modify_slice(original, slice, dylibPath, options);
    if (!modified) {
      return modified.failure;
    }
    removed_any_signature = removed_any_signature || modified.value->removed_signature;
    modified_slices.push_back(std::move(*modified.value));
  }

  std::vector<std::uint8_t> output;
  if (parsed.value->is_fat) {
    InjectionResult rebuild_failure;
    auto rebuilt = rebuild_fat(input.value(), *parsed.value, modified_slices, rebuild_failure);
    if (!rebuilt) {
      return rebuild_failure;
    }
    output = std::move(*rebuilt);
  } else {
    output = std::move(modified_slices.front().bytes);
  }

  MachOInspector inspector;
  auto verified = inspector.verify_dependency(output, dylibPath, removed_any_signature);
  if (!verified) {
    return failed(InjectionError::VerificationFailure, verified.error().message);
  }
  return write_atomically(binary, output);
}

}  // namespace cyan
