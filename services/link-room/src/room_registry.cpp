#include "sekailink_server/room_registry.hpp"

#include "sekailink_server/room_audit_store.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>
#include <vector>

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

PlayerRoomView parse_player(const nlohmann::json& value) {
  return PlayerRoomView{
      .team = value.value("team", 0),
      .slot = value.at("slot").get<int>(),
      .name = value.at("name").get<std::string>(),
      .alias = value.value("alias", value.at("name").get<std::string>()),
      .game = value.value("game", ""),
      .connected = value.value("connected", false),
  };
}

std::optional<std::string> optional_string(const nlohmann::json& input, const char* key) {
  if (!input.contains(key) || input.at(key).is_null()) {
    return std::nullopt;
  }
  return input.at(key).get<std::string>();
}

ReceivedItemView parse_received_item(const nlohmann::json& value) {
  return ReceivedItemView{
      .index = value.value("index", -1),
      .item_id = value.at("item_id").get<std::int64_t>(),
      .item_name = value.at("item_name").get<std::string>(),
      .location_id = value.at("location_id").get<std::int64_t>(),
      .sender_slot = value.at("sender_slot").get<int>(),
      .sender_alias = value.at("sender_alias").get<std::string>(),
      .flags = value.value("flags", 0),
      .event_key = optional_string(value, "event_key"),
      .mapped_value = optional_string(value, "mapped_value"),
      .tracker_semantic_id = optional_string(value, "tracker_semantic_id"),
  };
}

std::optional<std::string> command_room_id(const nlohmann::json& input) {
  if (const auto room_id = optional_string(input, "room_id"); room_id.has_value()) {
    return room_id;
  }
  return optional_string(input, "session_name");
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string json_string_or_empty(const nlohmann::json& value, const char* key) {
  if (!value.contains(key) || value.at(key).is_null() || !value.at(key).is_string()) {
    return "";
  }
  return value.at(key).get<std::string>();
}

std::size_t bounded_tail_start(std::size_t size, const nlohmann::json& command) {
  if (!command.contains("limit")) {
    return 0;
  }
  const auto raw_limit = command.at("limit").get<int>();
  if (raw_limit <= 0) {
    return 0;
  }
  const auto limit = static_cast<std::size_t>(raw_limit);
  if (limit >= size) {
    return 0;
  }
  return size - limit;
}

std::size_t bounded_offset(const nlohmann::json& command) {
  if (!command.contains("offset")) {
    return 0;
  }
  const auto raw_offset = command.at("offset").get<int>();
  if (raw_offset <= 0) {
    return 0;
  }
  return static_cast<std::size_t>(raw_offset);
}

std::optional<std::string> normalized_filter_string(const nlohmann::json& command, const char* key) {
  if (!command.contains(key) || command.at(key).is_null() || !command.at(key).is_string()) {
    return std::nullopt;
  }
  auto value = lower(command.at(key).get<std::string>());
  if (value.empty()) {
    return std::nullopt;
  }
  return value;
}

ClientReport parse_client_report(const nlohmann::json& value) {
  ClientReport report{
      .report_type = value.value("report_type", "client_error"),
      .source = value.value("source", "client"),
      .severity = value.value("severity", "error"),
      .message = value.at("message").get<std::string>(),
      .timestamp = value.value("timestamp", ""),
      .details = value.value("details", nlohmann::json::object()),
  };
  report.request_id = optional_string(value, "request_id");
  report.session_id = optional_string(value, "session_id");
  report.user_id = optional_string(value, "user_id");
  report.room_id = optional_string(value, "room_id");
  report.lobby_id = optional_string(value, "lobby_id");
  report.game = optional_string(value, "game");
  report.runtime = optional_string(value, "runtime");
  return report;
}

nlohmann::json seed_contract_from_command(const nlohmann::json& command) {
  if (command.contains("seed_contract") && command.at("seed_contract").is_object()) {
    return command.at("seed_contract");
  }
  if (command.contains("link_room_seed_contract") && command.at("link_room_seed_contract").is_object()) {
    return command.at("link_room_seed_contract");
  }
  if (command.contains("package") &&
      command.at("package").is_object() &&
      command.at("package").contains("link_room_seed_contract") &&
      command.at("package").at("link_room_seed_contract").is_object()) {
    return command.at("package").at("link_room_seed_contract");
  }
  if (command.value("schema_version", std::string{}) == "sekailink-link-room-seed-contract-v1") {
    return command;
  }
  return nlohmann::json::object();
}

}  // namespace

