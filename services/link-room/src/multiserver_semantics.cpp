#include "sekailink_server/multiserver_semantics.hpp"

#include <stdexcept>

namespace sekailink_server {

namespace {

std::string key_for(int team_id, int slot_id) {
  return std::to_string(team_id) + ":" + std::to_string(slot_id);
}

std::string lookup_alias(const nlohmann::json& aliases, int team_id, int slot_id, const std::string& fallback) {
  const auto key = key_for(team_id, slot_id);
  if (aliases.contains(key) && aliases.at(key).is_string()) {
    return aliases.at(key).get<std::string>();
  }
  return fallback;
}

bool lookup_connected(const nlohmann::json& connections, int slot_id) {
  if (!connections.contains(std::to_string(slot_id))) {
    return false;
  }
  return connections.at(std::to_string(slot_id)).get<int>() > 0;
}

}  // namespace

RoomSession import_multiserver_semantics(
    const nlohmann::json& multiserver_state,
    const MultiServerImportOptions& options) {
  if (!multiserver_state.contains("slot_info") || !multiserver_state.contains("player_names")) {
    throw std::runtime_error("invalid_multiserver_state");
  }

  const auto slot_key = std::to_string(options.slot_id);
  if (!multiserver_state.at("slot_info").contains(slot_key)) {
    throw std::runtime_error("missing_slot_info");
  }

  const auto& slot_info = multiserver_state.at("slot_info").at(slot_key);
  RoomSession session(RoomSessionConfig{
      .room_id = options.room_id,
      .room_type = options.room_type,
      .game = slot_info.at("game").get<std::string>(),
      .team_id = options.team_id,
      .slot_id = options.slot_id,
      .slot_name = slot_info.at("name").get<std::string>(),
      .slot_alias = lookup_alias(
          multiserver_state.value("name_aliases", nlohmann::json::object()),
          options.team_id,
          options.slot_id,
          slot_info.at("name").get<std::string>()),
      .seed_id = options.seed_id,
      .seed_hash = options.seed_hash,
      .patch_url = options.patch_url,
      .tracker_pack = options.tracker_pack,
      .tracker_variant = options.tracker_variant,
      .sni_required = options.sni_required,
      .memory_backend_state = options.memory_backend_state,
      .expires_at = options.expires_at,
  });

  const auto aliases = multiserver_state.value("name_aliases", nlohmann::json::object());
  const auto player_names = multiserver_state.at("player_names");
  const auto connections = multiserver_state.value("client_connections", nlohmann::json::object());
  for (auto it = player_names.begin(); it != player_names.end(); ++it) {
    const auto key = it.key();
    const auto separator = key.find(':');
    if (separator == std::string::npos) {
      continue;
    }
    const auto team_id = std::stoi(key.substr(0, separator));
    const auto slot_id = std::stoi(key.substr(separator + 1));
    const auto slot_lookup = std::to_string(slot_id);
    if (!multiserver_state.at("slot_info").contains(slot_lookup)) {
      continue;
    }
    const auto& player_slot_info = multiserver_state.at("slot_info").at(slot_lookup);
    session.upsert_player(PlayerRoomView{
        .team = team_id,
        .slot = slot_id,
        .name = it.value().get<std::string>(),
        .alias = lookup_alias(aliases, team_id, slot_id, it.value().get<std::string>()),
        .game = player_slot_info.at("game").get<std::string>(),
        .connected = lookup_connected(connections, slot_id),
    });
  }

  const auto checked_key = key_for(options.team_id, options.slot_id);
  if (multiserver_state.contains("checked_locations") &&
      multiserver_state.at("checked_locations").contains(checked_key)) {
    for (const auto& location_id : multiserver_state.at("checked_locations").at(checked_key)) {
      session.record_check(location_id.get<std::int64_t>());
    }
  }

  if (multiserver_state.contains("missing_locations") &&
      multiserver_state.at("missing_locations").contains(checked_key)) {
    session.set_missing_locations(
        multiserver_state.at("missing_locations").at(checked_key).get<std::vector<std::int64_t>>());
  }

  if (multiserver_state.contains("received_items") &&
      multiserver_state.at("received_items").contains(checked_key)) {
    int index = 0;
    for (const auto& item : multiserver_state.at("received_items").at(checked_key)) {
      session.enqueue_received_item(ReceivedItemView{
          .index = index++,
          .item_id = item.at("item").get<std::int64_t>(),
          .item_name = item.at("item_name").get<std::string>(),
          .location_id = item.at("location").get<std::int64_t>(),
          .sender_slot = item.at("player").get<int>(),
          .sender_alias = lookup_alias(
              aliases,
              options.team_id,
              item.at("player").get<int>(),
              item.value("sender_name", std::string{})),
          .flags = item.value("flags", 0),
      });
    }
  }

  if (multiserver_state.contains("stored_data")) {
    for (auto it = multiserver_state.at("stored_data").begin(); it != multiserver_state.at("stored_data").end();
         ++it) {
      session.set_stored_data(it.key(), it.value());
    }
  }

  if (multiserver_state.contains("hints") && multiserver_state.at("hints").contains(checked_key)) {
    session.set_hints(multiserver_state.at("hints").at(checked_key));
  }

  if (multiserver_state.contains("hints_used") && multiserver_state.at("hints_used").contains(checked_key)) {
    session.set_hints_used(multiserver_state.at("hints_used").at(checked_key).get<int>());
  }

  if (multiserver_state.contains("game_options")) {
    session.set_game_options(multiserver_state.at("game_options"));
    if (multiserver_state.at("game_options").contains("hint_points")) {
      session.set_hint_points(multiserver_state.at("game_options").at("hint_points").get<int>());
    }
  }

  if (multiserver_state.contains("er_hint_data") && multiserver_state.at("er_hint_data").contains(slot_key)) {
    session.set_er_hint_data(multiserver_state.at("er_hint_data").at(slot_key));
  }

  if (multiserver_state.contains("slot_data") && multiserver_state.at("slot_data").contains(slot_key)) {
    session.set_slot_data(multiserver_state.at("slot_data").at(slot_key));
  }

  if (multiserver_state.contains("locations") && multiserver_state.at("locations").contains(slot_key)) {
    for (auto it = multiserver_state.at("locations").at(slot_key).begin();
         it != multiserver_state.at("locations").at(slot_key).end(); ++it) {
      const auto location_id = std::stoll(it.key());
      const auto& value = it.value();
      session.set_location_mapping(
          location_id,
          value.at("receiver_slot").get<int>(),
          value.at("item_id").get<int>(),
          value.at("location_name").get<std::string>());
    }
  }

  if (multiserver_state.value("tracker_connected", false)) {
    session.mark_tracker_connected(true);
  }
  if (multiserver_state.value("emu_connected", false)) {
    session.mark_emu_connected(true);
  }

  return session;
}

}  // namespace sekailink_server
