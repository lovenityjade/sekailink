#include "sekailink_server/room_projection.hpp"

#include <algorithm>
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

std::vector<std::string> slot_data_keys(const nlohmann::json& slot_data) {
  std::vector<std::string> keys;
  if (!slot_data.is_object()) {
    return keys;
  }
  keys.reserve(slot_data.size());
  for (auto it = slot_data.begin(); it != slot_data.end(); ++it) {
    keys.push_back(it.key());
  }
  return keys;
}

nlohmann::json latest_received_item_summary(const RoomStateSnapshot& snapshot) {
  if (snapshot.received_items.empty()) {
    return nullptr;
  }
  return to_json(snapshot.received_items.back());
}

std::string string_or_empty(const nlohmann::json& value, const char* key) {
  if (!value.contains(key) || value.at(key).is_null() || !value.at(key).is_string()) {
    return "";
  }
  return value.at(key).get<std::string>();
}

std::size_t array_size_or_zero(const nlohmann::json& value, const char* key) {
  if (!value.contains(key) || value.at(key).is_null() || !value.at(key).is_array()) {
    return 0;
  }
  return value.at(key).size();
}

int pending_delivery_count(const RoomStateSnapshot& snapshot) {
  const auto received_count = static_cast<int>(snapshot.received_items.size());
  if (!snapshot.runtime_state.has_value()) {
    return received_count;
  }
  const auto acknowledged_count = static_cast<int>(snapshot.runtime_state->acknowledged_delivery_ids.size());
  return std::max(received_count - acknowledged_count, 0);
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
      {"pending_delivery_count", pending_delivery_count(snapshot)},
      {"acknowledged_delivery_count",
       snapshot.runtime_state.has_value()
           ? static_cast<int>(snapshot.runtime_state->acknowledged_delivery_ids.size())
           : 0},
      {"slot_data_shape", snapshot.slot_data.type_name()},
      {"slot_data_entry_count", snapshot.slot_data.is_object() ? snapshot.slot_data.size() : 0},
      {"slot_data_keys", slot_data_keys(snapshot.slot_data)},
      {"linkedworld_surface", snapshot.linkedworld_surface},
      {"linkedworld_id", string_or_empty(snapshot.linkedworld_surface, "linkedworld_id")},
      {"linkedworld_display_name", string_or_empty(snapshot.linkedworld_surface, "display_name")},
      {"linkedworld_item_semantic_count", array_size_or_zero(snapshot.linkedworld_surface, "item_semantics")},
      {"seed_contract", snapshot.seed_contract},
      {"seed_contract_summary", seed_contract_summary(snapshot.seed_contract)},
      {"seed_contract_applied", seed_contract_summary(snapshot.seed_contract).value("applied", false)},
      {"latest_received_item", latest_received_item_summary(snapshot)},
      {"next_received_item_index", snapshot.received_items.empty() ? 0 : snapshot.received_items.back().index + 1},
      {"runtime_connected", snapshot.runtime_state.has_value() ? snapshot.runtime_state->connected : false},
      {"emu_connected", snapshot.emu_connected},
      {"tracker_connected", snapshot.tracker_connected},
      {"sni_required", snapshot.sni_required},
      {"generated_at", snapshot.generated_at},
      {"payload", to_json(snapshot)},
  };
  set_optional(j, "seed_name", snapshot.seed_name);
  set_optional(j, "seed_id", snapshot.seed_id);
  set_optional(j, "seed_hash", snapshot.seed_hash);
  set_optional(j, "patch_url", snapshot.patch_url);
  set_optional(j, "tracker_pack", snapshot.tracker_pack);
  set_optional(j, "tracker_variant", snapshot.tracker_variant);
  if (snapshot.runtime_state.has_value()) {
    set_optional(j, "runtime_session_name", snapshot.runtime_state->runtime_session_name);
    set_optional(j, "driver_instance_id", snapshot.runtime_state->driver_instance_id);
    set_optional(j, "linkedworld_id", snapshot.runtime_state->linkedworld_id);
    set_optional(j, "core_profile", snapshot.runtime_state->core_profile);
    set_optional(j, "ticket_issued_at", snapshot.runtime_state->ticket_issued_at);
    set_optional(j, "last_runtime_heartbeat", snapshot.runtime_state->last_heartbeat_at);
    set_optional(j, "last_item_delivery_at", snapshot.runtime_state->last_item_delivery_at);
    set_optional(j, "last_check_report_at", snapshot.runtime_state->last_check_report_at);
    set_optional(j, "last_delivery_ack_at", snapshot.runtime_state->last_delivery_ack_at);
    set_optional(j, "last_delivery_ack_id", snapshot.runtime_state->last_delivery_ack_id);
  } else {
    j["runtime_session_name"] = nullptr;
    j["driver_instance_id"] = nullptr;
    j["linkedworld_id"] = nullptr;
    j["core_profile"] = nullptr;
    j["ticket_issued_at"] = nullptr;
    j["last_runtime_heartbeat"] = nullptr;
    j["last_item_delivery_at"] = nullptr;
    j["last_check_report_at"] = nullptr;
    j["last_delivery_ack_at"] = nullptr;
    j["last_delivery_ack_id"] = nullptr;
  }
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
  return build_projection_batch(snapshot, events, reports, 0, 0);
}

RoomProjectionBatch build_projection_batch(
    const RoomStateSnapshot& snapshot,
    const std::vector<RoomEvent>& events,
    const std::vector<ClientReport>& reports,
    std::size_t event_start_index,
    std::size_t report_start_index) {
  RoomProjectionBatch batch;
  batch.room_record = project_room_record(snapshot);
  const auto bounded_event_start = std::min(event_start_index, events.size());
  for (std::size_t index = bounded_event_start; index < events.size(); ++index) {
    batch.room_event_records.push_back(project_room_event_record(snapshot.room_id, events[index]));
  }
  const auto bounded_report_start = std::min(report_start_index, reports.size());
  for (std::size_t index = bounded_report_start; index < reports.size(); ++index) {
    batch.client_report_records.push_back(project_client_report_record(snapshot.room_id, reports[index]));
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