bool RoomRegistry::create_room(RoomSessionConfig config) {
  const auto room_id = config.room_id;
  return rooms_.emplace(room_id, RoomSession(std::move(config))).second;
}

bool RoomRegistry::insert_room(std::string room_id, RoomSession session) {
  return rooms_.emplace(std::move(room_id), std::move(session)).second;
}

bool RoomRegistry::remove_room(const std::string& room_id) {
  return rooms_.erase(room_id) > 0;
}

bool RoomRegistry::has_room(const std::string& room_id) const {
  return rooms_.contains(room_id);
}

RoomSession* RoomRegistry::find_room(const std::string& room_id) {
  const auto it = rooms_.find(room_id);
  return it == rooms_.end() ? nullptr : &it->second;
}

const RoomSession* RoomRegistry::find_room(const std::string& room_id) const {
  const auto it = rooms_.find(room_id);
  return it == rooms_.end() ? nullptr : &it->second;
}

std::vector<std::string> RoomRegistry::list_room_ids(
    std::optional<std::size_t> limit,
    std::size_t offset,
    const std::optional<std::string>& query,
    const std::optional<std::string>& room_type,
    const std::optional<std::string>& connection_state) const {
  std::vector<std::string> room_ids;
  room_ids.reserve(rooms_.size());
  const auto lowered_query = query.has_value() ? std::optional<std::string>(lower(*query)) : std::nullopt;
  const auto lowered_room_type = room_type.has_value() ? std::optional<std::string>(lower(*room_type)) : std::nullopt;
  const auto lowered_connection_state =
      connection_state.has_value() ? std::optional<std::string>(lower(*connection_state)) : std::nullopt;
  for (const auto& [room_id, session] : rooms_) {
    const auto snapshot = session.snapshot();
    if (lowered_room_type.has_value() && room_type_to_string(snapshot.room_type) != *lowered_room_type) {
      continue;
    }
    if (lowered_connection_state.has_value() &&
        lower(connection_state_to_string(snapshot.connection_state)) != *lowered_connection_state) {
      continue;
    }
    if (lowered_query.has_value()) {
      const auto room_id_l = lower(room_id);
      const auto game_l = lower(snapshot.game);
      const auto slot_l = lower(snapshot.slot_name);
      if (room_id_l.find(*lowered_query) == std::string::npos &&
          game_l.find(*lowered_query) == std::string::npos &&
          slot_l.find(*lowered_query) == std::string::npos) {
        continue;
      }
    }
    room_ids.push_back(room_id);
  }
  std::sort(room_ids.begin(), room_ids.end());
  if (offset > 0) {
    if (offset >= room_ids.size()) {
      return {};
    }
    room_ids.erase(room_ids.begin(), room_ids.begin() + static_cast<std::ptrdiff_t>(offset));
  }
  if (limit.has_value() && *limit < room_ids.size()) {
    room_ids.resize(*limit);
  }
  return room_ids;
}

std::vector<std::string> RoomRegistry::expired_room_ids(const std::string& now_utc) const {
  std::vector<std::string> room_ids;
  for (const auto& [room_id, session] : rooms_) {
    if (session.is_expired_at(now_utc)) {
      room_ids.push_back(room_id);
    }
  }
  std::sort(room_ids.begin(), room_ids.end());
  return room_ids;
}

bool RoomRegistry::remove_expired_rooms(const std::string& now_utc) {
  const auto expired = expired_room_ids(now_utc);
  for (const auto& room_id : expired) {
    rooms_.erase(room_id);
  }
  return !expired.empty();
}

