#include "sekailink_server/room_projection.hpp"

#include <fstream>
#include <stdexcept>

namespace sekailink_server {

namespace {

template <typename T>
void set_optional(nlohmann::json& j, const char* key, const std::optional<T>& value) {
  if (value.has_value()) {
    j[key] = *value;
  } else {
    j[key] = nullptr;
  }
}

}  // namespace

nlohmann::json project_room_record(const RoomStateSnapshot& snapshot) {
  nlohmann::json j = {
      {"record_type", "room_record"},
      {"room_id", snapshot.room_id},
      {"room_type", room_type_to_string(snapshot.room_type)},
      {"connection_state", connection_state_to_string(snapshot.connection_state)},
      {"game", snapshot.game},
      {"team_id", snapshot.team_id},
      {"slot_id", snapshot.slot_id},
      {"slot_name", snapshot.slot_name},
      {"slot_alias", snapshot.slot_alias},
      {"player_count", snapshot.players.size()},
      {"checked_count", snapshot.checked_locations.size()},
      {"missing_count", snapshot.missing_locations.size()},
      {"received_item_count", snapshot.received_items.size()},
      {"emu_connected", snapshot.emu_connected},
      {"tracker_connected", snapshot.tracker_connected},
      {"sni_required", snapshot.sni_required},
      {"generated_at", snapshot.generated_at},
      {"payload", to_json(snapshot)},
  };
  set_optional(j, "seed_id", snapshot.seed_id);
  set_optional(j, "seed_hash", snapshot.seed_hash);
  set_optional(j, "patch_url", snapshot.patch_url);
  set_optional(j, "tracker_pack", snapshot.tracker_pack);
  set_optional(j, "tracker_variant", snapshot.tracker_variant);
  return j;
}

nlohmann::json project_room_event_record(const std::string& room_id, const RoomEvent& event) {
  return {
      {"record_type", "room_event_record"},
      {"room_id", room_id},
      {"event_type", event.event_type},
      {"timestamp", event.timestamp},
      {"payload", event.payload},
  };
}

nlohmann::json project_client_report_record(const std::string& room_id, const ClientReport& report) {
  auto payload = to_json(report);
  payload["record_type"] = "client_report_record";
  payload["room_id"] = room_id;
  return payload;
}

RoomProjectionBatch build_projection_batch(
    const RoomStateSnapshot& snapshot,
    const std::vector<RoomEvent>& events,
    const std::vector<ClientReport>& reports) {
  RoomProjectionBatch batch;
  batch.room_record = project_room_record(snapshot);
  for (const auto& event : events) {
    batch.room_event_records.push_back(project_room_event_record(snapshot.room_id, event));
  }
  for (const auto& report : reports) {
    batch.client_report_records.push_back(project_client_report_record(snapshot.room_id, report));
  }
  return batch;
}

RoomProjectionSpool::RoomProjectionSpool(std::filesystem::path root) : root_(std::move(root)) {
  std::filesystem::create_directories(root_);
}

void RoomProjectionSpool::append_batch(const RoomProjectionBatch& batch) {
  append_jsonl(root_ / "room_records.jsonl", batch.room_record);
  for (const auto& event : batch.room_event_records) {
    append_jsonl(root_ / "room_event_records.jsonl", event);
  }
  for (const auto& report : batch.client_report_records) {
    append_jsonl(root_ / "client_report_records.jsonl", report);
  }
}

std::vector<nlohmann::json> RoomProjectionSpool::read_room_records() const {
  return read_jsonl(root_ / "room_records.jsonl");
}

std::vector<nlohmann::json> RoomProjectionSpool::read_room_event_records() const {
  return read_jsonl(root_ / "room_event_records.jsonl");
}

std::vector<nlohmann::json> RoomProjectionSpool::read_client_report_records() const {
  return read_jsonl(root_ / "client_report_records.jsonl");
}

void RoomProjectionSpool::append_jsonl(const std::filesystem::path& path, const nlohmann::json& payload) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::app);
  if (!out.is_open()) {
    throw std::runtime_error("room_projection_spool_open_failed");
  }
  out << payload.dump() << "\n";
}

std::vector<nlohmann::json> RoomProjectionSpool::read_jsonl(const std::filesystem::path& path) const {
  std::vector<nlohmann::json> entries;
  if (!std::filesystem::exists(path)) {
    return entries;
  }
  std::ifstream in(path);
  if (!in.is_open()) {
    throw std::runtime_error("room_projection_spool_read_failed");
  }
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }
    entries.push_back(nlohmann::json::parse(line));
  }
  return entries;
}

}  // namespace sekailink_server
