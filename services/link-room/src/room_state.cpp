#include "sekailink_server/room_state.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
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

std::string room_type_to_string(RoomType room_type) {
  switch (room_type) {
    case RoomType::Live:
      return "live";
    case RoomType::Async:
      return "async";
  }
  throw std::runtime_error("invalid_room_type");
}

std::string connection_state_to_string(ConnectionState connection_state) {
  switch (connection_state) {
    case ConnectionState::Offline:
      return "offline";
    case ConnectionState::Online:
      return "online";
  }
  throw std::runtime_error("invalid_connection_state");
}

std::string utc_timestamp_now() {
  const auto now = std::chrono::system_clock::now();
  const auto tt = std::chrono::system_clock::to_time_t(now);
  std::tm utc_tm{};
#if defined(_WIN32)
  gmtime_s(&utc_tm, &tt);
#else
  gmtime_r(&tt, &utc_tm);
#endif
  std::ostringstream out;
  out << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

nlohmann::json to_json(const CapabilitySet& capabilities) {
  return {
      {"supports_async", capabilities.supports_async},
      {"supports_tracker_state", capabilities.supports_tracker_state},
      {"supports_mobile_summary", capabilities.supports_mobile_summary},
      {"supports_sni_local", capabilities.supports_sni_local},
      {"supports_achievements", capabilities.supports_achievements},
      {"supports_shop", capabilities.supports_shop},
      {"supports_2fa_mobile_approve", capabilities.supports_2fa_mobile_approve},
  };
}

nlohmann::json to_json(const PlayerRoomView& player) {
  return {
      {"team", player.team},
      {"slot", player.slot},
      {"name", player.name},
      {"alias", player.alias},
      {"game", player.game},
      {"connected", player.connected},
  };
}

nlohmann::json to_json(const ReceivedItemView& item) {
  nlohmann::json j = {
      {"index", item.index},
      {"item_id", item.item_id},
      {"item_name", item.item_name},
      {"location_id", item.location_id},
      {"sender_slot", item.sender_slot},
      {"sender_alias", item.sender_alias},
      {"flags", item.flags},
  };
  set_optional(j, "event_key", item.event_key);
  set_optional(j, "mapped_value", item.mapped_value);
  set_optional(j, "tracker_semantic_id", item.tracker_semantic_id);
  return j;
}

nlohmann::json to_json(const RuntimeBridgeState& runtime_state) {
  nlohmann::json j = {
      {"heartbeat_count", runtime_state.heartbeat_count},
      {"connected", runtime_state.connected},
  };
  set_optional(j, "runtime_kind", runtime_state.runtime_kind);
  set_optional(j, "runtime_session_name", runtime_state.runtime_session_name);
  set_optional(j, "driver_instance_id", runtime_state.driver_instance_id);
  set_optional(j, "linkedworld_id", runtime_state.linkedworld_id);
  set_optional(j, "core_profile", runtime_state.core_profile);
  set_optional(j, "client_name", runtime_state.client_name);
  set_optional(j, "client_version", runtime_state.client_version);
  set_optional(j, "session_token_hash", runtime_state.session_token_hash);
  set_optional(j, "ticket_issued_at", runtime_state.ticket_issued_at);
  set_optional(j, "last_heartbeat_at", runtime_state.last_heartbeat_at);
  set_optional(j, "last_item_delivery_at", runtime_state.last_item_delivery_at);
  set_optional(j, "last_check_report_at", runtime_state.last_check_report_at);
  set_optional(j, "last_delivery_ack_at", runtime_state.last_delivery_ack_at);
  set_optional(j, "last_disconnect_reason", runtime_state.last_disconnect_reason);
  set_optional(j, "last_delivery_ack_id", runtime_state.last_delivery_ack_id);
  j["acknowledged_delivery_ids"] = runtime_state.acknowledged_delivery_ids;
  return j;
}

nlohmann::json to_json(const AsyncState& async_state) {
  nlohmann::json j = {
      {"allowed_players", async_state.allowed_players},
  };
  set_optional(j, "expires_at", async_state.expires_at);
  set_optional(j, "last_player_activity", async_state.last_player_activity);
  set_optional(j, "daily_summary_state", async_state.daily_summary_state);
  set_optional(j, "async_notification_state", async_state.async_notification_state);
  set_optional(j, "suspend_state", async_state.suspend_state);
  return j;
}

nlohmann::json to_json(const LogEventContext& ctx) {
  nlohmann::json j = {
      {"service_name", ctx.service_name},
      {"server_name", ctx.server_name},
      {"severity", ctx.severity},
      {"event_type", ctx.event_type},
  };
  set_optional(j, "request_id", ctx.request_id);
  set_optional(j, "session_id", ctx.session_id);
  set_optional(j, "user_id", ctx.user_id);
  set_optional(j, "room_id", ctx.room_id);
  set_optional(j, "lobby_id", ctx.lobby_id);
  return j;
}

nlohmann::json to_json(const ClientReport& report) {
  nlohmann::json j = {
      {"report_type", report.report_type},
      {"source", report.source},
      {"severity", report.severity},
      {"message", report.message},
      {"timestamp", report.timestamp},
      {"details", report.details},
  };
  set_optional(j, "request_id", report.request_id);
  set_optional(j, "session_id", report.session_id);
  set_optional(j, "user_id", report.user_id);
  set_optional(j, "room_id", report.room_id);
  set_optional(j, "lobby_id", report.lobby_id);
  set_optional(j, "game", report.game);
  set_optional(j, "runtime", report.runtime);
  return j;
}

nlohmann::json to_json(const RoomStateSnapshot& snapshot) {
  nlohmann::json players_json = nlohmann::json::array();
  for (const auto& player : snapshot.players) {
    players_json.push_back(to_json(player));
  }

  nlohmann::json items_json = nlohmann::json::array();
  for (const auto& item : snapshot.received_items) {
    items_json.push_back(to_json(item));
  }

  nlohmann::json j = {
      {"room_id", snapshot.room_id},
      {"room_type", room_type_to_string(snapshot.room_type)},
      {"connection_state", connection_state_to_string(snapshot.connection_state)},
      {"game", snapshot.game},
      {"team_id", snapshot.team_id},
      {"slot_id", snapshot.slot_id},
      {"slot_name", snapshot.slot_name},
      {"slot_alias", snapshot.slot_alias},
      {"players", players_json},
      {"checked_locations", snapshot.checked_locations},
      {"missing_locations", snapshot.missing_locations},
      {"received_items", items_json},
      {"stored_data", snapshot.stored_data},
      {"milestones", snapshot.milestones},
      {"notifications", snapshot.notifications},
      {"hints", snapshot.hints},
      {"er_hint_data", snapshot.er_hint_data},
      {"game_options", snapshot.game_options},
      {"hint_points", snapshot.hint_points},
      {"hints_used", snapshot.hints_used},
      {"slot_data", snapshot.slot_data},
      {"linkedworld_surface", snapshot.linkedworld_surface},
      {"seed_contract", snapshot.seed_contract},
      {"seed_contract_summary", seed_contract_summary(snapshot.seed_contract)},
      {"location_to_item", snapshot.location_to_item},
      {"location_to_item_id", snapshot.location_to_item_id},
      {"location_names", snapshot.location_names},
      {"player_aliases", snapshot.player_aliases},
      {"emu_connected", snapshot.emu_connected},
      {"tracker_connected", snapshot.tracker_connected},
      {"sni_required", snapshot.sni_required},
      {"capabilities", to_json(snapshot.capabilities)},
      {"generated_at", snapshot.generated_at.empty() ? utc_timestamp_now() : snapshot.generated_at},
  };
  set_optional(j, "seed_name", snapshot.seed_name);
  set_optional(j, "patch_url", snapshot.patch_url);
  set_optional(j, "tracker_pack", snapshot.tracker_pack);
  set_optional(j, "tracker_variant", snapshot.tracker_variant);
  set_optional(j, "seed_id", snapshot.seed_id);
  set_optional(j, "seed_hash", snapshot.seed_hash);
  set_optional(j, "memory_backend_state", snapshot.memory_backend_state);
  if (snapshot.runtime_state.has_value()) {
    j["runtime_state"] = to_json(*snapshot.runtime_state);
  } else {
    j["runtime_state"] = nullptr;
  }
  if (snapshot.async_state.has_value()) {
    j["async_state"] = to_json(*snapshot.async_state);
  } else {
    j["async_state"] = nullptr;
  }
  return j;
}

nlohmann::json seed_contract_summary(const nlohmann::json& seed_contract) {
  if (!seed_contract.is_object() || seed_contract.empty()) {
    return {
        {"applied", false},
    };
  }
  const auto slots = seed_contract.value("slots", nlohmann::json::array());
  const auto config_versions = seed_contract.value("config_versions", nlohmann::json::array());
  return {
      {"applied", true},
      {"schema_version", seed_contract.value("schema_version", std::string{})},
      {"generation_scope", seed_contract.value("generation_scope", std::string{})},
      {"room_id", seed_contract.value("room_id", std::string{})},
      {"seed_id", seed_contract.value("seed_id", std::string{})},
      {"slot_count", slots.is_array() ? slots.size() : 0},
      {"config_version_count", config_versions.is_array() ? config_versions.size() : 0},
      {"checks_ref", seed_contract.value("checks_ref", std::string{})},
      {"items_ref", seed_contract.value("items_ref", std::string{})},
      {"placements_ref", seed_contract.value("placements_ref", std::string{})},
  };
}

}  // namespace sekailink_server
