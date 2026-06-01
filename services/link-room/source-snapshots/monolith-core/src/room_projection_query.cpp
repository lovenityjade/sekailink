#include "sekailink_server/room_projection_query.hpp"
#include "sekailink_server/room_restore.hpp"

#include <unordered_map>

namespace sekailink_server {

namespace {

std::optional<RoomStateSnapshot> snapshot_from_record(const nlohmann::json& record) {
  if (!record.contains("payload")) {
    return std::nullopt;
  }
  return room_state_snapshot_from_json(record.at("payload"));
}

std::string record_sort_key(const nlohmann::json& record) {
  if (record.contains("generated_at") && record.at("generated_at").is_string()) {
    return record.at("generated_at").get<std::string>();
  }
  if (record.contains("timestamp") && record.at("timestamp").is_string()) {
    return record.at("timestamp").get<std::string>();
  }
  return {};
}

template <typename T, typename Parser>
std::vector<T> sorted_records_for_room(
    const std::vector<nlohmann::json>& records,
    const std::string& room_id,
    Parser parser) {
  std::vector<const nlohmann::json*> filtered;
  filtered.reserve(records.size());
  for (const auto& record : records) {
    if (!record.contains("room_id") || record.at("room_id").get<std::string>() != room_id) {
      continue;
    }
    filtered.push_back(&record);
  }
  std::sort(filtered.begin(), filtered.end(), [](const auto* left, const auto* right) {
    return record_sort_key(*left) < record_sort_key(*right);
  });

  std::vector<T> out;
  out.reserve(filtered.size());
  for (const auto* record : filtered) {
    out.push_back(parser(*record));
  }
  return out;
}

}  // namespace

std::optional<RoomStateSnapshot> latest_room_snapshot_from_records(
    const std::vector<nlohmann::json>& room_records,
    const std::string& room_id) {
  const nlohmann::json* latest = nullptr;
  std::string latest_key;
  for (const auto& record : room_records) {
    if (!record.contains("room_id") || record.at("room_id").get<std::string>() != room_id) {
      continue;
    }
    const auto key = record_sort_key(record);
    if (latest == nullptr || key >= latest_key) {
      latest = &record;
      latest_key = key;
    }
  }
  if (latest == nullptr) {
    return std::nullopt;
  }
  return snapshot_from_record(*latest);
}

std::vector<RoomStateSnapshot> latest_room_snapshots_from_records(const std::vector<nlohmann::json>& room_records) {
  std::unordered_map<std::string, const nlohmann::json*> latest_by_room;
  std::unordered_map<std::string, std::string> latest_keys;
  for (const auto& record : room_records) {
    if (!record.contains("room_id")) {
      continue;
    }
    const auto room_id = record.at("room_id").get<std::string>();
    const auto key = record_sort_key(record);
    if (!latest_by_room.contains(room_id) || key >= latest_keys[room_id]) {
      latest_by_room[room_id] = &record;
      latest_keys[room_id] = key;
    }
  }

  std::vector<RoomStateSnapshot> snapshots;
  snapshots.reserve(latest_by_room.size());
  for (const auto& [room_id, record] : latest_by_room) {
    if (auto snapshot = snapshot_from_record(*record); snapshot.has_value()) {
      snapshots.push_back(std::move(*snapshot));
    }
  }
  return snapshots;
}

std::optional<RoomStateSnapshot> latest_room_snapshot(const RoomProjectionSqliteStore& store, const std::string& room_id) {
  return latest_room_snapshot_from_records(store.read_room_records(), room_id);
}

std::optional<RoomStateSnapshot> latest_room_snapshot(const RoomProjectionMysqlStore& store, const std::string& room_id) {
  return latest_room_snapshot_from_records(store.read_room_records(), room_id);
}

std::vector<RoomStateSnapshot> latest_room_snapshots(const RoomProjectionSqliteStore& store) {
  return latest_room_snapshots_from_records(store.read_room_records());
}

std::vector<RoomStateSnapshot> latest_room_snapshots(const RoomProjectionMysqlStore& store) {
  return latest_room_snapshots_from_records(store.read_room_records());
}

std::vector<RoomEvent> room_events_from_records(
    const std::vector<nlohmann::json>& room_event_records,
    const std::string& room_id) {
  return sorted_records_for_room<RoomEvent>(room_event_records, room_id, [](const nlohmann::json& record) {
    return room_event_from_json(record);
  });
}

std::vector<ClientReport> client_reports_from_records(
    const std::vector<nlohmann::json>& client_report_records,
    const std::string& room_id) {
  return sorted_records_for_room<ClientReport>(client_report_records, room_id, [](const nlohmann::json& record) {
    return client_report_from_json(record);
  });
}

std::vector<RoomEvent> room_events(const RoomProjectionSqliteStore& store, const std::string& room_id) {
  return room_events_from_records(store.read_room_event_records(), room_id);
}

std::vector<RoomEvent> room_events(const RoomProjectionMysqlStore& store, const std::string& room_id) {
  return room_events_from_records(store.read_room_event_records(), room_id);
}

std::vector<ClientReport> client_reports(const RoomProjectionSqliteStore& store, const std::string& room_id) {
  return client_reports_from_records(store.read_client_report_records(), room_id);
}

std::vector<ClientReport> client_reports(const RoomProjectionMysqlStore& store, const std::string& room_id) {
  return client_reports_from_records(store.read_client_report_records(), room_id);
}

}  // namespace sekailink_server
