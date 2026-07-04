#include "sekailink_server/room_server_config.hpp"
#include "sekailink_server/room_state.hpp"

#ifndef _WIN32
#include <unistd.h>
#endif

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace sekailink_server {

namespace {

ProjectionBackend parse_projection_backend_json(const nlohmann::json& json) {
  if (json.is_string()) {
    return parse_projection_backend(json.get<std::string>());
  }
  throw std::runtime_error("invalid_projection_backend");
}

void maybe_assign_string(const nlohmann::json& json, const char* key, std::optional<std::string>& target) {
  if (json.contains(key) && !json.at(key).is_null()) {
    target = json.at(key).get<std::string>();
  }
}

}  // namespace

RoomServerNodeConfig load_room_server_node_config(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("room_server_config_open_failed");
  }

  nlohmann::json json;
  stream >> json;

  RoomServerNodeConfig config;
  if (json.contains("tcp_port")) {
    config.tcp_port = json.at("tcp_port").get<std::uint16_t>();
  }
  if (json.contains("http_port")) {
    config.http_port = json.at("http_port").get<std::uint16_t>();
  }
  if (json.contains("audit_root") && !json.at("audit_root").is_null()) {
    config.audit_root = json.at("audit_root").get<std::string>();
  }
  if (json.contains("projection_root") && !json.at("projection_root").is_null()) {
    config.projection_root = json.at("projection_root").get<std::string>();
  }
  if (json.contains("projection_backend")) {
    config.projection_backend = parse_projection_backend_json(json.at("projection_backend"));
  }
  if (json.contains("restore_from_audit")) {
    config.restore_from_audit = json.at("restore_from_audit").get<bool>();
  }
  if (json.contains("restore_from_projection")) {
    config.restore_from_projection = json.at("restore_from_projection").get<bool>();
  }
  if (json.contains("purge_expired_periodically")) {
    config.purge_expired_periodically = json.at("purge_expired_periodically").get<bool>();
  }
  if (json.contains("purge_interval_ms")) {
    config.purge_interval_ms = json.at("purge_interval_ms").get<std::uint32_t>();
  }
  if (json.contains("auth_policy") && json.at("auth_policy").is_object()) {
    const auto& auth = json.at("auth_policy");
    maybe_assign_string(auth, "admin_token", config.auth_policy.admin_token);
    maybe_assign_string(auth, "runtime_token", config.auth_policy.runtime_token);
    maybe_assign_string(auth, "client_report_token", config.auth_policy.client_report_token);
  }

  return config;
}

nlohmann::json to_json(const RoomServerNodeConfig& config) {
  nlohmann::json auth = nlohmann::json::object();
  if (config.auth_policy.admin_token.has_value()) {
    auth["admin_token"] = "<redacted>";
  }
  if (config.auth_policy.runtime_token.has_value()) {
    auth["runtime_token"] = "<redacted>";
  }
  if (config.auth_policy.client_report_token.has_value()) {
    auth["client_report_token"] = "<redacted>";
  }

  return {
      {"tcp_port", config.tcp_port},
      {"http_port", config.http_port},
      {"audit_root", config.audit_root.empty() ? nlohmann::json(nullptr) : nlohmann::json(config.audit_root.string())},
      {"projection_root", config.projection_root.empty() ? nlohmann::json(nullptr) : nlohmann::json(config.projection_root.string())},
      {"projection_backend", projection_backend_name(config.projection_backend)},
      {"restore_from_audit", config.restore_from_audit},
      {"restore_from_projection", config.restore_from_projection},
      {"purge_expired_periodically", config.purge_expired_periodically},
      {"purge_interval_ms", config.purge_interval_ms},
      {"auth_policy", std::move(auth)},
  };
}

void write_room_server_state_file(
    const std::filesystem::path& path,
    const RoomServerNodeConfig& requested_config,
    const RoomServerServiceState& state) {
  nlohmann::json json = {
      {"ok", state.ok},
      {"service", "sekailink_room_server"},
      {"status", state.status},
#ifdef _WIN32
      {"pid", 0},
#else
      {"pid", static_cast<std::uint64_t>(::getpid())},
#endif
      {"started_at", state.started_at.has_value() ? nlohmann::json(*state.started_at) : nlohmann::json(nullptr)},
      {"updated_at", state.updated_at.empty() ? nlohmann::json(utc_timestamp_now()) : nlohmann::json(state.updated_at)},
      {"effective_tcp_host", state.effective_tcp_host},
      {"effective_tcp_port", state.effective_tcp_port},
      {"effective_http_host", state.effective_http_host},
      {"effective_http_port", state.effective_http_port},
      {"boot_room_count", state.boot_room_count},
      {"room_count", state.room_count},
      {"stop_requested", state.stop_requested},
      {"loopback_only", state.loopback_only},
      {"admin_auth_enabled", state.admin_auth_enabled},
      {"runtime_auth_enabled", state.runtime_auth_enabled},
      {"client_report_auth_enabled", state.client_report_auth_enabled},
      {"last_error", state.last_error.has_value() ? nlohmann::json(*state.last_error) : nlohmann::json(nullptr)},
      {"requested_config", to_json(requested_config)},
  };

  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  std::ofstream stream(path);
  if (!stream) {
    throw std::runtime_error("room_server_state_file_open_failed");
  }
  stream << json.dump(2) << '\n';
}

}  // namespace sekailink_server
