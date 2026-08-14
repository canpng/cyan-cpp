#include "cyan/plist/plist_document.hpp"

#include <plist/plist.h>

#include <cstdint>
#include <fstream>
#include <limits>
#include <utility>
#include <vector>

namespace cyan {
namespace {

Result<std::vector<char>> read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    return Result<std::vector<char>>::failure(
        {ErrorCode::file_not_found, "could not open property list", path});
  }

  const std::streamoff end = input.tellg();
  if (end < 0 || static_cast<std::uint64_t>(end) >
                     static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
    return Result<std::vector<char>>::failure(
        {ErrorCode::archive_limit_exceeded, "property list is too large", path});
  }

  std::vector<char> bytes(static_cast<std::size_t>(end));
  input.seekg(0, std::ios::beg);
  if (!bytes.empty()) {
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  }
  if (!input) {
    return Result<std::vector<char>>::failure(
        {ErrorCode::filesystem_error, "could not read property list", path});
  }
  return Result<std::vector<char>>::success(std::move(bytes));
}

Result<void> write_atomic(const std::filesystem::path& path, const char* data,
                          std::uint32_t length) {
  const auto temporary = path.parent_path() / (path.filename().native() + L".cyan.tmp");
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not create temporary property list", temporary});
    }
    output.write(data, static_cast<std::streamsize>(length));
    if (!output) {
      return Result<void>::failure(
          {ErrorCode::filesystem_error, "could not write property list", temporary});
    }
  }

  std::error_code error;
  std::filesystem::rename(temporary, path, error);
  if (error) {
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
  }
  if (error) {
    std::filesystem::remove(temporary, error);
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not replace property list", path});
  }
  return Result<void>::success();
}

plist_t make_icon_dictionary(std::string_view icon_name,
                             const std::vector<std::string>& files_value) {
  plist_t primary = plist_new_dict();
  plist_t files = plist_new_array();
  for (const auto& file : files_value) {
    plist_array_append_item(files, plist_new_string(file.c_str()));
  }
  plist_dict_set_item(primary, "CFBundleIconFiles", files);
  plist_dict_set_item(primary, "CFBundleIconName",
                      plist_new_string(std::string(icon_name).c_str()));

  plist_t icons = plist_new_dict();
  plist_dict_set_item(icons, "CFBundlePrimaryIcon", primary);
  return icons;
}

}  // namespace

struct PlistDocument::Impl {
  plist_t root{nullptr};
  PlistFormat format{PlistFormat::xml};

  ~Impl() {
    if (root != nullptr) {
      plist_free(root);
    }
  }
};

PlistDocument::PlistDocument() = default;
PlistDocument::~PlistDocument() = default;
PlistDocument::PlistDocument(PlistDocument&&) noexcept = default;
PlistDocument& PlistDocument::operator=(PlistDocument&&) noexcept = default;

PlistDocument::PlistDocument(std::unique_ptr<Impl> implementation)
    : implementation_(std::move(implementation)) {}

Result<PlistDocument> PlistDocument::create_dictionary() {
  auto implementation = std::make_unique<Impl>();
  implementation->root = plist_new_dict();
  if (implementation->root == nullptr) {
    return Result<PlistDocument>::failure(
        {ErrorCode::internal_error, "could not allocate property list dictionary", {}});
  }
  return Result<PlistDocument>::success(PlistDocument(std::move(implementation)));
}

Result<PlistDocument> PlistDocument::load(const std::filesystem::path& path) {
  auto file = read_file(path);
  if (!file) {
    return Result<PlistDocument>::failure(file.error());
  }

  const auto& bytes = file.value();
  const auto view = std::span<const char>(bytes.data(), bytes.size());
  auto parsed = load_memory(std::as_bytes(view));
  if (!parsed) {
    Error error = parsed.error();
    error.path = path;
    return Result<PlistDocument>::failure(std::move(error));
  }
  return parsed;
}

