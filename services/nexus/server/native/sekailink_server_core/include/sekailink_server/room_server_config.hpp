#pragma once

#include "sekailink_server/room_server_node.hpp"

#include <filesystem>

namespace sekailink_server {

[[nodiscard]] RoomServerNodeConfig load_room_server_node_config(const std::filesystem::path& path);
[[nodiscard]] nlohmann::json to_json(const RoomServerNodeConfig& config);
void write_room_server_state_file(
    const std::filesystem::path& path,
    const RoomServerNodeConfig& requested_config,
    std::uint16_t effective_tcp_port,
    std::uint16_t effective_http_port);

}  // namespace sekailink_server
