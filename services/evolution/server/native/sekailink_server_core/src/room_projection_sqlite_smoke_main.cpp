#include "sekailink_server/room_projection_sqlite.hpp"

#include "sekailink_server/room_session.hpp"

#include <cstdlib>
#include <filesystem>
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
        .room_id = "room-sqlite-1",
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
        .message = "SQLite projection smoke",
        .timestamp = "",
        .request_id = std::nullopt,
        .session_id = std::nullopt,
        .user_id = std::nullopt,
        .room_id = "room-sqlite-1",
        .lobby_id = std::nullopt,
        .game = "A Link to the Past",
        .runtime = "snes",
        .details = {{"code", "SQLITE_SMOKE"}},
    });

    const auto batch = build_projection_batch(session.snapshot(), session.events(), session.client_reports());

    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_projection_sqlite_smoke";
    std::filesystem::remove_all(root);
    RoomProjectionSqliteStore store(root / "room_projection.sqlite3");
    store.append_batch(batch);

    const auto room_records = store.read_room_records();
    const auto room_event_records = store.read_room_event_records();
    const auto client_report_records = store.read_client_report_records();

    require(room_records.size() == 1, "room_records");
    require(!room_event_records.empty(), "room_event_records");
    require(client_report_records.size() == 1, "client_report_records");
    require(client_report_records[0]["message"] == "SQLite projection smoke", "client_report_message");

    std::cout << "db_path=" << store.db_path() << "\n";
    std::cout << "room_records=" << room_records.size() << "\n";
    std::cout << "room_event_records=" << room_event_records.size() << "\n";
    std::cout << "client_report_records=" << client_report_records.size() << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_room_projection_sqlite_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
