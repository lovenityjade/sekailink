#pragma once

#include "nlohmann/json.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sekailink_server {

struct GenerationSlotRequest {
  int slot_id = 0;
  std::int64_t user_id = 0;
  std::string display_name;
  std::string game_key;
  std::string linkedworld_id;
  std::int64_t config_version_id = 0;
  nlohmann::json config_snapshot = nlohmann::json::object();
};

struct GenerationPackageRequest {
  std::string job_id;
  std::string room_id;
  std::string seed_id;
  std::string rng_seed;
  std::filesystem::path output_root;
  std::vector<GenerationSlotRequest> slots;
};

struct LinkedWorldGenerationCapability {
  bool can_validate_options = false;
  bool can_build_item_pool = false;
  bool can_solve_logic = false;
  bool can_place_items = false;
  bool can_emit_patch = false;
  bool can_emit_room_contract = false;
  std::vector<std::string> external_tools_required;
  std::vector<std::string> unsupported_options;
};

struct LinkedWorldGenerationSurface {
  std::string module_id;
  std::string game_key;
  std::string linkedworld_id;
  std::string version;
  std::filesystem::path source_path;
  LinkedWorldGenerationCapability capabilities;
  nlohmann::json logic = nlohmann::json::object();
  nlohmann::json catalog = nlohmann::json::object();
  nlohmann::json generation_rules = nlohmann::json::object();
  nlohmann::json patch = nlohmann::json::object();
  nlohmann::json runtime = nlohmann::json::object();
};

struct GenerationPackageResult {
  bool ok = false;
  std::string error;
  std::filesystem::path package_dir;
  std::string package_hash;
  nlohmann::json manifest = nlohmann::json::object();
};

std::optional<LinkedWorldGenerationSurface> load_linkedworld_generation_surface(
    const std::filesystem::path& linkedworld_root,
    std::string* error = nullptr);

std::vector<std::string> missing_required_generation_capabilities(
    const LinkedWorldGenerationCapability& capabilities);

std::vector<std::string> missing_required_generation_surface_requirements(
    const LinkedWorldGenerationSurface& surface);

GenerationPackageResult generate_seed_package_from_linkedworlds(
    const GenerationPackageRequest& request,
    const std::vector<LinkedWorldGenerationSurface>& linkedworlds);

nlohmann::json to_json(const GenerationSlotRequest& slot);
nlohmann::json to_json(const LinkedWorldGenerationCapability& capabilities);
nlohmann::json to_json(const LinkedWorldGenerationSurface& surface);

}  // namespace sekailink_server
