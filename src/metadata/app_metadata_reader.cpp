#include "cyan/metadata/app_metadata_reader.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "cyan/platform/utf.hpp"
#include "cyan/plist/plist_document.hpp"

namespace cyan {
namespace {

constexpr std::uint64_t kMaximumPlistSize = 8ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kMaximumIconSize = 32ULL * 1024ULL * 1024ULL;

struct ArchiveReadDeleter {
  void operator()(archive* handle) const noexcept {
    if (handle != nullptr) {
      archive_read_free(handle);
    }
  }
};

using ReadArchive = std::unique_ptr<archive, ArchiveReadDeleter>;

struct EntryDescriptor {
  std::string name;
  std::uint64_t size{0};
};

struct MetadataCandidate {
  AppMetadata metadata;
  std::string plist_path;
  std::string root;
  std::string executable;
  std::string package_type;
  std::vector<std::string> icon_hints;
  bool exact_payload_app{false};
  int depth{0};
};

std::string lower_ascii(std::string value) {
  std::ranges::transform(value, value.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

std::string normalized_name(std::string value) {
  std::ranges::replace(value, '\\', '/');
  while (value.starts_with("./")) {
    value.erase(0, 2);
  }
  return value;
}

std::vector<std::string_view> components(const std::string& path) {
  std::vector<std::string_view> result;
  std::size_t start = 0;
  while (start < path.size()) {
    const std::size_t separator = path.find('/', start);
    const std::size_t end = separator == std::string::npos ? path.size() : separator;
    if (end > start) {
      result.emplace_back(path.data() + start, end - start);
    }
    if (separator == std::string::npos) {
      break;
    }
    start = separator + 1;
  }
  return result;
}

bool ends_with_case_insensitive(std::string_view value, std::string_view suffix) {
  if (value.size() < suffix.size()) {
    return false;
  }
  const auto offset = value.size() - suffix.size();
  for (std::size_t index = 0; index < suffix.size(); ++index) {
    if (std::tolower(static_cast<unsigned char>(value[offset + index])) !=
        std::tolower(static_cast<unsigned char>(suffix[index]))) {
      return false;
    }
  }
  return true;
}

std::string clean_value(std::optional<std::string> value) {
  if (!value || value->empty() || value->find("$(") != std::string::npos ||
      value->find("${") != std::string::npos) {
    return {};
  }
  return std::move(*value);
}

std::string file_stem(std::string_view name) {
  const std::size_t slash = name.find_last_of('/');
  std::string value(name.substr(slash == std::string_view::npos ? 0 : slash + 1));
  const std::size_t dot = value.find_last_of('.');
  if (dot != std::string::npos) {
    value.resize(dot);
  }
  return value;
}

std::string parent_path(std::string_view name) {
  const std::size_t slash = name.find_last_of('/');
  return slash == std::string_view::npos ? std::string{} : std::string(name.substr(0, slash));
}

void append_hint(std::vector<std::string>& hints, std::string value) {
  if (value.empty()) {
    return;
  }
  value = lower_ascii(normalized_name(std::move(value)));
  if (std::ranges::find(hints, value) == hints.end()) {
    hints.push_back(std::move(value));
  }
}

void append_hints(std::vector<std::string>& hints, const std::vector<std::string>& values) {
  for (const auto& value : values) {
    append_hint(hints, value);
  }
}

Result<std::vector<std::uint8_t>> read_archive_data(archive* reader, std::uint64_t maximum,
                                                    const std::filesystem::path& source) {
  std::vector<std::uint8_t> data;
  std::array<std::uint8_t, 64U * 1024U> buffer{};
  while (true) {
    const la_ssize_t received = archive_read_data(reader, buffer.data(), buffer.size());
    if (received == 0) {
      break;
    }
    if (received < 0) {
      return Result<std::vector<std::uint8_t>>::failure(
          {ErrorCode::archive_invalid, "could not read application metadata entry", source});
    }
    const auto chunk = static_cast<std::size_t>(received);
    if (data.size() > maximum || chunk > maximum - data.size()) {
      return Result<std::vector<std::uint8_t>>::failure(
          {ErrorCode::archive_limit_exceeded, "application metadata entry is too large", source});
    }
    data.insert(data.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(chunk));
  }
  return Result<std::vector<std::uint8_t>>::success(std::move(data));
}

std::optional<MetadataCandidate> parse_candidate(const std::string& path,
                                                 std::span<const std::uint8_t> bytes) {
  const auto parts = components(path);
  if (parts.size() < 3 || lower_ascii(std::string(parts.front())) != "payload" ||
      lower_ascii(std::string(parts.back())) != "info.plist") {
    return std::nullopt;
  }

  std::optional<std::size_t> app_index;
  for (std::size_t index = 1; index + 1 < parts.size(); ++index) {
    if (ends_with_case_insensitive(parts[index], ".app")) {
      app_index = index;
    }
  }
  if (!app_index) {
    return std::nullopt;
  }

  auto plist = PlistDocument::load_memory(std::as_bytes(bytes));
  if (!plist) {
    return std::nullopt;
  }

  MetadataCandidate candidate;
  candidate.plist_path = path;
  candidate.depth = static_cast<int>(parts.size());
  for (std::size_t index = 0; index <= *app_index; ++index) {
    if (!candidate.root.empty()) {
      candidate.root += '/';
    }
    candidate.root.append(parts[index]);
  }
  candidate.exact_payload_app = *app_index == 1 && parts.size() == 3;
  candidate.metadata.bundle_path = candidate.root;
  candidate.metadata.bundle_identifier = clean_value(plist.value().string("CFBundleIdentifier"));
  candidate.metadata.version = clean_value(plist.value().string("CFBundleShortVersionString"));
  if (candidate.metadata.version.empty()) {
    candidate.metadata.version = clean_value(plist.value().string("CFBundleVersion"));
  }
  candidate.metadata.minimum_os = clean_value(plist.value().string("MinimumOSVersion"));
  candidate.executable = clean_value(plist.value().string("CFBundleExecutable"));
  candidate.package_type = clean_value(plist.value().string("CFBundlePackageType"));
  candidate.metadata.app_name = clean_value(plist.value().string("CFBundleDisplayName"));
  if (candidate.metadata.app_name.empty()) {
    candidate.metadata.app_name = clean_value(plist.value().string("CFBundleName"));
  }
  if (candidate.metadata.app_name.empty()) {
    candidate.metadata.app_name = candidate.executable;
  }
  if (candidate.metadata.app_name.empty()) {
    candidate.metadata.app_name = file_stem(candidate.root);
  }

  append_hint(candidate.icon_hints, clean_value(plist.value().string("CFBundleIconFile")));
  append_hint(candidate.icon_hints, clean_value(plist.value().string("CFBundleIconName")));
  append_hints(candidate.icon_hints, plist.value().string_array("CFBundleIconFiles"));
  append_hint(candidate.icon_hints,
              clean_value(plist.value().string_path(
                  {"CFBundleIcons", "CFBundlePrimaryIcon", "CFBundleIconName"})));
  append_hints(candidate.icon_hints,
               plist.value().string_array_path(
                   {"CFBundleIcons", "CFBundlePrimaryIcon", "CFBundleIconFiles"}));
  append_hint(candidate.icon_hints,
              clean_value(plist.value().string_path(
                  {"CFBundleIcons~ipad", "CFBundlePrimaryIcon", "CFBundleIconName"})));
  append_hints(candidate.icon_hints,
               plist.value().string_array_path(
                   {"CFBundleIcons~ipad", "CFBundlePrimaryIcon", "CFBundleIconFiles"}));
  return candidate;
}

int candidate_score(const MetadataCandidate& candidate,
                    const std::unordered_set<std::string>& entries) {
  int score = candidate.exact_payload_app ? 10'000 : 1'000;
  score -= candidate.depth * 25;
  const std::string lower_path = lower_ascii(candidate.plist_path);
  for (const std::string_view discouraged :
       {"/plugins/", "/watch/", "/frameworks/", ".bundle/", ".storyboardc/", ".appex/"}) {
    if (lower_path.find(discouraged) != std::string::npos) {
      score -= 2'000;
    }
  }
  if (lower_ascii(candidate.package_type) == "appl") {
    score += 1'500;
  }
  if (!candidate.metadata.bundle_identifier.empty()) {
    score += 500;
  }
  if (!candidate.metadata.version.empty()) {
    score += 150;
  }
  if (!candidate.metadata.app_name.empty()) {
    score += 150;
  }
  if (!candidate.executable.empty()) {
    score += 400;
    if (entries.contains(lower_ascii(candidate.root + '/' + candidate.executable))) {
      score += 2'000;
    }
  }
  return score;
}

int icon_score(const EntryDescriptor& entry, const MetadataCandidate& candidate) {
  const std::string lower_name = lower_ascii(entry.name);
  const std::string lower_file = lower_ascii(
      std::string(std::string_view(entry.name).substr(entry.name.find_last_of('/') + 1)));
  if (parent_path(lower_name) != lower_ascii(candidate.root)) {
    return (std::numeric_limits<int>::min)();
  }
  if (!ends_with_case_insensitive(lower_file, ".png") &&
      !ends_with_case_insensitive(lower_file, ".jpg") &&
      !ends_with_case_insensitive(lower_file, ".jpeg")) {
    return (std::numeric_limits<int>::min)();
  }

  int score = 0;
  const std::string stem = file_stem(lower_file);
  for (const auto& hint : candidate.icon_hints) {
    const std::string hint_file = hint.substr(hint.find_last_of('/') + 1);
    const std::string hint_stem = file_stem(hint_file);
    if (lower_file == hint_file || lower_file == hint_file + ".png") {
      score += 4'000;
    } else if (!hint_stem.empty() && stem.starts_with(hint_stem)) {
      score += 2'500;
    }
  }
  if (lower_file.find("appicon") != std::string::npos) {
    score += 2'000;
  } else if (lower_file.find("icon") != std::string::npos) {
    score += 900;
  }
  for (const std::string_view discouraged :
       {"launch", "splash", "background", "notification", "settings", "spotlight", "marketing"}) {
    if (lower_file.find(discouraged) != std::string::npos) {
      score -= 1'000;
    }
  }
  if (lower_file.find("@3x") != std::string::npos) {
    score += 90;
  } else if (lower_file.find("@2x") != std::string::npos) {
    score += 60;
  }
  score += static_cast<int>((std::min)(entry.size / 1024U, std::uint64_t{200}));
  return score > 0 ? score : (std::numeric_limits<int>::min)();
}

std::optional<EntryDescriptor> select_icon(const std::vector<EntryDescriptor>& entries,
                                           const MetadataCandidate& candidate) {
  std::optional<EntryDescriptor> selected;
  int selected_score = (std::numeric_limits<int>::min)();
  for (const auto& entry : entries) {
    const int score = icon_score(entry, candidate);
    if (score > selected_score ||
        (score == selected_score && selected && entry.size > selected->size)) {
      selected = entry;
      selected_score = score;
    }
  }
  return selected;
}

ReadArchive open_archive(const std::filesystem::path& path) {
  ReadArchive reader(archive_read_new());
  if (!reader) {
    return {};
  }
  archive_read_support_filter_all(reader.get());
  archive_read_support_format_zip(reader.get());
  if (archive_read_open_filename_w(reader.get(), path.c_str(), 64U * 1024U) != ARCHIVE_OK) {
    return {};
  }
  return reader;
}

Result<AppMetadata> read_archive(const std::filesystem::path& input) {
  auto reader = open_archive(input);
  if (!reader) {
    return Result<AppMetadata>::failure(
        {ErrorCode::archive_open_failed, "could not open IPA metadata", input});
  }

  std::vector<MetadataCandidate> candidates;
  std::vector<EntryDescriptor> entries;
  std::unordered_set<std::string> entry_names;
  archive_entry* entry = nullptr;
  int status = ARCHIVE_OK;
  while ((status = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
    const char* raw_name = archive_entry_pathname_utf8(entry);
    if (raw_name == nullptr) {
      continue;
    }
    const std::string name = normalized_name(raw_name);
    const la_int64_t signed_size =
        archive_entry_size_is_set(entry) != 0 ? archive_entry_size(entry) : 0;
    const std::uint64_t size = signed_size > 0 ? static_cast<std::uint64_t>(signed_size) : 0;
    if (archive_entry_filetype(entry) == AE_IFREG) {
      entries.push_back({name, size});
      entry_names.insert(lower_ascii(name));
    }

    if (archive_entry_filetype(entry) == AE_IFREG &&
        ends_with_case_insensitive(name, "/Info.plist") && size <= kMaximumPlistSize) {
      auto data = read_archive_data(reader.get(), kMaximumPlistSize, input);
      if (!data) {
        return Result<AppMetadata>::failure(data.error());
      }
      auto candidate = parse_candidate(name, data.value());
      if (candidate) {
        candidates.push_back(std::move(*candidate));
      }
    }
  }
  if (status != ARCHIVE_EOF) {
    return Result<AppMetadata>::failure(
        {ErrorCode::archive_invalid, "could not scan IPA metadata", input});
  }
  if (candidates.empty()) {
    return Result<AppMetadata>::failure(
        {ErrorCode::invalid_input_type, "IPA contains no application Info.plist", input});
  }

  const auto selected = std::ranges::max_element(candidates, {}, [&](const auto& candidate) {
    return candidate_score(candidate, entry_names);
  });
  MetadataCandidate candidate = *selected;
  const auto icon = select_icon(entries, candidate);
  if (!icon || icon->size == 0 || icon->size > kMaximumIconSize) {
    return Result<AppMetadata>::success(std::move(candidate.metadata));
  }

  reader = open_archive(input);
  if (!reader) {
    return Result<AppMetadata>::failure(
        {ErrorCode::archive_open_failed, "could not reopen IPA icon", input});
  }
  while (archive_read_next_header(reader.get(), &entry) == ARCHIVE_OK) {
    const char* raw_name = archive_entry_pathname_utf8(entry);
    if (raw_name != nullptr && normalized_name(raw_name) == icon->name) {
      auto data = read_archive_data(reader.get(), kMaximumIconSize, input);
      if (data) {
        candidate.metadata.icon_name = icon->name;
        candidate.metadata.icon_data = data.take_value();
      }
      break;
    }
  }
  return Result<AppMetadata>::success(std::move(candidate.metadata));
}

Result<std::vector<std::uint8_t>> read_file(const std::filesystem::path& path,
                                            std::uint64_t maximum) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::file_not_found, "could not open application metadata file", path});
  }
  const std::streamoff end = stream.tellg();
  if (end < 0 || static_cast<std::uint64_t>(end) > maximum) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::archive_limit_exceeded, "application metadata file is too large", path});
  }
  std::vector<std::uint8_t> data(static_cast<std::size_t>(end));
  stream.seekg(0, std::ios::beg);
  if (!data.empty()) {
    stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
  }
  if (!stream) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::filesystem_error, "could not read application metadata file", path});
  }
  return Result<std::vector<std::uint8_t>>::success(std::move(data));
}

