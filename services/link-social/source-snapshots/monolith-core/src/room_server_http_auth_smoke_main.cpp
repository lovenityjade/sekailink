#include "sekailink_server/room_server_http.hpp"

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
    sekailink_server::RoomRegistry registry;
    require(registry.create_room(sekailink_server::RoomSessionConfig{
        .room_id = "http-auth-room-1",
        .room_type = sekailink_server::RoomType::Live,
        .game = "alttp",
        .slot_id = 1,
        .slot_name = "Jade",
        .slot_alias = "Jade",
    }), "room_create_failed");

    sekailink_server::RoomServerAuthPolicy auth_policy{
        .admin_token = std::string("http-admin-secret"),
    };
    const sekailink_server::RoomServerHttpService service(registry, &auth_policy);

    const auto health = service.handle_request("GET", "/health");
    const auto unauthorized = service.handle_request("GET", "/rooms");
    const auto authorized = service.handle_request("GET", "/rooms", std::string("http-admin-secret"));

    require(health.status_code == 200, "health_status");
    require(unauthorized.status_code == 401, "unauthorized_status");
    require(authorized.status_code == 200, "authorized_status");

    std::cout << "http_auth_health=" << health.status_code << "\n";
    std::cout << "http_auth_unauthorized=" << unauthorized.status_code << "\n";
    std::cout << "http_auth_authorized=" << authorized.status_code << "\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_server_http_auth_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
