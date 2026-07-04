#pragma once

#include "sekailink_server/room_projection_mysql.hpp"
#include "sekailink_server/room_projection_sqlite.hpp"
#include "sekailink_server/room_registry.hpp"

#include <string>
#include <vector>

namespace sekailink_server {

std::vector<std::string> restore_rooms_from_projection_records(
    RoomRegistry& registry,
    const std::vector<nlohmann::json>& room_records,
    const std::vector<nlohmann::json>& room_event_records = {},
    const std::vector<nlohmann::json>& client_report_records = {});

std::vector<std::string> restore_rooms_from_projection_store(
    RoomRegistry& registry,
    const RoomProjectionSqliteStore& store);

std::vector<std::string> restore_rooms_from_projection_store(
    RoomRegistry& registry,
    const RoomProjectionMysqlStore& store);

}  // namespace sekailink_server
