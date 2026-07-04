#include "sekailink_server/room_server_tcp.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
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
                 {"cmd", "client_reports"},
                 {"room_id", "room-tcp-1"},
             }},
        }));
    require(response["ok"] == true, "client_reports");
    require(response["reports"].size() == 1, "client_reports_size");

    service.stop();

    const auto reports = audit_store.read_client_reports("room-tcp-1");
    require(reports.size() == 1, "audit_reports");
    require(!projection_spool.read_room_records().empty(), "projection_room_records");
    require(!projection_spool.read_client_report_records().empty(), "projection_client_reports");

    std::cout << response.dump(2) << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_room_server_tcp_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
