#include "sklmi_bridge_registry.hpp"

#include <array>
#include <cstdlib>
#include <string_view>

namespace sekaiemu::spike {

namespace {

std::optional<std::filesystem::path> FindExecutableInPath(std::string_view executable_name) {
  const char* path_env = std::getenv("PATH");
  if (!path_env) {
    return std::nullopt;
  }
  std::string_view path_list(path_env);
  std::size_t start = 0;
  while (start <= path_list.size()) {
    const auto end = path_list.find(':', start);
    const auto part = path_list.substr(start, end == std::string_view::npos ? path_list.size() - start : end - start);
    if (!part.empty()) {
      const auto candidate = std::filesystem::path(std::string(part)) / std::string(executable_name);
      std::error_code ec;
      if (std::filesystem::exists(candidate, ec) && !ec) {
        return candidate;
      }
    }
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> ExistingDirectory(const std::filesystem::path& path) {
  if (path.empty()) {
    return std::nullopt;
  }
  std::error_code ec;
  if (std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec) && !ec) {
    return path;
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> ExistingFile(const std::filesystem::path& path) {
  if (path.empty()) {
    return std::nullopt;
  }
  std::error_code ec;
  if (std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec) && !ec) {
    return path;
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> HomePath() {
  if (const char* home_env = std::getenv("HOME")) {
    if (*home_env) {
      return std::filesystem::path(home_env);
    }
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> FindFirstExistingFile(
    std::initializer_list<std::filesystem::path> candidates) {
  for (const auto& candidate : candidates) {
    if (const auto resolved = ExistingFile(candidate)) {
      return resolved;
    }
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> FindFirstExistingDirectory(
    std::initializer_list<std::filesystem::path> candidates) {
  for (const auto& candidate : candidates) {
    if (const auto resolved = ExistingDirectory(candidate)) {
      return resolved;
    }
  }
  return std::nullopt;
}

}  // namespace

std::optional<SklmiBridgeSpec> ResolveSklmiBridgeSpec(std::string_view game_name) {
  static const std::array<SklmiBridgeSpec, 3> kKnownSpecs{{
      SklmiBridgeSpec{
          .game_name = "EarthBound",
          .bridge_id = "earthbound-phase1",
          .manifest_filename = "earthbound.phase1.json",
      },
      SklmiBridgeSpec{
          .game_name = "A Link to the Past",
          .bridge_id = "alttp-phase1",
          .manifest_filename = "alttp.phase1.json",
      },
      SklmiBridgeSpec{
          .game_name = "The Legend of Zelda",
          .bridge_id = "tloz-phase1",
          .manifest_filename = "tloz.phase1.json",
      },
  }};

  for (const auto& spec : kKnownSpecs) {
    if (spec.game_name == game_name) {
      return spec;
    }
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> ResolveSklmiRuntimeBinary(
    const std::filesystem::path& explicit_path) {
  if (const auto resolved = ExistingFile(explicit_path)) {
    return resolved;
  }
  if (const char* env_path = std::getenv("SEKAILINK_SKLMI_RUNTIME")) {
    if (const auto resolved = ExistingFile(env_path)) {
      return resolved;
    }
  }
  if (const auto home = HomePath()) {
    if (const auto resolved = FindFirstExistingFile({
            *home / "SekaiLink/canonical/services/sklmi/build/sekailink_sklmi_runtime",
            *home / "SekaiLink/canonical/runtime/bin/sekailink_sklmi_runtime",
            std::filesystem::path("/tmp/sekailink-sklmi-build-beta3/sekailink_sklmi_runtime"),
            std::filesystem::path("/tmp/sekailink-sklmi-build-clean/sekailink_sklmi_runtime"),
            std::filesystem::path("/tmp/sekailink-sklmi-build/sekailink_sklmi_runtime"),
        })) {
      return resolved;
    }
  } else {
    if (const auto resolved = FindFirstExistingFile({
            std::filesystem::path("/tmp/sekailink-sklmi-build-beta3/sekailink_sklmi_runtime"),
            std::filesystem::path("/tmp/sekailink-sklmi-build-clean/sekailink_sklmi_runtime"),
            std::filesystem::path("/tmp/sekailink-sklmi-build/sekailink_sklmi_runtime"),
        })) {
      return resolved;
    }
  }
  return FindExecutableInPath("sekailink_sklmi_runtime");
}

std::optional<std::filesystem::path> ResolveSklmiManifestDirectory(
    const std::filesystem::path& explicit_path,
    const std::optional<std::filesystem::path>& runtime_binary_path) {
  if (const auto resolved = ExistingDirectory(explicit_path)) {
    return resolved;
  }
  if (const char* env_dir = std::getenv("SEKAILINK_SKLMI_MANIFEST_DIR")) {
    if (const auto resolved = ExistingDirectory(env_dir)) {
      return resolved;
    }
  }
  if (const char* env_root = std::getenv("SEKAILINK_SKLMI_ROOT")) {
    if (const auto resolved = ExistingDirectory(std::filesystem::path(env_root) / "manifests")) {
      return resolved;
    }
  }
  if (const auto home = HomePath()) {
    if (const auto resolved = FindFirstExistingDirectory({
            *home / "SekaiLink/canonical/services/sklmi/manifests",
            *home / "SekaiLink/canonical/runtime/sklmi/manifests",
        })) {
      return resolved;
    }
  }
  if (runtime_binary_path.has_value()) {
    const auto direct = runtime_binary_path->parent_path() / "manifests";
    if (const auto resolved = ExistingDirectory(direct)) {
      return resolved;
    }
    const auto sibling = runtime_binary_path->parent_path().parent_path() / "manifests";
    if (const auto resolved = ExistingDirectory(sibling)) {
      return resolved;
    }
  }
  return std::nullopt;
}

}  // namespace sekaiemu::spike