Result<PlistDocument> PlistDocument::load_memory(std::span<const std::byte> bytes) {
  if (bytes.size() > static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())) {
    return Result<PlistDocument>::failure(
        {ErrorCode::archive_limit_exceeded, "property list is too large", {}});
  }
  auto implementation = std::make_unique<Impl>();
  plist_format_t parsed_format = PLIST_FORMAT_NONE;
  const plist_err_t parsed = plist_from_memory(reinterpret_cast<const char*>(bytes.data()),
                                               static_cast<std::uint32_t>(bytes.size()),
                                               &implementation->root, &parsed_format);
  if (parsed != PLIST_ERR_SUCCESS || implementation->root == nullptr) {
    return Result<PlistDocument>::failure(
        {ErrorCode::json_invalid, "invalid XML or binary property list", {}});
  }
  if (plist_get_node_type(implementation->root) != PLIST_DICT) {
    return Result<PlistDocument>::failure(
        {ErrorCode::invalid_argument, "property list root must be a dictionary", {}});
  }
  implementation->format =
      parsed_format == PLIST_FORMAT_BINARY ? PlistFormat::binary : PlistFormat::xml;
  return Result<PlistDocument>::success(PlistDocument(std::move(implementation)));
}

PlistFormat PlistDocument::source_format() const noexcept {
  return implementation_ ? implementation_->format : PlistFormat::xml;
}

bool PlistDocument::contains(std::string_view key) const {
  if (!implementation_) {
    return false;
  }
  const std::string owned_key(key);
  return plist_dict_get_item(implementation_->root, owned_key.c_str()) != nullptr;
}

std::optional<std::string> PlistDocument::string(std::string_view key) const {
  return string_path({key});
}

std::optional<std::string> PlistDocument::string_path(
    std::initializer_list<std::string_view> keys) const {
  if (!implementation_) {
    return std::nullopt;
  }
  plist_t item = implementation_->root;
  for (const auto key : keys) {
    if (item == nullptr || plist_get_node_type(item) != PLIST_DICT) {
      return std::nullopt;
    }
    const std::string owned_key(key);
    item = plist_dict_get_item(item, owned_key.c_str());
  }
  if (item == nullptr || plist_get_node_type(item) != PLIST_STRING) {
    return std::nullopt;
  }

  char* value = nullptr;
  plist_get_string_val(item, &value);
  if (value == nullptr) {
    return std::nullopt;
  }
  std::string result(value);
  plist_mem_free(value);
  return result;
}

std::vector<std::string> PlistDocument::string_array(std::string_view key) const {
  return string_array_path({key});
}

std::vector<std::string> PlistDocument::string_array_path(
    std::initializer_list<std::string_view> keys) const {
  std::vector<std::string> values;
  if (!implementation_) {
    return values;
  }
  plist_t item = implementation_->root;
  for (const auto key : keys) {
    if (item == nullptr || plist_get_node_type(item) != PLIST_DICT) {
      return values;
    }
    const std::string owned_key(key);
    item = plist_dict_get_item(item, owned_key.c_str());
  }
  if (item == nullptr || plist_get_node_type(item) != PLIST_ARRAY) {
    return values;
  }
  const std::uint32_t count = plist_array_get_size(item);
  values.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    plist_t value = plist_array_get_item(item, index);
    if (value == nullptr || plist_get_node_type(value) != PLIST_STRING) {
      continue;
    }
    char* text = nullptr;
    plist_get_string_val(value, &text);
    if (text != nullptr) {
      values.emplace_back(text);
      plist_mem_free(text);
    }
  }
  return values;
}

std::optional<bool> PlistDocument::boolean(std::string_view key) const {
  if (!implementation_) {
    return std::nullopt;
  }
  const std::string owned_key(key);
  plist_t item = plist_dict_get_item(implementation_->root, owned_key.c_str());
  if (item == nullptr || plist_get_node_type(item) != PLIST_BOOLEAN) {
    return std::nullopt;
  }

  std::uint8_t value = 0;
  plist_get_bool_val(item, &value);
  return value != 0;
}

