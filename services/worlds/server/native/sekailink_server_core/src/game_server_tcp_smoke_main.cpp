#include "sekailink_server/game_server_tcp.hpp"

#include <chrono>
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
    GameSessionRegistry registry;
    GameServerAuthPolicy auth_policy{
        .admin_token = std::string("admin-secret"),
        .core_token = std::string("core-secret"),
        .runtime_token = std::string("runtime-secret"),
    };
    GameServerTcpService service(registry, &auth_policy);
    require(service.start_background(0), "service_start");
    const auto port = service.port();
    require(port != 0, "bound_port");

    const nlohmann::json imported_ap = {
        {"slot_info",
         {
             {"1", {{"name", "Jade"}, {"game", "EarthBound"}, {"type", "player"}}},
             {"2", {{"name", "Ness"}, {"game", "EarthBound"}, {"type", "player"}}},
         }},
        {"locations",
         {
             {"1",
              {
                  {"1001",
                   {
                       {"receiver_slot", 2},
                       {"item_id", 5001},
                       {"item_name", "Sound Stone"},
                       {"location_name", "Onett Drugstore"},
                       {"flags", 0},
                   }},
              }},
         }},
        {"game_options", {{"release_mode", "enabled"}}},
    };

    auto response = nlohmann::json::parse(game_tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "admin"},
            {"auth_token", "admin-secret"},
            {"command",
             {
                 {"cmd", "create_session_from_ap_import"},
                 {"session_name", "tcp-game-session"},
                 {"world_id", "world-tcp-1"},
                 {"world_version", "1.0"},
                 {"seed_id", "seed-tcp-1"},
                 {"seed_hash", "TCP1"},
                 {"linkedworld_id", "earthbound-tcp"},
                 {"archipelago", imported_ap},
             }},
        }));
    require(response["ok"] == true, "create_session");

    response = nlohmann::json::parse(game_tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "admin"},
            {"auth_token", "admin-secret"},
            {"command",
             {
                 {"cmd", "issue_ticket"},
                 {"session_name", "tcp-game-session"},
                 {"slot_id", 1},
                 {"client_kind", "runtime"},
                 {"driver_instance_id", "driver-tcp-1"},
                 {"linkedworld_id", "earthbound-tcp"},
                 {"core_profile", "bsnes"},
             }},
        }));
    require(response["ok"] == true, "runtime_ticket");
    const auto runtime_token = response["ticket"]["session_token"].get<std::string>();

    response = nlohmann::json::parse(game_tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "runtime"},
            {"auth_token", "runtime-secret"},
            {"command",
             {
                 {"cmd", "runtime_event"},
                 {"session_name", "tcp-game-session"},
                 {"session_token", runtime_token},
                 {"slot_id", 1},
                 {"driver_instance_id", "driver-tcp-1"},
                 {"linkedworld_id", "earthbound-tcp"},
                 {"core_profile", "bsnes"},
                 {"event_type", "location_checked"},
                 {"canonical_id", 1001},
             }},
        }));
    require(response["accepted"] == true, "runtime_event");

    response = nlohmann::json::parse(game_tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "admin"},
            {"auth_token", "admin-secret"},
            {"command",
             {
                 {"cmd", "session_summary"},
                 {"session_name", "tcp-game-session"},
             }},
        }));
    require(response["ok"] == true, "session_summary");
    require(response["progress"]["checked_count"] == 1, "checked_count");

    service.stop();
    std::cout << response.dump(2) << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_game_server_tcp_smoke failed: " << exception.what() << "\n";
    return EXIT_FAILURE;
  }
}
