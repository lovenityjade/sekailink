#include "sekailink_server/room_projection.hpp"
#include "sekailink_server/room_projection_mysql.hpp"
#include "sekailink_server/room_projection_query.hpp"
#include "sekailink_server/room_projection_sqlite.hpp"
#include "sekailink_server/room_state.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

int main() {
  try {
    namespace fs = std::filesystem;
    const fs::path sqlite_path = fs::temp_directory_path() / "sekailink_room_projection_query_smoke.sqlite3";
    fs::remove(sqlite_path);

    sekailink_server::RoomProjectionSqliteStore sqlite_store(sqlite_path);
    sekailink_server::RoomStateSnapshot snapshot;
    snapshot.room_id = "query-room";
    snapshot.room_type = sekailink_server::RoomType::Live;
    snapshot.connection_state = sekailink_server::ConnectionState::Online;
    snapshot.game = "alttp";
    snapshot.team_id = 0;
    snapshot.slot_id = 1;
    snapshot.slot_name = "Jade";
    snapshot.slot_alias = "Jade";
    snapshot.generated_at = "2026-03-26T12:00:00Z";
    sqlite_store.append_batch(sekailink_server::build_projection_batch(snapshot, {}, {}));
    snapshot.generated_at = "2026-03-26T12:05:00Z";
    snapshot.checked_locations = {42};
    sqlite_store.append_batch(sekailink_server::build_projection_batch(snapshot, {}, {}));

    const auto latest_sqlite = sekailink_server::latest_room_snapshot(sqlite_store, "query-room");
    if (!latest_sqlite.has_value() || latest_sqlite->checked_locations.size() != 1) {
      throw std::runtime_error("sqlite_latest_snapshot_failed");
    }

    if (std::getenv("SEKAILINK_MYSQL_HOST") && std::getenv("SEKAILINK_MYSQL_USER") &&
        std::getenv("SEKAILINK_MYSQL_PASSWORD") && std::getenv("SEKAILINK_MYSQL_DATABASE")) {
      sekailink_server::RoomProjectionMysqlStore mysql_store(sekailink_server::MysqlConnectionConfig{
          .host = std::getenv("SEKAILINK_MYSQL_HOST"),
          .user = std::getenv("SEKAILINK_MYSQL_USER"),
          .password = std::getenv("SEKAILINK_MYSQL_PASSWORD"),
          .database = std::getenv("SEKAILINK_MYSQL_DATABASE"),
          .port = std::getenv("SEKAILINK_MYSQL_PORT") ? static_cast<std::uint32_t>(std::stoul(std::getenv("SEKAILINK_MYSQL_PORT"))) : 3306,
          .unix_socket = std::getenv("SEKAILINK_MYSQL_SOCKET") ? std::getenv("SEKAILINK_MYSQL_SOCKET") : "",
      });
      mysql_store.append_batch(sekailink_server::build_projection_batch(snapshot, {}, {}));
      const auto latest_mysql = sekailink_server::latest_room_snapshot(mysql_store, "query-room");
      if (!latest_mysql.has_value()) {
        throw std::runtime_error("mysql_latest_snapshot_failed");
      }
    }

    std::cout << "room_projection_query_ok=1\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_projection_query_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
