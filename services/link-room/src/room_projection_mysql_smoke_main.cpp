#include "sekailink_server/room_projection_mysql.hpp"

#include "sekailink_server/room_projection.hpp"

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
    sekailink_server::RoomProjectionBatch batch;
    batch.room_record = {
        {"room_id", "room-mysql-store-1"},
        {"room_type", "live"},
        {"game", "alttp"},
    };
    batch.room_event_records.push_back({
        {"room_id", "room-mysql-store-1"},
        {"event_type", "room_created"},
    });
    batch.client_report_records.push_back({
        {"room_id", "room-mysql-store-1"},
        {"report_type", "runtime_error"},
        {"message", "smoke"},
    });

    store.append_batch(batch);

    const auto rooms = store.read_room_records();
    const auto events = store.read_room_event_records();
    const auto reports = store.read_client_report_records();

    require(!rooms.empty(), "rooms_empty");
    require(!events.empty(), "events_empty");
    require(!reports.empty(), "reports_empty");
    require(rooms.back().at("room_id").get<std::string>() == "room-mysql-store-1", "room_id_mismatch");
    require(reports.back().at("report_type").get<std::string>() == "runtime_error", "report_type_mismatch");

    std::cout << "room_records=" << rooms.size() << "\n";
    std::cout << "room_event_records=" << events.size() << "\n";
    std::cout << "client_report_records=" << reports.size() << "\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_projection_mysql_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
