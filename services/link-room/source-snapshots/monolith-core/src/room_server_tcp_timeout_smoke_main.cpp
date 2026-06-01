#include "sekailink_server/room_server_tcp.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

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

#ifndef _WIN32
std::string send_partial_then_wait(std::uint16_t port) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    throw std::runtime_error("socket_create_failed");
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(fd);
    throw std::runtime_error("connect_failed");
  }

  const std::string partial = "{\"channel\":\"admin\"";
  if (::send(fd, partial.data(), partial.size(), 0) < 0) {
    ::close(fd);
    throw std::runtime_error("send_failed");
  }

  char buffer[4096];
  std::string response;
  while (true) {
    const auto received = ::recv(fd, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      break;
    }
    response.append(buffer, static_cast<std::size_t>(received));
    if (response.find('\n') != std::string::npos) {
      break;
    }
  }
  ::close(fd);
  const auto newline = response.find('\n');
  if (newline != std::string::npos) {
    response.resize(newline);
  }
  return response;
}
#endif

}  // namespace

int main() {
#ifdef _WIN32
  std::cerr << "sekailink_room_server_tcp_timeout_smoke failed: not supported on Windows yet\n";
  return EXIT_FAILURE;
#else
  try {
    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_server_tcp_timeout_smoke";
    std::filesystem::remove_all(root);

    RoomRegistry registry;
    RoomAuditStore audit_store(root);
    RoomProjectionSpool projection_spool(root / "projection");
    RoomServerTcpService service(registry, &audit_store, &projection_spool);
    require(service.start_background(0), "start_background");
    const auto port = service.port();
    require(port != 0, "port");

    const auto start = std::chrono::steady_clock::now();
    const auto partial_response = nlohmann::json::parse(send_partial_then_wait(port));
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    require(partial_response["ok"] == false, "partial_request_ok");
    require(partial_response["error"] == "incomplete_request", "partial_request_error");
    require(elapsed_ms >= 2500 && elapsed_ms < 6000, "partial_request_timeout_window");

    const auto response = nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "create_room"},
                 {"room_id", "room-timeout-1"},
                 {"room_type", "live"},
                 {"game", "A Link to the Past"},
                 {"team_id", 0},
                 {"slot_id", 1},
                 {"slot_name", "Jade"},
                 {"slot_alias", "Sekai Jade"},
             }},
        }));
    require(response["ok"] == true, "create_room_after_timeout");

    service.stop();
    std::cout << "room_server_tcp_timeout_ok=1\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_server_tcp_timeout_smoke failed: " << exception.what() << "\n";
    return EXIT_FAILURE;
  }
#endif
}
