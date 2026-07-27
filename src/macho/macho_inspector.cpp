#include "cyan/macho/macho_inspector.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

#include "macho_internal.hpp"

namespace cyan::macho_internal {
namespace {

enum class MagicKind {
  invalid,
  thin32_little,
  thin32_big,
  thin64_little,
  thin64_big,
  fat32_big,
  fat32_little,
  fat64_big,
  fat64_little
};

template <typename T>
ParseResult<T> fail(InjectionError error, std::string message) {
  return {std::nullopt, {error, std::move(message)}};
}

template <typename T>
ParseResult<T> okay(T value) {
  return {std::move(value), {}};
}

bool checked_add(std::size_t left, std::size_t right, std::size_t& output) {
  if (right > (std::numeric_limits<std::size_t>::max)() - left) {
    return false;
  }
  output = left + right;
  return true;
}

bool checked_multiply(std::size_t left, std::size_t right, std::size_t& output) {
  if (left != 0U && right > (std::numeric_limits<std::size_t>::max)() / left) {
    return false;
  }
  output = left * right;
  return true;
}

MagicKind detect_magic(std::span<const std::uint8_t> bytes) {
  if (bytes.size() < 4U) {
    return MagicKind::invalid;
  }
  const std::array<std::uint8_t, 4> magic{bytes[0], bytes[1], bytes[2], bytes[3]};
  if (magic == std::array<std::uint8_t, 4>{0xceU, 0xfaU, 0xedU, 0xfeU}) {
    return MagicKind::thin32_little;
  }
  if (magic == std::array<std::uint8_t, 4>{0xfeU, 0xedU, 0xfaU, 0xceU}) {
    return MagicKind::thin32_big;
  }
  if (magic == std::array<std::uint8_t, 4>{0xcfU, 0xfaU, 0xedU, 0xfeU}) {
    return MagicKind::thin64_little;
  }
  if (magic == std::array<std::uint8_t, 4>{0xfeU, 0xedU, 0xfaU, 0xcfU}) {
    return MagicKind::thin64_big;
  }
  if (magic == std::array<std::uint8_t, 4>{0xcaU, 0xfeU, 0xbaU, 0xbeU}) {
    return MagicKind::fat32_big;
  }
  if (magic == std::array<std::uint8_t, 4>{0xbeU, 0xbaU, 0xfeU, 0xcaU}) {
    return MagicKind::fat32_little;
  }
  if (magic == std::array<std::uint8_t, 4>{0xcaU, 0xfeU, 0xbaU, 0xbfU}) {
    return MagicKind::fat64_big;
  }
  if (magic == std::array<std::uint8_t, 4>{0xbfU, 0xbaU, 0xfeU, 0xcaU}) {
    return MagicKind::fat64_little;
  }
  return MagicKind::invalid;
}

void include_data_offset(std::size_t candidate, std::size_t& first_data) {
  if (candidate != 0U) {
    first_data = (std::min)(first_data, candidate);
  }
}

ParseResult<std::string> command_string(std::span<const std::uint8_t> bytes, const Command& command,
                                        std::uint32_t string_offset) {
  if (string_offset >= command.size) {
    return fail<std::string>(InjectionError::InvalidMachO,
                             "load-command string offset is outside its command");
  }
  const std::size_t begin = command.offset + static_cast<std::size_t>(string_offset);
  const std::size_t end = command.offset + static_cast<std::size_t>(command.size);
  auto terminator =
      std::find(bytes.begin() + static_cast<std::ptrdiff_t>(begin),
                bytes.begin() + static_cast<std::ptrdiff_t>(end), static_cast<std::uint8_t>(0));
  if (terminator == bytes.begin() + static_cast<std::ptrdiff_t>(end)) {
    return fail<std::string>(InjectionError::InvalidMachO,
                             "load-command string is not NUL terminated");
  }
  const auto length = static_cast<std::size_t>(
      std::distance(bytes.begin() + static_cast<std::ptrdiff_t>(begin), terminator));
  return okay(std::string(reinterpret_cast<const char*>(bytes.data() + begin), length));
}

ParseResult<Slice> parse_slice(std::span<const std::uint8_t> file, std::size_t file_offset,
                               std::size_t file_size) {
  if (file_offset > file.size() || file_size > file.size() - file_offset || file_size < 4U) {
    return fail<Slice>(InjectionError::InvalidMachO, "Mach-O slice is outside the file");
  }
  const auto bytes = file.subspan(file_offset, file_size);
  const MagicKind magic = detect_magic(bytes);

  Slice slice;
  slice.file_offset = file_offset;
  slice.file_size = file_size;
  switch (magic) {
    case MagicKind::thin32_little:
      slice.byte_order = ByteOrder::little;
      slice.is_64_bit = false;
      break;
    case MagicKind::thin32_big:
      slice.byte_order = ByteOrder::big;
      slice.is_64_bit = false;
      break;
    case MagicKind::thin64_little:
      slice.byte_order = ByteOrder::little;
      slice.is_64_bit = true;
      break;
    case MagicKind::thin64_big:
      slice.byte_order = ByteOrder::big;
      slice.is_64_bit = true;
      break;
    default:
      return fail<Slice>(InjectionError::UnsupportedMagic,
                         "slice does not contain a supported Mach-O magic");
  }

  slice.header_size = slice.is_64_bit ? 32U : 28U;
  if (bytes.size() < slice.header_size) {
    return fail<Slice>(InjectionError::InvalidMachO, "truncated Mach-O header");
  }
  slice.cpu_type = static_cast<std::int32_t>(read_u32(bytes, 4U, slice.byte_order));
  slice.cpu_subtype = static_cast<std::int32_t>(read_u32(bytes, 8U, slice.byte_order));
  slice.command_count = read_u32(bytes, 16U, slice.byte_order);
  slice.commands_size = read_u32(bytes, 20U, slice.byte_order);
  if (slice.command_count > 1'000'000U) {
    return fail<Slice>(InjectionError::InvalidMachO, "unreasonable Mach-O load-command count");
  }

  std::size_t commands_end = 0;
  if (!checked_add(slice.header_size, static_cast<std::size_t>(slice.commands_size),
                   commands_end) ||
      commands_end > bytes.size()) {
    return fail<Slice>(InjectionError::InvalidMachO, "Mach-O load-command table exceeds slice");
  }

  slice.first_data_offset = bytes.size();
  std::size_t cursor = slice.header_size;
  for (std::uint32_t index = 0; index < slice.command_count; ++index) {
    if (cursor > commands_end || commands_end - cursor < 8U) {
      return fail<Slice>(InjectionError::InvalidMachO, "truncated Mach-O load command");
    }
    Command command{cursor, read_u32(bytes, cursor, slice.byte_order),
                    read_u32(bytes, cursor + 4U, slice.byte_order)};
    if (command.size < 8U || (command.size % 4U) != 0U ||
        static_cast<std::size_t>(command.size) > commands_end - cursor) {
      return fail<Slice>(InjectionError::InvalidMachO, "invalid Mach-O load-command size");
    }
    slice.commands.push_back(command);

    if (is_dylib_command(command.type)) {
      if (command.size < 24U) {
        return fail<Slice>(InjectionError::InvalidMachO, "truncated dylib load command");
      }
      auto path = command_string(bytes, command, read_u32(bytes, cursor + 8U, slice.byte_order));
      if (!path) {
        return fail<Slice>(path.failure.error, path.failure.message);
      }
      slice.dependencies.push_back(std::move(*path.value));
    } else if (command.type == lc_rpath) {
      if (command.size < 12U) {
        return fail<Slice>(InjectionError::InvalidMachO, "truncated rpath load command");
      }
      auto path = command_string(bytes, command, read_u32(bytes, cursor + 8U, slice.byte_order));
      if (!path) {
        return fail<Slice>(path.failure.error, path.failure.message);
      }
      slice.rpaths.push_back(std::move(*path.value));
    } else if (command.type == lc_encryption_info || command.type == lc_encryption_info_64) {
      if (command.size < 20U) {
        return fail<Slice>(InjectionError::InvalidMachO, "truncated encryption load command");
      }
      include_data_offset(static_cast<std::size_t>(read_u32(bytes, cursor + 8U, slice.byte_order)),
                          slice.first_data_offset);
      slice.encrypted = slice.encrypted || read_u32(bytes, cursor + 16U, slice.byte_order) != 0U;
    } else if (command.type == lc_code_signature) {
      if (command.size < 16U || slice.signature) {
        return fail<Slice>(InjectionError::InvalidMachO,
                           "invalid or duplicate code-signature command");
      }
      Signature signature;
      signature.command_offset = cursor;
      signature.command_index = static_cast<std::size_t>(index);
      signature.command_size = command.size;
      signature.data_offset = read_u32(bytes, cursor + 8U, slice.byte_order);
      signature.data_size = read_u32(bytes, cursor + 12U, slice.byte_order);
      slice.signature = signature;
      include_data_offset(static_cast<std::size_t>(signature.data_offset), slice.first_data_offset);
    } else if (command.type == lc_symtab) {
      if (command.size < 24U || slice.symtab) {
        return fail<Slice>(InjectionError::InvalidSymtab, "invalid or duplicate LC_SYMTAB");
      }
      Symtab symtab;
      symtab.command_offset = cursor;
      symtab.string_offset = read_u32(bytes, cursor + 16U, slice.byte_order);
      symtab.string_size = read_u32(bytes, cursor + 20U, slice.byte_order);
      slice.symtab = symtab;
      include_data_offset(static_cast<std::size_t>(read_u32(bytes, cursor + 8U, slice.byte_order)),
                          slice.first_data_offset);
      include_data_offset(static_cast<std::size_t>(symtab.string_offset), slice.first_data_offset);
    }

    if (command.type == lc_segment) {
      if (command.size < 56U) {
        return fail<Slice>(InjectionError::InvalidMachO, "truncated LC_SEGMENT");
      }
      const std::uint32_t section_count = read_u32(bytes, cursor + 48U, slice.byte_order);
      std::size_t sections_size = 0;
      std::size_t required = 0;
      if (!checked_multiply(static_cast<std::size_t>(section_count), 68U, sections_size) ||
          !checked_add(56U, sections_size, required) || required > command.size) {
        return fail<Slice>(InjectionError::InvalidMachO, "LC_SEGMENT sections exceed command");
      }
      const std::uint32_t fileoff = read_u32(bytes, cursor + 32U, slice.byte_order);
      include_data_offset(static_cast<std::size_t>(fileoff), slice.first_data_offset);
      for (std::uint32_t section = 0; section < section_count; ++section) {
        const std::size_t section_offset = cursor + 56U + static_cast<std::size_t>(section) * 68U;
        include_data_offset(
            static_cast<std::size_t>(read_u32(bytes, section_offset + 40U, slice.byte_order)),
            slice.first_data_offset);
      }
      if (std::memcmp(bytes.data() + cursor + 8U, "__LINKEDIT", 10U) == 0) {
        if (slice.linkedit) {
          return fail<Slice>(InjectionError::InvalidMachO, "duplicate __LINKEDIT segment");
        }
        slice.linkedit = LinkEdit{cursor,
                                  false,
                                  read_u32(bytes, cursor + 24U, slice.byte_order),
                                  read_u32(bytes, cursor + 28U, slice.byte_order),
                                  fileoff,
                                  read_u32(bytes, cursor + 36U, slice.byte_order)};
      }
    } else if (command.type == lc_segment_64) {
      if (command.size < 72U) {
        return fail<Slice>(InjectionError::InvalidMachO, "truncated LC_SEGMENT_64");
      }
      const std::uint32_t section_count = read_u32(bytes, cursor + 64U, slice.byte_order);
      std::size_t sections_size = 0;
      std::size_t required = 0;
      if (!checked_multiply(static_cast<std::size_t>(section_count), 80U, sections_size) ||
          !checked_add(72U, sections_size, required) || required > command.size) {
        return fail<Slice>(InjectionError::InvalidMachO, "LC_SEGMENT_64 sections exceed command");
      }
      const std::uint64_t fileoff = read_u64(bytes, cursor + 40U, slice.byte_order);
      if (fileoff <= (std::numeric_limits<std::size_t>::max)()) {
        include_data_offset(static_cast<std::size_t>(fileoff), slice.first_data_offset);
      }
      for (std::uint32_t section = 0; section < section_count; ++section) {
        const std::size_t section_offset = cursor + 72U + static_cast<std::size_t>(section) * 80U;
        include_data_offset(
            static_cast<std::size_t>(read_u32(bytes, section_offset + 48U, slice.byte_order)),
            slice.first_data_offset);
      }
      if (std::memcmp(bytes.data() + cursor + 8U, "__LINKEDIT", 10U) == 0) {
        if (slice.linkedit) {
          return fail<Slice>(InjectionError::InvalidMachO, "duplicate __LINKEDIT segment");
        }
        slice.linkedit = LinkEdit{cursor,
                                  true,
                                  read_u64(bytes, cursor + 24U, slice.byte_order),
                                  read_u64(bytes, cursor + 32U, slice.byte_order),
                                  fileoff,
                                  read_u64(bytes, cursor + 48U, slice.byte_order)};
      }
    } else if (command.type == lc_dysymtab && command.size >= 80U) {
      for (const std::size_t field : {32U, 40U, 48U, 56U, 64U, 72U}) {
        include_data_offset(
            static_cast<std::size_t>(read_u32(bytes, cursor + field, slice.byte_order)),
            slice.first_data_offset);
      }
    } else if ((command.type == 0x00000022U || command.type == 0x80000022U) &&
               command.size >= 48U) {
      for (const std::size_t field : {8U, 16U, 24U, 32U, 40U}) {
        include_data_offset(
            static_cast<std::size_t>(read_u32(bytes, cursor + field, slice.byte_order)),
            slice.first_data_offset);
      }
    } else if ((command.type == 0x0000001eU || command.type == 0x00000026U ||
                command.type == 0x00000029U || command.type == 0x0000002bU ||
                command.type == 0x0000002eU || command.type == 0x00000033U) &&
               command.size >= 16U) {
      include_data_offset(static_cast<std::size_t>(read_u32(bytes, cursor + 8U, slice.byte_order)),
                          slice.first_data_offset);
    }

    cursor += static_cast<std::size_t>(command.size);
  }

  if (cursor != commands_end) {
    return fail<Slice>(InjectionError::InvalidMachO,
                       "Mach-O sizeofcmds does not equal load-command sizes");
  }
  if (slice.first_data_offset < commands_end || slice.first_data_offset > slice.file_size) {
    return fail<Slice>(InjectionError::InvalidMachO,
                       "Mach-O load commands overlap file-backed content");
  }
  return okay(std::move(slice));
}

}  // namespace

std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t offset, ByteOrder order) {
  if (order == ByteOrder::little) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
  }
  return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
         (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
         (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
         static_cast<std::uint32_t>(bytes[offset + 3U]);
}

std::uint64_t read_u64(std::span<const std::uint8_t> bytes, std::size_t offset, ByteOrder order) {
  if (order == ByteOrder::little) {
    return static_cast<std::uint64_t>(read_u32(bytes, offset, order)) |
           (static_cast<std::uint64_t>(read_u32(bytes, offset + 4U, order)) << 32U);
  }
  return (static_cast<std::uint64_t>(read_u32(bytes, offset, order)) << 32U) |
         static_cast<std::uint64_t>(read_u32(bytes, offset + 4U, order));
}

void write_u32(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value,
               ByteOrder order) {
  if (order == ByteOrder::little) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xffU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    bytes[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
  } else {
    bytes[offset] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    bytes[offset + 3U] = static_cast<std::uint8_t>(value & 0xffU);
  }
}

void write_u64(std::span<std::uint8_t> bytes, std::size_t offset, std::uint64_t value,
               ByteOrder order) {
  if (order == ByteOrder::little) {
    write_u32(bytes, offset, static_cast<std::uint32_t>(value & 0xffffffffULL), order);
    write_u32(bytes, offset + 4U, static_cast<std::uint32_t>(value >> 32U), order);
  } else {
    write_u32(bytes, offset, static_cast<std::uint32_t>(value >> 32U), order);
    write_u32(bytes, offset + 4U, static_cast<std::uint32_t>(value & 0xffffffffULL), order);
  }
}

bool is_dylib_command(std::uint32_t command) {
  return command == lc_load_dylib || command == lc_load_weak_dylib ||
         command == lc_reexport_dylib || command == lc_lazy_load_dylib ||
         command == lc_load_upward_dylib;
}

ParseResult<Container> parse_container(std::span<const std::uint8_t> bytes) {
  const MagicKind magic = detect_magic(bytes);
  if (magic == MagicKind::invalid) {
    return fail<Container>(InjectionError::UnsupportedMagic, "unsupported Mach-O magic");
  }

  Container container;
  const bool is_fat = magic == MagicKind::fat32_big || magic == MagicKind::fat32_little ||
                      magic == MagicKind::fat64_big || magic == MagicKind::fat64_little;
  if (!is_fat) {
    auto slice = parse_slice(bytes, 0U, bytes.size());
    if (!slice) {
      return fail<Container>(slice.failure.error, slice.failure.message);
    }
    container.slices.push_back(std::move(*slice.value));
    return okay(std::move(container));
  }

  container.is_fat = true;
  container.is_fat64 = magic == MagicKind::fat64_big || magic == MagicKind::fat64_little;
  container.fat_byte_order = (magic == MagicKind::fat32_little || magic == MagicKind::fat64_little)
                                 ? ByteOrder::little
                                 : ByteOrder::big;
  if (bytes.size() < 8U) {
    return fail<Container>(InjectionError::InvalidFatHeader, "truncated FAT header");
  }
  const std::uint32_t count = read_u32(bytes, 4U, container.fat_byte_order);
  if (count == 0U || count > 4096U) {
    return fail<Container>(InjectionError::InvalidFatHeader, "invalid FAT architecture count");
  }

  const std::size_t entry_size = container.is_fat64 ? 32U : 20U;
  std::size_t entries_size = 0;
  if (!checked_multiply(static_cast<std::size_t>(count), entry_size, entries_size) ||
      !checked_add(8U, entries_size, container.fat_table_end) ||
      container.fat_table_end > bytes.size()) {
    return fail<Container>(InjectionError::InvalidFatHeader, "FAT architecture table is truncated");
  }

  std::vector<std::pair<std::size_t, std::size_t>> ranges;
  for (std::uint32_t index = 0; index < count; ++index) {
    const std::size_t table_offset = 8U + static_cast<std::size_t>(index) * entry_size;
    const std::int32_t fat_cpu_type = static_cast<std::int32_t>(
        read_u32(bytes, table_offset, container.fat_byte_order));
    const std::int32_t fat_cpu_subtype = static_cast<std::int32_t>(
        read_u32(bytes, table_offset + 4U, container.fat_byte_order));
    const std::uint64_t offset64 =
        container.is_fat64 ? read_u64(bytes, table_offset + 8U, container.fat_byte_order)
                           : read_u32(bytes, table_offset + 8U, container.fat_byte_order);
    const std::uint64_t size64 =
        container.is_fat64 ? read_u64(bytes, table_offset + 16U, container.fat_byte_order)
                           : read_u32(bytes, table_offset + 12U, container.fat_byte_order);
    const std::uint32_t alignment =
        read_u32(bytes, table_offset + (container.is_fat64 ? 24U : 16U), container.fat_byte_order);
    if (offset64 > (std::numeric_limits<std::size_t>::max)() ||
        size64 > (std::numeric_limits<std::size_t>::max)() || alignment > 62U) {
      return fail<Container>(InjectionError::InvalidFatHeader,
                             "FAT slice offset, size, or alignment is unsupported");
    }
    const auto offset = static_cast<std::size_t>(offset64);
    const auto size = static_cast<std::size_t>(size64);
    if (offset < container.fat_table_end || offset > bytes.size() || size > bytes.size() - offset) {
      return fail<Container>(InjectionError::InvalidFatHeader, "FAT slice is outside the file");
    }
    const std::uint64_t required_alignment = 1ULL << alignment;
    if ((offset64 % required_alignment) != 0U) {
      return fail<Container>(InjectionError::InvalidFatHeader, "FAT slice is misaligned");
    }

    container.fat_arches.push_back({table_offset, alignment, offset, size});
    ranges.emplace_back(offset, offset + size);
    auto slice = parse_slice(bytes, offset, size);
    if (!slice) {
      return fail<Container>(slice.failure.error, slice.failure.message);
    }
    slice.value->cpu_type = fat_cpu_type;
    slice.value->cpu_subtype = fat_cpu_subtype;
    container.slices.push_back(std::move(*slice.value));
  }

  std::sort(ranges.begin(), ranges.end());
  for (std::size_t index = 1; index < ranges.size(); ++index) {
    if (ranges[index].first < ranges[index - 1U].second) {
      return fail<Container>(InjectionError::InvalidFatHeader, "FAT slices overlap");
    }
  }
  return okay(std::move(container));
}

}  // namespace cyan::macho_internal

