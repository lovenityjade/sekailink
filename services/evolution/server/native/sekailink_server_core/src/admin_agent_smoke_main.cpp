#include "sekailink_server/admin_agent.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <atomic>
#include <thread>

namespace {

std::string http_get(std::uint16_t port, const std::string& path, const std::string& bearer_token) {
#ifdef _WIN32
  throw std::runtime_error("admin_agent_smoke_not_supported_on_windows_yet");
#else
  const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    throw std::runtime_error("http_socket_failed");
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(sock);
    throw std::runtime_error("http_connect_failed");
  }

  const std::string request =
      "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: Bearer " + bearer_token + "\r\nConnection: close\r\n\r\n";
  if (::send(sock, request.data(), request.size(), 0) < 0) {
    ::close(sock);
    throw std::runtime_error("http_send_failed");
  }

  std::string response;
  char buffer[4096];
  while (true) {
    const auto received = ::recv(sock, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      break;
    }
    response.append(buffer, static_cast<std::size_t>(received));
  }
  ::close(sock);
  return response;
#endif
}

}  // namespace

int main() {
#ifdef _WIN32
  std::cerr << "sekailink_admin_agent_smoke failed: not supported on Windows yet\n";
  return 1;
#else
  try {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "sekailink_admin_agent_smoke";
    fs::remove_all(root);
    fs::create_directories(root);

    const fs::path config_path = root / "admin_agent.json";
    const fs::path state_path = root / "room_server_state.json";
    const fs::path log_path = root / "room_server.log";

    {
      std::ofstream state_stream(state_path);
      state_stream << R"({"ok":true,"effective_tcp_port":43123,"effective_http_port":18081})";
    }
    {
      std::ofstream log_stream(log_path);
      log_stream << "first line\n";
      log_stream << "second line\n";
    }
    {
      std::ofstream config_stream(config_path);
      config_stream << R"({
  "http_port": 0,
  "admin_token": "agent-secret",
  "services": [
    {
      "name": "room-server",
      "state_file": ")" << state_path.string() << R"(",
      "log_file": ")" << log_path.string() << R"("
    }
  ]
}
)";
    }

    auto config = sekailink_server::load_admin_agent_config(config_path);
    sekailink_server::AdminAgentHttpServer server(std::move(config));
    if (!server.start()) {
      throw std::runtime_error("admin_agent_start_failed");
    }
    std::atomic<bool> stop_requested{false};
    std::thread server_thread([&]() {
      while (!stop_requested) {
        try {
          server.serve_one();
        } catch (...) {
          if (stop_requested) {
            break;
          }
        }
      }
    });

    const auto health = http_get(server.port(), "/health", "wrong-token");
    if (health.find("200 OK") == std::string::npos || health.find("sekailink_admin_agent") == std::string::npos) {
      throw std::runtime_error("health_failed");
    }

    const auto services = http_get(server.port(), "/services", "agent-secret");
    if (services.find("200 OK") == std::string::npos || services.find("room-server") == std::string::npos) {
      throw std::runtime_error("services_failed");
    }

    const auto state = http_get(server.port(), "/services/room-server", "agent-secret");
    if (state.find("43123") == std::string::npos) {
      throw std::runtime_error("service_state_failed");
    }

    const auto logs = http_get(server.port(), "/services/room-server/logs", "agent-secret");
    if (logs.find("second line") == std::string::npos) {
      throw std::runtime_error("service_logs_failed");
    }

    const auto unauthorized = http_get(server.port(), "/services", "wrong-token");
    if (unauthorized.find("401 Unauthorized") == std::string::npos) {
      throw std::runtime_error("unauthorized_failed");
    }

    stop_requested = true;
    server.stop();
    if (server_thread.joinable()) {
      server_thread.join();
    }
    std::cout << "admin_agent_ok=1\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_admin_agent_smoke failed: " << exception.what() << "\n";
    return 1;
  }
#endif
}