Result<void> PlistDocument::set_string(std::string_view key, std::string_view value) {
  if (!implementation_) {
    return Result<void>::failure(
        {ErrorCode::internal_error, "property list is not initialized", {}});
  }
  const std::string owned_key(key);
  const std::string owned_value(value);
  plist_dict_set_item(implementation_->root, owned_key.c_str(),
                      plist_new_string(owned_value.c_str()));
  return Result<void>::success();
}

Result<void> PlistDocument::set_boolean(std::string_view key, bool value) {
  if (!implementation_) {
    return Result<void>::failure(
        {ErrorCode::internal_error, "property list is not initialized", {}});
  }
  const std::string owned_key(key);
  plist_dict_set_item(implementation_->root, owned_key.c_str(), plist_new_bool(value ? 1U : 0U));
  return Result<void>::success();
}

Result<void> PlistDocument::set_string_array(std::string_view key,
                                             const std::vector<std::string>& values) {
  if (!implementation_) {
    return Result<void>::failure(
        {ErrorCode::internal_error, "property list is not initialized", {}});
  }
  plist_t array = plist_new_array();
  for (const auto& value : values) {
    plist_array_append_item(array, plist_new_string(value.c_str()));
  }
  const std::string owned_key(key);
  plist_dict_set_item(implementation_->root, owned_key.c_str(), array);
  return Result<void>::success();
}

Result<void> PlistDocument::remove(std::string_view key) {
  if (!implementation_) {
    return Result<void>::failure(
        {ErrorCode::internal_error, "property list is not initialized", {}});
  }
  const std::string owned_key(key);
  plist_dict_remove_item(implementation_->root, owned_key.c_str());
  return Result<void>::success();
}

Result<void> PlistDocument::merge(const PlistDocument& overlay) {
  if (!implementation_ || !overlay.implementation_) {
    return Result<void>::failure(
        {ErrorCode::internal_error, "property list is not initialized", {}});
  }
  plist_dict_merge(&implementation_->root, overlay.implementation_->root);
  return Result<void>::success();
}

Result<void> PlistDocument::set_icon_configuration(std::string_view icon_name,
                                                   std::string_view phone_base_name,
                                                   std::string_view tablet_base_name) {
  if (!implementation_ || icon_name.empty() || phone_base_name.empty() ||
      tablet_base_name.empty()) {
    return Result<void>::failure({ErrorCode::invalid_argument, "icon names must not be empty", {}});
  }
  const std::string owned_icon_name(icon_name);
  const std::string phone(phone_base_name);
  const std::string tablet(tablet_base_name);
  plist_dict_set_item(implementation_->root, "CFBundleIcons",
                      make_icon_dictionary(icon_name, {phone}));
  plist_dict_set_item(implementation_->root, "CFBundleIcons~ipad",
                      make_icon_dictionary(icon_name, {phone, tablet}));
  plist_dict_set_item(implementation_->root, "CFBundleIconName",
                      plist_new_string(owned_icon_name.c_str()));
  return set_string_array("CFBundleIconFiles", {phone, tablet});
}

Result<void> PlistDocument::save(const std::filesystem::path& path,
                                 std::optional<PlistFormat> format) const {
  if (!implementation_) {
    return Result<void>::failure(
        {ErrorCode::internal_error, "property list is not initialized", path});
  }

  char* serialized = nullptr;
  std::uint32_t length = 0;
  const PlistFormat output_format = format.value_or(implementation_->format);
  const plist_err_t converted = output_format == PlistFormat::binary
                                    ? plist_to_bin(implementation_->root, &serialized, &length)
                                    : plist_to_xml(implementation_->root, &serialized, &length);
  if (converted != PLIST_ERR_SUCCESS || serialized == nullptr) {
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not serialize property list", path});
  }

  auto written = write_atomic(path, serialized, length);
  plist_mem_free(serialized);
  return written;
}

}  // namespace cyan
