#include "sekailink_server/room_projection_sql.hpp"

#include "sekailink_server/room_session.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace sekailink_server;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  try {
    RoomSession session(RoomSessionConfig{
        .room_id = "room-mysql-ready-1",
        .room_type = RoomType::Async,
        .game = "A Link to the Past",
        .team_id = 0,
        .slot_id = 1,
        .slot_name = "Jade",
        .slot_alias = "Sekai Jade",
    });
    session.record_check(1001);
    session.ingest_client_report(ClientReport{
        .report_type = "runtime_error",
        .source = "sekaiemu",
        .severity = "error",
        .message = "SQL smoke report",
        .timestamp = "",
        .request_id = std::nullopt,
        .session_id = std::nullopt,
        .user_id = std::nullopt,
        .room_id = "room-mysql-ready-1",
        .lobby_id = std::nullopt,
        .game = "A Link to the Past",
        .runtime = "snes",
        .details = {{"code", "SQL_SMOKE"}},
    });

    const auto batch = build_projection_batch(session.snapshot(), session.events(), session.client_reports());
    const auto schema = projection_mysql_schema_sql();
    const auto room_stmt = build_room_record_insert_sql(batch.room_record);
    const auto event_stmt = build_room_event_record_insert_sql(batch.room_event_records.front());
    const auto report_stmt = build_client_report_record_insert_sql(batch.client_report_records.front());

    require(schema.find("CREATE TABLE IF NOT EXISTS room_records") != std::string::npos, "schema_room_records");
    require(room_stmt.bindings.size() == 2, "room_stmt_bindings");
    require(event_stmt.bindings.size() == 2, "event_stmt_bindings");
    require(report_stmt.bindings.size() == 2, "report_stmt_bindings");
    require(room_stmt.bindings.front() == "room-mysql-ready-1", "room_stmt_room_id");
    require(report_stmt.bindings.front() == "room-mysql-ready-1", "report_stmt_room_id");

    std::cout << room_stmt.sql << "\n";
    std::cout << report_stmt.sql << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_room_projection_sql_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
