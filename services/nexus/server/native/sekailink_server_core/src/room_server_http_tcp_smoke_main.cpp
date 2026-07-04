#include "sekailink_server/room_server_http.hpp"

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
std::string http_get(std::uint16_t port, const std::string& path) {
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

  const auto request = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
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
    throw std::runtime_error("http_tcp_smoke_not_supported_on_windows_yet");
#else
    sekailink_server::RoomRegistry registry;
    const auto created = registry.create_room(sekailink_server::RoomSessionConfig{
        .room_id = "http-tcp-room-1",
        .room_type = sekailink_server::RoomType::Live,
        .game = "alttp",
        .team_id = 0,
        .slot_id = 1,
        .slot_name = "Jade",
        .slot_alias = "Jade",
    });
    require(created, "room_create_failed");

    constexpr std::uint16_t kPort = 39183;
    sekailink_server::RoomServerHttpTcpServer server(registry, kPort);
    std::thread worker([&server]() {
      server.serve_one();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto response = http_get(kPort, "/rooms/http-tcp-room-1/snapshot");
    worker.join();

    require(response.find("HTTP/1.1 200 OK") != std::string::npos, "status_line_missing");
    require(response.find("\"room_id\":\"http-tcp-room-1\"") != std::string::npos, "snapshot_missing");

    std::cout << "http_tcp_ok=1\n";
    return 0;
#endif
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_server_http_tcp_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