Result<AppMetadata> read_app_directory(const std::filesystem::path& input) {
  const auto plist_path = input / L"Info.plist";
  auto plist_data = read_file(plist_path, kMaximumPlistSize);
  if (!plist_data) {
    return Result<AppMetadata>::failure(plist_data.error());
  }
  auto filename = platform::utf8_from_wide(input.filename().native());
  if (!filename) {
    return Result<AppMetadata>::failure(filename.error());
  }
  const std::string root = "Payload/" + filename.value();
  auto candidate = parse_candidate(root + "/Info.plist", plist_data.value());
  if (!candidate) {
    return Result<AppMetadata>::failure(
        {ErrorCode::invalid_input_type, "application Info.plist is invalid", plist_path});
  }

  std::vector<EntryDescriptor> entries;
  std::error_code error;
  for (std::filesystem::directory_iterator iterator(input, error), end; iterator != end && !error;
       iterator.increment(error)) {
    if (!iterator->is_regular_file(error) || error) {
      continue;
    }
    const auto size = iterator->file_size(error);
    if (error) {
      error.clear();
      continue;
    }
    auto entry_filename = platform::utf8_from_wide(iterator->path().filename().native());
    if (entry_filename) {
      entries.push_back({candidate->root + '/' + entry_filename.value(), size});
    }
  }
  const auto icon = select_icon(entries, *candidate);
  if (icon && icon->size > 0 && icon->size <= kMaximumIconSize) {
    const std::string icon_filename = icon->name.substr(icon->name.find_last_of('/') + 1);
    auto wide_icon_filename = platform::wide_from_utf8(icon_filename);
    if (!wide_icon_filename) {
      return Result<AppMetadata>::failure(wide_icon_filename.error());
    }
    auto icon_data = read_file(input / wide_icon_filename.value(), kMaximumIconSize);
    if (icon_data) {
      candidate->metadata.icon_name = icon->name;
      candidate->metadata.icon_data = icon_data.take_value();
    }
  }
  return Result<AppMetadata>::success(std::move(candidate->metadata));
}

}  // namespace

Result<AppMetadata> AppMetadataReader::read(const std::filesystem::path& input) const {
  std::error_code error;
  if (std::filesystem::is_directory(input, error) && !error) {
    return read_app_directory(input);
  }
  if (std::filesystem::is_regular_file(input, error) && !error) {
    return read_archive(input);
  }
  return Result<AppMetadata>::failure(
      {ErrorCode::file_not_found, "application input does not exist", input});
}

}  // namespace cyan
