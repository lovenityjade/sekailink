#pragma once

#include <filesystem>
#include <string>

namespace sekaiemu::spike {

struct LaunchRequest;

struct PatchMaterializationResult {
  bool ok = false;
  std::filesystem::path game_path;
  std::string technical_error;
};

PatchMaterializationResult MaterializePatchedGame(const LaunchRequest& request);

}  // namespace sekaiemu::spike
