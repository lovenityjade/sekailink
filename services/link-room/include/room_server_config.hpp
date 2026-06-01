#pragma once

#include "sekailink_server/room_server_node.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace sekailink_server {

struct RoomServerServiceState {
  bool ok = true;
  std::string status = "running";
  std::optional<std::string> started_at;
  std::string updated_at;
  std::string effective_tcp_host = "127.0.0.1";
  std::uint16_t effective_tcp_port = 0;
  std::string effective_http_host = "127.0.0.1";
  std::uint16_t effective_http_port = 0;
  std::uint64_t boot_room_count = 0;
  std::uint64_t room_count = 0;
  bool stop_requested = false;
  bool loopback_only = true;
  bool admin_auth_enabled = false;
  bool runtime_auth_enabled = false;
  bool client_report_auth_enabled = false;
  std::optional<std::string> last_error;
};

[[nodiscard]] RoomServerNodeConfig load_room_server_node_config(const std::filesystem::path& path);
[[nodiscard]] nlohmann::json to_json(const RoomServerNodeConfig& config);
void write_room_server_state_file(
    const std::filesystem::path& path,
    const RoomServerNodeConfig& requested_config,
    const RoomServerServiceState& state);

}  // namespace sekailink_server
