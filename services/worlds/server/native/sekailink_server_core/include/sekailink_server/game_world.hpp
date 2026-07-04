#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace sekailink_server {

struct GameWorldSlot {
  int slot_id = 0;
  std::string name;
  std::string game;
  bool always_goal = false;
};

struct GameWorldLocation {
  std::int64_t location_id = 0;
  int owner_slot = 0;
  int receiver_slot = 0;
  std::int64_t item_id = 0;
  std::string location_name;
  std::string item_name;
  int flags = 0;
};

struct GameWorldPackage {
  std::string world_id;
  std::string world_version;
  std::string seed_id;
  std::string seed_hash;
  std::string linkedworld_id;
  nlohmann::json server_rules = nlohmann::json::object();
  std::map<int, GameWorldSlot> slots;
  std::map<std::int64_t, GameWorldLocation> locations;
};

struct ArchipelagoWorldImportOptions {
  std::string world_id;
  std::string world_version = "1.0";
  std::string seed_id;
  std::string seed_hash;
  std::string linkedworld_id;
};

bool validate_game_world_package(const GameWorldPackage& package, std::string* error = nullptr);
std::optional<GameWorldPackage> load_game_world_package(const std::filesystem::path& path,
                                                        std::string* error = nullptr);
bool save_game_world_package(const GameWorldPackage& package,
                             const std::filesystem::path& path,
                             std::string* error = nullptr);
GameWorldPackage import_archipelago_world_package(const nlohmann::json& multiserver_state,
                                                  const ArchipelagoWorldImportOptions& options);

nlohmann::json to_json(const GameWorldSlot& slot);
nlohmann::json to_json(const GameWorldLocation& location);
nlohmann::json to_json(const GameWorldPackage& package);

}  // namespace sekailink_server
