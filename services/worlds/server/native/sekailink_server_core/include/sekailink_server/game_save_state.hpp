#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace sekailink_server {

struct DeliveredItemRecord {
  int delivery_id = 0;
  int receiver_slot = 0;
  std::int64_t item_id = 0;
  std::string item_name;
  std::int64_t source_location_id = 0;
  int sender_slot = 0;
  bool acknowledged = false;
};

struct GameSaveState {
  int save_version = 1;
  std::string world_id;
  std::string seed_id;
  std::map<int, std::vector<std::int64_t>> checked_locations_by_slot;
  std::vector<DeliveredItemRecord> delivered_items;
  int next_delivery_id = 1;
};

bool validate_game_save_state(const GameSaveState& state, std::string* error = nullptr);
std::optional<GameSaveState> load_game_save_state(const std::filesystem::path& path,
                                                  std::string* error = nullptr);
bool save_game_save_state(const GameSaveState& state,
                          const std::filesystem::path& path,
                          std::string* error = nullptr);

nlohmann::json to_json(const DeliveredItemRecord& record);
nlohmann::json to_json(const GameSaveState& state);

}  // namespace sekailink_server
