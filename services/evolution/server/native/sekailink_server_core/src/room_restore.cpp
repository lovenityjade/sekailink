#include "sekailink_server/room_restore.hpp"

#include <set>
#include <stdexcept>

namespace sekailink_server {

namespace {

RoomType parse_room_type(const nlohmann::json& value) {
  const auto room_type = value.get<std::string>();
  if (room_type == "live") {
    return RoomType::Live;
  }
  if (room_type == "async") {
    return RoomType::Async;
  }
  throw std::runtime_error("invalid_room_type");
}

ConnectionState parse_connection_state(const nlohmann::json& value) {
  const auto state = value.get<std::string>();
  if (state == "offline") {
    return ConnectionState::Offline;
  }
  if (state == "online") {
    return ConnectionState::Online;
  }
  throw std::runtime_error("invalid_connection_state");
}

template <typename T>
std::optional<T> optional_value(const nlohmann::json& input, const char* key) {
  if (!input.contains(key) || input.at(key).is_null()) {
    return std::nullopt;
  }
  return input.at(key).get<T>();
}

PlayerRoomView player_from_json(const nlohmann::json& value) {
  return PlayerRoomView{
      .team = value.value("team", 0),
      .slot = value.at("slot").get<int>(),
      .name = value.at("name").get<std::string>(),
      .alias = value.value("alias", value.at("name").get<std::string>()),
      .game = value.value("game", ""),
      .connected = value.value("connected", false),
  };
}

ReceivedItemView received_item_from_json(const nlohmann::json& value) {
  return ReceivedItemView{
      .index = value.at("index").get<int>(),
      .item_id = value.at("item_id").get<std::int64_t>(),
      .item_name = value.at("item_name").get<std::string>(),
      .location_id = value.at("location_id").get<std::int64_t>(),
      .sender_slot = value.at("sender_slot").get<int>(),
      .sender_alias = value.at("sender_alias").get<std::string>(),
      .flags = value.value("flags", 0),
  };
}

AsyncState async_state_from_json(const nlohmann::json& value) {
  return AsyncState{
      .expires_at = optional_value<std::string>(value, "expires_at"),
      .last_player_activity = optional_value<std::string>(value, "last_player_activity"),
      .allowed_players = value.value("allowed_players", std::vector<int>{}),
      .daily_summary_state = optional_value<std::string>(value, "daily_summary_state"),
      .async_notification_state = optional_value<std::string>(value, "async_notification_state"),
      .suspend_state = optional_value<std::string>(value, "suspend_state"),
  };
}

CapabilitySet capability_set_from_json(const nlohmann::json& value) {
  return CapabilitySet{
      .supports_async = value.value("supports_async", false),
      .supports_tracker_state = value.value("supports_tracker_state", true),
      .supports_mobile_summary = value.value("supports_mobile_summary", false),
      .supports_sni_local = value.value("supports_sni_local", false),
      .supports_achievements = value.value("supports_achievements", false),
      .supports_shop = value.value("supports_shop", false),
      .supports_2fa_mobile_approve = value.value("supports_2fa_mobile_approve", false),
  };
}

std::map<std::int64_t, int> int64_int_map_from_json(const nlohmann::json& value) {
  std::map<std::int64_t, int> out;
  for (auto it = value.begin(); it != value.end(); ++it) {
    out.emplace(std::stoll(it.key()), it.value().get<int>());
  }
  return out;
}

std::map<std::int64_t, std::string> int64_string_map_from_json(const nlohmann::json& value) {
  std::map<std::int64_t, std::string> out;
  for (auto it = value.begin(); it != value.end(); ++it) {
    out.emplace(std::stoll(it.key()), it.value().get<std::string>());
  }
  return out;
}

std::map<int, std::string> int_string_map_from_json(const nlohmann::json& value) {
  std::map<int, std::string> out;
  for (auto it = value.begin(); it != value.end(); ++it) {
    out.emplace(std::stoi(it.key()), it.value().get<std::string>());
  }
  return out;
}

}  // namespace

RoomEvent room_event_from_json(const nlohmann::json& value) {
  return RoomEvent{
      .event_type = value.at("event_type").get<std::string>(),
      .timestamp = value.at("timestamp").get<std::string>(),
      .payload = value.value("payload", nlohmann::json::object()),
  };
}

ClientReport client_report_from_json(const nlohmann::json& value) {
  ClientReport report{
      .report_type = value.value("report_type", "client_error"),
      .source = value.value("source", "client"),
      .severity = value.value("severity", "error"),
      .message = value.at("message").get<std::string>(),
      .timestamp = value.value("timestamp", ""),
      .details = value.value("details", nlohmann::json::object()),
  };
  report.request_id = optional_value<std::string>(value, "request_id");
  report.session_id = optional_value<std::string>(value, "session_id");
  report.user_id = optional_value<std::string>(value, "user_id");
  report.room_id = optional_value<std::string>(value, "room_id");
  report.lobby_id = optional_value<std::string>(value, "lobby_id");
  report.game = optional_value<std::string>(value, "game");
  report.runtime = optional_value<std::string>(value, "runtime");
  return report;
}

RoomStateSnapshot room_state_snapshot_from_json(const nlohmann::json& value) {
  RoomStateSnapshot snapshot;
  snapshot.room_id = value.at("room_id").get<std::string>();
  snapshot.room_type = parse_room_type(value.at("room_type"));
  snapshot.connection_state = parse_connection_state(value.at("connection_state"));
  snapshot.game = value.at("game").get<std::string>();
  snapshot.team_id = value.value("team_id", 0);
  snapshot.slot_id = value.at("slot_id").get<int>();
  snapshot.slot_name = value.at("slot_name").get<std::string>();
  snapshot.slot_alias = value.value("slot_alias", snapshot.slot_name);
  for (const auto& player : value.value("players", nlohmann::json::array())) {
    snapshot.players.push_back(player_from_json(player));
  }
  snapshot.checked_locations = value.value("checked_locations", std::vector<std::int64_t>{});
  snapshot.missing_locations = value.value("missing_locations", std::vector<std::int64_t>{});
  for (const auto& item : value.value("received_items", nlohmann::json::array())) {
    snapshot.received_items.push_back(received_item_from_json(item));
  }
  snapshot.stored_data = value.value("stored_data", std::map<std::string, nlohmann::json>{});
  snapshot.milestones = value.value("milestones", std::vector<std::string>{});
  snapshot.notifications = value.value("notifications", std::vector<std::string>{});
  snapshot.hints = value.value("hints", nlohmann::json::array());
  snapshot.er_hint_data = value.value("er_hint_data", nlohmann::json::object());
  snapshot.game_options = value.value("game_options", nlohmann::json::object());
  snapshot.hint_points = value.value("hint_points", 0);
  snapshot.hints_used = value.value("hints_used", 0);
  snapshot.patch_url = optional_value<std::string>(value, "patch_url");
  snapshot.tracker_pack = optional_value<std::string>(value, "tracker_pack");
  snapshot.tracker_variant = optional_value<std::string>(value, "tracker_variant");
  snapshot.seed_id = optional_value<std::string>(value, "seed_id");
  snapshot.seed_hash = optional_value<std::string>(value, "seed_hash");
  snapshot.slot_data = value.value("slot_data", nlohmann::json::object());
  snapshot.location_to_item = int64_int_map_from_json(value.value("location_to_item", nlohmann::json::object()));
  snapshot.location_to_item_id = int64_int_map_from_json(value.value("location_to_item_id", nlohmann::json::object()));
  snapshot.location_names = int64_string_map_from_json(value.value("location_names", nlohmann::json::object()));
  snapshot.player_aliases = int_string_map_from_json(value.value("player_aliases", nlohmann::json::object()));
  snapshot.emu_connected = value.value("emu_connected", false);
  snapshot.tracker_connected = value.value("tracker_connected", false);
  snapshot.sni_required = value.value("sni_required", false);
  snapshot.memory_backend_state = optional_value<std::string>(value, "memory_backend_state");
  snapshot.capabilities = capability_set_from_json(value.value("capabilities", nlohmann::json::object()));
  if (value.contains("async_state") && !value.at("async_state").is_null()) {
    snapshot.async_state = async_state_from_json(value.at("async_state"));
  }
  snapshot.generated_at = value.value("generated_at", "");
  return snapshot;
}

std::optional<RoomStateSnapshot> load_latest_room_snapshot(const RoomAuditStore& audit_store, const std::string& room_id) {
  const auto snapshots = audit_store.read_snapshots(room_id);
  if (snapshots.empty()) {
    return std::nullopt;
  }
  return room_state_snapshot_from_json(snapshots.back());
}

std::optional<RoomSession> load_room_session_from_audit(const RoomAuditStore& audit_store, const std::string& room_id) {
  const auto snapshot = load_latest_room_snapshot(audit_store, room_id);
  if (!snapshot.has_value()) {
    return std::nullopt;
  }

  std::vector<RoomEvent> events;
  for (const auto& event : audit_store.read_events(room_id)) {
    events.push_back(room_event_from_json(event));
  }

  std::vector<ClientReport> reports;
  for (const auto& report : audit_store.read_client_reports(room_id)) {
    reports.push_back(client_report_from_json(report));
  }

  return RoomSession::from_snapshot(*snapshot, std::move(events), std::move(reports));
}

bool restore_room_from_audit(RoomRegistry& registry, const RoomAuditStore& audit_store, const std::string& room_id) {
  const auto restored = load_room_session_from_audit(audit_store, room_id);
  if (!restored.has_value()) {
    return false;
  }
  return registry.insert_room(room_id, std::move(*restored));
}

std::vector<std::string> restore_all_rooms_from_audit(RoomRegistry& registry, const RoomAuditStore& audit_store) {
  std::vector<std::string> restored_room_ids;
  for (const auto& room_id : audit_store.read_room_ids()) {
    if (restore_room_from_audit(registry, audit_store, room_id)) {
      restored_room_ids.push_back(room_id);
    }
  }
  return restored_room_ids;
}

}  // namespace sekailink_server
