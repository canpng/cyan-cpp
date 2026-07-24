#include "cyan/core/dependency_resolver.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_set>

namespace cyan {
namespace {

const std::array<CommonDependency, 5> common_dependencies{{
    {"substrate.", "CydiaSubstrate.framework", "@rpath/CydiaSubstrate.framework/CydiaSubstrate"},
    {"orion.", "Orion.framework", "@rpath/Orion.framework/Orion"},
    {"cepheiui.", "CepheiUI.framework", "@rpath/CepheiUI.framework/CepheiUI"},
    {"cepheiprefs.", "CepheiPrefs.framework", "@rpath/CepheiPrefs.framework/CepheiPrefs"},
    {"cephei.", "Cephei.framework", "@rpath/Cephei.framework/Cephei"},
}};

std::string ascii_lower(std::string_view value) {
  std::string lowered(value);
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return lowered;
}

bool ends_with(std::string_view value, std::string_view suffix) {
  return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

}  // namespace

std::optional<CommonDependency> DependencyResolver::resolve_common(
    std::string_view dependency) const {
  const std::string lowered = ascii_lower(dependency);
  for (const auto& candidate : common_dependencies) {
    if (lowered.find(candidate.key) != std::string::npos) {
      return candidate;
    }
  }
  return std::nullopt;
}

std::string DependencyResolver::canonical_user_dependency(std::string_view basename) const {
  if (ends_with(basename, ".framework")) {
    const std::string_view stem = basename.substr(0, basename.size() - 10U);
    return "@rpath/" + std::string(basename) + "/" + std::string(stem);
  }
  return "@rpath/" + std::string(basename);
}

std::vector<CommonDependency> DependencyResolver::required_dependencies(
    const std::vector<std::string>& dependencies) const {
  std::vector<CommonDependency> result;
  std::unordered_set<std::string> seen;
  bool needs_substrate = false;

  for (const auto& dependency : dependencies) {
    auto match = resolve_common(dependency);
    if (!match) {
      continue;
    }
    if (match->key == "orion.") {
      needs_substrate = true;
    }
    if (seen.insert(match->key).second) {
      result.push_back(std::move(*match));
    }
  }

  if (needs_substrate && seen.insert("substrate.").second) {
    result.push_back(common_dependencies.front());
  }
  return result;
}

}  // namespace cyan
