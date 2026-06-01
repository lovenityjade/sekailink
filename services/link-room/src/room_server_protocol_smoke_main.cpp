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
                 {"seed_name", "seed-proto-name-1"},
                 {"seed_id", "seed-proto-1"},
                 {"seed_hash", "hash-proto-1"},
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
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "issue_ticket"},
                 {"session_name", "room-proto-1"},
                 {"slot_id", 1},
                 {"client_kind", "runtime"},
                 {"driver_instance_id", "driver-proto-1"},
                 {"linkedworld_id", "alttp"},
                 {"core_profile", "snes_v1"},
             }},
        });
    require(response["ok"] == true, "issue_ticket");
    require(response["session_token"].is_string(), "issue_ticket_session_token");
    const auto session_token = response["session_token"].get<std::string>();

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "runtime_heartbeat"},
                 {"room_id", "room-proto-1"},
                 {"runtime_kind", "sklmi"},
                 {"runtime_session_name", "alttp-live-1"},
                 {"driver_instance_id", "driver-proto-1"},
                 {"linkedworld_id", "alttp"},
                 {"core_profile", "snes_v1"},
                 {"client_name", "sekailink_sklmi_runtime"},
                 {"client_version", "0.1"},
                 {"connected", true},
             }},
        });
    require(response["ok"] == true, "runtime_heartbeat");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "set_seed_metadata"},
                 {"room_id", "room-proto-1"},
                 {"seed_name", "seed-proto-name-2"},
                 {"seed_id", "seed-proto-2"},
                 {"seed_hash", "hash-proto-2"},
                 {"tracker_pack", "alttp-pack"},
                 {"tracker_variant", "Map Tracker - AP"},
             }},
        });
    require(response["ok"] == true, "runtime_set_seed_metadata");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "set_slot_data"},
                 {"room_id", "room-proto-1"},
                 {"slot_data", {{"difficulty", "normal"}, {"goal", "ganon"}}},
             }},
        });
    require(response["ok"] == true, "runtime_set_slot_data");
    require(response["channel"] == "runtime", "runtime_set_slot_data_channel");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "enqueue_received_item"},
                 {"room_id", "room-proto-1"},
                 {"item",
                  {
                      {"item_id", 2001},
                      {"item_name", "Bow"},
                      {"location_id", 1001},
                      {"sender_slot", 2},
                      {"sender_alias", "Cloud"},
                      {"flags", 0},
                  }},
             }},
        });
    require(response["ok"] == true, "runtime_enqueue_received_item");
    require(response["channel"] == "runtime", "runtime_enqueue_received_item_channel");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "enqueue_received_item"},
                 {"room_id", "room-proto-1"},
                 {"item",
                  {
                      {"item_id", 2002},
                      {"item_name", "Hookshot"},
                      {"location_id", 1002},
                      {"sender_slot", 2},
                      {"sender_alias", "Cloud"},
                      {"flags", 0},
                  }},
             }},
        });
    require(response["ok"] == true, "runtime_enqueue_received_item_second");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "pending_items"},
                 {"session_name", "room-proto-1"},
                 {"slot_id", 1},
                 {"session_token", session_token},
             }},
        });
    require(response["ok"] == true, "pending_items");
    require(response["pending_items"].size() == 2, "pending_items_size");
    require(response["pending_items"].at(0).at("delivery_id") == 0, "pending_items_delivery_id_0");
    require(response["pending_items"].at(1).at("delivery_id") == 1, "pending_items_delivery_id_1");

    {
      const auto projected_room_records = projection_spool.read_room_records();
      const auto projected_event_records = projection_spool.read_room_event_records();
      require(projected_room_records.size() == 7, "projection_room_records_after_item");
      require(projected_event_records.size() == 6, "projection_event_records_after_item");
      require(projected_room_records.back()["seed_name"] == "seed-proto-name-2", "projection_seed_name");
      require(projected_room_records.back()["seed_id"] == "seed-proto-2", "projection_seed_id");
      require(projected_room_records.back()["seed_hash"] == "hash-proto-2", "projection_seed_hash");
      require(projected_room_records.back()["slot_data_keys"].size() == 2, "projection_slot_data_keys");
      require(projected_room_records.back()["latest_received_item"]["item_name"] == "Hookshot", "projection_latest_item");
      require(projected_room_records.back()["runtime_connected"] == true, "projection_runtime_connected");
      require(projected_room_records.back()["driver_instance_id"] == "driver-proto-1", "projection_driver_instance_id");
      require(projected_room_records.back()["pending_delivery_count"] == 2, "projection_pending_delivery_count");
      require(projected_event_records.at(0)["event_type"] == "runtime_ticket_issued", "projection_runtime_ticket_event_type");
      require(projected_event_records.at(1)["event_type"] == "runtime_heartbeat", "projection_runtime_heartbeat_event_type");
      require(projected_event_records.at(2)["event_type"] == "seed_metadata_updated", "projection_seed_metadata_event_type");
      require(projected_event_records.at(3)["event_type"] == "slot_data_updated", "projection_slot_data_event_type");
      require(projected_event_records.at(3)["payload"]["slot_data_keys"].size() == 2, "projection_slot_data_event_keys");
      require(projected_event_records.back()["event_type"] == "item_received", "projection_item_event_type");
      require(projected_event_records.back()["payload"]["item_index"] == 1, "projection_item_index");
      require(projected_event_records.back()["payload"]["sender_alias"] == "Cloud", "projection_item_sender_alias");
      require(projected_event_records.back()["payload"]["received_item_count"] == 2, "projection_item_received_count");
    }

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
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "runtime_event"},
                 {"session_name", "room-proto-1"},
                 {"session_token", session_token},
                 {"slot_id", 1},
                 {"driver_instance_id", "driver-proto-1"},
                 {"linkedworld_id", "alttp"},
                 {"core_profile", "snes_v1"},
                 {"event_type", "location_checked"},
                 {"canonical_id", 1001},
             }},
        });
    require(response["ok"] == true, "runtime_event_duplicate_check");
    require(response["recorded"] == false, "runtime_event_duplicate_check_recorded");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "acknowledge_delivery"},
                 {"session_name", "room-proto-1"},
                 {"slot_id", 1},
                 {"delivery_id", 0},
                 {"session_token", session_token},
             }},
        });
    require(response["ok"] == true, "acknowledge_delivery");
    require(response["acknowledged"] == true, "acknowledge_delivery_value");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "acknowledge_delivery"},
                 {"session_name", "room-proto-1"},
                 {"slot_id", 1},
                 {"delivery_id", 0},
                 {"session_token", session_token},
             }},
        });
    require(response["ok"] == true, "acknowledge_delivery_again");
    require(response["acknowledged"] == true, "acknowledge_delivery_again_value");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "pending_items"},
                 {"session_name", "room-proto-1"},
                 {"slot_id", 1},
                 {"session_token", session_token},
             }},
        });
    require(response["ok"] == true, "pending_items_after_ack");
    require(response["pending_items"].size() == 1, "pending_items_after_ack_size");
    require(response["pending_items"].at(0).at("delivery_id") == 1, "pending_items_after_ack_delivery_id");

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

    const auto room_record_count_before_queries = projection_spool.read_room_records().size();
    const auto room_event_count_before_queries = projection_spool.read_room_event_records().size();
    const auto client_report_count_before_queries = projection_spool.read_client_report_records().size();
    require(room_record_count_before_queries == 10, "projection_room_records_after_mutations");
    require(room_event_count_before_queries == 9, "projection_event_records_after_mutations");
    require(client_report_count_before_queries == 1, "projection_client_reports_after_mutations");

    response = handle_protocol_json(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "sync_room"},
                 {"room_id", "room-proto-1"},
                 {"from_item_index", 0},
             }},
        });
    require(response["ok"] == true, "sync_room");
    require(response["sync"]["room"]["seed_name"] == "seed-proto-name-2", "sync_room_seed_name");
    require(response["sync"]["room"]["runtime_state"]["driver_instance_id"] == "driver-proto-1", "sync_room_driver_instance_id");
    require(response["sync"]["checks"]["checked_count"] == 1, "sync_room_checked_count");
    require(response["sync"]["items"]["next_index"] == 2, "sync_room_next_index");
    require(response["sync"]["items"]["items"].size() == 2, "sync_room_items_size");
    require(response["sync"]["items"]["items"].at(0).at("item_name") == "Bow", "sync_room_item_name");

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
                 {"cmd", "room_summary"},
                 {"room_id", "room-proto-1"},
             }},
        });
    require(response["ok"] == true, "room_summary");
    require(response["summary"]["items_received"] == 2, "room_summary_items_received");
    require(response["summary"]["seed_name"] == "seed-proto-name-2", "room_summary_seed_name");
    require(response["summary"]["runtime_connected"] == true, "room_summary_runtime_connected");
    require(response["summary"]["runtime_ticket_issued"] == true, "room_summary_runtime_ticket_issued");
    require(response["summary"]["pending_delivery_count"] == 1, "room_summary_pending_delivery_count");
    require(response["summary"]["acknowledged_delivery_count"] == 1, "room_summary_acknowledged_delivery_count");
    require(response["summary"]["last_delivery_ack_id"] == 0, "room_summary_last_delivery_ack_id");
    require(response["summary"]["next_received_item_index"] == 2, "room_summary_next_received_item_index");

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
    require(projection_spool.read_room_records().size() == room_record_count_before_queries,
            "projection_room_records_unchanged_on_queries");
    require(projection_spool.read_room_event_records().size() == room_event_count_before_queries,
            "projection_events_unchanged_on_queries");
    require(projection_spool.read_client_report_records().size() == client_report_count_before_queries,
            "projection_client_reports_unchanged_on_queries");

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
    const auto room_record_count_before_list_rooms = projection_spool.read_room_records().size();
    const auto room_event_count_before_list_rooms = projection_spool.read_room_event_records().size();
    const auto client_report_count_before_list_rooms = projection_spool.read_client_report_records().size();

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
    require(projection_spool.read_room_records().size() == room_record_count_before_list_rooms,
            "projection_room_records_unchanged_on_list_rooms");
    require(projection_spool.read_room_event_records().size() == room_event_count_before_list_rooms,
            "projection_events_unchanged_on_list_rooms");
    require(projection_spool.read_client_report_records().size() == client_report_count_before_list_rooms,
            "projection_client_reports_unchanged_on_list_rooms");

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