namespace cyan {
namespace {

Result<std::vector<std::uint8_t>> read_binary(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::file_not_found, "could not open Mach-O file", path});
  }
  const std::streamoff length = input.tellg();
  if (length < 0) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::filesystem_error, "could not determine Mach-O file size", path});
  }
  if (static_cast<unsigned long long>(length) >
      static_cast<unsigned long long>((std::numeric_limits<std::size_t>::max)())) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::filesystem_error, "Mach-O file is too large", path});
  }

  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
  input.seekg(0);
  if (!bytes.empty()) {
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  if (!input) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::filesystem_error, "could not read Mach-O file", path});
  }
  return Result<std::vector<std::uint8_t>>::success(std::move(bytes));
}

ErrorCode map_parse_error(InjectionError error) {
  switch (error) {
    case InjectionError::UnsupportedMagic:
    case InjectionError::UnsupportedArchitecture:
      return ErrorCode::macho_unsupported;
    default:
      return ErrorCode::macho_invalid;
  }
}

}  // namespace

Result<MachOInfo> MachOInspector::inspect(const std::filesystem::path& binary) const {
  auto bytes = read_binary(binary);
  if (!bytes) {
    return Result<MachOInfo>::failure(bytes.error());
  }
  auto result = inspect(bytes.value());
  if (!result && result.error().path.empty()) {
    result.error().path = binary;
  }
  return result;
}

