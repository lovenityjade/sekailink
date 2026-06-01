#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace sekaiemu::spike {

struct SklmiBridgeSpec {
  std::string game_name;
  std::string bridge_id;
  std::string manifest_filename;
};

std::optional<SklmiBridgeSpec> ResolveSklmiBridgeSpec(std::string_view game_name);
std::optional<std::filesystem::path> ResolveSklmiRuntimeBinary(
    const std::filesystem::path& explicit_path);
std::optional<std::filesystem::path> ResolveSklmiManifestDirectory(
    const std::filesystem::path& explicit_path,
    const std::optional<std::filesystem::path>& runtime_binary_path);

}  // namespace sekaiemu::spike
