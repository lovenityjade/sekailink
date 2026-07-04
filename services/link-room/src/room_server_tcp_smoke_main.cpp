#include "sekailink_server/room_server_tcp.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

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
    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_server_tcp_smoke";
    std::filesystem::remove_all(root);

    std::ostringstream captured_logs;
    auto* original_cout = std::cout.rdbuf(captured_logs.rdbuf());

    RoomRegistry registry;
    RoomAuditStore audit_store(root);
    RoomProjectionSpool projection_spool(root / "projection");
    RoomServerTcpService service(registry, &audit_store, &projection_spool);
    require(service.start_background(0), "service_start");
    const auto port = service.port();
    require(port != 0, "bound_port");

    auto response = nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "create_room"},
                 {"room_id", "room-tcp-1"},
                 {"room_type", "async"},
                 {"game", "A Link to the Past"},
                 {"team_id", 0},
                 {"slot_id", 1},
                 {"slot_name", "Jade"},
                 {"slot_alias", "Sekai Jade"},
                 {"seed_name", "seed-tcp-name-1"},
                 {"seed_id", "seed-tcp-1"},
                 {"seed_hash", "hash-tcp-1"},
                 {"tracker_pack", "alttp-pack"},
                 {"tracker_variant", "Map Tracker - AP"},
             }},
        }));
    require(response["ok"] == true, "create_room");

    response = nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "runtime_heartbeat"},
                 {"room_id", "room-tcp-1"},
                 {"runtime_kind", "sklmi"},
                 {"runtime_session_name", "tcp-live-1"},
                 {"driver_instance_id", "driver-tcp-1"},
                 {"linkedworld_id", "alttp"},
                 {"core_profile", "snes_v1"},
                 {"client_name", "sekailink_sklmi_runtime"},
                 {"client_version", "0.1"},
                 {"connected", true},
             }},
        }));
    require(response["ok"] == true, "runtime_heartbeat");

    response = nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "set_slot_data"},
                 {"room_id", "room-tcp-1"},
                 {"slot_data", {{"difficulty", "normal"}, {"goal", "ganon"}}},
             }},
        }));
    require(response["ok"] == true, "set_slot_data");

    response = nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "enqueue_received_item"},
                 {"room_id", "room-tcp-1"},
                 {"item",
                  {
                      {"index", 0},
                      {"item_id", 2001},
                      {"item_name", "Bow"},
                      {"location_id", 1001},
                      {"sender_slot", 2},
                      {"sender_alias", "Cloud"},
                      {"flags", 0},
                  }},
             }},
        }));
    require(response["ok"] == true, "enqueue_received_item");

    response = nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "record_check"},
                 {"room_id", "room-tcp-1"},
                 {"location_id", 1001},
             }},
        }));
    require(response["ok"] == true, "record_check");

    response = nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "client_report"},
            {"command",
             {
                 {"cmd", "ingest_client_report"},
                 {"room_id", "room-tcp-1"},
                 {"report",
                  {
                      {"report_type", "runtime_error"},
                      {"source", "sekaiemu"},
                      {"severity", "error"},
                      {"message", "TCP smoke report"},
                  }},
             }},
        }));
    require(response["ok"] == true, "ingest_client_report");

    response = nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "sync_room"},
                 {"room_id", "room-tcp-1"},
                 {"from_item_index", 0},
             }},
        }));
    require(response["ok"] == true, "sync_room");
    require(response["sync"]["room"]["seed_name"] == "seed-tcp-name-1", "sync_room_seed_name");
    require(response["sync"]["room"]["runtime_state"]["driver_instance_id"] == "driver-tcp-1", "sync_room_driver_instance_id");
    require(response["sync"]["items"]["next_index"] == 1, "sync_room_next_index");

    response = nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "client_reports"},
                 {"room_id", "room-tcp-1"},
             }},
        }));
    require(response["ok"] == true, "client_reports");
    require(response["reports"].size() == 1, "client_reports_size");

    service.stop();
    std::cout.rdbuf(original_cout);

    const auto reports = audit_store.read_client_reports("room-tcp-1");
    require(reports.size() == 1, "audit_reports");
    require(!projection_spool.read_room_records().empty(), "projection_room_records");
    require(!projection_spool.read_client_report_records().empty(), "projection_client_reports");
    const auto logs = captured_logs.str();
    require(logs.find("\"event\":\"room_server_tcp_command\"") != std::string::npos, "tcp_logs_event_missing");
    require(logs.find("\"cmd\":\"create_room\"") != std::string::npos, "tcp_logs_create_room_missing");
    require(logs.find("\"seed_name\":\"seed-tcp-name-1\"") != std::string::npos, "tcp_logs_seed_name_missing");
    require(logs.find("\"seed_id\":\"seed-tcp-1\"") != std::string::npos, "tcp_logs_seed_id_missing");
    require(logs.find("\"seed_hash\":\"hash-tcp-1\"") != std::string::npos, "tcp_logs_seed_hash_missing");
    require(logs.find("\"cmd\":\"runtime_heartbeat\"") != std::string::npos, "tcp_logs_runtime_heartbeat_missing");
    require(logs.find("\"driver_instance_id\":\"driver-tcp-1\"") != std::string::npos, "tcp_logs_driver_instance_id_missing");
    require(logs.find("\"linkedworld_id\":\"alttp\"") != std::string::npos, "tcp_logs_linkedworld_id_missing");
    require(logs.find("\"cmd\":\"set_slot_data\"") != std::string::npos, "tcp_logs_set_slot_data_missing");
    require(logs.find("\"slot_data_keys\":[\"difficulty\",\"goal\"]") != std::string::npos, "tcp_logs_slot_data_keys_missing");
    require(logs.find("\"cmd\":\"enqueue_received_item\"") != std::string::npos, "tcp_logs_enqueue_item_missing");
    require(logs.find("\"item_name\":\"Bow\"") != std::string::npos, "tcp_logs_item_name_missing");
    require(logs.find("\"sender_alias\":\"Cloud\"") != std::string::npos, "tcp_logs_sender_alias_missing");
    require(logs.find("\"received_item_count\":1") != std::string::npos, "tcp_logs_received_item_count_missing");
    require(logs.find("\"cmd\":\"sync_room\"") != std::string::npos, "tcp_logs_sync_room_missing");

    std::cout << response.dump(2) << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_room_server_tcp_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
