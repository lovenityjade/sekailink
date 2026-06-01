#include "sekailink_server/game_server_protocol.hpp"
#include "sekailink_server/game_room_projection.hpp"
#include "sekailink_server/room_state.hpp"

#include <algorithm>
#include <stdexcept>

namespace sekailink_server {

namespace {

GameProtocolChannel parse_channel(const nlohmann::json& value) {
  const auto channel = value.get<std::string>();
  if (channel == "admin") {
    return GameProtocolChannel::Admin;
  }
  if (channel == "core") {
    return GameProtocolChannel::Core;
  }
  if (channel == "runtime") {
    return GameProtocolChannel::Runtime;
  }
  throw std::runtime_error("invalid_game_protocol_channel");
}

GameClientKind parse_client_kind(const std::string& value) {
  if (value == "core") {
    return GameClientKind::Core;
  }
  if (value == "runtime") {
    return GameClientKind::Runtime;
  }
  if (value == "observer") {
    return GameClientKind::Observer;
  }
  if (value == "admin") {
    return GameClientKind::Admin;
  }
  throw std::runtime_error("invalid_game_client_kind");
}

std::string require_session_name(const nlohmann::json& command) {
  const auto session_name = command.at("session_name").get<std::string>();
  if (session_name.empty() || session_name.size() > 128) {
    throw std::runtime_error("invalid_session_name");
  }
  return session_name;
}

std::string require_string_field(const nlohmann::json& command, const char* key, std::size_t max_size = 256) {
  const auto value = command.at(key).get<std::string>();
  if (value.empty() || value.size() > max_size) {
    throw std::runtime_error(std::string("invalid_") + key);
  }
  return value;
}

std::filesystem::path require_path_field(const nlohmann::json& command, const char* key) {
  return std::filesystem::path(require_string_field(command, key, 4096));
}

std::optional<std::string> optional_string_field(const nlohmann::json& command, const char* key, std::size_t max_size = 256) {
  if (!command.contains(key) || command.at(key).is_null()) {
    return std::nullopt;
  }
  const auto value = command.at(key).get<std::string>();
  if (value.empty() || value.size() > max_size) {
    throw std::runtime_error(std::string("invalid_") + key);
  }
  return value;
}

}  // namespace

bool GameServerAuthPolicy::requires_auth(GameProtocolChannel channel) const {
  switch (channel) {
    case GameProtocolChannel::Admin:
      return admin_token.has_value();
    case GameProtocolChannel::Core:
      return core_token.has_value();
    case GameProtocolChannel::Runtime:
      return runtime_token.has_value();
  }
  return false;
}

bool GameServerAuthPolicy::token_valid(GameProtocolChannel channel,
                                       const std::optional<std::string>& presented_token) const {
  const auto token_matches = [&presented_token](const std::optional<std::string>& expected) {
    if (!expected.has_value()) {
      return true;
    }
    return presented_token.has_value() && *presented_token == *expected;
  };
  switch (channel) {
    case GameProtocolChannel::Admin:
      return token_matches(admin_token);
    case GameProtocolChannel::Core:
      return token_matches(core_token);
    case GameProtocolChannel::Runtime:
      return token_matches(runtime_token);
  }
  return false;
}

bool GameSessionRegistry::insert_session(std::string session_name, GameSession session, std::string* error) {
  if (session_name.empty()) {
    if (error) *error = "game_registry_missing_session_name";
    return false;
  }
  if (sessions_.contains(session_name)) {
    if (error) *error = "game_registry_session_exists";
    return false;
  }
  sessions_.emplace(std::move(session_name), std::move(session));
  return true;
}

bool GameSessionRegistry::create_session_from_world_package(const std::string& session_name,
                                                            const std::filesystem::path& world_package_path,
                                                            std::string* error) {
  auto package = load_game_world_package(world_package_path, error);
  if (!package.has_value()) {
    return false;
  }
  return insert_session(session_name, GameSession(std::move(*package)), error);
}

bool GameSessionRegistry::create_session_from_archipelago_import(const std::string& session_name,
                                                                 const nlohmann::json& multiserver_state,
                                                                 const ArchipelagoWorldImportOptions& options,
                                                                 std::string* error) {
  try {
    return insert_session(session_name, GameSession(import_archipelago_world_package(multiserver_state, options)), error);
  } catch (const std::exception& exception) {
    if (error) *error = exception.what();
    return false;
  }
}

bool GameSessionRegistry::restore_session_from_paths(const std::string& session_name,
                                                     const std::filesystem::path& world_package_path,
                                                     const std::filesystem::path& save_state_path,
                                                     std::string* error) {
  auto package = load_game_world_package(world_package_path, error);
  if (!package.has_value()) {
    return false;
  }
  auto save_state = load_game_save_state(save_state_path, error);
  if (!save_state.has_value()) {
    return false;
  }
  auto session = GameSession::from_save(std::move(*package), *save_state, error);
  if (!session.has_value()) {
    return false;
  }
  return insert_session(session_name, std::move(*session), error);
}

bool GameSessionRegistry::save_session_state(const std::string& session_name,
                                             const std::filesystem::path& save_state_path,
                                             std::string* error) const {
  const auto* session = find_session(session_name);
  if (session == nullptr) {
    if (error) *error = "game_registry_unknown_session";
    return false;
  }
  return save_game_save_state(session->export_save_state(), save_state_path, error);
}

bool GameSessionRegistry::remove_session(const std::string& session_name) {
  return sessions_.erase(session_name) > 0;
}

bool GameSessionRegistry::has_session(const std::string& session_name) const {
  return sessions_.contains(session_name);
}

GameSession* GameSessionRegistry::find_session(const std::string& session_name) {
  const auto it = sessions_.find(session_name);
  return it == sessions_.end() ? nullptr : &it->second;
}

const GameSession* GameSessionRegistry::find_session(const std::string& session_name) const {
  const auto it = sessions_.find(session_name);
  return it == sessions_.end() ? nullptr : &it->second;
}

std::vector<std::string> GameSessionRegistry::list_session_names() const {
  std::vector<std::string> names;
  names.reserve(sessions_.size());
  for (const auto& [name, _] : sessions_) {
    names.push_back(name);
  }
  std::sort(names.begin(), names.end());
  return names;
}

GameServerProtocolService::GameServerProtocolService(GameSessionRegistry& registry,
                                                     const GameServerAuthPolicy* auth_policy)
    : registry_(registry), auth_policy_(auth_policy) {}

nlohmann::json GameServerProtocolService::handle(const GameProtocolEnvelope& envelope) const {
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

  nlohmann::json response;
  try {
    switch (envelope.channel) {
      case GameProtocolChannel::Admin:
        response = handle_admin_command(envelope.command);
        break;
      case GameProtocolChannel::Core:
        response = handle_core_command(envelope.command);
        break;
      case GameProtocolChannel::Runtime:
        response = handle_runtime_command(envelope.command);
        break;
    }
  } catch (const std::exception& exception) {
    response = {
        {"ok", false},
        {"error", exception.what()},
    };
  }
  response["channel"] = channel_to_string(envelope.channel);
  return response;
}

std::string GameServerProtocolService::channel_to_string(GameProtocolChannel channel) {
  switch (channel) {
    case GameProtocolChannel::Admin:
      return "admin";
    case GameProtocolChannel::Core:
      return "core";
    case GameProtocolChannel::Runtime:
      return "runtime";
  }
  throw std::runtime_error("invalid_game_protocol_channel");
}

bool GameServerProtocolService::command_allowed(GameProtocolChannel channel, const std::string& cmd) {
  static const std::unordered_set<std::string> kAdminAllowed = {
      "create_session_from_world_package",
      "create_session_from_ap_import",
      "restore_session_from_paths",
      "save_session_state",
      "remove_session",
      "list_sessions",
      "issue_ticket",
      "session_summary",
      "project_session_rooms",
  };
  static const std::unordered_set<std::string> kCoreAllowed = {
      "issue_ticket",
      "pending_items",
      "acknowledge_delivery",
      "session_summary",
      "project_session_rooms",
  };
  static const std::unordered_set<std::string> kRuntimeAllowed = {
      "runtime_event",
      "pending_items",
      "acknowledge_delivery",
  };

  switch (channel) {
    case GameProtocolChannel::Admin:
      return kAdminAllowed.contains(cmd);
    case GameProtocolChannel::Core:
      return kCoreAllowed.contains(cmd);
    case GameProtocolChannel::Runtime:
      return kRuntimeAllowed.contains(cmd);
  }
  return false;
}

GameProtocolEnvelope GameServerProtocolService::parse(const nlohmann::json& envelope) {
  return game_protocol_envelope_from_json(envelope);
}

nlohmann::json GameServerProtocolService::handle_admin_command(const nlohmann::json& command) const {
  const auto cmd = command.at("cmd").get<std::string>();
  if (cmd == "create_session_from_world_package") {
    const auto session_name = require_session_name(command);
    const auto world_package_path = require_path_field(command, "world_package_path");
    std::string error;
    const auto ok = registry_.create_session_from_world_package(session_name, world_package_path, &error);
    return {
        {"ok", ok},
        {"error", ok ? "" : error},
        {"session_name", session_name},
    };
  }
  if (cmd == "create_session_from_ap_import") {
    const auto session_name = require_session_name(command);
    ArchipelagoWorldImportOptions options{
        .world_id = require_string_field(command, "world_id"),
        .world_version = command.value("world_version", std::string("1.0")),
        .seed_id = require_string_field(command, "seed_id"),
        .seed_hash = command.value("seed_hash", std::string()),
        .linkedworld_id = require_string_field(command, "linkedworld_id"),
    };
    std::string error;
    const auto ok = registry_.create_session_from_archipelago_import(
        session_name, command.at("archipelago"), options, &error);
    return {
        {"ok", ok},
        {"error", ok ? "" : error},
        {"session_name", session_name},
    };
  }
  if (cmd == "restore_session_from_paths") {
    const auto session_name = require_session_name(command);
    const auto world_package_path = require_path_field(command, "world_package_path");
    const auto save_state_path = require_path_field(command, "save_state_path");
    std::string error;
    const auto ok = registry_.restore_session_from_paths(
        session_name, world_package_path, save_state_path, &error);
    return {
        {"ok", ok},
        {"error", ok ? "" : error},
        {"session_name", session_name},
    };
  }
  if (cmd == "save_session_state") {
    const auto session_name = require_session_name(command);
    const auto save_state_path = require_path_field(command, "save_state_path");
    std::string error;
    const auto ok = registry_.save_session_state(session_name, save_state_path, &error);
    return {
        {"ok", ok},
        {"error", ok ? "" : error},
        {"session_name", session_name},
    };
  }
  if (cmd == "remove_session") {
    const auto session_name = require_session_name(command);
    return {
        {"ok", registry_.remove_session(session_name)},
        {"session_name", session_name},
    };
  }
  if (cmd == "list_sessions") {
    return {
        {"ok", true},
        {"sessions", registry_.list_session_names()},
    };
  }
  if (cmd == "issue_ticket" || cmd == "session_summary") {
    return handle_core_command(command);
  }
  if (cmd == "project_session_rooms") {
    return handle_core_command(command);
  }
  throw std::runtime_error("unsupported_admin_command");
}

nlohmann::json GameServerProtocolService::handle_core_command(const nlohmann::json& command) const {
  const auto cmd = command.at("cmd").get<std::string>();
  const auto session_name = require_session_name(command);
  auto* session = registry_.find_session(session_name);
  if (session == nullptr) {
    return {
        {"ok", false},
        {"error", "game_registry_unknown_session"},
        {"session_name", session_name},
    };
  }

  if (cmd == "issue_ticket") {
    std::string error;
    const auto maybe_ticket = session->issue_session_ticket(
        command.at("slot_id").get<int>(),
        parse_client_kind(command.at("client_kind").get<std::string>()),
        optional_string_field(command, "driver_instance_id"),
        optional_string_field(command, "linkedworld_id"),
        optional_string_field(command, "core_profile"),
        &error);
    return {
        {"ok", maybe_ticket.has_value()},
        {"error", maybe_ticket.has_value() ? "" : error},
        {"session_name", session_name},
        {"ticket", maybe_ticket.has_value() ? ticket_to_json(*maybe_ticket) : nlohmann::json(nullptr)},
    };
  }
  if (cmd == "pending_items") {
    const auto slot_id = command.at("slot_id").get<int>();
    const auto session_token = require_string_field(command, "session_token");
    nlohmann::json pending = nlohmann::json::array();
    for (const auto& record : session->pending_items_for_slot(slot_id, session_token)) {
      pending.push_back(to_json(record));
    }
    return {
        {"ok", true},
        {"session_name", session_name},
        {"slot_id", slot_id},
        {"pending_items", pending},
    };
  }
  if (cmd == "acknowledge_delivery") {
    const auto slot_id = command.at("slot_id").get<int>();
    const auto delivery_id = command.at("delivery_id").get<int>();
    const auto session_token = require_string_field(command, "session_token");
    return {
        {"ok", session->acknowledge_delivery(slot_id, delivery_id, session_token)},
        {"session_name", session_name},
        {"slot_id", slot_id},
        {"delivery_id", delivery_id},
    };
  }
  if (cmd == "session_summary") {
    return session_summary_json(session_name, *session);
  }
  if (cmd == "project_session_rooms") {
    return projected_rooms_to_json(session_name, *session);
  }
  throw std::runtime_error("unsupported_core_command");
}

nlohmann::json GameServerProtocolService::handle_runtime_command(const nlohmann::json& command) const {
  const auto cmd = command.at("cmd").get<std::string>();
  if (cmd == "pending_items" || cmd == "acknowledge_delivery") {
    return handle_core_command(command);
  }
  if (cmd != "runtime_event") {
    throw std::runtime_error("unsupported_runtime_command");
  }

  const auto session_name = require_session_name(command);
  auto* session = registry_.find_session(session_name);
  if (session == nullptr) {
    return {
        {"ok", false},
        {"error", "game_registry_unknown_session"},
        {"session_name", session_name},
    };
  }

  const RuntimeEvent event{
      .session_token = require_string_field(command, "session_token"),
      .slot_id = command.at("slot_id").get<int>(),
      .driver_instance_id = require_string_field(command, "driver_instance_id"),
      .linkedworld_id = require_string_field(command, "linkedworld_id"),
      .core_profile = require_string_field(command, "core_profile"),
      .event_type = require_string_field(command, "event_type"),
      .canonical_id = command.at("canonical_id").get<std::int64_t>(),
  };
  const auto result = session->apply_runtime_event(event);
  return {
      {"ok", result.accepted},
      {"accepted", result.accepted},
      {"duplicate", result.duplicate},
      {"reason", result.reason},
      {"created_delivery_ids", result.created_delivery_ids},
      {"session_name", session_name},
  };
}

nlohmann::json GameServerProtocolService::session_summary_json(const std::string& session_name,
                                                               const GameSession& session) const {
  const auto save_state = session.export_save_state();
  std::size_t checked_count = 0;
  for (const auto& [_, locations] : save_state.checked_locations_by_slot) {
    checked_count += locations.size();
  }
  std::size_t acknowledged_count = 0;
  for (const auto& record : save_state.delivered_items) {
    if (record.acknowledged) {
      ++acknowledged_count;
    }
  }
  return {
      {"ok", true},
      {"session_name", session_name},
      {"world",
       {
           {"world_id", session.package().world_id},
           {"world_version", session.package().world_version},
           {"seed_id", session.package().seed_id},
           {"linkedworld_id", session.package().linkedworld_id},
           {"slot_count", session.package().slots.size()},
           {"location_count", session.package().locations.size()},
       }},
      {"progress",
       {
           {"checked_count", checked_count},
           {"delivered_count", save_state.delivered_items.size()},
           {"acknowledged_count", acknowledged_count},
           {"pending_count", save_state.delivered_items.size() - acknowledged_count},
       }},
  };
}

nlohmann::json GameServerProtocolService::ticket_to_json(const SessionTicket& ticket) {
  nlohmann::json result = {
      {"session_id", ticket.session_id},
      {"session_token", ticket.session_token},
      {"slot_id", ticket.slot_id},
      {"client_kind", game_client_kind_to_string(ticket.client_kind)},
      {"driver_instance_id", ticket.driver_instance_id.has_value() ? nlohmann::json(*ticket.driver_instance_id) : nlohmann::json(nullptr)},
      {"linkedworld_id", ticket.linkedworld_id.has_value() ? nlohmann::json(*ticket.linkedworld_id) : nlohmann::json(nullptr)},
      {"core_profile", ticket.core_profile.has_value() ? nlohmann::json(*ticket.core_profile) : nlohmann::json(nullptr)},
  };
  return result;
}

nlohmann::json GameServerProtocolService::projected_rooms_to_json(const std::string& session_name,
                                                                  const GameSession& session) {
  nlohmann::json rooms = nlohmann::json::array();
  for (auto& [room_id, room] : project_game_session_to_rooms(session_name, session)) {
    rooms.push_back({
        {"room_id", room_id},
        {"snapshot", to_json(room.snapshot())},
    });
  }
  return {
      {"ok", true},
      {"session_name", session_name},
      {"rooms", std::move(rooms)},
  };
}

GameProtocolEnvelope game_protocol_envelope_from_json(const nlohmann::json& envelope) {
  if (!envelope.contains("channel") || !envelope.contains("command")) {
    throw std::runtime_error("invalid_game_protocol_envelope");
  }
  GameProtocolEnvelope parsed;
  parsed.channel = parse_channel(envelope.at("channel"));
  if (envelope.contains("auth_token") && !envelope.at("auth_token").is_null()) {
    parsed.auth_token = envelope.at("auth_token").get<std::string>();
  }
  parsed.command = envelope.at("command");
  return parsed;
}

nlohmann::json handle_game_protocol_json(GameSessionRegistry& registry,
                                         const GameServerAuthPolicy* auth_policy,
                                         const nlohmann::json& envelope_json) {
  const auto envelope = game_protocol_envelope_from_json(envelope_json);
  GameServerProtocolService service(registry, auth_policy);
  return service.handle(envelope);
}

}  // namespace sekailink_server
