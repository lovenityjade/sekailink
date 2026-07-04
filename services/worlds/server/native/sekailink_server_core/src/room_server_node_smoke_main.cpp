#include "sekailink_server/room_server_node.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

#ifndef _WIN32
std::string http_get(std::uint16_t port, const std::string& path, const std::string& bearer = {}) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    throw std::runtime_error("http_client_socket_failed");
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    ::close(fd);
    throw std::runtime_error("http_client_connect_failed");
  }

  std::string request = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
  if (!bearer.empty()) {
    request += "Authorization: Bearer " + bearer + "\r\n";
  }
  request += "\r\n";
  if (::send(fd, request.data(), request.size(), 0) < 0) {
    ::close(fd);
    throw std::runtime_error("http_client_send_failed");
  }

  std::string response;
  char buffer[4096];
  while (true) {
    const auto received = ::recv(fd, buffer, sizeof(buffer), 0);
    if (received == 0) {
      break;
    }
    if (received < 0) {
      ::close(fd);
      throw std::runtime_error("http_client_recv_failed");
    }
    response.append(buffer, static_cast<std::size_t>(received));
  }
  ::close(fd);
  return response;
}
#endif

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  try {
#ifdef _WIN32
    throw std::runtime_error("room_server_node_smoke_not_supported_on_windows_yet");
#else
    sekailink_server::RoomServerNodeConfig config;
    config.tcp_port = 0;
    config.http_port = 0;
    config.auth_policy.admin_token = std::string("node-admin-secret");

    sekailink_server::RoomServerNode node(config);
    require(node.start(), "node_start_failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    const auto create_response = sekailink_server::tcp_send_json_line(
        "127.0.0.1",
        node.tcp_port(),
        {
            {"channel", "admin"},
            {"auth_token", "node-admin-secret"},
            {"command", {
                {"cmd", "create_room"},
                {"room_id", "node-room-1"},
                {"room_type", "async"},
                {"game", "alttp"},
                {"slot_id", 1},
                {"slot_name", "Jade"},
            }},
        });
    const auto effective_http_port = node.http_port();
    const auto http_response = http_get(effective_http_port, "/rooms/node-room-1/snapshot", "node-admin-secret");

    node.stop();

    require(create_response.find("\"ok\":true") != std::string::npos, "tcp_create_failed");
    require(http_response.find("\"room_id\":\"node-room-1\"") != std::string::npos, "http_snapshot_failed");

    std::cout << "room_server_node_ok=1\n";
    return 0;
#endif
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_server_node_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
