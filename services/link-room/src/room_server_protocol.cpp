#include "sekailink_server/room_server_protocol.hpp"

#include <stdexcept>
#include <unordered_set>

namespace sekailink_server {

namespace {

ProtocolChannel parse_channel(const nlohmann::json& value) {
  const auto channel = value.get<std::string>();
  if (channel == "admin") {
    return ProtocolChannel::Admin;
  }
  if (channel == "runtime") {
    return ProtocolChannel::Runtime;
  }
  if (channel == "client_report") {
    return ProtocolChannel::ClientReport;
  }
  throw std::runtime_error("invalid_protocol_channel");
}

nlohmann::json projection_snapshot_compare_json(const RoomSession& room) {
  auto snapshot_json = to_json(room.snapshot());
  snapshot_json["generated_at"] = nullptr;
  return snapshot_json;
}

std::optional<std::string> command_room_id(const nlohmann::json& command) {
  if (command.contains("room_id") && !command.at("room_id").is_null()) {
    return command.at("room_id").get<std::string>();
  }
  if (command.contains("session_name") && !command.at("session_name").is_null()) {
    return command.at("session_name").get<std::string>();
  }
  return std::nullopt;
}

bool projection_command_enabled(const std::string& cmd) {
  static const std::unordered_set<std::string> kReadOnlyCommands = {
      "list_rooms",
      "expired_rooms",
      "purge_expired_rooms",
      "room_summary",
      "room_events",
      "client_reports",
  };
  return !kReadOnlyCommands.contains(cmd);
}

bool response_ok(const nlohmann::json& response) {
  return response.contains("ok") && response.at("ok").is_boolean() && response.at("ok").get<bool>();
}

}  // namespace

RoomServerProtocolService::RoomServerProtocolService(
    RoomRegistry& registry,
    RoomAuditStore* audit_store,
    RoomProjectionStore* projection_store,
    const RoomServerAuthPolicy* auth_policy)
    : registry_(registry), audit_store_(audit_store), projection_store_(projection_store), auth_policy_(auth_policy) {}

bool RoomServerAuthPolicy::requires_auth(ProtocolChannel channel) const {
  switch (channel) {
    case ProtocolChannel::Admin:
      return admin_token.has_value();
    case ProtocolChannel::Runtime:
      return runtime_token.has_value();
    case ProtocolChannel::ClientReport:
      return client_report_token.has_value();
  }
  return false;
}

bool RoomServerAuthPolicy::token_valid(ProtocolChannel channel, const std::optional<std::string>& presented_token) const {
  const auto token_matches = [&presented_token](const std::optional<std::string>& expected) {
    if (!expected.has_value()) {
      return true;
    }
    return presented_token.has_value() && *presented_token == *expected;
  };
  switch (channel) {
    case ProtocolChannel::Admin:
      return token_matches(admin_token);
    case ProtocolChannel::Runtime:
      return token_matches(runtime_token);
    case ProtocolChannel::ClientReport:
      return token_matches(client_report_token);
  }
  return false;
}

nlohmann::json RoomServerProtocolService::handle(const ProtocolEnvelope& envelope) const {
  if (auth_policy_ != nullptr && auth_policy_->requires_auth(envelope.channel) &&
      !auth_policy_->token_valid(envelope.channel, envelope.auth_token)) {
    return {
        {"ok", false},
        {"error", "unauthorized"},
        {"channel", channel_to_string(envelope.channel)},
    };
  }
  if (!envelope.command.contains("cmd")) {
    return {{"ok", false}, {"error", "missing_cmd"}};
  }
  const auto cmd = envelope.command.at("cmd").get<std::string>();
  if (!command_allowed(envelope.channel, cmd)) {
    return {
        {"ok", false},
        {"error", "command_not_allowed"},
        {"channel", channel_to_string(envelope.channel)},
        {"cmd", cmd},
    };
  }

  std::optional<std::string> room_id;
  std::optional<nlohmann::json> before_snapshot;
  std::size_t event_start_index = 0;
  std::size_t report_start_index = 0;
  if (const auto normalized_room_id = command_room_id(envelope.command); normalized_room_id.has_value()) {
    room_id = *normalized_room_id;
    if (const auto* room = registry_.find_room(*room_id); room != nullptr) {
      before_snapshot = projection_snapshot_compare_json(*room);
      event_start_index = room->events().size();
      report_start_index = room->client_reports().size();
    }
  }

  auto response = handle_room_server_command_with_audit(registry_, envelope.command, audit_store_);

  if (projection_store_ != nullptr && room_id.has_value() && response_ok(response) && projection_command_enabled(cmd)) {
    if (const auto* room = registry_.find_room(*room_id); room != nullptr) {
      const auto after_snapshot = room->snapshot();
      auto after_snapshot_json = to_json(after_snapshot);
      after_snapshot_json["generated_at"] = nullptr;
      const auto has_event_delta = event_start_index < room->events().size();
      const auto has_report_delta = report_start_index < room->client_reports().size();
      const auto snapshot_changed = !before_snapshot.has_value() || *before_snapshot != after_snapshot_json;
      if (snapshot_changed || has_event_delta || has_report_delta || cmd == "snapshot_room") {
        try {
          projection_store_->append_batch(build_projection_batch(
              after_snapshot,
              room->events(),
              room->client_reports(),
              event_start_index,
              report_start_index));
        } catch (const std::exception& exception) {
          response["projection_warning"] = {
              {"ok", false},
              {"error", exception.what()},
          };
        }
      }
    }
  }
  response["channel"] = channel_to_string(envelope.channel);
  return response;
}

std::string RoomServerProtocolService::channel_to_string(ProtocolChannel channel) {
  switch (channel) {
    case ProtocolChannel::Admin:
      return "admin";
    case ProtocolChannel::Runtime:
      return "runtime";
    case ProtocolChannel::ClientReport:
      return "client_report";
  }
  throw std::runtime_error("invalid_protocol_channel");
}

bool RoomServerProtocolService::command_allowed(ProtocolChannel channel, const std::string& cmd) {
  static const std::unordered_set<std::string> kAdminAllowed = {
      "create_room",          "remove_room",         "list_rooms",          "expired_rooms",
      "purge_expired_rooms",  "upsert_player",       "connect_player",      "disconnect_player",
      "mark_emu_connected",   "mark_tracker_connected", "record_check",     "set_missing_locations",
      "enqueue_received_item","set_seed_metadata",   "set_stored_data",     "set_slot_data",       "set_location_mapping",
      "apply_linkedworld_surface", "apply_seed_contract",
      "runtime_heartbeat",    "runtime_disconnect",  "issue_ticket",       "runtime_event",
      "pending_items",        "acknowledge_delivery",
      "set_allowed_players",  "set_daily_summary_state",
      "set_async_notification_state", "set_suspend_state", "set_expires_at", "set_milestones",
      "set_notifications",
      "ingest_client_report", "snapshot_room",       "sync_room",           "room_summary",        "room_events",
      "client_reports",
  };
  static const std::unordered_set<std::string> kRuntimeAllowed = {
      "connect_player",       "disconnect_player",    "mark_emu_connected",  "mark_tracker_connected",
      "record_check",         "set_missing_locations","enqueue_received_item","set_seed_metadata", "set_stored_data",
      "set_slot_data",        "set_location_mapping", "apply_linkedworld_surface", "runtime_heartbeat",   "runtime_disconnect",
      "issue_ticket",         "runtime_event",        "pending_items",       "acknowledge_delivery",
      "snapshot_room",        "sync_room",            "room_summary",
  };
  static const std::unordered_set<std::string> kClientReportAllowed = {
      "ingest_client_report",
  };

  switch (channel) {
    case ProtocolChannel::Admin:
      return kAdminAllowed.contains(cmd);
    case ProtocolChannel::Runtime:
      return kRuntimeAllowed.contains(cmd);
    case ProtocolChannel::ClientReport:
      return kClientReportAllowed.contains(cmd);
  }
  return false;
}

ProtocolEnvelope RoomServerProtocolService::parse(const nlohmann::json& envelope) {
  return protocol_envelope_from_json(envelope);
}

ProtocolEnvelope protocol_envelope_from_json(const nlohmann::json& envelope) {
  if (!envelope.contains("channel") || !envelope.contains("command")) {
    throw std::runtime_error("invalid_protocol_envelope");
  }
  ProtocolEnvelope parsed;
  parsed.channel = parse_channel(envelope.at("channel"));
  if (envelope.contains("auth_token") && !envelope.at("auth_token").is_null()) {
    parsed.auth_token = envelope.at("auth_token").get<std::string>();
  }
  parsed.command = envelope.at("command");
  return parsed;
}

nlohmann::json handle_protocol_json(
    RoomRegistry& registry,
    RoomAuditStore* audit_store,
    RoomProjectionStore* projection_store,
    const RoomServerAuthPolicy* auth_policy,
    const nlohmann::json& envelope_json) {
  const auto envelope = protocol_envelope_from_json(envelope_json);
  RoomServerProtocolService service(registry, audit_store, projection_store, auth_policy);
  return service.handle(envelope);
}

}  // namespace sekailink_server
