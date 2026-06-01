#include "sekailink_server/room_restore.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  try {
    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_restore_smoke";
    std::filesystem::remove_all(root);

    sekailink_server::RoomAuditStore audit_store(root);
    sekailink_server::RoomRegistry registry;
    require(registry.create_room(sekailink_server::RoomSessionConfig{
        .room_id = "restore-room-1",
        .room_type = sekailink_server::RoomType::Async,
        .game = "alttp",
        .team_id = 0,
        .slot_id = 7,
        .slot_name = "Restore",
        .slot_alias = "Restore",
    }), "create_room_failed");

    auto* room = registry.find_room("restore-room-1");
    require(room != nullptr, "room_missing");
    room->record_check(111);
    room->set_missing_locations({222, 333});
    room->mark_emu_connected(true);
    room->enqueue_received_item(sekailink_server::ReceivedItemView{
        .index = -1,
        .item_id = 2001,
        .item_name = "Bow",
        .location_id = 111,
        .sender_slot = 2,
        .sender_alias = "Cloud",
        .flags = 0,
    });
    std::string runtime_error;
    const auto session_token = room->issue_runtime_ticket(
        sekailink_server::RuntimeTicketRequest{
            .session_name = "restore-room-1",
            .slot_id = 7,
            .client_kind = "runtime",
            .driver_instance_id = "restore-driver-1",
            .linkedworld_id = "alttp",
            .core_profile = "snes_v1",
        },
        &runtime_error);
    require(session_token.has_value(), "runtime_ticket_missing");
    const auto acknowledged = room->acknowledge_delivery(7, 0, *session_token, &runtime_error);
    require(acknowledged.has_value() && *acknowledged, "runtime_delivery_ack_failed");
    room->ingest_client_report(sekailink_server::ClientReport{
        .report_type = "runtime_error",
        .source = "client",
        .severity = "error",
        .message = "restore smoke",
        .timestamp = sekailink_server::utc_timestamp_now(),
    });

    for (const auto& event : room->events()) {
      audit_store.append_event("restore-room-1", event);
    }
    for (const auto& report : room->client_reports()) {
      audit_store.append_client_report("restore-room-1", report);
    }
    audit_store.append_snapshot(room->snapshot());

    sekailink_server::RoomRegistry restored_registry;
    require(sekailink_server::restore_room_from_audit(restored_registry, audit_store, "restore-room-1"), "restore_failed");
    const auto* restored = restored_registry.find_room("restore-room-1");
    require(restored != nullptr, "restored_room_missing");

    const auto snapshot = restored->snapshot();
    require(snapshot.room_id == "restore-room-1", "snapshot_room_id");
    require(snapshot.checked_locations.size() == 1, "snapshot_checks");
    require(snapshot.missing_locations.size() == 2, "snapshot_missing_locations");
    require(snapshot.received_items.size() == 1, "snapshot_received_items");
    require(snapshot.emu_connected, "snapshot_emu_connected");
    require(snapshot.runtime_state.has_value(), "snapshot_runtime_state_missing");
    require(snapshot.runtime_state->session_token_hash.has_value(), "snapshot_runtime_token_hash_missing");
    require(snapshot.runtime_state->acknowledged_delivery_ids.size() == 1, "snapshot_runtime_ack_count");
    require(restored->events().size() >= 2, "events_restored");
    require(restored->client_reports().size() == 1, "reports_restored");

    std::cout << "restored_checks=" << snapshot.checked_locations.size() << "\n";
    std::cout << "restored_events=" << restored->events().size() << "\n";
    std::cout << "restored_reports=" << restored->client_reports().size() << "\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_restore_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