std::optional<RoomSessionConfig> room_session_config_from_json(const nlohmann::json& input) {
  if (!input.contains("room_id") || !input.contains("game") || !input.contains("slot_id") ||
      !input.contains("slot_name")) {
    return std::nullopt;
  }
  RoomSessionConfig config;
  config.room_id = input.at("room_id").get<std::string>();
  config.room_type = input.contains("room_type") ? parse_room_type(input.at("room_type")) : RoomType::Live;
  config.game = input.at("game").get<std::string>();
  config.team_id = input.value("team_id", 0);
  config.slot_id = input.at("slot_id").get<int>();
  config.slot_name = input.at("slot_name").get<std::string>();
  config.slot_alias = input.value("slot_alias", config.slot_name);
  config.seed_name = optional_string(input, "seed_name");
  config.seed_id = optional_string(input, "seed_id");
  config.seed_hash = optional_string(input, "seed_hash");
  config.patch_url = optional_string(input, "patch_url");
  config.tracker_pack = optional_string(input, "tracker_pack");
  config.tracker_variant = optional_string(input, "tracker_variant");
  config.sni_required = input.value("sni_required", false);
  config.memory_backend_state = optional_string(input, "memory_backend_state");
  config.expires_at = optional_string(input, "expires_at");
  return config;
}

