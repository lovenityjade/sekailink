#include "sekailink_server/game_world.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace sekailink_server {

namespace {

bool contains_forbidden_programming_fields(const nlohmann::json& value) {
  const auto text = value.dump();
  static constexpr const char* kForbiddenFields[] = {
      "\"script\"",
      "\"lua\"",
      "\"code\"",
      "\"callback\"",
      "\"expression\"",
      "\"program\"",
  };
  for (const auto* field : kForbiddenFields) {
    if (text.find(field) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::string item_name_for(const nlohmann::json& value) {
  if (value.contains("item_name") && value.at("item_name").is_string()) {
    return value.at("item_name").get<std::string>();
  }
  if (value.contains("item_id") && value.at("item_id").is_number_integer()) {
    return "item:" + std::to_string(value.at("item_id").get<std::int64_t>());
  }
  return "item:unknown";
}

bool slot_type_always_goal(const nlohmann::json& slot_info) {
  if (!slot_info.contains("type")) {
    return false;
  }
  if (slot_info.at("type").is_number_integer()) {
    return slot_info.at("type").get<int>() != 1;
  }
  if (slot_info.at("type").is_string()) {
    return slot_info.at("type").get<std::string>() != "player";
  }
  return false;
}

std::optional<GameWorldPackage> parse_game_world_package_json(const nlohmann::json& root, std::string* error) {
  if (!root.is_object()) {
    if (error) *error = "world_package_root_not_object";
    return std::nullopt;
  }
  if (contains_forbidden_programming_fields(root)) {
    if (error) *error = "world_package_contains_forbidden_programming_fields";
    return std::nullopt;
  }

  GameWorldPackage package;
  package.world_id = root.value("world_id", "");
  package.world_version = root.value("world_version", "");
  package.seed_id = root.value("seed_id", "");
  package.seed_hash = root.value("seed_hash", "");
  package.linkedworld_id = root.value("linkedworld_id", "");
  package.server_rules = root.value("server_rules", nlohmann::json::object());

  if (root.contains("slots") && root.at("slots").is_array()) {
    for (const auto& slot_value : root.at("slots")) {
      GameWorldSlot slot;
      slot.slot_id = slot_value.value("slot_id", 0);
      slot.name = slot_value.value("name", "");
      slot.game = slot_value.value("game", "");
      slot.always_goal = slot_value.value("always_goal", false);
      if (slot.slot_id > 0) {
        package.slots[slot.slot_id] = std::move(slot);
      }
    }
  }

  if (root.contains("locations") && root.at("locations").is_array()) {
    for (const auto& location_value : root.at("locations")) {
      GameWorldLocation location;
      location.location_id = location_value.value("location_id", static_cast<std::int64_t>(0));
      location.owner_slot = location_value.value("owner_slot", 0);
      location.receiver_slot = location_value.value("receiver_slot", 0);
      location.item_id = location_value.value("item_id", static_cast<std::int64_t>(0));
      location.location_name = location_value.value("location_name", "");
      location.item_name = location_value.value("item_name", "");
      location.flags = location_value.value("flags", 0);
      if (location.location_id > 0) {
        package.locations[location.location_id] = std::move(location);
      }
    }
  }

  if (!validate_game_world_package(package, error)) {
    return std::nullopt;
  }
  return package;
}

}  // namespace

bool validate_game_world_package(const GameWorldPackage& package, std::string* error) {
  if (package.world_id.empty()) {
    if (error) *error = "world_package_missing_world_id";
    return false;
  }
  if (package.world_version.empty()) {
    if (error) *error = "world_package_missing_world_version";
    return false;
  }
  if (package.seed_id.empty()) {
    if (error) *error = "world_package_missing_seed_id";
    return false;
  }
  if (package.linkedworld_id.empty()) {
    if (error) *error = "world_package_missing_linkedworld_id";
    return false;
  }
  if (!package.server_rules.is_object()) {
    if (error) *error = "world_package_invalid_server_rules";
    return false;
  }
  if (package.slots.empty()) {
    if (error) *error = "world_package_missing_slots";
    return false;
  }
  if (package.locations.empty()) {
    if (error) *error = "world_package_missing_locations";
    return false;
  }
  for (const auto& [slot_id, slot] : package.slots) {
    if (slot_id <= 0 || slot.name.empty() || slot.game.empty()) {
      if (error) *error = "world_package_invalid_slot";
      return false;
    }
  }
  for (const auto& [location_id, location] : package.locations) {
    if (location_id <= 0 || location.owner_slot <= 0 || location.receiver_slot <= 0 ||
        location.item_id <= 0 || location.location_name.empty() || location.item_name.empty()) {
      if (error) *error = "world_package_invalid_location";
      return false;
    }
    if (!package.slots.contains(location.owner_slot) || !package.slots.contains(location.receiver_slot)) {
      if (error) *error = "world_package_location_unknown_slot";
      return false;
    }
  }
  return true;
}

std::optional<GameWorldPackage> load_game_world_package(const std::filesystem::path& path,
                                                        std::string* error) {
  std::ifstream input(path);
  if (!input) {
    if (error) *error = "world_package_open_failed";
    return std::nullopt;
  }
  nlohmann::json root;
  try {
    input >> root;
  } catch (const std::exception&) {
    if (error) *error = "world_package_parse_failed";
    return std::nullopt;
  }
  return parse_game_world_package_json(root, error);
}

bool save_game_world_package(const GameWorldPackage& package,
                             const std::filesystem::path& path,
                             std::string* error) {
  if (!validate_game_world_package(package, error)) {
    return false;
  }
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::trunc);
  if (!output) {
    if (error) *error = "world_package_write_failed";
    return false;
  }
  output << to_json(package).dump(2) << "\n";
  return true;
}

GameWorldPackage import_archipelago_world_package(const nlohmann::json& multiserver_state,
                                                  const ArchipelagoWorldImportOptions& options) {
  if (!multiserver_state.contains("slot_info") || !multiserver_state.at("slot_info").is_object()) {
    throw std::runtime_error("missing_slot_info");
  }
  if (!multiserver_state.contains("locations") || !multiserver_state.at("locations").is_object()) {
    throw std::runtime_error("missing_locations");
  }
  if (contains_forbidden_programming_fields(multiserver_state)) {
    throw std::runtime_error("world_package_contains_forbidden_programming_fields");
  }

  GameWorldPackage package;
  package.world_id = options.world_id;
  package.world_version = options.world_version;
  package.seed_id = options.seed_id;
  package.seed_hash = options.seed_hash;
  package.linkedworld_id = options.linkedworld_id;
  package.server_rules = multiserver_state.value("game_options", nlohmann::json::object());

  for (auto it = multiserver_state.at("slot_info").begin(); it != multiserver_state.at("slot_info").end(); ++it) {
    const auto slot_id = std::stoi(it.key());
    const auto& slot_info = it.value();
    package.slots[slot_id] = GameWorldSlot{
        .slot_id = slot_id,
        .name = slot_info.value("name", ""),
        .game = slot_info.value("game", ""),
        .always_goal = slot_type_always_goal(slot_info),
    };
  }

  for (auto owner_it = multiserver_state.at("locations").begin(); owner_it != multiserver_state.at("locations").end(); ++owner_it) {
    const auto owner_slot = std::stoi(owner_it.key());
    for (auto location_it = owner_it.value().begin(); location_it != owner_it.value().end(); ++location_it) {
      const auto location_id = std::stoll(location_it.key());
      const auto& mapping = location_it.value();
      package.locations[location_id] = GameWorldLocation{
          .location_id = location_id,
          .owner_slot = owner_slot,
          .receiver_slot = mapping.value("receiver_slot", 0),
          .item_id = mapping.value("item_id", static_cast<std::int64_t>(0)),
          .location_name = mapping.value("location_name", ""),
          .item_name = item_name_for(mapping),
          .flags = mapping.value("flags", 0),
      };
    }
  }

  std::string error;
  if (!validate_game_world_package(package, &error)) {
    throw std::runtime_error(error);
  }
  return package;
}

nlohmann::json to_json(const GameWorldSlot& slot) {
  return {
      {"slot_id", slot.slot_id},
      {"name", slot.name},
      {"game", slot.game},
      {"always_goal", slot.always_goal},
  };
}

nlohmann::json to_json(const GameWorldLocation& location) {
  return {
      {"location_id", location.location_id},
      {"owner_slot", location.owner_slot},
      {"receiver_slot", location.receiver_slot},
      {"item_id", location.item_id},
      {"location_name", location.location_name},
      {"item_name", location.item_name},
      {"flags", location.flags},
  };
}

nlohmann::json to_json(const GameWorldPackage& package) {
  nlohmann::json root = {
      {"world_id", package.world_id},
      {"world_version", package.world_version},
      {"seed_id", package.seed_id},
      {"seed_hash", package.seed_hash},
      {"linkedworld_id", package.linkedworld_id},
      {"server_rules", package.server_rules},
      {"slots", nlohmann::json::array()},
      {"locations", nlohmann::json::array()},
  };
  for (const auto& [_, slot] : package.slots) {
    root["slots"].push_back(to_json(slot));
  }
  for (const auto& [_, location] : package.locations) {
    root["locations"].push_back(to_json(location));
  }
  return root;
}

}  // namespace sekailink_server
