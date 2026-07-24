#include <catch2/catch_test_macros.hpp>

#include "cyan/core/dependency_resolver.hpp"

TEST_CASE("common tweak dependencies map to canonical framework paths") {
  cyan::DependencyResolver resolver;
  auto substrate = resolver.resolve_common("/usr/lib/libsubstrate.dylib");
  REQUIRE(substrate);
  CHECK(substrate->canonical_path == "@rpath/CydiaSubstrate.framework/CydiaSubstrate");

  auto cephei = resolver.resolve_common("@loader_path/Cephei.framework/Cephei");
  REQUIRE(cephei);
  CHECK(cephei->framework_name == "Cephei.framework");
}

TEST_CASE("specific Cephei names win over the general Cephei match") {
  cyan::DependencyResolver resolver;
  auto prefs = resolver.resolve_common("/Library/Frameworks/CepheiPrefs.framework/CepheiPrefs");
  REQUIRE(prefs);
  CHECK(prefs->framework_name == "CepheiPrefs.framework");
}

TEST_CASE("Orion implies Substrate") {
  cyan::DependencyResolver resolver;
  const auto required = resolver.required_dependencies({"@rpath/Orion.framework/Orion"});
  REQUIRE(required.size() == 2U);
  CHECK(required[0].framework_name == "Orion.framework");
  CHECK(required[1].framework_name == "CydiaSubstrate.framework");
}

TEST_CASE("user frameworks receive framework executable paths") {
  cyan::DependencyResolver resolver;
  CHECK(resolver.canonical_user_dependency("Example.framework") ==
        "@rpath/Example.framework/Example");
  CHECK(resolver.canonical_user_dependency("Example.dylib") == "@rpath/Example.dylib");
}