nlohmann::json handle_room_server_command(RoomRegistry& registry, const nlohmann::json& command) {
  const auto cmd = command.at("cmd").get<std::string>();
  if (cmd == "create_room") {
    const auto config = room_session_config_from_json(command);
    if (!config.has_value()) {
      return {{"ok", false}, {"error", "invalid_room_config"}};
    }
    return {{"ok", registry.create_room(*config)}};
  }
  if (cmd == "remove_room") {
    return {{"ok", registry.remove_room(command.at("room_id").get<std::string>())}};
  }
  if (cmd == "list_rooms") {
    const auto limit = command.contains("limit")
                           ? std::optional<std::size_t>(static_cast<std::size_t>(std::max(command.at("limit").get<int>(), 0)))
                           : std::nullopt;
    const auto offset =
        command.contains("offset") ? static_cast<std::size_t>(std::max(command.at("offset").get<int>(), 0)) : 0U;
    return {{"ok", true},
            {"limit", limit.has_value() ? nlohmann::json(*limit) : nlohmann::json(nullptr)},
            {"offset", offset},
            {"room_ids",
             registry.list_room_ids(
                 limit,
                 offset,
                 optional_string(command, "query"),
                 optional_string(command, "room_type"),
                 optional_string(command, "connection_state"))}};
  }
  if (cmd == "expired_rooms") {
    return {{"ok", true}, {"room_ids", registry.expired_room_ids(command.at("now_utc").get<std::string>())}};
  }
  if (cmd == "purge_expired_rooms") {
    return {{"ok", registry.remove_expired_rooms(command.at("now_utc").get<std::string>())}};
  }

  const auto room_id = command_room_id(command);
  if (!room_id.has_value()) {
    return {{"ok", false}, {"error", "missing_room_id"}, {"reason", "missing_room_id"}};
  }

  auto* room = registry.find_room(*room_id);
  if (room == nullptr) {
    return {{"ok", false}, {"error", "room_not_found"}, {"reason", "room_not_found"}};
  }

  if (cmd == "upsert_player") {
    room->upsert_player(parse_player(command.at("player")));
    return {{"ok", true}};
  }
  if (cmd == "connect_player") {
    room->connect_player(command.at("slot").get<int>());
    return {{"ok", true}};
  }
  if (cmd == "disconnect_player") {
    room->disconnect_player(command.at("slot").get<int>());
    return {{"ok", true}};
  }
  if (cmd == "mark_emu_connected") {
    room->mark_emu_connected(command.at("connected").get<bool>());
    return {{"ok", true}};
  }
  if (cmd == "mark_tracker_connected") {
    room->mark_tracker_connected(command.at("connected").get<bool>());
    return {{"ok", true}};
  }
  if (cmd == "record_check") {
    return {{"ok", room->record_check(command.at("location_id").get<std::int64_t>())}};
  }
  if (cmd == "set_missing_locations") {
    room->set_missing_locations(command.at("missing_locations").get<std::vector<std::int64_t>>());
    return {{"ok", true}};
  }
  if (cmd == "enqueue_received_item") {
    room->enqueue_received_item(parse_received_item(command.at("item")));
    return {{"ok", true}};
  }
  if (cmd == "set_seed_metadata") {
    room->set_seed_metadata(
        optional_string(command, "seed_name"),
        optional_string(command, "seed_id"),
        optional_string(command, "seed_hash"),
        optional_string(command, "tracker_pack"),
        optional_string(command, "tracker_variant"));
    return {{"ok", true}};
  }
  if (cmd == "set_stored_data") {
    room->set_stored_data(command.at("key").get<std::string>(), command.at("value"));
    return {{"ok", true}};
  }
  if (cmd == "set_slot_data") {
    room->set_slot_data(command.at("slot_data"));
    return {{"ok", true}};
  }
  if (cmd == "apply_linkedworld_surface") {
    room->apply_linkedworld_surface(command);
    return {{"ok", true}, {"linkedworld_surface", room->snapshot().linkedworld_surface}};
  }
  if (cmd == "apply_seed_contract") {
    std::string reason;
    const auto contract = seed_contract_from_command(command);
    if (!room->apply_seed_contract(contract, &reason)) {
      return {{"ok", false}, {"error", reason}, {"reason", reason}};
    }
    const auto snapshot = room->snapshot();
    return {{"ok", true},
            {"seed_id", snapshot.seed_id.has_value() ? nlohmann::json(*snapshot.seed_id) : nlohmann::json(nullptr)},
            {"slot_data", snapshot.slot_data},
            {"seed_contract", snapshot.seed_contract},
            {"seed_contract_summary", seed_contract_summary(snapshot.seed_contract)}};
  }
  if (cmd == "set_location_mapping") {
    room->set_location_mapping(
        command.at("location_id").get<std::int64_t>(),
        command.at("receiver_slot").get<int>(),
        command.at("item_id").get<int>(),
        command.at("location_name").get<std::string>());
    return {{"ok", true}};
  }
  if (cmd == "runtime_heartbeat") {
    room->heartbeat_runtime({
        .runtime_kind = optional_string(command, "runtime_kind"),
        .runtime_session_name = optional_string(command, "runtime_session_name"),
        .driver_instance_id = optional_string(command, "driver_instance_id"),
        .linkedworld_id = optional_string(command, "linkedworld_id"),
        .core_profile = optional_string(command, "core_profile"),
        .client_name = optional_string(command, "client_name"),
        .client_version = optional_string(command, "client_version"),
        .connected = command.contains("connected") ? std::optional<bool>(command.at("connected").get<bool>()) : std::nullopt,
    });
    return {{"ok", true}};
  }
  if (cmd == "runtime_disconnect") {
    room->disconnect_runtime(optional_string(command, "reason"));
    return {{"ok", true}};
  }
  if (cmd == "issue_ticket") {
    std::string reason;
    const auto session_token = room->issue_runtime_ticket(
        RuntimeTicketRequest{
            .session_name = command.at("session_name").get<std::string>(),
            .slot_id = command.at("slot_id").get<int>(),
            .client_kind = command.value("client_kind", "runtime"),
            .driver_instance_id = optional_string(command, "driver_instance_id"),
            .linkedworld_id = optional_string(command, "linkedworld_id"),
            .core_profile = optional_string(command, "core_profile"),
        },
        &reason);
    if (!session_token.has_value()) {
      return {{"ok", false}, {"error", reason}, {"reason", reason}};
    }
    const auto snapshot = room->snapshot();
    return {{"ok", true},
            {"session_token", *session_token},
            {"room_id", snapshot.room_id},
            {"slot_name", snapshot.slot_name},
            {"slot_alias", snapshot.slot_alias},
            {"seed_id", snapshot.seed_id.has_value() ? nlohmann::json(*snapshot.seed_id) : nlohmann::json(nullptr)},
            {"seed_hash", snapshot.seed_hash.has_value() ? nlohmann::json(*snapshot.seed_hash) : nlohmann::json(nullptr)},
            {"tracker_pack", snapshot.tracker_pack.has_value() ? nlohmann::json(*snapshot.tracker_pack) : nlohmann::json(nullptr)},
            {"tracker_variant", snapshot.tracker_variant.has_value() ? nlohmann::json(*snapshot.tracker_variant) : nlohmann::json(nullptr)},
            {"slot_data", snapshot.slot_data},
            {"seed_contract", snapshot.seed_contract},
            {"seed_contract_summary", seed_contract_summary(snapshot.seed_contract)},
            {"linkedworld_surface", snapshot.linkedworld_surface}};
  }
  if (cmd == "runtime_event") {
    std::string reason;
    const auto recorded = room->record_runtime_event(
        RuntimeEventRequest{
            .slot_id = command.at("slot_id").get<int>(),
            .session_token = command.at("session_token").get<std::string>(),
            .event_type = command.at("event_type").get<std::string>(),
            .canonical_id = command.at("canonical_id").get<std::int64_t>(),
            .driver_instance_id = optional_string(command, "driver_instance_id"),
            .linkedworld_id = optional_string(command, "linkedworld_id"),
            .core_profile = optional_string(command, "core_profile"),
        },
        &reason);
    if (!recorded.has_value()) {
      return {{"ok", false}, {"error", reason}, {"reason", reason}};
    }
    return {{"ok", true}, {"recorded", *recorded}};
  }
  if (cmd == "pending_items") {
    std::string reason;
    const auto pending = room->pending_deliveries(
        command.at("slot_id").get<int>(),
        command.at("session_token").get<std::string>(),
        &reason);
    if (!pending.has_value()) {
      return {{"ok", false}, {"error", reason}, {"reason", reason}};
    }
    nlohmann::json items = nlohmann::json::array();
    for (const auto& item : *pending) {
      items.push_back({
          {"delivery_id", item.delivery_id},
          {"item_id", item.item_id},
          {"item_name", item.item_name},
          {"location_id", item.location_id},
          {"sender_slot", item.sender_slot},
          {"sender_alias", item.sender_alias},
          {"flags", item.flags},
          {"event_key", item.event_key.has_value() ? nlohmann::json(*item.event_key) : nlohmann::json(nullptr)},
          {"mapped_value", item.mapped_value.has_value() ? nlohmann::json(*item.mapped_value) : nlohmann::json(nullptr)},
          {"tracker_semantic_id", item.tracker_semantic_id.has_value()
                                      ? nlohmann::json(*item.tracker_semantic_id)
                                      : nlohmann::json(nullptr)},
      });
    }
    return {{"ok", true}, {"pending_items", items}};
  }
  if (cmd == "acknowledge_delivery") {
    std::string reason;
    const auto acknowledged = room->acknowledge_delivery(
        command.at("slot_id").get<int>(),
        command.at("delivery_id").get<std::int64_t>(),
        command.at("session_token").get<std::string>(),
        &reason);
    if (!acknowledged.has_value()) {
      return {{"ok", false}, {"error", reason}, {"reason", reason}};
    }
    if (!*acknowledged && !reason.empty()) {
      return {{"ok", false}, {"error", reason}, {"reason", reason}};
    }
    return {{"ok", true}, {"acknowledged", *acknowledged}};
  }
  if (cmd == "set_allowed_players") {
    room->set_allowed_players(command.at("allowed_players").get<std::vector<int>>());
    return {{"ok", true}};
  }
  if (cmd == "set_expires_at") {
    room->set_expires_at(optional_string(command, "expires_at"));
    return {{"ok", true}};
  }
  if (cmd == "set_daily_summary_state") {
    room->set_daily_summary_state(optional_string(command, "state"));
    return {{"ok", true}};
  }
  if (cmd == "set_async_notification_state") {
    room->set_async_notification_state(optional_string(command, "state"));
    return {{"ok", true}};
  }
  if (cmd == "set_suspend_state") {
    room->set_suspend_state(optional_string(command, "state"));
    return {{"ok", true}};
  }
  if (cmd == "set_milestones") {
    room->set_milestones(command.at("milestones").get<std::vector<std::string>>());
    return {{"ok", true}};
  }
  if (cmd == "set_notifications") {
    room->set_notifications(command.at("notifications").get<std::vector<std::string>>());
    return {{"ok", true}};
  }
  if (cmd == "ingest_client_report") {
    auto report = parse_client_report(command.at("report"));
    if (!report.room_id.has_value()) {
      report.room_id = command.at("room_id").get<std::string>();
    }
    room->ingest_client_report(std::move(report));
    return {{"ok", true}};
  }
  if (cmd == "snapshot_room") {
    return {{"ok", true}, {"snapshot", to_json(room->snapshot())}};
  }
  if (cmd == "sync_room") {
    const auto from_item_index = command.value("from_item_index", 0);
    return {{"ok", true}, {"sync", room->sync_payload(from_item_index)}};
  }
  if (cmd == "room_summary") {
    const auto summary = room->activity_summary();
    const auto snapshot = room->snapshot();
    return {
        {"ok", true},
        {"summary",
         {
             {"room_id", snapshot.room_id},
             {"game", snapshot.game},
             {"slot_id", snapshot.slot_id},
             {"slot_name", snapshot.slot_name},
             {"slot_alias", snapshot.slot_alias},
             {"seed_name", snapshot.seed_name.has_value() ? nlohmann::json(*snapshot.seed_name) : nlohmann::json(nullptr)},
             {"seed_id", snapshot.seed_id.has_value() ? nlohmann::json(*snapshot.seed_id) : nlohmann::json(nullptr)},
             {"seed_hash", snapshot.seed_hash.has_value() ? nlohmann::json(*snapshot.seed_hash) : nlohmann::json(nullptr)},
             {"tracker_pack", snapshot.tracker_pack.has_value() ? nlohmann::json(*snapshot.tracker_pack) : nlohmann::json(nullptr)},
             {"tracker_variant", snapshot.tracker_variant.has_value() ? nlohmann::json(*snapshot.tracker_variant) : nlohmann::json(nullptr)},
             {"linkedworld_id",
              !json_string_or_empty(snapshot.linkedworld_surface, "linkedworld_id").empty()
                  ? nlohmann::json(json_string_or_empty(snapshot.linkedworld_surface, "linkedworld_id"))
                  : snapshot.runtime_state.has_value() && snapshot.runtime_state->linkedworld_id.has_value()
                        ? nlohmann::json(*snapshot.runtime_state->linkedworld_id)
                        : nlohmann::json(nullptr)},
             {"linkedworld_surface", snapshot.linkedworld_surface},
             {"seed_contract_summary", seed_contract_summary(snapshot.seed_contract)},
             {"player_connections", summary.player_connections},
             {"player_disconnections", summary.player_disconnections},
             {"checks_recorded", summary.checks_recorded},
             {"items_received", summary.items_received},
             {"tracker_connections", summary.tracker_connections},
             {"emu_connections", summary.emu_connections},
             {"checked_count", snapshot.checked_locations.size()},
             {"missing_count", snapshot.missing_locations.size()},
             {"received_item_count", snapshot.received_items.size()},
             {"next_received_item_index", room->next_received_item_index()},
             {"pending_delivery_count", room->pending_delivery_count()},
             {"acknowledged_delivery_count",
              snapshot.runtime_state.has_value()
                  ? nlohmann::json(snapshot.runtime_state->acknowledged_delivery_ids.size())
                  : nlohmann::json(0)},
             {"runtime_connected", snapshot.runtime_state.has_value() ? nlohmann::json(snapshot.runtime_state->connected) : nlohmann::json(false)},
             {"runtime_ticket_issued",
              snapshot.runtime_state.has_value() && snapshot.runtime_state->session_token_hash.has_value()
                  ? nlohmann::json(true)
                  : nlohmann::json(false)},
             {"last_runtime_heartbeat",
              snapshot.runtime_state.has_value() && snapshot.runtime_state->last_heartbeat_at.has_value()
                  ? nlohmann::json(*snapshot.runtime_state->last_heartbeat_at)
                  : nlohmann::json(nullptr)},
             {"last_delivery_ack_at",
              snapshot.runtime_state.has_value() && snapshot.runtime_state->last_delivery_ack_at.has_value()
                  ? nlohmann::json(*snapshot.runtime_state->last_delivery_ack_at)
                  : nlohmann::json(nullptr)},
             {"last_delivery_ack_id",
              snapshot.runtime_state.has_value() && snapshot.runtime_state->last_delivery_ack_id.has_value()
                  ? nlohmann::json(*snapshot.runtime_state->last_delivery_ack_id)
                  : nlohmann::json(nullptr)},
             {"driver_instance_id",
              snapshot.runtime_state.has_value() && snapshot.runtime_state->driver_instance_id.has_value()
                  ? nlohmann::json(*snapshot.runtime_state->driver_instance_id)
                  : nlohmann::json(nullptr)},
             {"core_profile",
              snapshot.runtime_state.has_value() && snapshot.runtime_state->core_profile.has_value()
                  ? nlohmann::json(*snapshot.runtime_state->core_profile)
                  : nlohmann::json(nullptr)},
         }},
    };
  }
  if (cmd == "room_events") {
    const auto& room_events = room->events();
    const auto requested_event_type = normalized_filter_string(command, "event_type");
    const auto requested_severity = normalized_filter_string(command, "severity");
    const auto requested_source = normalized_filter_string(command, "source");
    std::vector<nlohmann::json> matched_events;
    matched_events.reserve(room_events.size());
    for (std::size_t index = 0; index < room_events.size(); ++index) {
      const auto& event = room_events[index];
      if (requested_event_type.has_value() && lower(event.event_type) != *requested_event_type) {
        continue;
      }
      const auto payload_severity = event.payload.contains("severity") && event.payload.at("severity").is_string()
                                        ? std::optional<std::string>(lower(event.payload.at("severity").get<std::string>()))
                                        : std::nullopt;
      const auto payload_source = event.payload.contains("source") && event.payload.at("source").is_string()
                                      ? std::optional<std::string>(lower(event.payload.at("source").get<std::string>()))
                                      : std::nullopt;
      if (requested_severity.has_value() && payload_severity != requested_severity) {
        continue;
      }
      if (requested_source.has_value() && payload_source != requested_source) {
        continue;
      }
      matched_events.push_back({
          {"event_type", event.event_type},
          {"timestamp", event.timestamp},
          {"source", payload_source.has_value() ? nlohmann::json(*payload_source) : nlohmann::json(nullptr)},
          {"severity", payload_severity.has_value() ? nlohmann::json(*payload_severity) : nlohmann::json(nullptr)},
          {"payload", event.payload},
      });
    }
    const auto offset = bounded_offset(command);
    const auto end = offset >= matched_events.size() ? std::size_t(0) : matched_events.size() - offset;
    const auto start = bounded_tail_start(end, command);
    nlohmann::json events = nlohmann::json::array();
    for (std::size_t index = start; index < end; ++index) {
      events.push_back(matched_events[index]);
    }
    return {{"ok", true}, {"events", events}};
  }
  if (cmd == "client_reports") {
    const auto& client_reports = room->client_reports();
    const auto requested_report_type = normalized_filter_string(command, "report_type");
    const auto requested_severity = normalized_filter_string(command, "severity");
    const auto requested_source = normalized_filter_string(command, "source");
    std::vector<nlohmann::json> matched_reports;
    matched_reports.reserve(client_reports.size());
    for (std::size_t index = 0; index < client_reports.size(); ++index) {
      const auto& report = client_reports[index];
      if (requested_report_type.has_value() && lower(report.report_type) != *requested_report_type) {
        continue;
      }
      if (requested_severity.has_value() && lower(report.severity) != *requested_severity) {
        continue;
      }
      if (requested_source.has_value() && lower(report.source) != *requested_source) {
        continue;
      }
      matched_reports.push_back(to_json(report));
    }
    const auto offset = bounded_offset(command);
    const auto end = offset >= matched_reports.size() ? std::size_t(0) : matched_reports.size() - offset;
    const auto start = bounded_tail_start(end, command);
    nlohmann::json reports = nlohmann::json::array();
    for (std::size_t index = start; index < end; ++index) {
      reports.push_back(matched_reports[index]);
    }
    return {{"ok", true}, {"reports", reports}};
  }

  return {{"ok", false}, {"error", "unknown_command"}};
}

nlohmann::json handle_room_server_command_with_audit(
    RoomRegistry& registry,
    const nlohmann::json& command,
    RoomAuditStore* audit_store) {
  const auto response = handle_room_server_command(registry, command);
  if (audit_store == nullptr) {
    return response;
  }

  if (!command.contains("room_id")) {
    return response;
  }

  const auto room_id = command.at("room_id").get<std::string>();
  const auto* room = registry.find_room(room_id);
  if (room == nullptr) {
    return response;
  }

  const auto cmd = command.value("cmd", "");
  if (cmd == "snapshot_room") {
    audit_store->append_snapshot(room->snapshot());
    return response;
  }
  if (cmd == "room_events" || cmd == "client_reports" || cmd == "room_summary") {
    return response;
  }

  const auto& events = room->events();
  if (!events.empty()) {
    audit_store->append_event(room_id, events.back());
  }
  if (cmd == "ingest_client_report") {
    const auto& reports = room->client_reports();
    if (!reports.empty()) {
      audit_store->append_client_report(room_id, reports.back());
    }
  }
  if (cmd == "create_room") {
    audit_store->append_snapshot(room->snapshot());
  }
  return response;
}

}  // namespace sekailink_server
