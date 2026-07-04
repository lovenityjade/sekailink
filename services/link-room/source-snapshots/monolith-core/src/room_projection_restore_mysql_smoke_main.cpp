#include "sekailink_server/room_projection_restore.hpp"

#include "sekailink_server/room_projection.hpp"
#include "sekailink_server/room_session.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

std::string require_env(const char* name) {
  const auto* value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    throw std::runtime_error(std::string("missing_env_") + name);
  }
  return value;
}

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  try {
    sekailink_server::MysqlConnectionConfig config{
        .host = require_env("SEKAILINK_MYSQL_HOST"),
        .user = require_env("SEKAILINK_MYSQL_USER"),
        .password = require_env("SEKAILINK_MYSQL_PASSWORD"),
        .database = require_env("SEKAILINK_MYSQL_DATABASE"),
        .port = static_cast<std::uint32_t>(std::stoul(require_env("SEKAILINK_MYSQL_PORT"))),
        .unix_socket = {},
    };
    sekailink_server::RoomProjectionMysqlStore store(config);
    store.append_batch(sekailink_server::build_projection_batch(
        sekailink_server::RoomSession(sekailink_server::RoomSessionConfig{
            .room_id = "mysql-restore-room-1",
            .room_type = sekailink_server::RoomType::Async,
            .game = "alttp",
            .slot_id = 1,
            .slot_name = "MysqlRestore",
            .slot_alias = "MysqlRestore",
        }).snapshot(),
        {},
        {}));

    sekailink_server::RoomRegistry registry;
    const auto restored = sekailink_server::restore_rooms_from_projection_store(registry, store);
    require(!restored.empty(), "restored_empty");
    require(registry.find_room("mysql-restore-room-1") != nullptr, "mysql_restore_room_missing");

    std::cout << "mysql_projection_restored_rooms=" << restored.size() << "\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_projection_restore_mysql_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