Result<MachOInfo> MachOInspector::inspect(std::span<const std::uint8_t> bytes) const {
  auto parsed = macho_internal::parse_container(bytes);
  if (!parsed) {
    return Result<MachOInfo>::failure(
        {map_parse_error(parsed.failure.error), parsed.failure.message, {}});
  }

  MachOInfo information;
  information.is_fat = parsed.value->is_fat;
  information.is_fat64 = parsed.value->is_fat64;
  for (const auto& slice : parsed.value->slices) {
    MachOSliceInfo item;
    item.cpu_type = slice.cpu_type;
    item.cpu_subtype = slice.cpu_subtype;
    item.is_64_bit = slice.is_64_bit;
    item.byte_order = slice.byte_order;
    item.encrypted = slice.encrypted;
    item.has_code_signature = slice.signature.has_value();
    item.dependencies = slice.dependencies;
    item.rpaths = slice.rpaths;
    information.slices.push_back(std::move(item));
  }
  return Result<MachOInfo>::success(std::move(information));
}

Result<void> MachOInspector::verify_dependency(std::span<const std::uint8_t> bytes,
                                               std::string_view dependency,
                                               bool require_signature_removed) const {
  auto inspected = inspect(bytes);
  if (!inspected) {
    return Result<void>::failure(inspected.error());
  }
  if (inspected.value().slices.empty()) {
    return Result<void>::failure(
        {ErrorCode::verification_failed, "Mach-O has no architecture slices", {}});
  }
  for (const auto& slice : inspected.value().slices) {
    if (std::find(slice.dependencies.begin(), slice.dependencies.end(), dependency) ==
        slice.dependencies.end()) {
      return Result<void>::failure({ErrorCode::verification_failed,
                                    "injected dependency is missing from at least one Mach-O slice",
                                    {}});
    }
    if (require_signature_removed && slice.has_code_signature) {
      return Result<void>::failure({ErrorCode::verification_failed,
                                    "code-signature command remains after requested removal",
                                    {}});
    }
  }
  return Result<void>::success();
}

}  // namespace cyan
