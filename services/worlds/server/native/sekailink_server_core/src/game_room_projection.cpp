#include "sekailink_server/game_room_projection.hpp"

#include "sekailink_server/game_server_protocol.hpp"

#include <algorithm>
#include <set>

namespace sekailink_server {

namespace {

std::string alias_for_slot(const GameWorldPackage& package, int slot_id) {
  const auto it = package.slots.find(slot_id);
  if (it == package.slots.end()) {
    return "slot:" + std::to_string(slot_id);
  }
  return it->second.name;
}

std::vector<std::int64_t> missing_locations_for_slot(const GameWorldPackage& package,
                                                     const std::set<std::int64_t>& checked_locations,
                                                     int slot_id) {
  std::vector<std::int64_t> missing;
  for (const auto& [location_id, location] : package.locations) {
    if (location.owner_slot == slot_id && !checked_locations.contains(location_id)) {
      missing.push_back(location_id);
    }
  }
  std::sort(missing.begin(), missing.end());
  return missing;
}

std::map<int, std::set<std::int64_t>> checked_locations_map(const GameSaveState& save_state) {
  std::map<int, std::set<std::int64_t>> checked;
  for (const auto& [slot_id, locations] : save_state.checked_locations_by_slot) {
    checked[slot_id] = std::set<std::int64_t>(locations.begin(), locations.end());
  }
  return checked;
}

}  // namespace

std::string projected_room_id_for_session_slot(const std::string& session_name,
                                               int slot_id,
                                               const GameRoomProjectionOptions& options) {
  return options.room_id_prefix + "::" + session_name + "::slot::" + std::to_string(slot_id);
}

RoomSession project_game_session_slot_to_room_session(const std::string& session_name,
                                                      const GameSession& session,
                                                      int slot_id,
                                                      const GameRoomProjectionOptions& options) {
  const auto& package = session.package();
  const auto slot_it = package.slots.find(slot_id);
  if (slot_it == package.slots.end()) {
    throw std::runtime_error("projection_unknown_slot");
  }

  const auto save_state = session.export_save_state();
  const auto checked_by_slot = checked_locations_map(save_state);
  const auto checked_it = checked_by_slot.find(slot_id);
  const auto checked_locations = checked_it == checked_by_slot.end() ? std::set<std::int64_t>{} : checked_it->second;

  RoomSession room(RoomSessionConfig{
      .room_id = projected_room_id_for_session_slot(session_name, slot_id, options),
      .room_type = options.room_type,
      .game = slot_it->second.game,
      .team_id = 0,
      .slot_id = slot_id,
      .slot_name = slot_it->second.name,
      .slot_alias = slot_it->second.name,
      .seed_id = package.seed_id,
      .seed_hash = package.seed_hash,
  });

  for (const auto& [player_slot_id, slot] : package.slots) {
    room.upsert_player(PlayerRoomView{
        .team = 0,
        .slot = player_slot_id,
        .name = slot.name,
        .alias = slot.name,
        .game = slot.game,
        .connected = false,
    });
  }

  for (const auto location_id : checked_locations) {
    room.record_check(location_id);
  }
  room.set_missing_locations(missing_locations_for_slot(package, checked_locations, slot_id));

  int received_index = 0;
  for (const auto& delivery : save_state.delivered_items) {
    if (delivery.receiver_slot != slot_id) {
      continue;
    }
    if (!options.include_acknowledged_items && delivery.acknowledged) {
      continue;
    }
    int flags = 0;
    if (delivery.acknowledged) {
      flags |= 1;
    }
    room.enqueue_received_item(ReceivedItemView{
        .index = received_index++,
        .item_id = delivery.item_id,
        .item_name = delivery.item_name,
        .location_id = delivery.source_location_id,
        .sender_slot = delivery.sender_slot,
        .sender_alias = alias_for_slot(package, delivery.sender_slot),
        .flags = flags,
    });
  }

  for (const auto& [location_id, location] : package.locations) {
    if (location.owner_slot == slot_id) {
      room.set_location_mapping(location_id,
                                location.receiver_slot,
                                static_cast<int>(location.item_id),
                                location.location_name);
    }
  }

  room.set_game_options(package.server_rules);
  room.set_slot_data({
      {"session_name", session_name},
      {"world_id", package.world_id},
      {"linkedworld_id", package.linkedworld_id},
      {"slot_id", slot_id},
  });
  room.set_stored_data("game_server_world_id", package.world_id);
  room.set_stored_data("game_server_linkedworld_id", package.linkedworld_id);
  room.set_stored_data("game_server_seed_id", package.seed_id);
  room.set_stored_data("game_server_session_name", session_name);
  return room;
}

std::vector<std::pair<std::string, RoomSession>> project_game_session_to_rooms(
    const std::string& session_name,
    const GameSession& session,
    const GameRoomProjectionOptions& options) {
  std::vector<std::pair<std::string, RoomSession>> projected;
  projected.reserve(session.package().slots.size());
  for (const auto& [slot_id, _] : session.package().slots) {
    auto room = project_game_session_slot_to_room_session(session_name, session, slot_id, options);
    projected.emplace_back(projected_room_id_for_session_slot(session_name, slot_id, options), std::move(room));
  }
  return projected;
}

void sync_game_session_projection(const std::string& session_name,
                                  const GameSession& session,
                                  RoomRegistry& room_registry,
                                  const GameRoomProjectionOptions& options) {
  const auto prefix = options.room_id_prefix + "::" + session_name + "::";
  for (const auto& room_id : room_registry.list_room_ids()) {
    if (room_id.rfind(prefix, 0) == 0) {
      room_registry.remove_room(room_id);
    }
  }
  for (auto& [room_id, room] : project_game_session_to_rooms(session_name, session, options)) {
    room_registry.insert_room(room_id, std::move(room));
  }
}

void sync_all_game_session_projections(const GameSessionRegistry& game_registry,
                                       RoomRegistry& room_registry,
                                       const GameRoomProjectionOptions& options) {
  const auto prefix = options.room_id_prefix + "::";
  for (const auto& room_id : room_registry.list_room_ids()) {
    if (room_id.rfind(prefix, 0) == 0) {
      room_registry.remove_room(room_id);
    }
  }
  for (const auto& session_name : game_registry.list_session_names()) {
    const auto* session = game_registry.find_session(session_name);
    if (session != nullptr) {
      for (auto& [room_id, room] : project_game_session_to_rooms(session_name, *session, options)) {
        room_registry.insert_room(room_id, std::move(room));
      }
    }
  }
}

}  // namespace sekailink_server
