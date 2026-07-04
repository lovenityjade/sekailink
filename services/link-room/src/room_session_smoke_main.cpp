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
        .seed_name = "alttp-live-seed",
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
    session.heartbeat_runtime({
        .runtime_kind = "sklmi",
        .runtime_session_name = "alttp-live-1",
        .driver_instance_id = "sklmi-driver-1",
        .linkedworld_id = "alttp",
        .core_profile = "snes_v1",
        .client_name = "sekailink_sklmi_runtime",
        .client_version = "0.1",
        .connected = true,
    });
    session.record_check(1001);
    session.enqueue_received_item(ReceivedItemView{-1, 2001, "Bow", 1001, 2, "Cloud", 0});
    session.enqueue_received_item(ReceivedItemView{-1, 2002, "Hookshot", 1002, 2, "Cloud", 0});
    session.set_seed_metadata("alttp-live-seed-r2", "seed-456", "hash-def", "alttp-pack", "Map Tracker - AP");
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

    std::string ticket_error;
    const auto session_token = session.issue_runtime_ticket(
        RuntimeTicketRequest{
            .session_name = "room-session-1",
            .slot_id = 1,
            .client_kind = "runtime",
            .driver_instance_id = "sklmi-driver-1",
            .linkedworld_id = "alttp",
            .core_profile = "snes_v1",
        },
        &ticket_error);
    require(session_token.has_value(), "issue_runtime_ticket");
    require(ticket_error.empty(), "issue_runtime_ticket_error");

    const auto pending_before_ack = session.pending_deliveries(1, *session_token, &ticket_error);
    require(pending_before_ack.has_value(), "pending_before_ack");
    require(pending_before_ack->size() == 2, "pending_before_ack_size");

    const auto duplicate_runtime_check = session.record_runtime_event(
        RuntimeEventRequest{
            .slot_id = 1,
            .session_token = *session_token,
            .event_type = "location_checked",
            .canonical_id = 1001,
            .driver_instance_id = "sklmi-driver-1",
            .linkedworld_id = "alttp",
            .core_profile = "snes_v1",
        },
        &ticket_error);
    require(duplicate_runtime_check.has_value(), "runtime_event_duplicate_check");
    require(*duplicate_runtime_check == false, "runtime_event_duplicate_recorded");

    const auto acknowledged = session.acknowledge_delivery(1, 0, *session_token, &ticket_error);
    require(acknowledged.has_value(), "acknowledge_delivery");
    require(*acknowledged == true, "acknowledge_delivery_value");
    const auto acknowledged_again = session.acknowledge_delivery(1, 0, *session_token, &ticket_error);
    require(acknowledged_again.has_value(), "acknowledge_delivery_again");
    require(*acknowledged_again == true, "acknowledge_delivery_again_value");

    const auto pending_after_ack = session.pending_deliveries(1, *session_token, &ticket_error);
    require(pending_after_ack.has_value(), "pending_after_ack");
    require(pending_after_ack->size() == 1, "pending_after_ack_size");
    require(pending_after_ack->at(0).delivery_id == 1, "pending_after_ack_delivery_id");

    const auto summary = session.activity_summary();
    require(summary.player_connections == 1, "player_connections");
    require(summary.checks_recorded == 1, "checks_recorded");
    require(summary.items_received == 2, "items_received");

    const auto snapshot = session.snapshot();
    const auto payload = to_json(snapshot);
    const auto sync = session.sync_payload(0);
    require(payload["room_type"] == "async", "room_type");
    require(payload["players"].size() == 2, "players");
    require(payload["received_items"][0]["item_name"] == "Bow", "received_item");
    require(payload["received_items"][0]["index"] == 0, "received_item_index");
    require(payload["received_items"][1]["item_name"] == "Hookshot", "received_item_second");
    require(payload["checked_locations"][0] == 1001, "checked_locations");
    require(payload["seed_name"] == "alttp-live-seed-r2", "seed_name");
    require(payload["async_state"]["daily_summary_state"] == "pending", "daily_summary_state");
    require(payload["runtime_state"]["connected"] == true, "runtime_connected");
    require(payload["runtime_state"]["driver_instance_id"] == "sklmi-driver-1", "runtime_driver_instance_id");
    require(payload["runtime_state"]["session_token_hash"].is_string(), "runtime_session_token_hash");
    require(payload["runtime_state"]["acknowledged_delivery_ids"].size() == 1, "runtime_acknowledged_delivery_ids");
    require(payload["runtime_state"]["last_delivery_ack_id"] == 0, "runtime_last_delivery_ack_id");
    require(sync["items"]["next_index"] == 2, "sync_next_index");
    require(sync["checks"]["checked_count"] == 1, "sync_checked_count");
    require(sync["room"]["runtime_state"]["linkedworld_id"] == "alttp", "sync_linkedworld_id");
    require(session.next_received_item_index() == 2, "next_received_item_index");
    require(session.pending_delivery_count() == 1, "pending_delivery_count");
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
