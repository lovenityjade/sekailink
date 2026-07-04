#include "sekailink_server/room_server_http.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

#ifndef _WIN32
std::string http_get(std::uint16_t port, const std::string& path, bool split_request = false) {
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
  if (!split_request) {
    if (::send(fd, request.data(), request.size(), 0) < 0) {
      ::close(fd);
      throw std::runtime_error("http_client_send_failed");
    }
  } else {
    const auto midpoint = request.size() / 2;
    if (::send(fd, request.data(), midpoint, 0) < 0) {
      ::close(fd);
      throw std::runtime_error("http_client_send_partial_failed");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (::send(fd, request.data() + midpoint, request.size() - midpoint, 0) < 0) {
      ::close(fd);
      throw std::runtime_error("http_client_send_remainder_failed");
    }
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
    std::ostringstream captured_logs;
    auto* original_cout = std::cout.rdbuf(captured_logs.rdbuf());
    const auto created = registry.create_room(sekailink_server::RoomSessionConfig{
        .room_id = "http-tcp-room-1",
        .room_type = sekailink_server::RoomType::Live,
        .game = "alttp",
        .team_id = 0,
        .slot_id = 1,
        .slot_name = "Jade",
        .slot_alias = "Jade",
        .seed_name = "seed-http-tcp-1",
    });
    require(created, "room_create_failed");
    auto* room = registry.find_room("http-tcp-room-1");
    require(room != nullptr, "room_missing");
    room->heartbeat_runtime({
        .runtime_kind = "sklmi",
        .runtime_session_name = "http-tcp-live-1",
        .driver_instance_id = "driver-http-tcp-1",
        .linkedworld_id = "alttp",
        .core_profile = "snes_v1",
        .connected = true,
    });

    constexpr std::uint16_t kPort = 39183;
    sekailink_server::RoomServerHttpTcpServer server(registry, kPort);
    std::thread worker([&server]() {
      server.serve_one();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto response = http_get(kPort, "/rooms/http-tcp-room-1/snapshot", true);
    worker.join();
    std::cout.rdbuf(original_cout);

    require(response.find("HTTP/1.1 200 OK") != std::string::npos, "status_line_missing");
    require(response.find("\"room_id\":\"http-tcp-room-1\"") != std::string::npos, "snapshot_missing");
    const auto logs = captured_logs.str();
    require(logs.find("\"event\":\"room_server_http_request\"") != std::string::npos, "http_log_event_missing");
    require(logs.find("\"path\":\"/rooms/http-tcp-room-1/snapshot\"") != std::string::npos, "http_log_path_missing");
    require(logs.find("\"status\":200") != std::string::npos, "http_log_status_missing");
    require(logs.find("\"room_id\":\"http-tcp-room-1\"") != std::string::npos, "http_log_room_id_missing");
    require(logs.find("\"seed_name\":\"seed-http-tcp-1\"") != std::string::npos, "http_log_seed_name_missing");

    std::cout << "http_tcp_ok=1\n";
    return 0;
#endif
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_server_http_tcp_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
