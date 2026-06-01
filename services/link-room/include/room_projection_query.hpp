#pragma once

#include "sekailink_server/room_projection_mysql.hpp"
#include "sekailink_server/room_projection_sqlite.hpp"
#include "sekailink_server/room_state.hpp"

#include <optional>
#include <vector>

namespace sekailink_server {

std::optional<RoomStateSnapshot> latest_room_snapshot_from_records(
    const std::vector<nlohmann::json>& room_records,
    const std::string& room_id);

std::vector<RoomStateSnapshot> latest_room_snapshots_from_records(const std::vector<nlohmann::json>& room_records);

std::optional<RoomStateSnapshot> latest_room_snapshot(const RoomProjectionSqliteStore& store, const std::string& room_id);
std::optional<RoomStateSnapshot> latest_room_snapshot(const RoomProjectionMysqlStore& store, const std::string& room_id);

std::vector<RoomStateSnapshot> latest_room_snapshots(const RoomProjectionSqliteStore& store);
std::vector<RoomStateSnapshot> latest_room_snapshots(const RoomProjectionMysqlStore& store);

std::vector<RoomEvent> room_events_from_records(
    const std::vector<nlohmann::json>& room_event_records,
    const std::string& room_id);

std::vector<ClientReport> client_reports_from_records(
    const std::vector<nlohmann::json>& client_report_records,
    const std::string& room_id);

std::vector<RoomEvent> room_events(const RoomProjectionSqliteStore& store, const std::string& room_id);
std::vector<RoomEvent> room_events(const RoomProjectionMysqlStore& store, const std::string& room_id);

std::vector<ClientReport> client_reports(const RoomProjectionSqliteStore& store, const std::string& room_id);
std::vector<ClientReport> client_reports(const RoomProjectionMysqlStore& store, const std::string& room_id);

}  // namespace sekailink_server
