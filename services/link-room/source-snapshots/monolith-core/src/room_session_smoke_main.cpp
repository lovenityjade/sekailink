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
        .room_id = "room-session-1",
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

    session.upsert_player(PlayerRoomView{0, 1, "Jade", "Sekai Jade", "A Link to the Past", false});
    session.upsert_player(PlayerRoomView{0, 2, "Cloud", "Cloud", "A Link to the Past", false});
    session.connect_player(1);
    session.mark_emu_connected(true);
    session.mark_tracker_connected(true);
    session.set_missing_locations({1002, 1003});
    session.record_check(1001);
    session.enqueue_received_item(ReceivedItemView{0, 2001, "Bow", 1001, 2, "Cloud", 0});
    session.set_stored_data("lttp_big_key_eastern", true);
    session.set_slot_data({{"mode", "open"}, {"shuffle", "none"}});
    session.set_location_mapping(1001, 1, 2001, "Link's House");
    session.set_location_mapping(1002, 2, 2002, "Eastern Palace - Big Chest");
    session.set_allowed_players({1, 2});
    session.set_daily_summary_state("pending");
    session.set_async_notification_state("queued");
    session.set_milestones({"dark_world"});
    session.set_notifications({"go_mode_ready"});
    session.ingest_client_report(ClientReport{
        .report_type = "runtime_error",
        .source = "sekaiemu",
        .severity = "error",
        .message = "Test execution failure report",
        .timestamp = "",
        .request_id = "req-1",
        .session_id = "sess-1",
        .user_id = "user-1",
        .room_id = "room-session-1",
        .lobby_id = std::nullopt,
        .game = "A Link to the Past",
        .runtime = "snes",
        .details = {{"code", "SMOKE_TEST"}, {"frame", 123}},
    });

    const auto summary = session.activity_summary();
    require(summary.player_connections == 1, "player_connections");
    require(summary.checks_recorded == 1, "checks_recorded");
    require(summary.items_received == 1, "items_received");

    const auto snapshot = session.snapshot();
    const auto payload = to_json(snapshot);
    require(payload["room_type"] == "async", "room_type");
    require(payload["players"].size() == 2, "players");
    require(payload["received_items"][0]["item_name"] == "Bow", "received_item");
    require(payload["checked_locations"][0] == 1001, "checked_locations");
    require(payload["async_state"]["daily_summary_state"] == "pending", "daily_summary_state");
    require(session.events().size() >= 5, "events");
    require(session.client_reports().size() == 1, "client_reports");
    require(!session.is_expired_at("2026-03-27T00:00:00Z"), "not_expired");
    require(session.is_expired_at("2026-04-02T00:00:00Z"), "expired");

    std::cout << payload.dump(2) << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_server_room_session_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
