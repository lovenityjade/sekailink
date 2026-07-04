#include "sekailink_server/room_projection.hpp"

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
        .room_id = "room-projection-1",
        .room_type = RoomType::Async,
        .game = "A Link to the Past",
        .team_id = 0,
        .slot_id = 1,
        .slot_name = "Jade",
        .slot_alias = "Sekai Jade",
        .seed_id = "seed-123",
        .seed_hash = "hash-abc",
        .patch_url = "https://cdn.sekailink.com/patch.apz",
        .tracker_pack = "alttp-pack",
        .tracker_variant = "Map Tracker - AP",
        .sni_required = true,
        .memory_backend_state = "required",
        .expires_at = "2026-04-01T00:00:00Z",
    });
    session.upsert_player(PlayerRoomView{0, 1, "Jade", "Sekai Jade", "A Link to the Past", true});
    session.record_check(1001);
    session.enqueue_received_item(ReceivedItemView{0, 2001, "Bow", 1001, 2, "Cloud", 0});
    session.ingest_client_report(ClientReport{
        .report_type = "runtime_error",
        .source = "sekaiemu",
        .severity = "error",
        .message = "Projection smoke report",
        .timestamp = "",
        .request_id = "req-1",
        .session_id = "sess-1",
        .user_id = "user-1",
        .room_id = "room-projection-1",
        .lobby_id = std::nullopt,
        .game = "A Link to the Past",
        .runtime = "snes",
        .details = {{"code", "PROJECTION_SMOKE"}},
    });

    const auto batch = build_projection_batch(session.snapshot(), session.events(), session.client_reports());
    require(batch.room_record["room_id"] == "room-projection-1", "room_record");
    require(batch.room_event_records.size() >= 2, "event_records");
    require(batch.client_report_records.size() == 1, "report_records");

    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_projection_smoke";
    std::filesystem::remove_all(root);
    RoomProjectionSpool spool(root);
    spool.append_batch(batch);

    const auto room_records = spool.read_room_records();
    const auto room_event_records = spool.read_room_event_records();
    const auto client_report_records = spool.read_client_report_records();

    require(room_records.size() == 1, "room_records");
    require(!room_event_records.empty(), "room_event_records");
    require(client_report_records.size() == 1, "client_report_records");
    require(client_report_records[0]["message"] == "Projection smoke report", "client_report_message");

    std::cout << "room_records=" << room_records.size() << "\n";
    std::cout << "room_event_records=" << room_event_records.size() << "\n";
    std::cout << "client_report_records=" << client_report_records.size() << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_room_projection_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
