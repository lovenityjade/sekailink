#include "sekailink_server/room_server_tcp.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

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
    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_server_tcp_loop_smoke";
    std::filesystem::remove_all(root);

    RoomRegistry registry;
    RoomAuditStore audit_store(root);
    RoomProjectionSpool projection_spool(root / "projection");
    RoomServerTcpService service(registry, &audit_store, &projection_spool);
    require(service.start_background(0), "start_background");
    const auto port = service.port();
    require(port != 0, "port");

    std::vector<nlohmann::json> responses;
    responses.push_back(nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "create_room"},
                 {"room_id", "room-loop-1"},
                 {"room_type", "live"},
                 {"game", "A Link to the Past"},
                 {"team_id", 0},
                 {"slot_id", 1},
                 {"slot_name", "Jade"},
                 {"slot_alias", "Sekai Jade"},
             }},
        })));
    responses.push_back(nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "mark_emu_connected"},
                 {"room_id", "room-loop-1"},
                 {"connected", true},
             }},
        })));
    responses.push_back(nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "runtime"},
            {"command",
             {
                 {"cmd", "record_check"},
                 {"room_id", "room-loop-1"},
                 {"location_id", 12345},
             }},
        })));
    responses.push_back(nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "room_summary"},
                 {"room_id", "room-loop-1"},
             }},
        })));

    require(responses[0]["ok"] == true, "create_room");
    require(responses[1]["ok"] == true, "mark_emu_connected");
    require(responses[2]["ok"] == true, "record_check");
    require(responses[3]["ok"] == true, "room_summary");
    require(responses[3]["summary"]["checks_recorded"] == 1, "checks_recorded");
    require(responses[3]["summary"]["emu_connections"] == 1, "emu_connections");

    service.stop();
    require(!projection_spool.read_room_records().empty(), "projection_room_records");
    require(!projection_spool.read_room_event_records().empty(), "projection_room_event_records");

    std::cout << responses[3].dump(2) << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_room_server_tcp_loop_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
