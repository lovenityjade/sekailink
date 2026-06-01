#include "sekailink_server/room_audit_store.hpp"

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
    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_audit_smoke";
    std::filesystem::remove_all(root);

    RoomAuditStore store(root);

    RoomEvent event{
        .event_type = "client_report_ingested",
        .timestamp = utc_timestamp_now(),
        .payload = {{"severity", "error"}, {"source", "sekaiemu"}},
    };
    ClientReport report{
        .report_type = "runtime_error",
        .source = "sekaiemu",
        .severity = "error",
        .message = "Bridge timeout",
        .timestamp = utc_timestamp_now(),
        .request_id = "req-1",
        .session_id = "sess-1",
        .user_id = "user-1",
        .room_id = "room-audit-1",
        .lobby_id = std::nullopt,
        .game = "A Link to the Past",
        .runtime = "snes",
        .details = {{"code", "BRIDGE_TIMEOUT"}},
    };

    RoomStateSnapshot snapshot;
    snapshot.room_id = "room-audit-1";
    snapshot.room_type = RoomType::Async;
    snapshot.connection_state = ConnectionState::Online;
    snapshot.game = "A Link to the Past";
    snapshot.slot_id = 1;
    snapshot.slot_name = "Jade";
    snapshot.slot_alias = "Sekai Jade";
    snapshot.capabilities.supports_async = true;

    store.append_event("room-audit-1", event);
    store.append_client_report("room-audit-1", report);
    store.append_snapshot(snapshot);

    const auto events = store.read_events("room-audit-1");
    const auto reports = store.read_client_reports("room-audit-1");
    const auto snapshots = store.read_snapshots("room-audit-1");

    require(events.size() == 1, "events_size");
    require(reports.size() == 1, "reports_size");
    require(snapshots.size() == 1, "snapshots_size");
    require(events[0]["event_type"] == "client_report_ingested", "event_type");
    require(reports[0]["message"] == "Bridge timeout", "report_message");
    require(snapshots[0]["room_type"] == "async", "snapshot_room_type");

    std::cout << "audit_root=" << root << "\n";
    std::cout << "events=" << events.size() << "\n";
    std::cout << "reports=" << reports.size() << "\n";
    std::cout << "snapshots=" << snapshots.size() << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_server_room_audit_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
