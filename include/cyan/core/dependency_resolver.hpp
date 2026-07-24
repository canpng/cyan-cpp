#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cyan {

struct CommonDependency {
  std::string key;
  std::string framework_name;
  std::string canonical_path;
};

class DependencyResolver {
 public:
  [[nodiscard]] std::optional<CommonDependency> resolve_common(std::string_view dependency) const;

  [[nodiscard]] std::string canonical_user_dependency(std::string_view basename) const;

  [[nodiscard]] std::vector<CommonDependency> required_dependencies(
      const std::vector<std::string>& dependencies) const;
};

}  // namespace cyan
