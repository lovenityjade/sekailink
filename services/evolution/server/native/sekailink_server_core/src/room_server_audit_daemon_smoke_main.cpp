#include "sekailink_server/room_audit_store.hpp"
#include "sekailink_server/room_projection.hpp"
#include "sekailink_server/room_registry.hpp"

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
    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_server_daemon_audit_smoke";
    std::filesystem::remove_all(root);

    RoomRegistry registry;
    RoomAuditStore store(root);
    RoomProjectionSpool projection_spool(root / "projection");

    auto response = handle_room_server_command_with_audit(
        registry,
        nlohmann::json{
            {"cmd", "create_room"},
            {"room_id", "room-audit-daemon-1"},
            {"room_type", "async"},
            {"game", "A Link to the Past"},
            {"team_id", 0},
            {"slot_id", 1},
            {"slot_name", "Jade"},
            {"slot_alias", "Sekai Jade"},
            {"expires_at", "2026-04-01T00:00:00Z"},
        },
        &store);
    require(response["ok"] == true, "create_room");

    response = handle_room_server_command_with_audit(
        registry,
        nlohmann::json{
            {"cmd", "ingest_client_report"},
            {"room_id", "room-audit-daemon-1"},
            {"report",
             {
                 {"report_type", "runtime_error"},
                 {"source", "sekaiemu"},
                 {"severity", "error"},
                 {"message", "Room daemon audit smoke"},
                 {"game", "A Link to the Past"},
                 {"runtime", "snes"},
                 {"details", {{"code", "AUDIT_SMOKE"}}},
             }},
        },
        &store);
    require(response["ok"] == true, "ingest_client_report");

    response = handle_room_server_command_with_audit(
        registry,
        nlohmann::json{
            {"cmd", "snapshot_room"},
            {"room_id", "room-audit-daemon-1"},
        },
        &store);
    require(response["ok"] == true, "snapshot_room");

    const auto events = store.read_events("room-audit-daemon-1");
    const auto reports = store.read_client_reports("room-audit-daemon-1");
    const auto snapshots = store.read_snapshots("room-audit-daemon-1");

    require(!events.empty(), "events");
    require(!reports.empty(), "reports");
    require(!snapshots.empty(), "snapshots");
    require(reports.back()["message"] == "Room daemon audit smoke", "report_message");
    projection_spool.append_batch(build_projection_batch(
        registry.find_room("room-audit-daemon-1")->snapshot(),
        registry.find_room("room-audit-daemon-1")->events(),
        registry.find_room("room-audit-daemon-1")->client_reports()));
    require(!projection_spool.read_room_records().empty(), "projection_room_records");

    std::cout << "audit_root=" << root << "\n";
    std::cout << "events=" << events.size() << "\n";
    std::cout << "reports=" << reports.size() << "\n";
    std::cout << "snapshots=" << snapshots.size() << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_room_server_audit_daemon_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
