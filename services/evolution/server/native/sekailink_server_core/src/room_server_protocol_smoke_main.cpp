#include "sekailink_server/room_server_protocol.hpp"

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

class FailingProjectionStore final : public RoomProjectionStore {
 public:
  void append_batch(const RoomProjectionBatch&) override {
    throw std::runtime_error("projection_store_unavailable");
  }
};

}  // namespace

int main() {
  try {
    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_server_protocol_smoke";
    std::filesystem::remove_all(root);

    RoomRegistry registry;
    RoomAuditStore audit_store(root);
    RoomProjectionSpool projection_spool(root / "projection");

    auto response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "create_room"},
                 {"room_id", "room-proto-1"},
                 {"room_type", "async"},
                 {"game", "A Link to the Past"},
                 {"team_id", 0},
                 {"slot_id", 1},
                 {"slot_name", "Jade"},
                 {"slot_alias", "Sekai Jade"},
             }},
        });
    require(response["ok"] == true, "create_room");
    require(response["channel"] == "admin", "create_channel");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "record_check"},
                 {"room_id", "room-proto-1"},
                 {"location_id", 1001},
             }},
        });
    require(response["ok"] == true, "runtime_record_check");
    require(response["channel"] == "runtime", "runtime_channel");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "client_report"},
            {"command",
             {
                 {"cmd", "ingest_client_report"},
                 {"room_id", "room-proto-1"},
                 {"report",
                  {
                      {"report_type", "runtime_error"},
                      {"source", "sekaiemu"},
                      {"severity", "error"},
                      {"message", "Protocol smoke report"},
                  }},
             }},
        });
    require(response["ok"] == true, "client_report");
    require(response["channel"] == "client_report", "client_report_channel");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "client_report"},
            {"command",
             {
                 {"cmd", "record_check"},
                 {"room_id", "room-proto-1"},
                 {"location_id", 1002},
             }},
        });
    require(response["ok"] == false, "client_report_reject");
    require(response["error"] == "command_not_allowed", "client_report_reject_error");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "client_reports"},
                 {"room_id", "room-proto-1"},
             }},
        });
    require(response["ok"] == true, "client_reports");
    require(response["reports"].size() == 1, "client_reports_size");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "room_events"},
                 {"room_id", "room-proto-1"},
                 {"limit", 1},
             }},
        });
    require(response["ok"] == true, "room_events_limited");
    require(response["events"].size() == 1, "room_events_limited_size");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "client_reports"},
                 {"room_id", "room-proto-1"},
                 {"limit", 1},
             }},
        });
    require(response["ok"] == true, "client_reports_limited");
    require(response["reports"].size() == 1, "client_reports_limited_size");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "room_events"},
                 {"room_id", "room-proto-1"},
                 {"event_type", "client_report_ingested"},
                 {"severity", "error"},
             }},
        });
    require(response["ok"] == true, "room_events_filtered");
    require(response["events"].size() == 1, "room_events_filtered_size");
    require(response["events"].at(0).at("source").get<std::string>() == "sekaiemu", "room_events_filtered_source_value");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "room_events"},
                 {"room_id", "room-proto-1"},
                 {"event_type", "client_report_ingested"},
                 {"severity", "error"},
                 {"source", "sekaiemu"},
             }},
        });
    require(response["ok"] == true, "room_events_filtered_source");
    require(response["events"].size() == 1, "room_events_filtered_source_size");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "room_events"},
                 {"room_id", "room-proto-1"},
                 {"limit", 1},
                 {"event_type", "client_report_ingested"},
                 {"severity", "error"},
                 {"offset", 1},
             }},
        });
    require(response["ok"] == true, "room_events_filtered_offset");
    require(response["events"].empty(), "room_events_filtered_offset_empty");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "client_reports"},
                 {"room_id", "room-proto-1"},
                 {"report_type", "runtime_error"},
                 {"severity", "error"},
                 {"source", "sekaiemu"},
             }},
        });
    require(response["ok"] == true, "client_reports_filtered");
    require(response["reports"].size() == 1, "client_reports_filtered_size");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "client_reports"},
                 {"room_id", "room-proto-1"},
                 {"limit", 1},
                 {"report_type", "runtime_error"},
                 {"severity", "error"},
                 {"source", "sekaiemu"},
                 {"offset", 1},
             }},
        });
    require(response["ok"] == true, "client_reports_filtered_offset");
    require(response["reports"].empty(), "client_reports_filtered_offset_empty");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "set_expires_at"},
                 {"room_id", "room-proto-1"},
                 {"expires_at", "2026-04-02T00:00:00Z"},
             }},
        });
    require(response["ok"] == true, "set_expires_at_allowed");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "snapshot_room"},
                 {"room_id", "room-proto-1"},
             }},
        });
    require(response["ok"] == true, "snapshot_room_after_set_expires");
    require(response["snapshot"]["async_state"]["expires_at"] == "2026-04-02T00:00:00Z",
            "snapshot_room_expires_at");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "list_rooms"},
                 {"limit", 1},
                 {"query", "proto"},
                 {"room_type", "async"},
             }},
        });
    require(response["ok"] == true, "list_rooms_filtered");
    require(response["room_ids"].size() == 1, "list_rooms_filtered_size");
    require(response["room_ids"].at(0) == "room-proto-1", "list_rooms_filtered_id");

    require(!projection_spool.read_room_records().empty(), "projection_room_records");
    require(!projection_spool.read_client_report_records().empty(), "projection_client_reports");

    FailingProjectionStore failing_projection_store;
    response = handle_protocol_json(
        registry,
        &audit_store,
        &failing_projection_store,
        nullptr,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "create_room"},
                 {"room_id", "room-proto-2"},
                 {"room_type", "live"},
                 {"game", "A Link to the Past"},
                 {"team_id", 0},
                 {"slot_id", 2},
                 {"slot_name", "Alice"},
             }},
        });
    require(response["ok"] == true, "projection_warning_create_room_ok");
    require(response.contains("projection_warning"), "projection_warning_present");
    require(response["projection_warning"]["ok"] == false, "projection_warning_ok_false");
    require(response["projection_warning"]["error"] == "projection_store_unavailable", "projection_warning_error");

    std::cout << response.dump(2) << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_room_server_protocol_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
