#include "sekailink_server/game_save_state.hpp"

#include <fstream>

namespace sekailink_server {

namespace {

std::optional<GameSaveState> parse_game_save_state_json(const nlohmann::json& root, std::string* error) {
  if (!root.is_object()) {
    if (error) *error = "game_save_root_not_object";
    return std::nullopt;
  }

  GameSaveState state;
  state.save_version = root.value("save_version", 0);
  state.world_id = root.value("world_id", "");
  state.seed_id = root.value("seed_id", "");
  state.next_delivery_id = root.value("next_delivery_id", 1);

  if (root.contains("checked_locations_by_slot") && root.at("checked_locations_by_slot").is_object()) {
    for (auto it = root.at("checked_locations_by_slot").begin(); it != root.at("checked_locations_by_slot").end(); ++it) {
      state.checked_locations_by_slot[std::stoi(it.key())] = it.value().get<std::vector<std::int64_t>>();
    }
  }

  if (root.contains("delivered_items") && root.at("delivered_items").is_array()) {
    for (const auto& value : root.at("delivered_items")) {
      state.delivered_items.push_back(DeliveredItemRecord{
          .delivery_id = value.value("delivery_id", 0),
          .receiver_slot = value.value("receiver_slot", 0),
          .item_id = value.value("item_id", static_cast<std::int64_t>(0)),
          .item_name = value.value("item_name", ""),
          .source_location_id = value.value("source_location_id", static_cast<std::int64_t>(0)),
          .sender_slot = value.value("sender_slot", 0),
          .acknowledged = value.value("acknowledged", false),
      });
    }
  }

  if (!validate_game_save_state(state, error)) {
    return std::nullopt;
  }
  return state;
}

}  // namespace

bool validate_game_save_state(const GameSaveState& state, std::string* error) {
  if (state.save_version != 1) {
    if (error) *error = "game_save_unsupported_version";
    return false;
  }
  if (state.world_id.empty()) {
    if (error) *error = "game_save_missing_world_id";
    return false;
  }
  if (state.seed_id.empty()) {
    if (error) *error = "game_save_missing_seed_id";
    return false;
  }
  if (state.next_delivery_id <= 0) {
    if (error) *error = "game_save_invalid_next_delivery_id";
    return false;
  }
  for (const auto& [slot_id, locations] : state.checked_locations_by_slot) {
    if (slot_id <= 0) {
      if (error) *error = "game_save_invalid_checked_slot";
      return false;
    }
    for (const auto& location_id : locations) {
      if (location_id <= 0) {
        if (error) *error = "game_save_invalid_checked_location";
        return false;
      }
    }
  }
  for (const auto& delivery : state.delivered_items) {
    if (delivery.delivery_id <= 0 || delivery.receiver_slot <= 0 || delivery.item_id <= 0 ||
        delivery.source_location_id <= 0 || delivery.sender_slot <= 0 || delivery.item_name.empty()) {
      if (error) *error = "game_save_invalid_delivery";
      return false;
    }
  }
  return true;
}

std::optional<GameSaveState> load_game_save_state(const std::filesystem::path& path,
                                                  std::string* error) {
  std::ifstream input(path);
  if (!input) {
    if (error) *error = "game_save_open_failed";
    return std::nullopt;
  }
  nlohmann::json root;
  try {
    input >> root;
  } catch (const std::exception&) {
    if (error) *error = "game_save_parse_failed";
    return std::nullopt;
  }
  return parse_game_save_state_json(root, error);
}

bool save_game_save_state(const GameSaveState& state,
                          const std::filesystem::path& path,
                          std::string* error) {
  if (!validate_game_save_state(state, error)) {
    return false;
  }
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::trunc);
  if (!output) {
    if (error) *error = "game_save_write_failed";
    return false;
  }
  output << to_json(state).dump(2) << "\n";
  return true;
}

nlohmann::json to_json(const DeliveredItemRecord& record) {
  return {
      {"delivery_id", record.delivery_id},
      {"receiver_slot", record.receiver_slot},
      {"item_id", record.item_id},
      {"item_name", record.item_name},
      {"source_location_id", record.source_location_id},
      {"sender_slot", record.sender_slot},
      {"acknowledged", record.acknowledged},
  };
}

nlohmann::json to_json(const GameSaveState& state) {
  nlohmann::json checked = nlohmann::json::object();
  for (const auto& [slot_id, locations] : state.checked_locations_by_slot) {
    checked[std::to_string(slot_id)] = locations;
  }

  nlohmann::json delivered = nlohmann::json::array();
  for (const auto& record : state.delivered_items) {
    delivered.push_back(to_json(record));
  }

  return {
      {"save_version", state.save_version},
      {"world_id", state.world_id},
      {"seed_id", state.seed_id},
      {"checked_locations_by_slot", checked},
      {"delivered_items", delivered},
      {"next_delivery_id", state.next_delivery_id},
  };
}

}  // namespace sekailink_server
