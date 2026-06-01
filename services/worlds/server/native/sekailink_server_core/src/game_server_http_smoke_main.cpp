#include "sekailink_server/game_server_http.hpp"

#include <iostream>
#include <stdexcept>

using namespace sekailink_server;

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  try {
    GameSessionRegistry registry;
    const nlohmann::json imported_ap = {
        {"slot_info",
         {
             {"1", {{"name", "Jade"}, {"game", "EarthBound"}, {"type", "player"}}},
         }},
        {"locations",
         {
             {"1",
              {
                  {"1001",
                   {
                       {"receiver_slot", 1},
                       {"item_id", 5001},
                       {"item_name", "Sound Stone"},
                       {"location_name", "Onett Drugstore"},
                       {"flags", 0},
                   }},
              }},
         }},
        {"game_options", {{"release_mode", "enabled"}}},
    };
    std::string error;
    require(registry.create_session_from_archipelago_import(
                "http-game-session",
                imported_ap,
                ArchipelagoWorldImportOptions{
                    .world_id = "world-http-1",
                    .world_version = "1.0",
                    .seed_id = "seed-http-1",
                    .seed_hash = "HT1",
                    .linkedworld_id = "earthbound-http",
                },
                &error),
            error);

    GameServerAuthPolicy auth_policy{
        .admin_token = std::string("game-http-admin-secret"),
    };
    const GameServerHttpService service(registry, &auth_policy);

    const auto health = service.handle_request("GET", "/health");
    const auto unauthorized = service.handle_request("GET", "/sessions");
    const auto authorized = service.handle_request("GET", "/sessions", std::string("game-http-admin-secret"));
    const auto summary = service.handle_request("GET", "/sessions/http-game-session/summary", std::string("game-http-admin-secret"));
    const auto rooms = service.handle_request("GET", "/sessions/http-game-session/rooms", std::string("game-http-admin-secret"));

    require(health.status_code == 200, "health_status");
    require(unauthorized.status_code == 401, "unauthorized_status");
    require(authorized.status_code == 200, "authorized_status");
    require(summary.status_code == 200, "summary_status");
    require(rooms.status_code == 200, "rooms_status");
    require(summary.body.find("\"session_name\":\"http-game-session\"") != std::string::npos, "summary_body");
    require(rooms.body.find("\"room_id\":\"game::http-game-session::slot::1\"") != std::string::npos, "rooms_body");

    std::cout << "game_http_health=" << health.status_code << "\n";
    std::cout << "game_http_unauthorized=" << unauthorized.status_code << "\n";
    std::cout << "game_http_authorized=" << authorized.status_code << "\n";
    std::cout << "game_http_summary=" << summary.status_code << "\n";
    std::cout << "game_http_rooms=" << rooms.status_code << "\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_game_server_http_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
