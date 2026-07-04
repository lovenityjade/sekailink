#include "sekailink_server/room_projection_restore.hpp"

#include "sekailink_server/room_restore.hpp"

#include <map>

namespace sekailink_server {

std::vector<std::string> restore_rooms_from_projection_records(
    RoomRegistry& registry,
    const std::vector<nlohmann::json>& room_records,
    const std::vector<nlohmann::json>& room_event_records,
    const std::vector<nlohmann::json>& client_report_records) {
  std::map<std::string, nlohmann::json> latest_by_room;
  std::map<std::string, std::vector<RoomEvent>> events_by_room;
  std::map<std::string, std::vector<ClientReport>> reports_by_room;
  for (const auto& record : room_records) {
    if (!record.contains("room_id") || !record.contains("payload")) {
      continue;
    }
    latest_by_room[record.at("room_id").get<std::string>()] = record.at("payload");
  }
  for (const auto& record : room_event_records) {
    if (!record.contains("room_id")) {
      continue;
    }
    events_by_room[record.at("room_id").get<std::string>()].push_back(room_event_from_json(record));
  }
  for (const auto& record : client_report_records) {
    if (!record.contains("room_id")) {
      continue;
    }
    reports_by_room[record.at("room_id").get<std::string>()].push_back(client_report_from_json(record));
  }

  std::vector<std::string> restored_room_ids;
  for (const auto& [room_id, payload] : latest_by_room) {
    const auto snapshot = room_state_snapshot_from_json(payload);
    if (registry.insert_room(
            room_id,
            RoomSession::from_snapshot(snapshot, events_by_room[room_id], reports_by_room[room_id]))) {
      restored_room_ids.push_back(room_id);
    }
  }
  return restored_room_ids;
}

std::vector<std::string> restore_rooms_from_projection_store(
    RoomRegistry& registry,
    const RoomProjectionSqliteStore& store) {
  return restore_rooms_from_projection_records(
      registry,
      store.read_room_records(),
      store.read_room_event_records(),
      store.read_client_report_records());
}

std::vector<std::string> restore_rooms_from_projection_store(
    RoomRegistry& registry,
    const RoomProjectionMysqlStore& store) {
  return restore_rooms_from_projection_records(
      registry,
      store.read_room_records(),
      store.read_room_event_records(),
      store.read_client_report_records());
}

}  // namespace sekailink_server
