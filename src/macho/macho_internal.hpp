#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "cyan/macho/insert_dylib_engine.hpp"
#include "cyan/macho/macho_inspector.hpp"

namespace cyan::macho_internal {

constexpr std::uint32_t lc_segment = 0x00000001U;
constexpr std::uint32_t lc_symtab = 0x00000002U;
constexpr std::uint32_t lc_dysymtab = 0x0000000bU;
constexpr std::uint32_t lc_load_dylib = 0x0000000cU;
constexpr std::uint32_t lc_id_dylib = 0x0000000dU;
constexpr std::uint32_t lc_load_weak_dylib = 0x80000018U;
constexpr std::uint32_t lc_segment_64 = 0x00000019U;
constexpr std::uint32_t lc_code_signature = 0x0000001dU;
constexpr std::uint32_t lc_reexport_dylib = 0x8000001fU;
constexpr std::uint32_t lc_lazy_load_dylib = 0x00000020U;
constexpr std::uint32_t lc_encryption_info = 0x00000021U;
constexpr std::uint32_t lc_rpath = 0x8000001cU;
constexpr std::uint32_t lc_load_upward_dylib = 0x80000023U;
constexpr std::uint32_t lc_encryption_info_64 = 0x0000002cU;

struct ParseFailure {
  InjectionError error{InjectionError::InvalidMachO};
  std::string message;
};

template <typename T>
struct ParseResult {
  std::optional<T> value;
  ParseFailure failure;

  explicit operator bool() const noexcept { return value.has_value(); }
};

struct Command {
  std::size_t offset{0};
  std::uint32_t type{0};
  std::uint32_t size{0};
};

struct LinkEdit {
  std::size_t command_offset{0};
  bool is_64_bit{false};
  std::uint64_t vm_address{0};
  std::uint64_t vm_size{0};
  std::uint64_t file_offset{0};
  std::uint64_t file_size{0};
};

struct Symtab {
  std::size_t command_offset{0};
  std::uint32_t string_offset{0};
  std::uint32_t string_size{0};
};

struct Signature {
  std::size_t command_offset{0};
  std::size_t command_index{0};
  std::uint32_t command_size{0};
  std::uint32_t data_offset{0};
  std::uint32_t data_size{0};
};

struct Slice {
  std::size_t file_offset{0};
  std::size_t file_size{0};
  ByteOrder byte_order{ByteOrder::little};
  bool is_64_bit{false};
  std::int32_t cpu_type{0};
  std::int32_t cpu_subtype{0};
  std::uint32_t command_count{0};
  std::uint32_t commands_size{0};
  std::size_t header_size{0};
  std::size_t first_data_offset{0};
  std::vector<Command> commands;
  std::vector<std::string> dependencies;
  std::vector<std::string> rpaths;
  std::optional<LinkEdit> linkedit;
  std::optional<Symtab> symtab;
  std::optional<Signature> signature;
  bool encrypted{false};
};

struct FatArch {
  std::size_t table_offset{0};
  std::uint32_t alignment_exponent{0};
  std::size_t slice_offset{0};
  std::size_t slice_size{0};
};

struct Container {
  bool is_fat{false};
  bool is_fat64{false};
  ByteOrder fat_byte_order{ByteOrder::big};
  std::size_t fat_table_end{0};
  std::vector<FatArch> fat_arches;
  std::vector<Slice> slices;
};

ParseResult<Container> parse_container(std::span<const std::uint8_t> bytes);

std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t offset, ByteOrder order);
std::uint64_t read_u64(std::span<const std::uint8_t> bytes, std::size_t offset, ByteOrder order);
void write_u32(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value,
               ByteOrder order);
void write_u64(std::span<std::uint8_t> bytes, std::size_t offset, std::uint64_t value,
               ByteOrder order);

bool is_dylib_command(std::uint32_t command);

}  // namespace cyan::macho_internal
