#include "sekailink_server/room_server_tcp.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

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
    sekailink_server::RoomServerAuthPolicy auth_policy{
        .admin_token = std::string("admin-secret"),
        .runtime_token = std::string("runtime-secret"),
        .client_report_token = std::string("report-secret"),
    };

    sekailink_server::RoomServerTcpService service(registry, nullptr, nullptr, &auth_policy);
    require(service.start_background(0), "tcp_start_failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto unauthorized = sekailink_server::tcp_send_json_line(
        "127.0.0.1",
        service.port(),
        {
            {"channel", "admin"},
            {"command", {{"cmd", "list_rooms"}}},
        });
    const auto authorized = sekailink_server::tcp_send_json_line(
        "127.0.0.1",
        service.port(),
        {
            {"channel", "admin"},
            {"auth_token", "admin-secret"},
            {"command", {{"cmd", "list_rooms"}}},
        });

    service.stop();

    require(unauthorized.find("\"unauthorized\"") != std::string::npos, "tcp_unauthorized_missing");
    require(authorized.find("\"ok\":true") != std::string::npos, "tcp_authorized_missing");

    std::cout << "tcp_auth_ok=1\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_server_tcp_auth_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
